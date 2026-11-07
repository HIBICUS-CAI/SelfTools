#include "Pager.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "Matcher.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace view
{
int CharWidth(char32_t c)
{
    if (c == 0x200B || (c >= 0x0300 && c <= 0x036F) || (c >= 0x3099 && c <= 0x309A) || (c >= 0xFE00 && c <= 0xFE0F)) return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0x303E) || (c >= 0x3041 && c <= 0x33FF) || (c >= 0x3400 && c <= 0x4DBF) ||
        (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xA000 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) || (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1F64F) ||
        (c >= 0x1F900 && c <= 0x1F9FF) || (c >= 0x20000 && c <= 0x3FFFD))
        return 2;
    return 1;
}

int TextWidth(const std::string& s)
{
    const uint8_t* p = (const uint8_t*)s.data();
    const uint64_t n = s.size();
    int w = 0;
    for (uint64_t i = 0; i < n;)
    {
        const int l = utf8::ValidLen(p + i, n - i);
        if (l == 0)
        {
            ++w;
            ++i;
            continue;
        }
        w += CharWidth(utf8::DecodeAt(p, n, i));
        i += (uint64_t)l;
    }
    return w;
}

int TerminalWidth()
{
#ifdef _WIN32
    const DWORD ids[] = {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    for (DWORD id : ids)
    {
        CONSOLE_SCREEN_BUFFER_INFO ci;
        if (GetConsoleScreenBufferInfo(GetStdHandle(id), &ci)) return ci.srWindow.Right - ci.srWindow.Left + 1;
    }
#else
    for (int fd : {1, 2, 0})
    {
        winsize ws{};
        if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    }
#endif
    return 0;
}

namespace
{
// t[i] から 1 文字を表示用に変換 (制御文字は \t \r \n \xHH、不正 UTF-8 は \xHH)。戻り値: 消費バイト数
uint64_t Piece(const uint8_t* t, uint64_t n, uint64_t i, std::string& s, int& w)
{
    static const char kHex[] = "0123456789ABCDEF";
    const uint8_t b = t[i];
    s.clear();
    if (b >= 0x20 && b < 0x7F)
    {
        s.push_back((char)b);
        w = 1;
        return 1;
    }
    if (b >= 0x80)
    {
        if (const int l = utf8::ValidLen(t + i, n - i))
        {
            s.assign((const char*)t + i, (size_t)l);
            w = CharWidth(utf8::DecodeAt(t, n, i));
            return (uint64_t)l;
        }
    }
    if (b == '\t')
        s = "\\t";
    else if (b == '\r')
        s = "\\r";
    else if (b == '\n')
        s = "\\n";
    else
    {
        s = "\\x";
        s.push_back(kHex[b >> 4]);
        s.push_back(kHex[b & 15]);
    }
    w = (int)s.size();
    return 1;
}
}  // namespace

void BuildContext(const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget, Context& out)
{
    out = Context();
    if (budget < 1) budget = 1;
    std::string pc;
    int pw = 0;
    // 一致部分
    int mw = 0;
    {
        const uint64_t e = pos + len;
        for (uint64_t i = pos; i < e;)
        {
            const uint64_t l = Piece(t, n, i, pc, pw);
            if (mw + pw > budget)
            {
                // 収まらない: 末尾を ".." にして切り詰める
                while (mw + 2 > budget && !out.match.empty())
                {
                    // 直前の 1 文字を落とす
                    size_t k = out.match.size() - 1;
                    while (k > 0 && ((uint8_t)out.match[k] & 0xC0) == 0x80) --k;
                    out.match.resize(k);
                    mw = TextWidth(out.match);
                }
                out.match += "..";
                return;
            }
            out.match += pc;
            mw += pw;
            i += l;
        }
    }
    const int side = (budget - mw) / 2;
    const int rightBudget = budget - mw - side;

    // 左側: 一致位置から行頭方向へ
    {
        std::string rev;  // 1 文字ずつ前に足していく
        int lw = 0;
        uint64_t i = pos;
        while (i > 0)
        {
            uint64_t k = i - 1;
            while (k > 0 && (t[k] & 0xC0) == 0x80 && i - k < 4) --k;
            if (utf8::ValidLen(t + k, i - k) != (int)(i - k)) k = i - 1;  // 不正な列は 1 バイトずつ
            if (t[k] == '\n') break;
            Piece(t, i, k, pc, pw);  // [k, i) の範囲だけで解釈する
            if (lw + pw > side) break;
            rev.insert(0, pc);
            lw += pw;
            i = k;
        }
        out.left = std::move(rev);
        out.pad = side - lw;
    }
    // 右側: 一致の直後から行末方向へ (CRLF の \r は行末として扱う)
    {
        int rw = 0;
        for (uint64_t i = pos + len; i < n;)
        {
            if (t[i] == '\n' || (t[i] == '\r' && i + 1 < n && t[i + 1] == '\n')) break;
            const uint64_t l = Piece(t, n, i, pc, pw);
            if (rw + pw > rightBudget) break;
            out.right += pc;
            rw += pw;
            i += l;
        }
    }
}

void AppendClipped(std::string& out, const std::string& s, uint64_t& col, uint64_t hscroll, uint64_t cols)
{
    const uint64_t end = hscroll + cols;
    const uint8_t* p = (const uint8_t*)s.data();
    const uint64_t n = s.size();
    for (uint64_t i = 0; i < n && col < end;)
    {
        int l = utf8::ValidLen(p + i, n - i);
        int w;
        bool printable;
        if (l == 0)
        {
            l = 1;
            w = 1;
            printable = false;
        }
        else
        {
            const char32_t cp = utf8::DecodeAt(p, n, i);
            w = CharWidth(cp);
            printable = !(cp < 0x20 || cp == 0x7F);
        }
        if (col + (uint64_t)w <= hscroll)
        {
            // 左に隠れる部分
        }
        else if (col < hscroll)
            out.push_back(' ');  // 横スクロール境界をまたぐ全角文字
        else if (col + (uint64_t)w > end)
            break;
        else if (printable)
            out.append((const char*)p + i, (size_t)l);
        else
            out.push_back('?');  // 制御シーケンスの混入を防ぐ
        col += (uint64_t)w;
        i += (uint64_t)l;
    }
}
}  // namespace view

namespace pager
{
namespace
{
enum class Key
{
    None,
    Up,
    Down,
    PageUp,
    PageDown,
    Home,
    End,
    Left,
    Right,
    WheelUp,
    WheelDown,
    Quit,
    Redraw
};

// ---- 端末入出力 ----
#ifdef _WIN32
struct Term
{
    HANDLE in = nullptr, out = nullptr;
    DWORD inMode = 0, outMode = 0;
    UINT outCp = 0;

    bool Begin()
    {
        in = GetStdHandle(STD_INPUT_HANDLE);
        out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleMode(in, &inMode) || !GetConsoleMode(out, &outMode)) return false;
        outCp = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleMode(out, outMode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        // 行入力・エコー・Ctrl+C 処理・簡易編集を無効化し、マウスとウィンドウサイズ変更を受け取る
        SetConsoleMode(in, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);
        Write("\x1b[?1049h\x1b[?25l");
        return true;
    }
    void End()
    {
        Write("\x1b[0m\x1b[?25h\x1b[?1049l");
        SetConsoleMode(in, inMode);
        SetConsoleMode(out, outMode);
        SetConsoleOutputCP(outCp);
    }
    void Write(const std::string& s)
    {
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w((size_t)n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        DWORD done = 0;
        WriteConsoleW(out, w.data(), (DWORD)w.size(), &done, nullptr);
    }
    void Size(int& rows, int& cols)
    {
        CONSOLE_SCREEN_BUFFER_INFO ci;
        if (GetConsoleScreenBufferInfo(out, &ci))
        {
            rows = ci.srWindow.Bottom - ci.srWindow.Top + 1;
            cols = ci.srWindow.Right - ci.srWindow.Left + 1;
        }
        else
        {
            rows = 25;
            cols = 80;
        }
    }
    Key Read()
    {
        for (;;)
        {
            INPUT_RECORD r;
            DWORD n = 0;
            if (!ReadConsoleInputW(in, &r, 1, &n) || n == 0) return Key::Quit;
            if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) return Key::Redraw;
            if (r.EventType == MOUSE_EVENT && r.Event.MouseEvent.dwEventFlags == MOUSE_WHEELED)
                return ((int)(short)HIWORD(r.Event.MouseEvent.dwButtonState) > 0) ? Key::WheelUp : Key::WheelDown;
            if (r.EventType != KEY_EVENT || !r.Event.KeyEvent.bKeyDown) continue;
            switch (r.Event.KeyEvent.wVirtualKeyCode)
            {
            case VK_UP: return Key::Up;
            case VK_DOWN: return Key::Down;
            case VK_PRIOR: return Key::PageUp;
            case VK_NEXT: return Key::PageDown;
            case VK_HOME: return Key::Home;
            case VK_END: return Key::End;
            case VK_LEFT: return Key::Left;
            case VK_RIGHT: return Key::Right;
            case VK_ESCAPE: return Key::Quit;
            default: break;
            }
            switch (r.Event.KeyEvent.uChar.UnicodeChar)
            {
            case L'q':
            case L'Q':
            case 3: return Key::Quit;  // Ctrl+C
            case L'j':
            case L'\r': return Key::Down;
            case L'k': return Key::Up;
            case L' ':
            case L'f': return Key::PageDown;
            case L'b': return Key::PageUp;
            case L'g': return Key::Home;
            case L'G': return Key::End;
            case L'h': return Key::Left;
            case L'l': return Key::Right;
            default: break;
            }
        }
    }
};

bool InteractiveImpl()
{
    DWORD m;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &m) && GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &m);
}
#else
struct Term
{
    termios saved{};

    bool Begin()
    {
        if (tcgetattr(0, &saved) != 0) return false;
        termios raw = saved;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
        raw.c_iflag &= ~(IXON | ICRNL);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &raw);
        Write("\x1b[?1049h\x1b[?25l");
        return true;
    }
    void End()
    {
        Write("\x1b[0m\x1b[?25h\x1b[?1049l");
        tcsetattr(0, TCSANOW, &saved);
    }
    void Write(const std::string& s)
    {
        size_t off = 0;
        while (off < s.size())
        {
            const ssize_t w = write(1, s.data() + off, s.size() - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
    }
    void Size(int& rows, int& cols)
    {
        winsize ws{};
        if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0)
        {
            rows = ws.ws_row;
            cols = ws.ws_col;
        }
        else
        {
            rows = 24;
            cols = 80;
        }
    }
    int Get(int timeoutMs)
    {
        pollfd pf{0, POLLIN, 0};
        if (poll(&pf, 1, timeoutMs) <= 0) return -1;
        unsigned char c;
        return read(0, &c, 1) == 1 ? c : -2;
    }
    Key Read()
    {
        for (;;)
        {
            int c = Get(250);
            if (c == -1) return Key::Redraw;  // 端末サイズ変更に追従するため定期的に再描画
            if (c == -2) return Key::Quit;
            if (c == 0x1b)
            {
                int c1 = Get(30);
                if (c1 < 0) return Key::Quit;  // Esc 単独
                if (c1 != '[' && c1 != 'O') continue;
                int c2 = Get(30);
                switch (c2)
                {
                case 'A': return Key::Up;
                case 'B': return Key::Down;
                case 'C': return Key::Right;
                case 'D': return Key::Left;
                case 'H': return Key::Home;
                case 'F': return Key::End;
                default: break;
                }
                if (c2 >= '0' && c2 <= '9')
                {
                    int c3 = Get(30);
                    if (c3 == '~')
                    {
                        switch (c2)
                        {
                        case '5': return Key::PageUp;
                        case '6': return Key::PageDown;
                        case '1':
                        case '7': return Key::Home;
                        case '4':
                        case '8': return Key::End;
                        default: break;
                        }
                    }
                }
                continue;
            }
            switch (c)
            {
            case 'q':
            case 'Q':
            case 3: return Key::Quit;
            case 'j':
            case '\r':
            case '\n': return Key::Down;
            case 'k': return Key::Up;
            case ' ':
            case 'f': return Key::PageDown;
            case 'b': return Key::PageUp;
            case 'g': return Key::Home;
            case 'G': return Key::End;
            case 'h': return Key::Left;
            case 'l': return Key::Right;
            default: break;
            }
        }
    }
};

bool InteractiveImpl() { return isatty(0) && isatty(1); }
#endif

}  // namespace

bool IsInteractive() { return InteractiveImpl(); }

void Run(Source& src, const std::string& title)
{
    Term term;
    if (!term.Begin()) return;
    const uint64_t N = src.Count();
    uint64_t top = 0, hscroll = 0;
    for (;;)
    {
        int rows, cols;
        term.Size(rows, cols);
        const uint64_t body = (uint64_t)std::max(1, rows - 1);
        const uint64_t maxTop = N > body ? N - body : 0;
        top = std::min(top, maxTop);

        std::string frame = "\x1b[H";
        for (uint64_t r = 0; r < body; ++r)
        {
            const uint64_t i = top + r;
            if (i < N)
                src.Render(i, cols, hscroll, frame);
            else
                frame += "~";
            frame += "\x1b[0m\x1b[K\r\n";
        }
        // ステータス行
        std::string st = " " + title + "  " + std::to_string(N ? top + 1 : 0) + "-" + std::to_string(std::min(N, top + body)) + " / " +
                         std::to_string(N) + " 行";
        if (hscroll) st += "  (横 +" + std::to_string(hscroll) + ")";
        if (top >= maxTop) st += "  (END)";
        st += "   q:閉じる ↑↓:行 PgUp/PgDn:ページ Home/End ←→:横";
        frame += "\x1b[7m";
        uint64_t c = 0;
        view::AppendClipped(frame, st, c, 0, (uint64_t)cols);
        frame += "\x1b[K\x1b[0m";
        term.Write(frame);

        switch (term.Read())
        {
        case Key::Quit: term.End(); return;
        case Key::Up: top = top ? top - 1 : 0; break;
        case Key::Down: top = std::min(maxTop, top + 1); break;
        case Key::WheelUp: top = top > 3 ? top - 3 : 0; break;
        case Key::WheelDown: top = std::min(maxTop, top + 3); break;
        case Key::PageUp: top = top > body ? top - body : 0; break;
        case Key::PageDown: top = std::min(maxTop, top + body); break;
        case Key::Home: top = 0; break;
        case Key::End: top = maxTop; break;
        case Key::Left: hscroll = hscroll > 8 ? hscroll - 8 : 0; break;
        case Key::Right: hscroll += 8; break;
        default: break;
        }
    }
}
}  // namespace pager
