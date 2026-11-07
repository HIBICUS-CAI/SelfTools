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
const char kHex[] = "0123456789ABCDEF";

// 妥当な UTF-8 列なら長さ (cp に値)、不正なら 0 (インライン版。前後文の生成は一致ごとに走るため)
inline int Dec(const uint8_t* p, uint64_t avail, char32_t& cp)
{
    const uint8_t b0 = p[0];
    if (b0 < 0x80)
    {
        cp = b0;
        return 1;
    }
    if (b0 < 0xC2) return 0;
    if (b0 < 0xE0)
    {
        if (avail < 2 || (p[1] & 0xC0) != 0x80) return 0;
        cp = ((b0 & 0x1Fu) << 6) | (p[1] & 0x3Fu);
        return 2;
    }
    if (b0 < 0xF0)
    {
        if (avail < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0;
        cp = ((b0 & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
        if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        return 3;
    }
    if (b0 < 0xF5)
    {
        if (avail < 4 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return 0;
        cp = ((b0 & 0x07u) << 18) | ((p[1] & 0x3Fu) << 12) | ((p[2] & 0x3Fu) << 6) | (p[3] & 0x3Fu);
        if (cp < 0x10000 || cp > 0x10FFFF) return 0;
        return 4;
    }
    return 0;
}

inline int Width(char32_t c)
{
    if (c < 0x300) return 1;
    if (c >= 0x3041 && c <= 0x9FFF && c != 0x3099 && c != 0x309A) return 2;  // かな・漢字 (頻出)
    return CharWidth(c);
}

// t[i] (i < lim) の 1 文字の表示幅と消費バイト数 (制御文字は \t \r \n \xHH、不正 UTF-8 は \xHH の幅)
// E3..E9 で始まる 3 バイト列 (U+3000..U+9FFF: かな・漢字・全角記号) の幅。対象外なら -1
inline int Cjk3Width(const uint8_t* p, uint64_t avail)
{
    if (p[0] < 0xE3 || p[0] > 0xE9 || avail < 3 || (p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return -1;
    if (p[0] == 0xE3)
    {
        if (p[1] == 0x82 && (p[2] == 0x99 || p[2] == 0x9A)) return 0;  // 結合用濁点・半濁点
        if (p[1] == 0x80 && p[2] == 0xBF) return 1;                     // U+303F
    }
    return 2;
}

inline int PieceWidth(const uint8_t* t, uint64_t lim, uint64_t i, uint64_t& len)
{
    const uint8_t b = t[i];
    len = 1;
    if (b >= 0x20 && b < 0x7F) return 1;
    if (const int cw = Cjk3Width(t + i, lim - i); cw >= 0)
    {
        len = 3;
        return cw;
    }
    if (b >= 0x80)
    {
        char32_t cp;
        if (const int l = Dec(t + i, lim - i, cp))
        {
            len = (uint64_t)l;
            return Width(cp);
        }
        return 4;
    }
    return (b == '\t' || b == '\r' || b == '\n') ? 2 : 4;
}

// t[a, b) を表示用に変換して追加
void EmitRange(std::string& out, const uint8_t* t, uint64_t a, uint64_t b)
{
    uint64_t run = a, i = a;
    while (i < b)
    {
        const uint8_t c = t[i];
        if (c >= 0x20 && c < 0x7F)
        {
            ++i;
            continue;
        }
        if (c >= 0x80)
        {
            if (Cjk3Width(t + i, b - i) >= 0)
            {
                i += 3;
                continue;
            }
            char32_t cp;
            if (const int l = Dec(t + i, b - i, cp))
            {
                i += (uint64_t)l;
                continue;
            }
        }
        out.append((const char*)t + run, (size_t)(i - run));
        if (c == '\t')
            out += "\\t";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\n')
            out += "\\n";
        else
        {
            const char e[4] = {'\\', 'x', kHex[c >> 4], kHex[c & 15]};
            out.append(e, 4);
        }
        run = ++i;
    }
    out.append((const char*)t + run, (size_t)(b - run));
}

// 前後文の範囲を決める
struct Bounds
{
    int pad = 0;
    uint64_t leftStart = 0;  // 左の文脈の開始位置
    uint64_t matchEnd = 0;   // 表示する一致部分の終端 (切り詰め時は途中)
    bool cut = false;        // 一致部分を切り詰めたか ("..")
    uint64_t rightEnd = 0;   // 右の文脈の終端
};

void ComputeBounds(const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget, Bounds& bd)
{
    if (budget < 1) budget = 1;
    const uint64_t me = pos + len;
    // 一致部分
    int mw = 0;
    uint64_t i = pos;
    while (i < me)
    {
        uint64_t l;
        const int w = PieceWidth(t, me, i, l);
        if (mw + w > budget) break;
        mw += w;
        i += l;
    }
    if (i < me)
    {
        // 収まらない: ".." の分 (2 桁) を空けて切り詰める
        bd.cut = true;
        uint64_t j = pos;
        int w2 = 0;
        while (j < me)
        {
            uint64_t l;
            const int w = PieceWidth(t, me, j, l);
            if (w2 + w + 2 > budget) break;
            w2 += w;
            j += l;
        }
        bd.matchEnd = j;
        bd.leftStart = pos;
        bd.rightEnd = me;
        return;
    }
    bd.matchEnd = me;
    const int side = (budget - mw) / 2;
    const int rightBudget = budget - mw - side;
    // 左側: 一致位置から行頭方向へ
    int lw = 0;
    uint64_t k = pos;
    while (k > 0)
    {
        uint64_t s = k - 1;
        int w;
        const uint8_t c = t[s];
        if (c >= 0x20 && c < 0x7F)
            w = 1;  // ASCII
        else if (k >= 3 && (c & 0xC0) == 0x80 && (w = Cjk3Width(t + k - 3, 3)) >= 0)
            s = k - 3;  // かな・漢字
        else
        {
            while (s > 0 && (t[s] & 0xC0) == 0x80 && k - s < 4) --s;
            char32_t cp;
            if (Dec(t + s, k - s, cp) != (int)(k - s)) s = k - 1;  // 不正な列は 1 バイトずつ
            if (t[s] == '\n') break;
            uint64_t l;
            w = PieceWidth(t, k, s, l);
        }
        if (lw + w > side) break;
        lw += w;
        k = s;
    }
    bd.leftStart = k;
    bd.pad = side - lw;
    // 右側: 一致の直後から行末方向へ (CRLF の \r は行末として扱う)
    int rw = 0;
    uint64_t r = me;
    while (r < n)
    {
        if (t[r] == '\n' || (t[r] == '\r' && r + 1 < n && t[r + 1] == '\n')) break;
        uint64_t l;
        const int w = PieceWidth(t, n, r, l);
        if (rw + w > rightBudget) break;
        rw += w;
        r += l;
    }
    bd.rightEnd = r;
}
}  // namespace

void BuildContext(const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget, Context& out)
{
    out = Context();
    Bounds bd;
    ComputeBounds(t, n, pos, len, budget, bd);
    out.pad = bd.pad;
    EmitRange(out.left, t, bd.leftStart, pos);
    EmitRange(out.match, t, pos, bd.matchEnd);
    if (bd.cut)
        out.match += "..";
    else
        EmitRange(out.right, t, pos + len, bd.rightEnd);
}

void AppendContextText(std::string& out, const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget)
{
    Bounds bd;
    ComputeBounds(t, n, pos, len, budget, bd);
    out.append((size_t)bd.pad, ' ');
    EmitRange(out, t, bd.leftStart, pos);
    out += "【";
    EmitRange(out, t, pos, bd.matchEnd);
    if (bd.cut) out += "..";
    out += "】";
    if (!bd.cut) EmitRange(out, t, pos + len, bd.rightEnd);
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
