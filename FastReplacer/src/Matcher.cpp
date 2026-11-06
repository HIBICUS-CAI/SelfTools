#include "Matcher.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>

#include <emmintrin.h>
#include <immintrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define FR_TARGET_AVX2
#else
#include <cpuid.h>
#define FR_TARGET_AVX2 __attribute__((target("avx2")))
#endif

namespace
{
#include "UnicodeData.inc"
}

// ============================================================================
// UTF-8 / Unicode
// ============================================================================
namespace utf8
{
static int SeqLen(uint8_t b)
{
    if (b < 0x80) return 1;
    if (b < 0xC2) return 0;
    if (b < 0xE0) return 2;
    if (b < 0xF0) return 3;
    if (b < 0xF5) return 4;
    return 0;
}

// 妥当な UTF-8 列なら長さ、不正なら 0
static int DecodeOne(const uint8_t* p, uint64_t avail, char32_t& cp)
{
    if (avail == 0) return 0;
    const uint8_t b0 = p[0];
    const int len = SeqLen(b0);
    if (len == 0 || (uint64_t)len > avail) return 0;
    if (len == 1)
    {
        cp = b0;
        return 1;
    }
    for (int i = 1; i < len; ++i)
        if ((p[i] & 0xC0) != 0x80) return 0;
    if (len == 2)
    {
        cp = ((b0 & 0x1Fu) << 6) | (p[1] & 0x3Fu);
        return 2;
    }
    if (len == 3)
    {
        cp = ((b0 & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        return 3;
    }
    cp = ((b0 & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu);
    if (cp < 0x10000 || cp > 0x10FFFF) return 0;
    return 4;
}

int ValidLen(const uint8_t* p, uint64_t avail)
{
    char32_t cp;
    return DecodeOne(p, avail, cp);
}

bool Decode(const std::string& s, std::u32string& out)
{
    out.clear();
    const uint8_t* p = (const uint8_t*)s.data();
    uint64_t i = 0, n = s.size();
    while (i < n)
    {
        char32_t cp;
        int l = DecodeOne(p + i, n - i, cp);
        if (l == 0) return false;
        out.push_back(cp);
        i += l;
    }
    return true;
}

void Append(std::string& s, char32_t cp)
{
    if (cp < 0x80)
        s.push_back((char)cp);
    else if (cp < 0x800)
    {
        s.push_back((char)(0xC0 | (cp >> 6)));
        s.push_back((char)(0x80 | (cp & 0x3F)));
    }
    else if (cp < 0x10000)
    {
        s.push_back((char)(0xE0 | (cp >> 12)));
        s.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back((char)(0x80 | (cp & 0x3F)));
    }
    else
    {
        s.push_back((char)(0xF0 | (cp >> 18)));
        s.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        s.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        s.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

char32_t DecodeAt(const uint8_t* t, uint64_t n, uint64_t pos)
{
    char32_t cp;
    if (DecodeOne(t + pos, n - pos, cp) == 0) return 0xFFFD;
    return cp;
}

char32_t DecodeBefore(const uint8_t* t, uint64_t pos)
{
    uint64_t k = pos - 1;
    while (k > 0 && (t[k] & 0xC0) == 0x80 && pos - k < 4) --k;
    char32_t cp;
    const int l = DecodeOne(t + k, pos - k, cp);
    if (l == 0 || (uint64_t)l != pos - k) return 0xFFFD;
    return cp;
}

static bool g_asciiWord[128];
static const bool g_asciiInit = []
{
    for (int c = 0; c < 128; ++c)
        g_asciiWord[c] = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    return true;
}();

bool IsWordChar(char32_t cp)
{
    if (cp < 128) return g_asciiWord[cp];
    size_t lo = 0, hi = sizeof(kWordRanges) / sizeof(kWordRanges[0]);
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if (cp < kWordRanges[mid][0])
            hi = mid;
        else if (cp > kWordRanges[mid][1])
            lo = mid + 1;
        else
            return true;
    }
    return false;
}

char32_t SimpleFold(char32_t cp)
{
    size_t lo = 0, hi = sizeof(kFoldPairs) / sizeof(kFoldPairs[0]);
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        if (cp < kFoldPairs[mid][0])
            hi = mid;
        else if (cp > kFoldPairs[mid][0])
            lo = mid + 1;
        else
            return kFoldPairs[mid][1];
    }
    return cp;
}
}  // namespace utf8

// ============================================================================
// CPU 判定 / SIMD 補助
// ============================================================================
namespace simd
{
static bool DetectAvx2()
{
#if defined(_MSC_VER) && !defined(__clang__)
    int r[4];
    __cpuid(r, 0);
    if (r[0] < 7) return false;
    __cpuid(r, 1);
    const bool osxsave = (r[2] & (1 << 27)) != 0, avx = (r[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) return false;
    if ((_xgetbv(0) & 6) != 6) return false;
    __cpuidex(r, 7, 0);
    return (r[1] & (1 << 5)) != 0;
#else
    unsigned a, b, c, d;
    if (!__get_cpuid(0, &a, &b, &c, &d) || a < 7) return false;
    __get_cpuid(1, &a, &b, &c, &d);
    if (!(c & (1u << 27)) || !(c & (1u << 28))) return false;
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    if ((lo & 6) != 6) return false;
    __cpuid_count(7, 0, a, b, c, d);
    return (b & (1u << 5)) != 0;
#endif
}

static const bool g_avx2 = DetectAvx2() && !std::getenv("FASTREPLACER_NO_AVX2");  // 環境変数で SSE2 経路を強制 (テスト用)
bool HasAvx2() { return g_avx2; }

uint64_t CountByte(const uint8_t* p, uint64_t len, uint8_t b)
{
    uint64_t cnt = 0, i = 0;
    const __m128i nb = _mm_set1_epi8((char)b);
    for (; i + 16 <= len; i += 16)
        cnt += std::popcount((unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)(p + i)), nb)));
    for (; i < len; ++i) cnt += (p[i] == b);
    return cnt;
}

uint64_t CountCodePoints(const uint8_t* p, uint64_t len)
{
    // 継続バイト (0x80..0xBF) は signed で -128..-65。それ以外 (> -65) を数える
    uint64_t cnt = 0, i = 0;
    const __m128i th = _mm_set1_epi8(-65);
    for (; i + 16 <= len; i += 16)
        cnt += std::popcount((unsigned)_mm_movemask_epi8(_mm_cmpgt_epi8(_mm_loadu_si128((const __m128i*)(p + i)), th)));
    for (; i < len; ++i) cnt += ((p[i] & 0xC0) != 0x80);
    return cnt;
}

const uint8_t* FindLastByte(const uint8_t* p, uint64_t len, uint8_t b)
{
    uint64_t i = len;
    const __m128i nb = _mm_set1_epi8((char)b);
    while (i >= 16)
    {
        unsigned m = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128((const __m128i*)(p + i - 16)), nb));
        if (m) return p + i - 16 + (31 - std::countl_zero(m));
        i -= 16;
    }
    while (i > 0)
    {
        --i;
        if (p[i] == b) return p + i;
    }
    return nullptr;
}
}  // namespace simd

// ============================================================================
// パターンのコンパイル
// ============================================================================
static int ByteFreq(uint8_t b)
{
    // 大きいほど頻出 (候補位置の選択用ヒューリスティック)
    static const char kLower[] = "etaoinsrhldcumfpgwybvkxjqz";
    if (b == ' ') return 255;
    if (b >= 'a' && b <= 'z') return 250 - (int)(std::strchr(kLower, b) - kLower) * 4;
    if (b >= 'A' && b <= 'Z') return 130 - (int)(std::strchr(kLower, b + 32) - kLower) * 2;
    if (b >= '0' && b <= '9') return 120;
    if (b == '\n' || b == '\r' || b == '\t') return 150;
    if (b < 0x20 || b == 0x7F) return 5;
    if (b < 0x80) return 110;  // 記号
    if (b == 0xE3) return 220;  // 日本語 かな/記号の先頭
    if (b >= 0x80 && b <= 0x83) return 180;
    if (b <= 0xBF) return 140;  // 継続バイト
    if (b >= 0xE4 && b <= 0xE9) return 160;
    if (b >= 0xC2 && b <= 0xDF) return 90;
    if (b == 0xEF || b == 0xE2) return 100;
    return 10;
}

static Matcher::Variant MakeVariant(char32_t cp)
{
    std::string s;
    utf8::Append(s, cp);
    Matcher::Variant v{};
    v.len = (uint8_t)s.size();
    std::memcpy(v.b, s.data(), s.size());
    return v;
}

bool Matcher::Compile(const std::u32string& pattern, const MatchOptions& opt, std::string& err)
{
    if (pattern.empty())
    {
        err = "検索文字列が空です";
        return false;
    }
    classes_.clear();
    literal_.clear();
    wholeWord_ = opt.wholeWord;
    wordStart_ = utf8::IsWordChar(pattern.front());
    wordEnd_ = utf8::IsWordChar(pattern.back());
    minLen_ = maxLen_ = 0;

    for (char32_t cp : pattern)
    {
        Class c{};
        if (opt.matchCase)
        {
            c.v[c.count++] = MakeVariant(cp);
        }
        else
        {
            const char32_t f = utf8::SimpleFold(cp);
            const bool ascii = cp < 0x80;
            std::vector<char32_t> mem;
            if ((f < 0x80) == ascii) mem.push_back(f);
            for (const auto& pr : kFoldPairs)
                if (pr[1] == f && ((pr[0] < 0x80) == ascii)) mem.push_back(pr[0]);
            std::sort(mem.begin(), mem.end());
            mem.erase(std::unique(mem.begin(), mem.end()), mem.end());
            if (mem.size() > 4)
            {
                err = "内部エラー: ケース同値類が大きすぎます";
                return false;
            }
            for (char32_t m : mem) c.v[c.count++] = MakeVariant(m);
        }
        uint32_t mn = 4, mx = 0;
        for (int i = 0; i < c.count; ++i)
        {
            mn = std::min<uint32_t>(mn, c.v[i].len);
            mx = std::max<uint32_t>(mx, c.v[i].len);
        }
        minLen_ += mn;
        maxLen_ += mx;
        classes_.push_back(c);
    }

    isLiteral_ = std::all_of(classes_.begin(), classes_.end(), [](const Class& c) { return c.count == 1; });
    if (isLiteral_)
        for (const Class& c : classes_) literal_.append((const char*)c.v[0].b, c.v[0].len);

    // 固定長の先頭部分 (全バリアントが同じ長さのクラスが続く範囲) で候補バイト集合を作る
    struct Slot
    {
        uint8_t set[4];
        int n;
        int score;
    };
    std::vector<Slot> slots;
    for (const Class& c : classes_)
    {
        bool uniform = true;
        for (int i = 1; i < c.count; ++i) uniform &= (c.v[i].len == c.v[0].len);
        for (int bi = 0; bi < c.v[0].len; ++bi)
        {
            Slot s{};
            for (int i = 0; i < c.count; ++i)
            {
                const uint8_t b = c.v[i].b[bi];
                if (std::find(s.set, s.set + s.n, b) == s.set + s.n) s.set[s.n++] = b;
            }
            for (int i = 0; i < s.n; ++i) s.score += ByteFreq(s.set[i]);
            slots.push_back(s);
            if (!uniform) break;  // 不均一クラスは先頭バイトのみ固定位置
        }
        if (!uniform) break;
    }

    // スコア最小 (最も稀) の 2 位置を選ぶ
    int best1 = 0;
    for (int i = 1; i < (int)slots.size(); ++i)
        if (slots[i].score < slots[best1].score) best1 = i;
    int best2 = -1;
    for (int i = 0; i < (int)slots.size(); ++i)
        if (i != best1 && (best2 < 0 || slots[i].score < slots[best2].score)) best2 = i;
    if (best2 < 0) best2 = best1;
    if (best1 > best2) std::swap(best1, best2);
    off1_ = (uint32_t)best1;
    off2_ = (uint32_t)best2;

    const Slot& s1 = slots[best1];
    const Slot& s2 = slots[best2];
    const int nmax = std::max(s1.n, s2.n);
    setN_ = nmax == 1 ? 1 : (nmax == 2 ? 2 : 4);
    for (int i = 0; i < 4; ++i)
    {
        set1_[i] = i < s1.n ? s1.set[i] : s1.set[0];
        set2_[i] = i < s2.n ? s2.set[i] : s2.set[0];
    }
    return true;
}

// ============================================================================
// 照合
// ============================================================================
uint32_t Matcher::Verify(const uint8_t* t, uint64_t n, uint64_t pos) const
{
    uint64_t q;
    if (isLiteral_)
    {
        const uint64_t L = literal_.size();
        if (n - pos < L || std::memcmp(t + pos, literal_.data(), L) != 0) return 0;
        q = pos + L;
    }
    else
    {
        q = pos;
        for (const Class& c : classes_)
        {
            int k = 0;
            for (; k < c.count; ++k)
            {
                const Variant& v = c.v[k];
                if (n - q < v.len) continue;
                bool eq = true;
                for (int j = 0; j < v.len; ++j) eq &= (t[q + j] == v.b[j]);
                if (eq) break;
            }
            if (k == c.count) return 0;
            q += c.v[k].len;
        }
    }
    if (wholeWord_)
    {
        if (wordStart_ && pos > 0 && utf8::IsWordChar(utf8::DecodeBefore(t, pos))) return 0;
        if (wordEnd_ && q < n && utf8::IsWordChar(utf8::DecodeAt(t, n, q))) return 0;
    }
    return (uint32_t)(q - pos);
}

namespace
{
struct Ctx
{
    const Matcher* m;
    const uint8_t* t;
    uint64_t n;
    std::vector<Match>* out;
    Match* first;
};

// 戻り値: first モードで見つかったら true
inline bool Emit(const Ctx& c, uint64_t p, uint32_t len)
{
    if (c.first)
    {
        c.first->pos = p;
        c.first->len = len;
        return true;
    }
    c.out->push_back({p, len});
    return false;
}

inline bool InSet(const uint8_t* s, int n, uint8_t b)
{
    for (int i = 0; i < n; ++i)
        if (s[i] == b) return true;
    return false;
}

// SIMD で届かない範囲 (ファイル末尾付近) のスカラー処理。i は次の候補位置。
bool ScalarTail(const Ctx& c, uint64_t& i, uint64_t stop)
{
    const Matcher& M = *c.m;
    while (i < stop)
    {
        if (InSet(M.set1_, M.setN_, c.t[i + M.off1_]) && InSet(M.set2_, M.setN_, c.t[i + M.off2_]))
        {
            if (uint32_t len = M.Verify(c.t, c.n, i))
            {
                if (Emit(c, i, len)) return true;
                i += len;
                continue;
            }
        }
        ++i;
    }
    return false;
}

template <int N>
bool ScanSse2(const Ctx& c, uint64_t begin, uint64_t stop)
{
    const Matcher& M = *c.m;
    const uint8_t* t = c.t;
    const uint64_t jmax = M.off2_;
    __m128i a[N], b[N];
    for (int k = 0; k < N; ++k)
    {
        a[k] = _mm_set1_epi8((char)M.set1_[k]);
        b[k] = _mm_set1_epi8((char)M.set2_[k]);
    }
    uint64_t i = begin;
    while (i < stop && i + jmax + 16 <= c.n)
    {
        const __m128i v1 = _mm_loadu_si128((const __m128i*)(t + i + M.off1_));
        const __m128i v2 = _mm_loadu_si128((const __m128i*)(t + i + M.off2_));
        __m128i m1 = _mm_cmpeq_epi8(v1, a[0]), m2 = _mm_cmpeq_epi8(v2, b[0]);
        for (int k = 1; k < N; ++k)
        {
            m1 = _mm_or_si128(m1, _mm_cmpeq_epi8(v1, a[k]));
            m2 = _mm_or_si128(m2, _mm_cmpeq_epi8(v2, b[k]));
        }
        unsigned mask = (unsigned)_mm_movemask_epi8(_mm_and_si128(m1, m2));
        uint64_t next = i + 16;
        while (mask)
        {
            const uint64_t p = i + std::countr_zero(mask);
            if (p >= stop) return false;
            if (uint32_t len = M.Verify(t, c.n, p))
            {
                if (Emit(c, p, len)) return true;
                next = p + len;
                break;
            }
            mask &= mask - 1;
        }
        i = next;
    }
    return ScalarTail(c, i, stop);
}

template <int N>
FR_TARGET_AVX2 bool ScanAvx2(const Ctx& c, uint64_t begin, uint64_t stop)
{
    const Matcher& M = *c.m;
    const uint8_t* t = c.t;
    const uint64_t jmax = M.off2_;
    __m256i a[N], b[N];
    for (int k = 0; k < N; ++k)
    {
        a[k] = _mm256_set1_epi8((char)M.set1_[k]);
        b[k] = _mm256_set1_epi8((char)M.set2_[k]);
    }
    uint64_t i = begin;
    while (i < stop && i + jmax + 32 <= c.n)
    {
        const __m256i v1 = _mm256_loadu_si256((const __m256i*)(t + i + M.off1_));
        const __m256i v2 = _mm256_loadu_si256((const __m256i*)(t + i + M.off2_));
        __m256i m1 = _mm256_cmpeq_epi8(v1, a[0]), m2 = _mm256_cmpeq_epi8(v2, b[0]);
        for (int k = 1; k < N; ++k)
        {
            m1 = _mm256_or_si256(m1, _mm256_cmpeq_epi8(v1, a[k]));
            m2 = _mm256_or_si256(m2, _mm256_cmpeq_epi8(v2, b[k]));
        }
        uint32_t mask = (uint32_t)_mm256_movemask_epi8(_mm256_and_si256(m1, m2));
        uint64_t next = i + 32;
        while (mask)
        {
            const uint64_t p = i + std::countr_zero(mask);
            if (p >= stop) return false;
            if (uint32_t len = M.Verify(t, c.n, p))
            {
                if (Emit(c, p, len)) return true;
                next = p + len;
                break;
            }
            mask &= mask - 1;
        }
        i = next;
    }
    return ScalarTail(c, i, stop);
}
}  // namespace

bool Matcher::Scan(const uint8_t* t, uint64_t n, uint64_t begin, uint64_t end, std::vector<Match>* out, Match* first) const
{
    if (n < minLen_) return false;
    // 候補位置 p は p + minLen <= n かつ p + off2 < n を満たす必要がある
    uint64_t stop = std::min<uint64_t>(end, n - minLen_ + 1);
    stop = std::min<uint64_t>(stop, n - off2_);
    if (begin >= stop) return false;
    const Ctx c{this, t, n, out, first};
    if (simd::HasAvx2())
    {
        switch (setN_)
        {
        case 1: return ScanAvx2<1>(c, begin, stop);
        case 2: return ScanAvx2<2>(c, begin, stop);
        default: return ScanAvx2<4>(c, begin, stop);
        }
    }
    switch (setN_)
    {
    case 1: return ScanSse2<1>(c, begin, stop);
    case 2: return ScanSse2<2>(c, begin, stop);
    default: return ScanSse2<4>(c, begin, stop);
    }
}

void Matcher::FindAll(const uint8_t* text, uint64_t n, uint64_t begin, uint64_t end, std::vector<Match>& out) const
{
    Scan(text, n, begin, end, &out, nullptr);
}

bool Matcher::FindNext(const uint8_t* text, uint64_t n, uint64_t from, uint64_t end, Match& m) const
{
    return Scan(text, n, from, end, nullptr, &m);
}
