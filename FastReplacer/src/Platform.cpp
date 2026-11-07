#include "Platform.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#endif

namespace plat
{
#ifdef _WIN32
// ============================================================================
// Windows
// ============================================================================
static std::string WideToUtf8(const wchar_t* w, size_t len)
{
    if (len == 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, (int)len, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, (int)len, s.data(), n, nullptr, nullptr);
    return s;
}

static std::string LastErrorText(DWORD code = GetLastError())
{
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                             (LPWSTR)&buf, 0, nullptr);
    std::string s = n ? WideToUtf8(buf, n) : std::string();
    if (buf) LocalFree(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s + " (code " + std::to_string(code) + ")";
}

void ConsoleInit()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

std::string ToUtf8(const NativePath& p)
{
    std::wstring w = p;
    if (w.rfind(L"\\\\?\\UNC\\", 0) == 0)
        w = L"\\\\" + w.substr(8);
    else if (w.rfind(L"\\\\?\\", 0) == 0)
        w = w.substr(4);
    return WideToUtf8(w.data(), w.size());
}

NativePath FromUtf8(const std::string& s)
{
    std::wstring w;
    if (!s.empty())
    {
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        w.resize((size_t)n);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    }
    if (w.rfind(L"\\\\?\\", 0) == 0) return w;
    DWORD need = GetFullPathNameW(w.c_str(), 0, nullptr, nullptr);
    if (need == 0) return w;
    std::wstring full((size_t)need, L'\0');
    DWORD got = GetFullPathNameW(w.c_str(), need, full.data(), nullptr);
    full.resize(got);
    if (full.rfind(L"\\\\", 0) == 0) return L"\\\\?\\UNC\\" + full.substr(2);
    return L"\\\\?\\" + full;
}

static std::wstring JoinPath(const std::wstring& dir, const wchar_t* name)
{
    std::wstring r = dir;
    if (!r.empty() && r.back() != L'\\') r.push_back(L'\\');
    r += name;
    return r;
}

static void ListRec(const std::wstring& dir, bool recursive, std::vector<NativePath>& out, bool root, bool& rootOk, std::string& err)
{
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW(JoinPath(dir, L"*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE)
    {
        if (root)
        {
            rootOk = false;
            err = LastErrorText();
        }
        return;
    }
    std::vector<std::wstring> subdirs;
    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = JoinPath(dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (recursive && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) subdirs.push_back(std::move(full));
        }
        else
        {
            out.push_back(std::move(full));
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (const auto& d : subdirs) ListRec(d, recursive, out, false, rootOk, err);
}

bool ListFiles(const NativePath& dir, bool recursive, std::vector<NativePath>& out, std::string& err)
{
    bool ok = true;
    ListRec(dir, recursive, out, true, ok, err);
    std::sort(out.begin(), out.end());
    return ok;
}

bool IsDirectory(const NativePath& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

bool SameFile(const NativePath& a, const NativePath& b)
{
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b.c_str(), (int)b.size(), TRUE) == CSTR_EQUAL;
}

// ---- InputFile ----
bool InputFile::Open(const NativePath& path, std::string& err)
{
    Close();
    path_ = path;
    // 通常ハンドルで開く。FILE_FLAG_NO_BUFFERING で開くと、それだけで OS がこのファイルのキャッシュを
    // 書き出して破棄するため、非バッファ読み込み (--direct) 時のみ PrepareRam で開き直す
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        err = "オープン失敗: " + LastErrorText();
        return false;
    }
    file_ = h;
    unbuffered_ = false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz))
    {
        err = "サイズ取得失敗: " + LastErrorText();
        Close();
        return false;
    }
    size_ = (uint64_t)sz.QuadPart;
    return true;
}

bool InputFile::PrepareRam(std::string& err)
{
    if (size_ == 0)
    {
        mode_ = Mode::Ram;
        return true;
    }
    // 非バッファ + 非同期で開き直す (失敗時はバッファ付き非同期)
    HANDLE h = CreateFileW(path_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, nullptr);
    bool unbuf = h != INVALID_HANDLE_VALUE;
    if (!unbuf)
        h = CreateFileW(path_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                        nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        err = "オープン失敗: " + LastErrorText();
        return false;
    }
    // 非バッファ読み込みはセクタ単位なので末尾を 4096 の倍数に切り上げて確保
    const uint64_t cap = ((size_ + 4095) & ~4095ull) + 4096;
    void* p = VirtualAlloc(nullptr, (SIZE_T)cap, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p)
    {
        err = "バッファ確保失敗: " + LastErrorText();
        CloseHandle(h);
        return false;
    }
    CloseHandle((HANDLE)file_);
    file_ = h;
    unbuffered_ = unbuf;
    data_ = (const uint8_t*)p;
    mode_ = Mode::Ram;
    return true;
}

bool InputFile::PrepareMapped(std::string& err)
{
    if (size_ == 0)
    {
        mode_ = Mode::Mapped;
        return true;
    }
    HANDLE m = CreateFileMappingW((HANDLE)file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!m)
    {
        err = "マッピング失敗: " + LastErrorText();
        return false;
    }
    map_ = m;
    void* v = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
    if (!v)
    {
        err = "MapViewOfFile 失敗: " + LastErrorText();
        return false;
    }
    data_ = (const uint8_t*)v;
    mode_ = Mode::Mapped;
    return true;
}

bool InputFile::Load(uint64_t unit, unsigned queueDepth, const std::function<void(size_t)>& onDone, std::string& err)
{
    if (mode_ != Mode::Ram || size_ == 0) return true;
    const size_t units = (size_t)((size_ + unit - 1) / unit);
    const unsigned qd = std::max(1u, std::min<unsigned>(queueDepth, (unsigned)std::min<size_t>(units, 64)));
    struct Req
    {
        OVERLAPPED ov;
        HANDLE ev;
        size_t idx;
        DWORD expect;
    };
    std::vector<Req> reqs(qd);
    for (auto& r : reqs)
    {
        r.ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!r.ev)
        {
            err = "イベント作成失敗: " + LastErrorText();
            for (auto& q : reqs)
                if (q.ev) CloseHandle(q.ev);
            return false;
        }
    }
    uint8_t* buf = (uint8_t*)data_;
    bool ok = true;
    size_t issued = 0, completed = 0;
    auto issue = [&](Req& r, size_t i) -> bool
    {
        const uint64_t off = (uint64_t)i * unit;
        const uint64_t avail = std::min(unit, size_ - off);
        const uint64_t len = unbuffered_ ? ((avail + 4095) & ~4095ull) : avail;  // 非バッファはセクタ単位
        std::memset(&r.ov, 0, sizeof(r.ov));
        r.ov.Offset = (DWORD)(off & 0xFFFFFFFFu);
        r.ov.OffsetHigh = (DWORD)(off >> 32);
        r.ov.hEvent = r.ev;
        r.idx = i;
        r.expect = (DWORD)avail;
        ResetEvent(r.ev);
        if (!ReadFile((HANDLE)file_, buf + off, (DWORD)len, nullptr, &r.ov) && GetLastError() != ERROR_IO_PENDING)
        {
            err = "読み込み失敗: " + LastErrorText();
            return false;
        }
        return true;
    };
    while (ok && issued < units && issued < qd)
    {
        ok = issue(reqs[issued % qd], issued);
        if (ok) ++issued;
    }
    while (completed < issued)
    {
        Req& r = reqs[completed % qd];
        DWORD got = 0;
        const BOOL res = GetOverlappedResult((HANDLE)file_, &r.ov, &got, TRUE);
        if (ok && (!res || got != r.expect))
        {
            ok = false;
            err = res ? "読み込み失敗: サイズ不一致 (読み込み中にファイルが変更された可能性)" : "読み込み失敗: " + LastErrorText();
        }
        ++completed;
        if (ok)
        {
            onDone(r.idx);
            if (issued < units)
            {
                ok = issue(r, issued);
                if (ok) ++issued;
            }
        }
    }
    for (auto& r : reqs) CloseHandle(r.ev);
    return ok;
}

void InputFile::Close()
{
    if (data_)
    {
        if (mode_ == Mode::Mapped)
            UnmapViewOfFile(data_);
        else
            VirtualFree((void*)data_, 0, MEM_RELEASE);
    }
    if (map_) CloseHandle((HANDLE)map_);
    if (file_) CloseHandle((HANDLE)file_);
    data_ = nullptr;
    map_ = file_ = nullptr;
    size_ = 0;
    mode_ = Mode::None;
}

namespace
{
struct RangeEntry
{
    PVOID VirtualAddress;
    SIZE_T NumberOfBytes;
};
using PrefetchFn = BOOL(WINAPI*)(HANDLE, ULONG_PTR, RangeEntry*, ULONG);
PrefetchFn GetPrefetch()
{
    static PrefetchFn fn = (PrefetchFn)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "PrefetchVirtualMemory");
    return fn;
}
}  // namespace

void InputFile::Prefetch(uint64_t off, uint64_t len) const
{
    if (mode_ != Mode::Mapped || !data_ || off >= size_) return;
    len = std::min(len, size_ - off);
    if (PrefetchFn fn = GetPrefetch())
    {
        RangeEntry e{(PVOID)(data_ + off), (SIZE_T)len};
        fn(GetCurrentProcess(), 1, &e, 0);
    }
}

uint64_t AvailableMemory()
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    return (uint64_t)ms.ullAvailPhys;
}

// ---- OutputFile ----
void* AllocAligned(uint64_t size) { return VirtualAlloc(nullptr, (SIZE_T)size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }
void FreeAligned(void* p)
{
    if (p) VirtualFree(p, 0, MEM_RELEASE);
}

OutputFile::~OutputFile()
{
    if (open_) Abort();
}

bool OutputFile::Create(const NativePath& path, uint64_t expectedSize, std::string& err)
{
    path_ = path;
    logical_ = physical_ = 0;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, nullptr);
    unbuffered_ = h != INVALID_HANDLE_VALUE;
    if (!unbuffered_)
    {
        h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            err = "出力ファイル作成失敗: " + LastErrorText();
            return false;
        }
    }
    h_ = h;
    open_ = true;
    // 断片化を減らすため領域を予約 (失敗しても続行)
    FILE_ALLOCATION_INFO ai;
    ai.AllocationSize.QuadPart = (LONGLONG)((expectedSize + kAlign - 1) & ~(kAlign - 1));
    SetFileInformationByHandle(h, FileAllocationInfo, &ai, sizeof(ai));
    return true;
}

bool OutputFile::WriteBlock(uint8_t* p, uint64_t len, std::string& err)
{
    logical_ += len;
    uint64_t bytes = len;
    if (unbuffered_ && (len & (kAlign - 1)))
    {
        bytes = (len + kAlign - 1) & ~(kAlign - 1);  // 末尾ブロックのみ: セクタ境界へ切り上げ
        std::memset(p + len, 0, (size_t)(bytes - len));
    }
    while (bytes)
    {
        const DWORD chunk = (DWORD)std::min<uint64_t>(bytes, 1ull << 30);
        DWORD done = 0;
        if (!WriteFile((HANDLE)h_, p, chunk, &done, nullptr) || done != chunk)
        {
            err = "書き込み失敗: " + LastErrorText();
            return false;
        }
        p += chunk;
        bytes -= chunk;
        physical_ += chunk;
    }
    return true;
}

bool OutputFile::Finish(std::string& err)
{
    bool ok = true;
    if (physical_ != logical_)
    {
        // 非バッファ書き込みのパディング分を切り詰める
        FILE_END_OF_FILE_INFO eof;
        eof.EndOfFile.QuadPart = (LONGLONG)logical_;
        if (!SetFileInformationByHandle((HANDLE)h_, FileEndOfFileInfo, &eof, sizeof(eof)))
        {
            ok = false;
            err = "サイズ確定失敗: " + LastErrorText();
        }
    }
    if (!CloseHandle((HANDLE)h_) && ok)
    {
        ok = false;
        err = "クローズ失敗: " + LastErrorText();
    }
    h_ = nullptr;
    open_ = false;
    if (!ok) DeleteFileW(path_.c_str());
    return ok;
}

void OutputFile::Abort()
{
    if (!open_) return;
    CloseHandle((HANDLE)h_);
    h_ = nullptr;
    open_ = false;
    DeleteFileW(path_.c_str());
}

bool ReplaceWith(const NativePath& target, const NativePath& tmp, std::string& err)
{
    if (ReplaceFileW(target.c_str(), tmp.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr, nullptr))
        return true;
    const DWORD e1 = GetLastError();
    if (MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    err = "置き換え失敗: ReplaceFile=" + LastErrorText(e1) + " / MoveFileEx=" + LastErrorText();
    DeleteFileW(tmp.c_str());
    return false;
}

void DeleteFileQuiet(const NativePath& p) { DeleteFileW(p.c_str()); }

bool TextWriter::Open(const NativePath& path, std::string& err)
{
    fp_ = _wfopen(path.c_str(), L"wb");
    if (!fp_)
    {
        err = "レポート作成失敗: " + LastErrorText();
        return false;
    }
    setvbuf(fp_, nullptr, _IOFBF, 8 << 20);
    return true;
}

#else
// ============================================================================
// POSIX (開発・テスト用)
// ============================================================================
static std::string ErrnoText() { return std::string(std::strerror(errno)) + " (errno " + std::to_string(errno) + ")"; }

void ConsoleInit() {}
std::string ToUtf8(const NativePath& p) { return p; }

NativePath FromUtf8(const std::string& s)
{
    if (!s.empty() && s[0] == '/') return s;
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf))) return s;
    std::string r = buf;
    if (s.empty() || s == ".") return r;
    return r + "/" + s;
}

static void ListRec(const std::string& dir, bool recursive, std::vector<NativePath>& out, bool root, bool& rootOk, std::string& err)
{
    DIR* d = opendir(dir.c_str());
    if (!d)
    {
        if (root)
        {
            rootOk = false;
            err = ErrnoText();
        }
        return;
    }
    std::vector<std::string> subdirs;
    while (dirent* e = readdir(d))
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        std::string full = dir + (dir.back() == '/' ? "" : "/") + e->d_name;
        struct stat st;
        if (lstat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode))
        {
            if (recursive) subdirs.push_back(full);
        }
        else if (S_ISREG(st.st_mode))
            out.push_back(full);
    }
    closedir(d);
    for (const auto& s : subdirs) ListRec(s, recursive, out, false, rootOk, err);
}

bool ListFiles(const NativePath& dir, bool recursive, std::vector<NativePath>& out, std::string& err)
{
    bool ok = true;
    ListRec(dir, recursive, out, true, ok, err);
    std::sort(out.begin(), out.end());
    return ok;
}

bool IsDirectory(const NativePath& p)
{
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool SameFile(const NativePath& a, const NativePath& b) { return a == b; }

bool InputFile::Open(const NativePath& path, std::string& err)
{
    Close();
    path_ = path;
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0)
    {
        err = "オープン失敗: " + ErrnoText();
        return false;
    }
    struct stat st;
    fstat(fd_, &st);
    size_ = (uint64_t)st.st_size;
    return true;
}

bool InputFile::PrepareRam(std::string& err)
{
    mode_ = Mode::Ram;
    if (size_ == 0) return true;
    void* p = nullptr;
    if (posix_memalign(&p, 4096, (size_t)(((size_ + 4095) & ~4095ull) + 4096)) != 0)
    {
        mode_ = Mode::None;
        err = "バッファ確保失敗";
        return false;
    }
    data_ = (const uint8_t*)p;
    return true;
}

bool InputFile::PrepareMapped(std::string& err)
{
    if (size_ > 0)
    {
        void* p = mmap(nullptr, size_, PROT_READ, MAP_SHARED, fd_, 0);
        if (p == MAP_FAILED)
        {
            err = "mmap 失敗: " + ErrnoText();
            return false;
        }
        data_ = (const uint8_t*)p;
    }
    mode_ = Mode::Mapped;
    return true;
}

bool InputFile::Load(uint64_t unit, unsigned, const std::function<void(size_t)>& onDone, std::string& err)
{
    if (mode_ != Mode::Ram || size_ == 0) return true;
    uint8_t* buf = (uint8_t*)data_;
    for (uint64_t off = 0, i = 0; off < size_; off += unit, ++i)
    {
        const uint64_t len = std::min(unit, size_ - off);
        uint64_t done = 0;
        while (done < len)
        {
            ssize_t r = pread(fd_, buf + off + done, (size_t)(len - done), (off_t)(off + done));
            if (r <= 0)
            {
                err = r == 0 ? std::string("読み込み失敗: 予期しない EOF") : "読み込み失敗: " + ErrnoText();
                return false;
            }
            done += (uint64_t)r;
        }
        onDone((size_t)i);
    }
    return true;
}

void InputFile::Close()
{
    if (data_)
    {
        if (mode_ == Mode::Mapped)
            munmap((void*)data_, size_);
        else
            std::free((void*)data_);
    }
    if (fd_ >= 0) close(fd_);
    data_ = nullptr;
    fd_ = -1;
    size_ = 0;
    mode_ = Mode::None;
}

void InputFile::Prefetch(uint64_t off, uint64_t len) const
{
    if (mode_ != Mode::Mapped || !data_ || off >= size_) return;
    len = std::min(len, size_ - off);
    const uint64_t a = off & ~4095ull;
    madvise((void*)(data_ + a), len + (off - a), MADV_WILLNEED);
}

uint64_t AvailableMemory()
{
    FILE* f = std::fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256];
    uint64_t kb = 0;
    while (std::fgets(line, sizeof(line), f))
        if (std::sscanf(line, "MemAvailable: %llu kB", (unsigned long long*)&kb) == 1) break;
    std::fclose(f);
    return kb * 1024;
}

void* AllocAligned(uint64_t size) { return std::aligned_alloc(4096, (size_t)((size + 4095) & ~4095ull)); }
void FreeAligned(void* p) { std::free(p); }

OutputFile::~OutputFile()
{
    if (open_) Abort();
}

bool OutputFile::Create(const NativePath& path, uint64_t, std::string& err)
{
    path_ = path;
    logical_ = physical_ = 0;
    fd_ = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0)
    {
        err = "出力ファイル作成失敗: " + ErrnoText();
        return false;
    }
    open_ = true;
    return true;
}

bool OutputFile::WriteBlock(uint8_t* p, uint64_t len, std::string& err)
{
    logical_ += len;
    while (len)
    {
        ssize_t w = write(fd_, p, (size_t)std::min<uint64_t>(len, 1ull << 30));
        if (w <= 0)
        {
            err = "書き込み失敗: " + ErrnoText();
            return false;
        }
        p += w;
        len -= (uint64_t)w;
        physical_ += (uint64_t)w;
    }
    return true;
}

bool OutputFile::Finish(std::string& err)
{
    bool ok = close(fd_) == 0;
    if (!ok) err = "close 失敗: " + ErrnoText();
    fd_ = -1;
    open_ = false;
    if (!ok) unlink(path_.c_str());
    return ok;
}

void OutputFile::Abort()
{
    if (!open_) return;
    close(fd_);
    fd_ = -1;
    open_ = false;
    unlink(path_.c_str());
}

bool ReplaceWith(const NativePath& target, const NativePath& tmp, std::string& err)
{
    struct stat st;
    if (stat(target.c_str(), &st) == 0) chmod(tmp.c_str(), st.st_mode & 07777);
    if (rename(tmp.c_str(), target.c_str()) == 0) return true;
    err = "置き換え失敗: " + ErrnoText();
    unlink(tmp.c_str());
    return false;
}

void DeleteFileQuiet(const NativePath& p) { unlink(p.c_str()); }

bool TextWriter::Open(const NativePath& path, std::string& err)
{
    fp_ = std::fopen(path.c_str(), "wb");
    if (!fp_)
    {
        err = "レポート作成失敗: " + ErrnoText();
        return false;
    }
    setvbuf(fp_, nullptr, _IOFBF, 8 << 20);
    return true;
}
#endif

void TextWriter::Write(const std::string& s)
{
    if (fp_ && !s.empty() && std::fwrite(s.data(), 1, s.size(), fp_) != s.size()) ok_ = false;
}

bool TextWriter::Close()
{
    if (fp_)
    {
        if (std::fclose(fp_) != 0) ok_ = false;
        fp_ = nullptr;
    }
    return ok_;
}
}  // namespace plat
