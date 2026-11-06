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
    std::string find, replace;
    bool hasReplace = false;
    bool searchOnly = false;
    bool recursive = false;
    bool escapes = false;
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
        "  FastReplacer <フォルダ> -f <検索文字列> -r <置換文字列> [オプション]   置換モード\n"
        "  FastReplacer <フォルダ> -f <検索文字列> -s [オプション]                検索のみ (レポート出力)\n"
        "\n"
        "オプション:\n"
        "  -f, --find <text>      検索文字列 (必須)\n"
        "  -r, --replace <text>   置換文字列 (空文字列可 = 削除)\n"
        "  -s, --search-only      検索のみ。結果をレポートファイルに書き出す\n"
        "  -c, --match-case       大文字/小文字を区別する (既定: 区別しない)\n"
        "  -w, --whole-word       単語単位で検索する (既定: 無効)\n"
        "  -R, --recursive        サブフォルダも対象にする (既定: 指定フォルダ直下のみ)\n"
        "  -e, --escape           検索/置換文字列のエスケープを解釈する\n"
        "                         (\\\\ \\n \\r \\t \\0 \\xHH \\uXXXX \\UXXXXXXXX)\n"
        "  -o, --report <path>    レポートファイルのパス\n"
        "                         (既定: カレントに FastReplacer_report_<日時>.txt。置換モードは指定時のみ出力)\n"
        "  -t, --threads <n>      スレッド数 (既定: 論理コア数)\n"
        "  -h, --help             このヘルプ\n"
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

int ParseArgs(const std::vector<std::string>& a, Config& cfg)
{
    bool hasFind = false;
    for (size_t i = 1; i < a.size(); ++i)
    {
        const std::string& s = a[i];
        auto need = [&](std::string& dst) -> bool
        {
            if (i + 1 >= a.size())
            {
                std::fprintf(stderr, "エラー: %s に値がありません\n", s.c_str());
                return false;
            }
            dst = a[++i];
            return true;
        };
        if (s == "-h" || s == "--help" || s == "/?")
        {
            PrintUsage();
            return 1;
        }
        else if (s == "-f" || s == "--find")
        {
            if (!need(cfg.find)) return 2;
            hasFind = true;
        }
        else if (s == "-r" || s == "--replace")
        {
            if (!need(cfg.replace)) return 2;
            cfg.hasReplace = true;
        }
        else if (s == "-s" || s == "--search-only")
            cfg.searchOnly = true;
        else if (s == "-c" || s == "--match-case")
            cfg.opt.matchCase = true;
        else if (s == "-w" || s == "--whole-word")
            cfg.opt.wholeWord = true;
        else if (s == "-R" || s == "--recursive")
            cfg.recursive = true;
        else if (s == "-e" || s == "--escape")
            cfg.escapes = true;
        else if (s == "-o" || s == "--report")
        {
            if (!need(cfg.reportPath)) return 2;
        }
        else if (s == "-t" || s == "--threads")
        {
            std::string v;
            if (!need(v)) return 2;
            cfg.threads = (unsigned)std::strtoul(v.c_str(), nullptr, 10);
        }
        else if (s == "--chunk-size")  // テスト用 (非公開)
        {
            std::string v;
            if (!need(v)) return 2;
            cfg.chunk = std::max<uint64_t>(1, std::strtoull(v.c_str(), nullptr, 10));
        }
        else if (!s.empty() && s[0] == '-' && s.size() > 1)
        {
            std::fprintf(stderr, "エラー: 不明なオプション %s\n", s.c_str());
            return 2;
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
        if (!ParseEscapes(cfg.find, t, err))
        {
            std::fprintf(stderr, "エラー: 検索文字列: %s\n", err.c_str());
            return 2;
        }
        cfg.find = t;
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

// 文字境界へ前方に寄せる
inline uint64_t SnapForward(const uint8_t* t, uint64_t n, uint64_t p, uint64_t limit)
{
    while (p < limit && p < n && (t[p] & 0xC0) == 0x80) ++p;
    return p;
}
// 文字境界へ後方に寄せる (p は終端位置)
inline uint64_t SnapBackward(const uint8_t* t, uint64_t n, uint64_t p, uint64_t limit)
{
    while (p > limit && p < n && (t[p] & 0xC0) == 0x80) --p;
    return p;
}

constexpr uint64_t kCtx = 100;       // 前後の表示バイト数
constexpr uint64_t kMatchShow = 200;  // 一致部分の最大表示バイト数

void AppendContext(std::string& out, const uint8_t* t, uint64_t n, const Match& m)
{
    // 行頭 (最大 kCtx バイト前まで)
    const uint64_t lb = m.pos > kCtx ? m.pos - kCtx : 0;
    const uint8_t* nl = simd::FindLastByte(t + lb, m.pos - lb, '\n');
    uint64_t s = nl ? (uint64_t)(nl - t) + 1 : lb;
    const bool cutL = !nl && lb > 0;
    if (cutL) s = SnapForward(t, n, s, m.pos);
    // 行末 (最大 kCtx バイト後まで)
    const uint64_t me = m.pos + m.len;
    const uint64_t rl = std::min(n, me + kCtx);
    const void* nr = std::memchr(t + me, '\n', (size_t)(rl - me));
    uint64_t e = nr ? (uint64_t)((const uint8_t*)nr - t) : rl;
    if (e > me && t[e - 1] == '\r' && nr) --e;
    const bool cutR = !nr && rl < n;
    if (cutR) e = SnapBackward(t, n, e, me);

    if (cutL) out += "…";
    AppendVisible(out, t + s, m.pos - s);
    out += "【";
    if (m.len > kMatchShow)
    {
        const uint64_t ce = SnapBackward(t, n, m.pos + kMatchShow, m.pos);
        AppendVisible(out, t + m.pos, ce - m.pos);
        out += "…";
    }
    else
        AppendVisible(out, t + m.pos, m.len);
    out += "】";
    AppendVisible(out, t + me, e - me);
    if (cutR) out += "…";
}

// ============================================================================
// ファイル処理
// ============================================================================
struct FileStat
{
    uint64_t size = 0;
    uint64_t matches = 0;
    uint64_t newSize = 0;
    double seconds = 0;
    bool error = false;
    std::string message;
};

class Processor
{
public:
    Processor(const Config& cfg, const Matcher& m) : cfg_(cfg), m_(m) {}

    FileStat Process(const NativePath& path, plat::TextWriter* report);

private:
    void FindMatches(const uint8_t* t, uint64_t n, std::vector<Match>& all, const plat::MappedFile& mf);
    void WriteSearchReport(const uint8_t* t, uint64_t n, const std::vector<Match>& all, const NativePath& path, plat::TextWriter& rep);
    bool WriteReplaced(const uint8_t* t, uint64_t n, const std::vector<Match>& all, const NativePath& tmp, uint64_t newSize,
                       std::string& err);

    const Config& cfg_;
    const Matcher& m_;
};

void Processor::FindMatches(const uint8_t* t, uint64_t n, std::vector<Match>& all, const plat::MappedFile& mf)
{
    const uint64_t cs = cfg_.chunk;
    const size_t nChunks = (size_t)((n + cs - 1) / cs);
    std::vector<std::vector<Match>> per(nChunks);
    ParallelFor(cfg_.threads, nChunks,
                [&](size_t c)
                {
                    const uint64_t b = c * cs, e = std::min(n, b + cs);
                    mf.Prefetch(b, e - b + m_.MaxLen());
                    m_.FindAll(t, n, b, e, per[c]);
                });

    // チャンク境界をまたぐ重なりを解消 (逐次スキャンと完全に同じ結果にする)
    size_t total = 0;
    for (auto& v : per) total += v.size();
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

void Processor::WriteSearchReport(const uint8_t* t, uint64_t n, const std::vector<Match>& all, const NativePath& path, plat::TextWriter& rep)
{
    const uint64_t cs = cfg_.chunk;
    const size_t nChunks = (size_t)((n + cs - 1) / cs);

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

    // 3) 並列に整形しつつ順に書き出す
    char head[128];
    std::snprintf(head, sizeof(head), "  (%llu 件, %s)\n", (unsigned long long)all.size(), FormatBytes(n).c_str());
    rep.Write("=== " + plat::ToUtf8(path) + head);
    OrderedPipeline<std::string>(
        cfg_.threads, nChunks,
        [&](size_t c, std::string& out)
        {
            const Sum& s = sums[c];
            if (s.mBegin == s.mEnd) return;
            out.reserve((s.mEnd - s.mBegin) * 160);
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
                out += "  L";
                AppendU64(out, ln);
                out += ":C";
                AppendU64(out, cl + 1);
                out += "  @";
                AppendU64(out, m.pos);
                out += "  ";
                AppendContext(out, t, n, m);
                out.push_back('\n');
            }
        },
        [&](size_t, std::string& out)
        {
            rep.Write(out);
            return true;
        });
    rep.Write("\n");
}

bool Processor::WriteReplaced(const uint8_t* t, uint64_t n, const std::vector<Match>& all, const NativePath& tmp, uint64_t newSize,
                              std::string& err)
{
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
                    if (i < M && rl == 0 && o == outStart[i])
                    {
                        prevIn = all[i].pos + all[i].len;
                        prevOut = o;
                        ++i;
                    }
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
    (void)n;
    if (!ok)
    {
        out.Abort();
        return false;
    }
    return out.Finish(err);
}

FileStat Processor::Process(const NativePath& path, plat::TextWriter* report)
{
    FileStat st;
    const auto t0 = std::chrono::steady_clock::now();
    plat::MappedFile mf;
    std::string err;
    if (!mf.Open(path, err))
    {
        st.error = true;
        st.message = err;
        return st;
    }
    const uint8_t* t = mf.Data();
    const uint64_t n = mf.Size();
    st.size = st.newSize = n;

    std::vector<Match> all;
    if (n > 0) FindMatches(t, n, all, mf);
    st.matches = all.size();

    if (cfg_.searchOnly)
    {
        if (report && !all.empty()) WriteSearchReport(t, n, all, path, *report);
    }
    else if (!all.empty())
    {
        uint64_t removed = 0;
        for (const Match& m : all) removed += m.len;
        st.newSize = n - removed + (uint64_t)all.size() * cfg_.replace.size();
        const NativePath tmp = path + FR_NATIVE(".frtmp");
        if (!WriteReplaced(t, n, all, tmp, st.newSize, err))
        {
            plat::DeleteFileQuiet(tmp);
            st.error = true;
            st.message = err;
        }
        else
        {
            mf.Close();
            if (!plat::ReplaceWith(path, tmp, err))
            {
                st.error = true;
                st.message = err;
            }
        }
        if (report)
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "\t%llu\t%s\n", (unsigned long long)all.size(), st.error ? "ERROR" : "OK");
            report->Write(plat::ToUtf8(path) + buf);
        }
    }
    st.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return st;
}

bool EndsWith(const NativePath& s, const NativePath& suf) { return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0; }

int Run(const std::vector<std::string>& args)
{
    Config cfg;
    if (int r = ParseArgs(args, cfg)) return r == 1 ? 0 : r;

    std::u32string pat;
    if (!utf8::Decode(cfg.find, pat))
    {
        std::fprintf(stderr, "エラー: 検索文字列が UTF-8 として不正です\n");
        return 2;
    }
    {
        std::u32string tmp;
        if (!utf8::Decode(cfg.replace, tmp))
        {
            std::fprintf(stderr, "エラー: 置換文字列が UTF-8 として不正です\n");
            return 2;
        }
    }
    Matcher matcher;
    std::string err;
    if (!matcher.Compile(pat, cfg.opt, err))
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

    // レポート
    plat::TextWriter report;
    bool useReport = cfg.searchOnly || !cfg.reportPath.empty();
    NativePath reportPath;
    if (useReport)
    {
        if (cfg.reportPath.empty()) cfg.reportPath = "FastReplacer_report_" + NowString(true) + ".txt";
        reportPath = plat::FromUtf8(cfg.reportPath);
        if (!report.Open(reportPath, err))
        {
            std::fprintf(stderr, "エラー: %s\n", err.c_str());
            return 2;
        }
    }
    // レポート自身と一時ファイルは対象外
    files.erase(std::remove_if(files.begin(), files.end(),
                               [&](const NativePath& f) { return (useReport && plat::SameFile(f, reportPath)) || EndsWith(f, FR_NATIVE(".frtmp")); }),
                files.end());

    const char* mode = cfg.searchOnly ? "検索のみ" : "置換";
    std::printf("FastReplacer  モード: %s  大小区別: %s  単語単位: %s  スレッド: %u  AVX2: %s\n", mode, cfg.opt.matchCase ? "ON" : "OFF",
                cfg.opt.wholeWord ? "ON" : "OFF", cfg.threads, simd::HasAvx2() ? "ON" : "OFF");
    std::printf("フォルダ: %s  (%zu ファイル)\n", plat::ToUtf8(dir).c_str(), files.size());
    std::fflush(stdout);

    if (useReport)
    {
        std::string h = "\xEF\xBB\xBF";  // UTF-8 BOM
        h += "FastReplacer レポート\n";
        h += "日時      : " + NowString(false) + "\n";
        h += std::string("モード    : ") + mode + "\n";
        h += "フォルダ  : " + plat::ToUtf8(dir) + (cfg.recursive ? "  (サブフォルダ含む)" : "") + "\n";
        h += "検索      : ";
        AppendVisible(h, (const uint8_t*)cfg.find.data(), cfg.find.size());
        h += "\n";
        if (!cfg.searchOnly)
        {
            h += "置換      : ";
            AppendVisible(h, (const uint8_t*)cfg.replace.data(), cfg.replace.size());
            h += "\n";
        }
        h += std::string("大小区別  : ") + (cfg.opt.matchCase ? "ON" : "OFF") + "\n";
        h += std::string("単語単位  : ") + (cfg.opt.wholeWord ? "ON" : "OFF") + "\n";
        h += cfg.searchOnly ? "書式      : L<行>:C<列(文字)>  @<バイトオフセット>  <前後の文脈>【一致】\n\n"
                            : "\n<ファイル>\t<置換数>\t<結果>\n";
        report.Write(h);
    }

    Processor proc(cfg, matcher);
    const auto t0 = std::chrono::steady_clock::now();
    uint64_t totalBytes = 0, totalMatches = 0, hitFiles = 0, errFiles = 0;
    std::vector<std::string> errors;
    for (size_t i = 0; i < files.size(); ++i)
    {
        FileStat st = proc.Process(files[i], useReport ? &report : nullptr);
        const std::string name = plat::ToUtf8(files[i]);
        if (st.error)
        {
            ++errFiles;
            errors.push_back(name + " : " + st.message);
            std::printf("[%zu/%zu] %s : エラー: %s\n", i + 1, files.size(), name.c_str(), st.message.c_str());
        }
        else
        {
            const double gbps = st.seconds > 0 ? st.size / st.seconds / 1e9 : 0;
            std::printf("[%zu/%zu] %s : %llu 件%s  (%s, %.2f s, %.2f GB/s)\n", i + 1, files.size(), name.c_str(), (unsigned long long)st.matches,
                        (!cfg.searchOnly && st.matches) ? " 置換" : "", FormatBytes(st.size).c_str(), st.seconds, gbps);
        }
        std::fflush(stdout);
        totalBytes += st.size;
        totalMatches += st.matches;
        hitFiles += st.matches ? 1 : 0;
    }
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    char sum[256];
    std::snprintf(sum, sizeof(sum), "合計: %zu ファイル / 一致 %llu ファイル / %llu 件 / エラー %llu  (%s, %.2f s, %.2f GB/s)\n", files.size(),
                  (unsigned long long)hitFiles, (unsigned long long)totalMatches, (unsigned long long)errFiles, FormatBytes(totalBytes).c_str(), sec,
                  sec > 0 ? totalBytes / sec / 1e9 : 0.0);
    std::fputs(sum, stdout);
    if (useReport)
    {
        std::string tail = "\n---\n";
        tail += sum;
        for (const auto& e : errors) tail += "エラー: " + e + "\n";
        report.Write(tail);
        if (!report.Close())
            std::fprintf(stderr, "エラー: レポートの書き込みに失敗しました\n");
        else
            std::printf("レポート: %s\n", plat::ToUtf8(reportPath).c_str());
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
