#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct Match
{
    uint64_t pos;
    uint32_t len;
};

struct MatchOptions
{
    bool matchCase = false;  // 大小文字を区別する
    bool wholeWord = false;  // 単語単位
};

// UTF-8 テキスト用の検索器。
//  - matchCase=false : Unicode 単純ケースフォールディング (C+S) で同一視。
//                      ただし ASCII と非 ASCII は相互に一致させない (K(U+212A)≠k, ſ≠s)。
//  - wholeWord=true  : パターン先頭/末尾が単語文字 (\w = L, Mn, Nd, Pc) の場合、
//                      その外側が単語文字でないことを要求する。
class Matcher
{
public:
    bool Compile(const std::u32string& pattern, const MatchOptions& opt, std::string& err);

    // 開始位置が [begin, end) にある重ならない一致を begin から貪欲に全て列挙する。
    // 一致の末尾は end を越えてよい (n まで参照する)。
    void FindAll(const uint8_t* text, uint64_t n, uint64_t begin, uint64_t end, std::vector<Match>& out) const;

    // 開始位置が [from, end) にある最初の一致。
    bool FindNext(const uint8_t* text, uint64_t n, uint64_t from, uint64_t end, Match& m) const;

    uint32_t MinLen() const { return minLen_; }
    uint32_t MaxLen() const { return maxLen_; }

    // ---- 内部用 (SIMD カーネルから参照) ----
    struct Variant
    {
        uint8_t b[4];
        uint8_t len;
    };
    struct Class
    {
        Variant v[4];
        uint8_t count;
    };

    uint32_t Verify(const uint8_t* t, uint64_t n, uint64_t pos) const;

    uint32_t off1_ = 0, off2_ = 0;
    uint8_t set1_[4] = {}, set2_[4] = {};
    int setN_ = 1;  // 1, 2, 4

private:
    bool Scan(const uint8_t* t, uint64_t n, uint64_t begin, uint64_t end, std::vector<Match>* out, Match* first) const;

    std::vector<Class> classes_;
    std::string literal_;
    bool isLiteral_ = false;
    bool wholeWord_ = false;
    bool wordStart_ = false;
    bool wordEnd_ = false;
    uint32_t minLen_ = 0, maxLen_ = 0;
};

// ---- UTF-8 / Unicode ユーティリティ ----
namespace utf8
{
bool Decode(const std::string& s, std::u32string& out);      // 不正なら false
int ValidLen(const uint8_t* p, uint64_t avail);              // 先頭の妥当な UTF-8 列の長さ。不正なら 0
void Append(std::string& s, char32_t cp);                    // UTF-8 で追加
char32_t DecodeAt(const uint8_t* t, uint64_t n, uint64_t pos);   // 不正は U+FFFD
char32_t DecodeBefore(const uint8_t* t, uint64_t pos);           // pos 直前の文字。不正は U+FFFD
bool IsWordChar(char32_t cp);
char32_t SimpleFold(char32_t cp);
}  // namespace utf8

// ---- SIMD 補助 ----
namespace simd
{
uint64_t CountByte(const uint8_t* p, uint64_t len, uint8_t b);
uint64_t CountCodePoints(const uint8_t* p, uint64_t len);  // 継続バイト以外の数
const uint8_t* FindLastByte(const uint8_t* p, uint64_t len, uint8_t b);  // 無ければ nullptr
bool HasAvx2();
}  // namespace simd
