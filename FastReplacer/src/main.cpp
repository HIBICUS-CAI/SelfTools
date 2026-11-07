// FastReplacer - フォルダ内の UTF-8 テキストファイルを高速に検索・置換する
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Matcher.h"
#include "Pager.h"
#include "Platform.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef _WIN32
#define FR_NATIVE(s) L##s
#else
#define FR_NATIVE(s) s
#endif

static_assert(sizeof(void*) == 8, "FastReplacer は 64bit ビルド専用です");

namespace
{
// ============================================================================
// 設定
// ============================================================================
struct Config
{
    std::string folder;
    std::vector<std::string> finds;
    std::string replace;
    bool hasReplace = false;
    bool searchOnly = false;
    bool recursive = false;
    bool escapes = false;
    bool sequential = false;  // ファイル間の重ね合わせを行わない
    bool viewTempFile = false;  // ページャ表示用の結果を一時ファイルに置く (既定はメモリ)
    int width = 0;              // 1 項目の表示幅 (0 = 端末の幅)
    int textWidth = 160;        // テキスト出力 (標準出力 / -o) で使う実際の幅
    MatchOptions opt;
    std::string reportPath;
    unsigned threads = 0;
    uint64_t chunk = 32ull << 20;
};

void PrintUsage()
{
    std::fputs(
        "FastReplacer - フォルダ内の全ファイルを UTF-8 テキストとして高速に検索/置換\n"
        "\n"
        "使い方:\n"
        "  FastReplacer <フォルダ> -f <検索> [-f <検索2> ...] -r <置換> [オプション]   置換モード\n"
        "  FastReplacer <フォルダ> -f <検索> [-f <検索2> ...] -s [オプション]          検索のみ (レポート出力)\n"
        "\n"
        "オプション:\n"
        "  -f, --find <text>      検索文字列 (必須、複数指定可。全て同じ置換文字列に置き換える)\n"
        "  -r, --replace <text>   置換文字列 (空文字列可 = 削除)\n"
        "  -s, --search-only      検索のみ。結果を画面に一部ずつ表示する (↑↓ PgUp/PgDn Home/End ←→ で移動、q で閉じる)\n"
        "                         リダイレクト/パイプ時は全件を標準出力へ流す\n"
        "  -c, --match-case       大文字/小文字を区別する (既定: 区別しない)\n"
        "  -w, --whole-word       単語単位で検索する (既定: 無効)\n"
        "  -R, --recursive        サブフォルダも対象にする (既定: 指定フォルダ直下のみ)\n"
        "  -e, --escape           検索/置換文字列のエスケープを解釈する\n"
        "                         (\\\\ \\n \\r \\t \\0 \\xHH \\uXXXX \\UXXXXXXXX)\n"
        "  -o, --report <path>    結果をファイルに書き出す (指定時のみ。検索モードでは画面表示の代わり)\n"
        "  -W, --width <cols>     結果 1 項目の表示幅 (桁)。検索文字を中央に、前後文を 1 行に収める\n"
        "                         (既定: 端末の幅。取得できなければ 160)\n"
        "  -t, --threads <n>      スレッド数 (既定: 論理コア数)\n"
        "      --tempfile         画面表示用の結果をメモリではなく一時ファイルに置く (結果が膨大な場合向け)\n"
        "      --sequential       ファイルを 1 つずつ処理する (既定は書き出しと次ファイルの検索を重ねる。\n"
        "                         HDD など読み書きの同時実行が遅いディスク向け)\n"
        "  -h, --help             このヘルプ\n"
        "\n"
        "短いオプションはまとめて指定できる (例: -cwR)。値を取るオプションをまとめた場合、\n"
        "値は後続の引数から順に取る (例: -rc <置換>、-fr <検索> <置換>)。\n"
        "\n"
        "終了コード: 0=成功 1=一部ファイルでエラー 2=引数エラー\n",
        stdout);
}

bool ParseEscapes(const std::string& in, std::string& out, std::string& err)
{
    out.clear();
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] != '\\')
        {
            out.push_back(in[i]);
            continue;
        }
        if (++i >= in.size())
        {
            err = "末尾の '\\' が不正です";
            return false;
        }
        const char c = in[i];
        int hex = 0;
        switch (c)
        {
        case '\\': out.push_back('\\'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case '0': out.push_back('\0'); break;
        case 'x': hex = 2; break;
        case 'u': hex = 4; break;
        case 'U': hex = 8; break;
        default: err = std::string("不明なエスケープ \\") + c; return false;
        }
        if (hex)
        {
            if (i + (size_t)hex >= in.size())
            {
                err = std::string("エスケープ \\") + c + " の桁数が不足しています";
                return false;
            }
            uint32_t v = 0;
            for (int k = 1; k <= hex; ++k)
            {
                const char h = in[i + k];
                int d = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : (h >= 'A' && h <= 'F') ? h - 'A' + 10 : -1;
                if (d < 0)
                {
                    err = std::string("エスケープ \\") + c + " に 16 進以外の文字があります";
                    return false;
                }
                v = v * 16 + (uint32_t)d;
            }
            if (v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF))
            {
                err = "エスケープのコードポイントが不正です";
                return false;
            }
            utf8::Append(out, v);
            i += (size_t)hex;
        }
    }
    return true;
}

// 戻り値: 0=続行 1=ヘルプ表示済み 2=エラー
int ParseArgs(const std::vector<std::string>& a, Config& cfg)
{
    enum Opt
    {
        kFind,
        kReplace,
        kSearch,
        kCase,
        kWord,
        kRec,
        kEsc,
        kReport,
        kThreads,
        kWidth,
        kSeq,
        kTemp,
        kChunk,
        kHelp,
        kNone
    };
    struct Def
    {
        char shortName;
        const char* longName;
        Opt opt;
        bool value;
    };
    static const Def defs[] = {
        {'f', "find", kFind, true},          {'r', "replace", kReplace, true},  {'s', "search-only", kSearch, false},
        {'c', "match-case", kCase, false},   {'w', "whole-word", kWord, false}, {'R', "recursive", kRec, false},
        {'e', "escape", kEsc, false},        {'o', "report", kReport, true},
        {'t', "threads", kThreads, true},    {'W', "width", kWidth, true},    {0, "sequential", kSeq, false}, {0, "tempfile", kTemp, false},         {0, "chunk-size", kChunk, true},  // テスト用 (非公開)
        {'h', "help", kHelp, false},
    };

    bool hasFind = false;
    auto apply = [&](const Def& d, const std::string& v) -> int
    {
        switch (d.opt)
        {
        case kFind:
            cfg.finds.push_back(v);
            hasFind = true;
            break;
        case kReplace:
            if (cfg.hasReplace)
            {
                std::fprintf(stderr, "エラー: -r は 1 つだけ指定できます\n");
                return 2;
            }
            cfg.replace = v;
            cfg.hasReplace = true;
            break;
        case kSearch: cfg.searchOnly = true; break;
        case kCase: cfg.opt.matchCase = true; break;
        case kWord: cfg.opt.wholeWord = true; break;
        case kRec: cfg.recursive = true; break;
        case kEsc: cfg.escapes = true; break;
        case kReport: cfg.reportPath = v; break;
        case kThreads: cfg.threads = (unsigned)std::strtoul(v.c_str(), nullptr, 10); break;
        case kWidth: cfg.width = (int)std::strtol(v.c_str(), nullptr, 10); break;
        case kSeq: cfg.sequential = true; break;
        case kTemp: cfg.viewTempFile = true; break;
        case kChunk: cfg.chunk = std::max<uint64_t>(1, std::strtoull(v.c_str(), nullptr, 10)); break;
        case kHelp: PrintUsage(); return 1;
        default: break;
        }
        return 0;
    };

    for (size_t i = 1; i < a.size(); ++i)
    {
        const std::string& s = a[i];
        if (s == "/?")
        {
            PrintUsage();
            return 1;
        }
        if (s.size() > 2 && s[0] == '-' && s[1] == '-')
        {
            // 長いオプション (--name / --name=value)
            const size_t eq = s.find('=');
            const std::string name = s.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
            const Def* d = nullptr;
            for (const Def& x : defs)
                if (name == x.longName) d = &x;
            if (!d)
            {
                std::fprintf(stderr, "エラー: 不明なオプション %s\n", s.c_str());
                return 2;
            }
            std::string v;
            if (d->value)
            {
                if (eq != std::string::npos)
                    v = s.substr(eq + 1);
                else if (i + 1 < a.size())
                    v = a[++i];
                else
                {
                    std::fprintf(stderr, "エラー: --%s に値がありません\n", d->longName);
                    return 2;
                }
            }
            else if (eq != std::string::npos)
            {
                std::fprintf(stderr, "エラー: --%s は値を取りません\n", d->longName);
                return 2;
            }
            if (int r = apply(*d, v)) return r;
        }
        else if (s.size() > 1 && s[0] == '-')
        {
            // 短いオプション (まとめ指定可)。値を取るものは後続の引数から順に値を取る
            std::vector<const Def*> needValue;
            for (size_t k = 1; k < s.size(); ++k)
            {
                const Def* d = nullptr;
                for (const Def& x : defs)
                    if (x.shortName && x.shortName == s[k]) d = &x;
                if (!d)
                {
                    std::fprintf(stderr, "エラー: 不明なオプション -%c (%s)\n", s[k], s.c_str());
                    return 2;
                }
                if (d->value)
                    needValue.push_back(d);
                else if (int r = apply(*d, std::string()))
                    return r;
            }
            for (const Def* d : needValue)
            {
                if (i + 1 >= a.size())
                {
                    std::fprintf(stderr, "エラー: -%c に値がありません\n", d->shortName);
                    return 2;
                }
                if (int r = apply(*d, a[++i])) return r;
            }
        }
        else if (cfg.folder.empty())
            cfg.folder = s;
        else
        {
            std::fprintf(stderr, "エラー: 余分な引数 %s\n", s.c_str());
            return 2;
        }
    }
    if (cfg.folder.empty() || !hasFind)
    {
        PrintUsage();
        return 2;
    }
    if (cfg.searchOnly == cfg.hasReplace)
    {
        std::fprintf(stderr, "エラー: -r <置換文字列> か -s のどちらか一方を指定してください\n");
        return 2;
    }
    if (cfg.escapes)
    {
        std::string err, t;
        for (size_t k = 0; k < cfg.finds.size(); ++k)
        {
            if (!ParseEscapes(cfg.finds[k], t, err))
            {
                std::fprintf(stderr, "エラー: 検索文字列 #%zu: %s\n", k + 1, err.c_str());
                return 2;
            }
            cfg.finds[k] = t;
        }
        if (!ParseEscapes(cfg.replace, t, err))
        {
            std::fprintf(stderr, "エラー: 置換文字列: %s\n", err.c_str());
            return 2;
        }
        cfg.replace = t;
    }
    if (cfg.threads == 0) cfg.threads = std::max(1u, std::thread::hardware_concurrency());
    return 0;
}

// ============================================================================
// 並列ユーティリティ
// ============================================================================
void ParallelFor(unsigned threads, size_t count, const std::function<void(size_t)>& fn)
{
    if (count == 0) return;
    const unsigned nt = (unsigned)std::min<size_t>(threads, count);
    std::atomic<size_t> next{0};
    auto body = [&]
    {
        for (size_t i; (i = next.fetch_add(1, std::memory_order_relaxed)) < count;) fn(i);
    };
    std::vector<std::thread> ts;
    ts.reserve(nt);
    for (unsigned k = 1; k < nt; ++k) ts.emplace_back(body);
    body();
    for (auto& t : ts) t.join();
}

// produce(i, buf) を並列に実行し、consume(i, buf) を i の昇順に呼び出し側スレッドで実行する。
// 同時に保持するバッファは window 個まで。consume が false を返すと中断。
template <class Buf, class Produce, class Consume>
bool OrderedPipeline(unsigned threads, size_t count, Produce produce, Consume consume)
{
    if (count == 0) return true;
    const size_t window = std::max<size_t>(4, (size_t)threads * 2);
    std::vector<Buf> bufs(count);
    std::vector<char> ready(count, 0);
    std::mutex mu;
    std::condition_variable cvReady, cvSpace;
    size_t next = 0, done = 0;
    bool abort = false;
    auto worker = [&]
    {
        for (;;)
        {
            size_t i;
            {
                std::unique_lock<std::mutex> lk(mu);
                cvSpace.wait(lk, [&] { return abort || next >= count || next < done + window; });
                if (abort || next >= count) return;
                i = next++;
            }
            produce(i, bufs[i]);
            {
                std::lock_guard<std::mutex> lk(mu);
                ready[i] = 1;
            }
            cvReady.notify_all();
        }
    };
    const unsigned nt = (unsigned)std::max<size_t>(1, std::min<size_t>(threads, count));
    std::vector<std::thread> ts;
    ts.reserve(nt);
    for (unsigned k = 0; k < nt; ++k) ts.emplace_back(worker);
    bool ok = true;
    for (size_t i = 0; i < count && ok; ++i)
    {
        {
            std::unique_lock<std::mutex> lk(mu);
            cvReady.wait(lk, [&] { return ready[i] != 0; });
        }
        ok = consume(i, bufs[i]);
        Buf().swap(bufs[i]);
        {
            std::lock_guard<std::mutex> lk(mu);
            ++done;
            if (!ok) abort = true;
        }
        cvSpace.notify_all();
    }
    for (auto& th : ts) th.join();
    return ok;
}

// 4096 境界整列ブロックの再利用プール
class BlockPool
{
public:
    explicit BlockPool(uint64_t size) : size_(size) {}
    ~BlockPool()
    {
        for (void* p : free_) plat::FreeAligned(p);
    }
    uint8_t* Get()
    {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (!free_.empty())
            {
                void* p = free_.back();
                free_.pop_back();
                return (uint8_t*)p;
            }
        }
        return (uint8_t*)plat::AllocAligned(size_);
    }
    void Put(uint8_t* p)
    {
        if (!p) return;
        std::lock_guard<std::mutex> lk(mu_);
        free_.push_back(p);
    }

private:
    uint64_t size_;
    std::mutex mu_;
    std::vector<void*> free_;
};

struct PooledBlock
{
    BlockPool* pool = nullptr;
    uint8_t* p = nullptr;
    uint64_t n = 0;
    PooledBlock() = default;
    PooledBlock(const PooledBlock&) = delete;
    PooledBlock& operator=(const PooledBlock&) = delete;
    ~PooledBlock()
    {
        if (pool) pool->Put(p);
    }
    void swap(PooledBlock& o) noexcept
    {
        std::swap(pool, o.pool);
        std::swap(p, o.p);
        std::swap(n, o.n);
    }
};

// ============================================================================
// レポート整形
// ============================================================================
std::string FormatBytes(uint64_t b)
{
    char buf[64];
    if (b >= (1ull << 30))
        std::snprintf(buf, sizeof(buf), "%.2f GiB", b / double(1ull << 30));
    else if (b >= (1ull << 20))
        std::snprintf(buf, sizeof(buf), "%.2f MiB", b / double(1ull << 20));
    else
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)b);
    return buf;
}

std::string NowString(bool compact)
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), compact ? "%Y%m%d_%H%M%S" : "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// 表示用に不正 UTF-8 や制御文字をエスケープして追加
void AppendVisible(std::string& out, const uint8_t* p, uint64_t len)
{
    static const char kHex[] = "0123456789ABCDEF";
    uint64_t i = 0, run = 0;
    while (i < len)
    {
        const uint8_t b = p[i];
        if (b >= 0x20 && b < 0x7F)
        {
            ++i;
            continue;
        }
        if (b >= 0x80)
        {
            if (const int l = utf8::ValidLen(p + i, len - i))
            {
                i += (uint64_t)l;
                continue;
            }
        }
        out.append((const char*)p + run, (size_t)(i - run));
        if (b == '\r')
            out += "\\r";
        else if (b == '\n')
            out += "\\n";
        else if (b == '\t')
            out += "\\t";
        else
        {
            const char e[4] = {'\\', 'x', kHex[b >> 4], kHex[b & 15]};
            out.append(e, 4);
        }
        run = ++i;
    }
    out.append((const char*)p + run, (size_t)(len - run));
}

inline void AppendU64(std::string& out, uint64_t v)
{
    char buf[24];
    char* e = buf + sizeof(buf);
    char* q = e;
    do
    {
        *--q = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    out.append(q, (size_t)(e - q));
}

// ============================================================================
// ファイル処理 (検索 → 出力 の 2 段パイプライン。段ごとにファイル順を保つ)
// ============================================================================
// ページャ表示用の 1 項目 (前後文は表示時にファイルから作る)
struct Item
{
    uint64_t pos;
    uint64_t line;
    uint64_t col;  // 1 始まり
    uint32_t len;
    uint32_t pat;
};

std::string ItemLabel(uint64_t line, uint64_t col, uint64_t pos, uint32_t pat, bool multi)
{
    std::string s = "  L";
    AppendU64(s, line);
    s += ":C";
    AppendU64(s, col);
    s += "  @";
    AppendU64(s, pos);
    if (multi)
    {
        s += "  #";
        AppendU64(s, (uint64_t)pat + 1);
    }
    s += "  ";
    return s;
}

// テキスト出力用の 1 行 (検索文字を中央に、全体を width 桁に収める。一致は【】で囲む)
void AppendItemText(std::string& out, const uint8_t* t, uint64_t n, const Match& m, uint64_t line, uint64_t col, bool multi, int width)
{
    const std::string label = ItemLabel(line, col, m.pos, m.pat, multi);
    const int budget = std::max(8, width - view::TextWidth(label) - 4);
    view::Context cx;
    view::BuildContext(t, n, m.pos, m.len, budget, cx);
    out += label;
    out.append((size_t)cx.pad, ' ');
    out += cx.left;
    out += "【";
    out += cx.match;
    out += "】";
    out += cx.right;
    out.push_back('\n');
}

// ページャに渡す表示データ (見出し等のテキスト行と、一致項目の並び)
class ViewData : public pager::Source
{
public:
    ViewData(int fixedWidth, bool multi) : fixedWidth_(fixedWidth), multi_(multi) {}
    ~ViewData() override { Cleanup(); }

    bool Begin(bool tempFile, std::string& err)
    {
        if (!tempFile) return true;
        tmpPath_ = plat::TempFilePath("FastReplacer_view_");
        if (!tmp_.Open(tmpPath_, err)) return false;
        useTmp_ = true;
        return true;
    }
    void AddText(const std::string& text)  // 改行で分割して追加
    {
        size_t b = 0;
        while (b <= text.size())
        {
            size_t e = text.find('\n', b);
            if (e == std::string::npos) e = text.size();
            if (e == text.size() && b == e && b != 0) break;  // 末尾の改行
            if (segs_.empty() || segs_.back().items || segs_.back().first + segs_.back().count != texts_.size())
                PushSeg(false, 0, texts_.size());
            texts_.push_back(text.substr(b, e - b));
            ++segs_.back().count;
            ++rows_;
            b = e + 1;
        }
    }
    void SetFile(uint32_t file, const NativePath& path)
    {
        if (paths_.size() <= file) paths_.resize(file + 1);
        paths_[file] = path;
    }
    void AddItems(uint32_t file, const std::vector<Item>& v)
    {
        if (v.empty()) return;
        if (segs_.empty() || !segs_.back().items || segs_.back().file != file) PushSeg(true, file, itemCount_);
        if (useTmp_)
            tmp_.Write(std::string((const char*)v.data(), v.size() * sizeof(Item)));
        else
            mem_.insert(mem_.end(), v.begin(), v.end());
        itemCount_ += v.size();
        segs_.back().count += v.size();
        rows_ += v.size();
    }
    bool Finish(std::string& err)
    {
        if (!useTmp_)
        {
            items_ = mem_.data();
            return true;
        }
        if (!tmp_.Close())
        {
            err = "一時ファイルへの書き込みに失敗しました";
            return false;
        }
        if (itemCount_ && !tmpIn_.Open(tmpPath_, err)) return false;
        items_ = (const Item*)tmpIn_.Data();
        return true;
    }
    void Cleanup()
    {
        cache_.clear();
        if (useTmp_)
        {
            tmp_.Close();
            tmpIn_.Close();
            plat::DeleteFileQuiet(tmpPath_);
            useTmp_ = false;
        }
    }

    uint64_t Count() override { return rows_; }

    void Render(uint64_t row, int cols, uint64_t hscroll, std::string& out) override
    {
        const size_t si = (size_t)(std::upper_bound(segStart_.begin(), segStart_.end(), row) - segStart_.begin()) - 1;
        const Seg& sg = segs_[si];
        const uint64_t idx = sg.first + (row - segStart_[si]);
        uint64_t col = 0;
        if (!sg.items)
        {
            view::AppendClipped(out, texts_[(size_t)idx], col, hscroll, (uint64_t)cols);
            return;
        }
        const Item& it = items_[idx];
        const std::string label = ItemLabel(it.line, it.col, it.pos, it.pat, multi_);
        const plat::InputFile* f = File(sg.file);
        if (!f)
        {
            view::AppendClipped(out, label + "(ファイルを開けません)", col, hscroll, (uint64_t)cols);
            return;
        }
        const int width = (fixedWidth_ > 0 ? fixedWidth_ : cols) + (int)hscroll;
        const int budget = std::max(8, width - view::TextWidth(label));
        view::Context cx;
        view::BuildContext(f->Data(), f->Size(), it.pos, it.len, budget, cx);
        view::AppendClipped(out, label, col, hscroll, (uint64_t)cols);
        view::AppendClipped(out, std::string((size_t)cx.pad, ' '), col, hscroll, (uint64_t)cols);
        view::AppendClipped(out, cx.left, col, hscroll, (uint64_t)cols);
        out += "\x1b[30;43m";  // 一致部分を強調
        view::AppendClipped(out, cx.match, col, hscroll, (uint64_t)cols);
        out += "\x1b[0m";
        view::AppendClipped(out, cx.right, col, hscroll, (uint64_t)cols);
    }

private:
    struct Seg
    {
        bool items;
        uint32_t file;
        uint64_t first;
        uint64_t count;
    };
    void PushSeg(bool items, uint32_t file, uint64_t first)
    {
        segStart_.push_back(rows_);
        segs_.push_back(Seg{items, file, first, 0});
    }
    // 表示中のファイルを開いておく (最近使った 8 個)
    const plat::InputFile* File(uint32_t file)
    {
        for (size_t i = 0; i < cache_.size(); ++i)
            if (cache_[i].first == file)
            {
                if (i) std::rotate(cache_.begin(), cache_.begin() + (ptrdiff_t)i, cache_.begin() + (ptrdiff_t)i + 1);
                return cache_[0].second.get();
            }
        auto f = std::make_unique<plat::InputFile>();
        std::string err;
        if (file >= paths_.size() || !f->Open(paths_[file], err)) return nullptr;
        cache_.insert(cache_.begin(), {file, std::move(f)});
        if (cache_.size() > 8) cache_.pop_back();
        return cache_[0].second.get();
    }

    const int fixedWidth_;
    const bool multi_;
    std::vector<std::string> texts_;
    std::vector<Seg> segs_;
    std::vector<uint64_t> segStart_;
    uint64_t rows_ = 0;
    std::vector<Item> mem_;
    uint64_t itemCount_ = 0;
    const Item* items_ = nullptr;
    bool useTmp_ = false;
    NativePath tmpPath_;
    plat::TextWriter tmp_;
    plat::InputFile tmpIn_;
    std::vector<NativePath> paths_;
    std::vector<std::pair<uint32_t, std::unique_ptr<plat::InputFile>>> cache_;
};

struct FileStat
{
    uint64_t size = 0;
    uint64_t matches = 0;
    uint64_t newSize = 0;
    double seconds = 0;
    bool error = false;
    std::string message;
};

struct Job
{
    size_t index = 0;
    NativePath path;
    plat::InputFile in;
    std::chrono::steady_clock::time_point t0;
    bool openFailed = false;
    std::string openErr;
    std::vector<Match> all;
    FileStat st;
};

class Processor
{
public:
    Processor(const Config& cfg, const MultiMatcher& m, plat::TextWriter* report, ViewData* view)
        : cfg_(cfg), m_(m), report_(report), view_(view)
    {
    }

    // 全ファイルを処理し、ファイル順に onFileDone(i, stat) を呼ぶ (出力段のスレッドから)
    void Run(const std::vector<NativePath>& files, const std::function<void(size_t, const FileStat&)>& onFileDone);

private:
    // 検索段が出力段より先行できるファイル数 (1 = 出力中のファイルの次を検索)
    static constexpr size_t kLookahead = 1;

    void SearchLane();
    void OutputLane(const std::function<void(size_t, const FileStat&)>& onFileDone);

    void FindMatches(Job& job);
    void WriteSearchReport(const Job& job, plat::TextWriter* rep, ViewData* view);
    bool WriteReplaced(const Job& job, const NativePath& tmp, uint64_t newSize, std::string& err);
    void Output(Job& job);

    const Config& cfg_;
    const MultiMatcher& m_;
    plat::TextWriter* report_;
    ViewData* view_;

    std::vector<std::unique_ptr<Job>> jobs_;
    // 段の進行管理
    std::mutex mu_;
    std::condition_variable cv_;
    size_t searched_ = 0;  // 検索を終えたジョブ数
    size_t released_ = 0;  // 出力を終えて解放したジョブ数
};

void Processor::Run(const std::vector<NativePath>& files, const std::function<void(size_t, const FileStat&)>& onFileDone)
{
    jobs_.clear();
    for (const auto& f : files)
    {
        jobs_.push_back(std::make_unique<Job>());
        jobs_.back()->path = f;
        jobs_.back()->index = jobs_.size() - 1;
    }
    std::thread output([&] { OutputLane(onFileDone); });
    SearchLane();
    output.join();
}

void Processor::SearchLane()
{
    const size_t ahead = cfg_.sequential ? 0 : kLookahead;
    for (size_t i = 0; i < jobs_.size(); ++i)
    {
        Job& job = *jobs_[i];
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return i <= released_ + ahead; });
        }
        job.t0 = std::chrono::steady_clock::now();
        std::string err;
        if (!job.in.Open(job.path, err))
        {
            job.openFailed = true;
            job.openErr = err;
        }
        else if (job.in.Size() > 0)
            FindMatches(job);
        {
            std::lock_guard<std::mutex> lk(mu_);
            searched_ = i + 1;
        }
        cv_.notify_all();
    }
}

void Processor::OutputLane(const std::function<void(size_t, const FileStat&)>& onFileDone)
{
    for (size_t i = 0; i < jobs_.size(); ++i)
    {
        Job& job = *jobs_[i];
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return searched_ > i; });
        }
        Output(job);
        job.in.Close();
        job.st.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - job.t0).count();
        onFileDone(i, job.st);
        std::vector<Match>().swap(job.all);
        {
            std::lock_guard<std::mutex> lk(mu_);
            released_ = i + 1;
        }
        cv_.notify_all();
    }
}

void Processor::FindMatches(Job& job)
{
    const uint8_t* t = job.in.Data();
    const uint64_t n = job.in.Size();
    const uint64_t cs = cfg_.chunk;
    const size_t nChunks = (size_t)((n + cs - 1) / cs);
    std::vector<std::vector<Match>> per(nChunks);
    ParallelFor(cfg_.threads, nChunks,
                [&](size_t c)
                {
                    const uint64_t b = c * cs, e = std::min(n, b + cs);
                    job.in.Prefetch(b, e - b + m_.MaxLen());
                    m_.FindAll(t, n, b, e, per[c]);
                });

    // チャンク境界をまたぐ重なりを解消 (逐次スキャンと完全に同じ結果にする)
    size_t total = 0;
    for (auto& v : per) total += v.size();
    std::vector<Match>& all = job.all;
    all.clear();
    all.reserve(total);
    uint64_t lastEnd = 0;
    for (size_t c = 0; c < nChunks; ++c)
    {
        std::vector<Match>& L = per[c];
        const uint64_t e = std::min(n, (uint64_t)(c + 1) * cs);
        size_t idx = 0;
        while (idx < L.size() && L[idx].pos < lastEnd) ++idx;
        if (idx > 0)
        {
            // 捨てた一致の影で見逃した一致を逐次スキャンで拾い、リストと再同期する
            uint64_t from = lastEnd;
            for (;;)
            {
                Match mm;
                if (!m_.FindNext(t, n, from, e, mm))
                {
                    idx = L.size();
                    break;
                }
                while (idx < L.size() && L[idx].pos < mm.pos) ++idx;
                if (idx < L.size() && L[idx].pos == mm.pos) break;  // 再同期
                all.push_back(mm);
                lastEnd = from = mm.pos + mm.len;
            }
        }
        for (; idx < L.size(); ++idx)
        {
            all.push_back(L[idx]);
            lastEnd = L[idx].pos + L[idx].len;
        }
        std::vector<Match>().swap(L);
    }
}

void Processor::WriteSearchReport(const Job& job, plat::TextWriter* rep, ViewData* view)
{
    const uint8_t* t = job.in.Data();
    const uint64_t n = job.in.Size();
    const std::vector<Match>& all = job.all;
    const uint64_t cs = cfg_.chunk;
    const size_t nChunks = (size_t)((n + cs - 1) / cs);
    const bool multi = m_.Count() > 1;

    // 1) チャンクごとの改行数と「最後の改行以降の文字数」
    struct Sum
    {
        uint64_t nl = 0, tailChars = 0;
        size_t mBegin = 0, mEnd = 0;
    };
    std::vector<Sum> sums(nChunks);
    {
        size_t j = 0;
        for (size_t c = 0; c < nChunks; ++c)
        {
            const uint64_t e = std::min(n, (uint64_t)(c + 1) * cs);
            sums[c].mBegin = j;
            while (j < all.size() && all[j].pos < e) ++j;
            sums[c].mEnd = j;
        }
    }
    ParallelFor(cfg_.threads, nChunks,
                [&](size_t c)
                {
                    const uint64_t b = c * cs, e = std::min(n, b + cs);
                    sums[c].nl = simd::CountByte(t + b, e - b, '\n');
                    const uint8_t* last = sums[c].nl ? simd::FindLastByte(t + b, e - b, '\n') : nullptr;
                    const uint64_t from = last ? (uint64_t)(last - t) + 1 : b;
                    sums[c].tailChars = simd::CountCodePoints(t + from, e - from);
                });

    // 2) 先頭からの累積 (行番号の基準, 行頭からの文字数の持ち越し)
    std::vector<uint64_t> baseLine(nChunks), carryCol(nChunks);
    uint64_t line = 1, col = 0;
    for (size_t c = 0; c < nChunks; ++c)
    {
        baseLine[c] = line;
        carryCol[c] = col;
        line += sums[c].nl;
        col = sums[c].nl ? sums[c].tailChars : col + sums[c].tailChars;
    }

    // 3) 見出し (複数パターン時は内訳も)
    std::string head = "=== " + plat::ToUtf8(job.path) + "  (";
    AppendU64(head, all.size());
    head += " 件";
    if (multi)
    {
        std::vector<uint64_t> cnt(m_.Count(), 0);
        for (const Match& m : all) ++cnt[m.pat];
        head += ":";
        for (size_t k = 0; k < cnt.size(); ++k)
        {
            head += " #";
            AppendU64(head, k + 1);
            head += "=";
            AppendU64(head, cnt[k]);
        }
    }
    head += ", " + FormatBytes(n) + ")\n";
    if (view)
    {
        view->SetFile((uint32_t)job.index, job.path);
        view->AddText(head);
    }
    else
        rep->Write(head);

    // 4) 並列に整形 (テキスト) または項目化 (ページャ) しつつ順に出力する
    struct Out
    {
        std::string text;
        std::vector<Item> items;
        void swap(Out& o) noexcept
        {
            text.swap(o.text);
            items.swap(o.items);
        }
    };
    const int width = cfg_.textWidth;
    OrderedPipeline<Out>(
        cfg_.threads, nChunks,
        [&](size_t c, Out& out)
        {
            const Sum& s = sums[c];
            if (s.mBegin == s.mEnd) return;
            if (view)
                out.items.reserve(s.mEnd - s.mBegin);
            else
                out.text.reserve((s.mEnd - s.mBegin) * (size_t)(width + 16));
            uint64_t pos = c * cs, ln = baseLine[c], cl = carryCol[c];
            for (size_t k = s.mBegin; k < s.mEnd; ++k)
            {
                const Match& m = all[k];
                const uint64_t gap = m.pos - pos;
                const uint64_t nl = simd::CountByte(t + pos, gap, '\n');
                if (nl)
                {
                    ln += nl;
                    const uint8_t* last = simd::FindLastByte(t + pos, gap, '\n');
                    cl = simd::CountCodePoints(last + 1, (uint64_t)(t + m.pos - (last + 1)));
                }
                else
                    cl += simd::CountCodePoints(t + pos, gap);
                pos = m.pos;
                if (view)
                    out.items.push_back(Item{m.pos, ln, cl + 1, m.len, m.pat});
                else
                    AppendItemText(out.text, t, n, m, ln, cl + 1, multi, width);
            }
        },
        [&](size_t, Out& out)
        {
            if (view)
                view->AddItems((uint32_t)job.index, out.items);
            else
                rep->Write(out.text);
            return true;
        });
    if (view)
        view->AddText("");
    else
        rep->Write("\n");
}

bool Processor::WriteReplaced(const Job& job, const NativePath& tmp, uint64_t newSize, std::string& err)
{
    const uint8_t* t = job.in.Data();
    const std::vector<Match>& all = job.all;
    const std::string& rep = cfg_.replace;
    const uint64_t rl = rep.size();
    const size_t M = all.size();

    // 各一致の置換文字列が出力中で始まる位置
    std::vector<uint64_t> outStart(M);
    {
        int64_t cum = 0;
        for (size_t i = 0; i < M; ++i)
        {
            outStart[i] = (uint64_t)((int64_t)all[i].pos + cum);
            cum += (int64_t)rl - (int64_t)all[i].len;
        }
    }

    // 出力を固定長ブロックに分割し、各ブロックを並列に直接生成 → 先頭から順に書き込む
    // (順次書き込みにより NTFS の VDL ゼロ埋めを避ける)
    const uint64_t B = std::max<uint64_t>(plat::OutputFile::kAlign, (cfg_.chunk + plat::OutputFile::kAlign - 1) & ~(plat::OutputFile::kAlign - 1));
    const size_t nBlocks = (size_t)((newSize + B - 1) / B);

    plat::OutputFile out;
    if (!out.Create(tmp, newSize, err)) return false;
    BlockPool pool(B);

    const bool ok = OrderedPipeline<PooledBlock>(
        cfg_.threads, nBlocks,
        [&](size_t bi, PooledBlock& blk)
        {
            const uint64_t ob = bi * B, oe = std::min(newSize, ob + B);
            blk.pool = &pool;
            blk.p = pool.Get();
            blk.n = oe - ob;
            if (!blk.p) return;
            // 置換文字列の終端が ob より後ろにある最初の一致
            size_t i = (size_t)(std::upper_bound(outStart.begin(), outStart.end(), ob,
                                                 [&](uint64_t v, uint64_t os) { return v < os + rl; }) -
                                outStart.begin());
            uint64_t prevIn = i ? all[i - 1].pos + all[i - 1].len : 0;
            uint64_t prevOut = i ? outStart[i - 1] + rl : 0;
            uint8_t* d = blk.p;
            uint64_t o = ob;
            while (o < oe)
            {
                if (i < M && o >= outStart[i])
                {
                    // 置換文字列の中
                    const uint64_t e = std::min(oe, outStart[i] + rl);
                    std::memcpy(d, rep.data() + (o - outStart[i]), (size_t)(e - o));
                    d += e - o;
                    o = e;
                    if (o == outStart[i] + rl)
                    {
                        prevIn = all[i].pos + all[i].len;
                        prevOut = o;
                        ++i;
                    }
                }
                else
                {
                    // 一致と一致の間 (入力のコピー)
                    const uint64_t e = (i < M) ? std::min(oe, outStart[i]) : oe;
                    std::memcpy(d, t + prevIn + (o - prevOut), (size_t)(e - o));
                    d += e - o;
                    o = e;
                }
            }
        },
        [&](size_t, PooledBlock& blk)
        {
            if (!blk.p)
            {
                err = "メモリ確保失敗";
                return false;
            }
            return out.WriteBlock(blk.p, blk.n, err);
        });
    if (!ok)
    {
        out.Abort();
        return false;
    }
    return out.Finish(err);
}

void Processor::Output(Job& job)
{
    FileStat& st = job.st;
    st.size = st.newSize = job.in.Size();
    if (job.openFailed)
    {
        st.error = true;
        st.message = job.openErr;
        return;
    }
    st.matches = job.all.size();
    if (cfg_.searchOnly)
    {
        if ((report_ || view_) && !job.all.empty()) WriteSearchReport(job, report_, view_);
        return;
    }
    if (job.all.empty()) return;

    uint64_t removed = 0;
    for (const Match& m : job.all) removed += m.len;
    st.newSize = st.size - removed + (uint64_t)job.all.size() * cfg_.replace.size();
    const NativePath tmp = job.path + FR_NATIVE(".frtmp");
    std::string err;
    if (!WriteReplaced(job, tmp, st.newSize, err))
    {
        plat::DeleteFileQuiet(tmp);
        st.error = true;
        st.message = err;
    }
    else
    {
        job.in.Close();  // 置き換え前にハンドル/マップを解放
        if (!plat::ReplaceWith(job.path, tmp, err))
        {
            st.error = true;
            st.message = err;
        }
    }
    if (report_)
    {
        std::string line = plat::ToUtf8(job.path) + "\t";
        AppendU64(line, job.all.size());
        line += st.error ? "\tERROR\n" : "\tOK\n";
        report_->Write(line);
    }
}

bool EndsWith(const NativePath& s, const NativePath& suf) { return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0; }

int Run(const std::vector<std::string>& args)
{
    Config cfg;
    if (int r = ParseArgs(args, cfg)) return r == 1 ? 0 : r;

    std::vector<std::u32string> pats;
    for (size_t k = 0; k < cfg.finds.size(); ++k)
    {
        std::u32string p;
        if (!utf8::Decode(cfg.finds[k], p))
        {
            std::fprintf(stderr, "エラー: 検索文字列 #%zu が UTF-8 として不正です\n", k + 1);
            return 2;
        }
        pats.push_back(std::move(p));
    }
    {
        std::u32string tmp;
        if (!utf8::Decode(cfg.replace, tmp))
        {
            std::fprintf(stderr, "エラー: 置換文字列が UTF-8 として不正です\n");
            return 2;
        }
    }
    MultiMatcher matcher;
    std::string err;
    if (!matcher.Compile(pats, cfg.opt, err))
    {
        std::fprintf(stderr, "エラー: %s\n", err.c_str());
        return 2;
    }

    const NativePath dir = plat::FromUtf8(cfg.folder);
    if (!plat::IsDirectory(dir))
    {
        std::fprintf(stderr, "エラー: フォルダが見つかりません: %s\n", cfg.folder.c_str());
        return 2;
    }
    std::vector<NativePath> files;
    if (!plat::ListFiles(dir, cfg.recursive, files, err))
    {
        std::fprintf(stderr, "エラー: フォルダを列挙できません: %s\n", err.c_str());
        return 2;
    }

    // 結果の出力先
    //  -o 指定           : ファイル
    //  検索 + 対話端末   : メモリ (--tempfile 時は一時ファイル) に溜めてページャで表示
    //  検索 + リダイレクト: 標準出力へ全件
    plat::TextWriter report;
    const bool useFile = !cfg.reportPath.empty();
    const bool toPager = cfg.searchOnly && !useFile && pager::IsInteractive();
    const bool toStdout = cfg.searchOnly && !useFile && !toPager;
    std::unique_ptr<ViewData> view;
    NativePath reportPath;
    if (toPager)
    {
        view = std::make_unique<ViewData>(cfg.width, cfg.finds.size() > 1);
        if (!view->Begin(cfg.viewTempFile, err))
        {
            std::fprintf(stderr, "エラー: %s\n", err.c_str());
            return 2;
        }
    }
    else if (useFile)
    {
        reportPath = plat::FromUtf8(cfg.reportPath);
        if (!report.Open(reportPath, err))
        {
            std::fprintf(stderr, "エラー: %s\n", err.c_str());
            return 2;
        }
    }
    else if (toStdout)
    {
        std::setvbuf(stdout, nullptr, _IOFBF, 8 << 20);  // 大量出力向けに全バッファリング (ファイルごとにフラッシュ)
        report.OpenStdout();
    }
    // テキスト出力の幅 (指定がなければ端末の幅、取得できなければ 160)
    cfg.textWidth = cfg.width > 0 ? cfg.width : (view::TerminalWidth() > 0 ? view::TerminalWidth() : 160);
    // レポート自身と一時ファイルは対象外
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const NativePath& f)
                               { return (useFile && plat::SameFile(f, reportPath)) || EndsWith(f, FR_NATIVE(".frtmp")); }),
                files.end());

    const char* mode = cfg.searchOnly ? "検索のみ" : "置換";
    std::printf("FastReplacer  モード: %s  大小区別: %s  単語単位: %s  検索文字列: %zu 個  スレッド: %u  AVX2: %s\n", mode,
                cfg.opt.matchCase ? "ON" : "OFF", cfg.opt.wholeWord ? "ON" : "OFF", cfg.finds.size(), cfg.threads, simd::HasAvx2() ? "ON" : "OFF");
    std::printf("フォルダ: %s  (%zu ファイル)\n", plat::ToUtf8(dir).c_str(), files.size());

    // 検索文字列の一覧と書式 (標準出力・ページャ用の見出し)
    std::string fmt;
    for (size_t k = 0; k < cfg.finds.size(); ++k)
    {
        fmt += "検索 #";
        AppendU64(fmt, k + 1);
        fmt += ": ";
        AppendVisible(fmt, (const uint8_t*)cfg.finds[k].data(), cfg.finds[k].size());
        fmt += "\n";
    }
    fmt += "書式: L<行>:C<列(文字)>  @<バイトオフセット>";
    if (cfg.finds.size() > 1) fmt += "  #<検索文字列番号>";
    fmt += toPager ? "  <前文><一致><後文>" : "  <前文>【一致】<後文>";
    fmt += "\n\n";
    if (toStdout) std::fputs(fmt.c_str(), stdout);
    if (toPager) view->AddText(fmt);
    std::fflush(stdout);

    if (useFile)
    {
        std::string h = "\xEF\xBB\xBF";  // UTF-8 BOM
        h += "FastReplacer レポート\n";
        h += "日時      : " + NowString(false) + "\n";
        h += std::string("モード    : ") + mode + "\n";
        h += "フォルダ  : " + plat::ToUtf8(dir) + (cfg.recursive ? "  (サブフォルダ含む)" : "") + "\n";
        for (size_t k = 0; k < cfg.finds.size(); ++k)
        {
            h += "検索 #";
            AppendU64(h, k + 1);
            h += "  : ";
            AppendVisible(h, (const uint8_t*)cfg.finds[k].data(), cfg.finds[k].size());
            h += "\n";
        }
        if (!cfg.searchOnly)
        {
            h += "置換      : ";
            AppendVisible(h, (const uint8_t*)cfg.replace.data(), cfg.replace.size());
            h += "\n";
        }
        h += std::string("大小区別  : ") + (cfg.opt.matchCase ? "ON" : "OFF") + "\n";
        h += std::string("単語単位  : ") + (cfg.opt.wholeWord ? "ON" : "OFF") + "\n";
        if (cfg.searchOnly)
        {
            h += "書式      : L<行>:C<列(文字)>  @<バイトオフセット>";
            if (cfg.finds.size() > 1) h += "  #<検索文字列番号>";
            h += "  <前文>【一致】<後文>\n\n";
        }
        else
            h += "\n<ファイル>\t<置換数>\t<結果>\n";
        report.Write(h);
    }

    Processor proc(cfg, matcher, (useFile || toStdout) ? &report : nullptr, view.get());
    const auto t0 = std::chrono::steady_clock::now();
    uint64_t totalBytes = 0, totalMatches = 0, hitFiles = 0, errFiles = 0;
    std::vector<std::string> errors;
    proc.Run(files,
             [&](size_t i, const FileStat& st)
             {
                 const std::string name = plat::ToUtf8(files[i]);
                 if (st.error)
                 {
                     ++errFiles;
                     errors.push_back(name + " : " + st.message);
                     std::printf("[%zu/%zu] %s : エラー: %s\n", i + 1, files.size(), name.c_str(), st.message.c_str());
                 }
                 else if (!(toStdout && st.matches))  // 標準出力モードで一致ありのファイルは見出し付きで出力済み
                     std::printf("[%zu/%zu] %s : %llu 件%s  (%s, %.2f s)\n", i + 1, files.size(), name.c_str(), (unsigned long long)st.matches,
                                 (!cfg.searchOnly && st.matches) ? " 置換" : "", FormatBytes(st.size).c_str(), st.seconds);
                 std::fflush(stdout);
                 totalBytes += st.size;
                 totalMatches += st.matches;
                 hitFiles += st.matches ? 1 : 0;
             });
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    char sum[256];
    std::snprintf(sum, sizeof(sum), "合計: %zu ファイル / 一致 %llu ファイル / %llu 件 / エラー %llu  (%s, %.2f s, %.2f GB/s)\n", files.size(),
                  (unsigned long long)hitFiles, (unsigned long long)totalMatches, (unsigned long long)errFiles, FormatBytes(totalBytes).c_str(), sec,
                  sec > 0 ? totalBytes / sec / 1e9 : 0.0);
    std::string tail;
    for (const auto& e : errors) tail += "エラー: " + e + "\n";
    if (toStdout)
    {
        report.Close();
        std::fputs(sum, stdout);
    }
    else if (toPager)
    {
        view->AddText(std::string("---\n") + sum + tail);
        std::string e2;
        if (!view->Finish(e2))
            std::fprintf(stderr, "エラー: %s\n", e2.c_str());
        else if (totalMatches > 0)
        {
            std::string title = "FastReplacer 検索結果 ";
            AppendU64(title, totalMatches);
            title += " 件";
            pager::Run(*view, title);
        }
        view->Cleanup();
        std::fputs(sum, stdout);
    }
    else
    {
        std::fputs(sum, stdout);
        if (useFile)
        {
            report.Write("\n---\n" + std::string(sum) + tail);
            if (!report.Close())
                std::fprintf(stderr, "エラー: レポートの書き込みに失敗しました\n");
            else
                std::printf("レポート: %s\n", plat::ToUtf8(reportPath).c_str());
        }
    }
    return errFiles ? 1 : 0;
}
}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    plat::ConsoleInit();
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i)
    {
        const int len = (int)wcslen(argv[i]);
        std::string s;
        if (len)
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], len, nullptr, 0, nullptr, nullptr);
            s.resize((size_t)n);
            WideCharToMultiByte(CP_UTF8, 0, argv[i], len, s.data(), n, nullptr, nullptr);
        }
        args.push_back(std::move(s));
    }
    return Run(args);
}
#else
int main(int argc, char** argv)
{
    std::vector<std::string> args(argv, argv + argc);
    return Run(args);
}
#endif
