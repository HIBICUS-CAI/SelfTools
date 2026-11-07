#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
using NativePath = std::wstring;  // \\?\ 付きの絶対パス
#else
using NativePath = std::string;
#endif

namespace plat
{
void ConsoleInit();
std::string ToUtf8(const NativePath& p);       // 表示用 (\\?\ を除去)
NativePath FromUtf8(const std::string& s);     // 絶対パス化 (+ Windows は \\?\ 付与)

// フォルダ内のファイルを列挙 (recursive 指定時はサブフォルダも。リパースポイントのフォルダは辿らない)
bool ListFiles(const NativePath& dir, bool recursive, std::vector<NativePath>& out, std::string& err);
bool IsDirectory(const NativePath& p);
bool SameFile(const NativePath& a, const NativePath& b);

// 入力ファイル (読み取り専用メモリマップ)。Close() まで他プロセスからの書き込みを禁止する
class InputFile
{
public:
    InputFile() = default;
    InputFile(const InputFile&) = delete;
    InputFile& operator=(const InputFile&) = delete;
    ~InputFile() { Close(); }

    bool Open(const NativePath& path, std::string& err);
    void Prefetch(uint64_t off, uint64_t len) const;  // 先読みヒント
    void Close();

    const uint8_t* Data() const { return data_; }
    uint64_t Size() const { return size_; }

private:
    const uint8_t* data_ = nullptr;
    uint64_t size_ = 0;
#ifdef _WIN32
    void* file_ = nullptr;
    void* map_ = nullptr;
#else
    int fd_ = -1;
#endif
};

// 4096 バイト境界に整列したメモリ
void* AllocAligned(uint64_t size);
void FreeAligned(void* p);

// 先頭から順にブロック単位で書き込む出力ファイル (Windows はキャッシュを経由しない非バッファ I/O)
class OutputFile
{
public:
    static constexpr uint64_t kAlign = 4096;

    OutputFile() = default;
    OutputFile(const OutputFile&) = delete;
    OutputFile& operator=(const OutputFile&) = delete;
    ~OutputFile();

    bool Create(const NativePath& path, uint64_t expectedSize, std::string& err);
    // p は AllocAligned で確保した領域。len は最終ブロック以外 kAlign の倍数であること。
    // 最終ブロックは kAlign の倍数に切り上げた容量を確保しておくこと (パディングを書き込む)。
    bool WriteBlock(uint8_t* p, uint64_t len, std::string& err);
    bool Finish(std::string& err);  // クローズしてサイズ確定
    void Abort();                   // クローズして削除

private:
    NativePath path_;
    uint64_t logical_ = 0;   // 論理サイズ
    uint64_t physical_ = 0;  // 実際に書いたサイズ (パディング込み)
    bool open_ = false;
#ifdef _WIN32
    void* h_ = nullptr;
    bool unbuffered_ = false;
#else
    int fd_ = -1;
#endif
};

// tmp で target を置き換える (属性等は target のものを維持)
bool ReplaceWith(const NativePath& target, const NativePath& tmp, std::string& err);
void DeleteFileQuiet(const NativePath& p);

// レポート等の通常書き込み用
class TextWriter
{
public:
    ~TextWriter() { Close(); }
    bool Open(const NativePath& path, std::string& err);
    void Write(const std::string& s);
    bool Close();
    bool Ok() const { return ok_; }

private:
    std::FILE* fp_ = nullptr;
    bool ok_ = true;
};
}  // namespace plat
