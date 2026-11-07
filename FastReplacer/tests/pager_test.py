#!/usr/bin/env python3
"""ページャを疑似端末 (pty) 上で操作して検証する (POSIX 専用)。

usage: python3 pager_test.py <FastReplacer 実行ファイル> [--tempfile]
"""
import fcntl
import os
import pty
import re
import select
import shutil
import struct
import sys
import tempfile
import termios
import time

ROWS, COLS = 12, 60


def read_until(fd, pred, timeout=10.0):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                chunk = os.read(fd, 65536)
            except OSError:
                break
            if not chunk:
                break
            buf += chunk
            if pred(buf):
                return buf
    return buf


def char_width(c):
    o = ord(c)
    if o == 0x200B or 0x300 <= o <= 0x36F or 0x3099 <= o <= 0x309A or 0xFE00 <= o <= 0xFE0F:
        return 0
    for a, b in ((0x1100, 0x115F), (0x2E80, 0x303E), (0x3041, 0x33FF), (0x3400, 0x4DBF), (0x4E00, 0x9FFF), (0xA000, 0xA4CF),
                 (0xAC00, 0xD7A3), (0xF900, 0xFAFF), (0xFE30, 0xFE4F), (0xFF00, 0xFF60), (0xFFE0, 0xFFE6), (0x1F300, 0x1F64F),
                 (0x1F900, 0x1F9FF), (0x20000, 0x3FFFD)):
        if a <= o <= b:
            return 2
    return 1


def width(line):
    return sum(char_width(c) for c in line)


def frame_lines(buf: bytes):
    return frame_text(buf).replace("\r", "").split("\n")


def frame_text(buf: bytes) -> str:
    """最後に描画されたフレーム (ESC[H 以降) を素の文字列にする"""
    s = buf.decode("utf-8", "replace")
    s = s[s.rfind("\x1b[H"):]
    return re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", s)


def main():
    exe = sys.argv[1]
    extra = sys.argv[2:]
    root = tempfile.mkdtemp(prefix="frpager_")
    try:
        lines = []
        for i in range(100):
            lines.append("row %03d 日本語 hit" % i if i % 2 == 0 else "row %03d nothing" % i)
        with open(os.path.join(root, "a.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        pid, fd = pty.fork()
        if pid == 0:
            os.execv(exe, [exe, root, "-f", "hit", "-s"] + extra)
        fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLS, 0, 0))
        fails = []

        out = read_until(fd, lambda b: b"q:" in b)
        f0 = frame_text(out)
        if "\x1b[?1049h" not in out.decode("utf-8", "replace"):
            fails.append("代替画面に切り替わっていない")
        if "=== " not in f0 or "1-11 / " not in f0:
            fails.append("初期画面が不正:\n" + f0)

        def key(seq, expect):
            os.write(fd, seq)
            o = read_until(fd, lambda b: expect.encode() in frame_text(b).encode())
            return frame_text(o)

        # 前後文付きで 1 項目 1 行、端末幅に収まっていること
        item_rows = [l for l in frame_lines(out) if l.startswith("  L")]
        if not item_rows or not all("日本語 hit" in l for l in item_rows):
            fails.append("項目行に前後文がない:\n" + "\n".join(item_rows))
        if "\x1b[30;43m" not in out.decode("utf-8", "replace"):
            fails.append("一致部分が強調されていない")
        too_wide = [l for l in frame_lines(out) if width(l) > COLS]
        if too_wide:
            fails.append("端末幅を超える行:\n" + "\n".join(too_wide))
        # 検索文字の桁が揃っていること (中央配置)
        cols_hit = {width(l[:l.index("hit")]) for l in item_rows if "hit" in l}
        if len(cols_hit) > 2:
            fails.append("検索文字の位置が揃っていない: %s" % cols_hit)

        f1 = key(b"\x1b[B", "2-12 / ")  # ↓
        if "2-12 / " not in f1:
            fails.append("↓ で 1 行進まない:\n" + f1)
        f2 = key(b"\x1b[6~", "13-23 / ")  # PgDn
        if "13-23 / " not in f2:
            fails.append("PgDn で 1 ページ進まない:\n" + f2)
        f3 = key(b"G", "(END)")  # 末尾
        if "(END)" not in f3 or "合計" not in f3:
            fails.append("G で末尾に移動しない:\n" + f3)
        f4 = key(b"g", "1-11 / ")  # 先頭
        if "1-11 / " not in f4:
            fails.append("g で先頭に戻らない:\n" + f4)
        f5 = key(b"\x1b[C", "(横 +8)")  # →
        if "(横 +8)" not in f5:
            fails.append("→ で横スクロールしない:\n" + f5)
        # 端末幅の変更に追従すること
        fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, 40, 0, 0))
        os.write(fd, b"\x1b[D")
        o6 = read_until(fd, lambda b: frame_text(b).count("\n") >= ROWS - 1 and "(横" not in frame_text(b))
        rows6 = [l for l in frame_lines(o6) if l.startswith("  L")]
        if not rows6 or any(width(l) > 40 for l in rows6) or not all("hit" in l for l in rows6):
            fails.append("幅変更後の表示が不正:\n" + "\n".join(frame_lines(o6)))
        os.write(fd, b"q")
        tail = read_until(fd, lambda b: "合計".encode() in b and b"\x1b[?1049l" in b)
        _, status = os.waitpid(pid, 0)
        if b"\x1b[?1049l" not in tail:
            fails.append("q で代替画面を抜けていない")
        if os.WEXITSTATUS(status) != 0:
            fails.append("終了コード %d" % os.WEXITSTATUS(status))
        if [x for x in os.listdir(tempfile.gettempdir()) if x.startswith("FastReplacer_view_")]:
            fails.append("一時ファイルが残っている")
        if fails:
            print("PAGER RESULT: NG\n" + "\n".join(fails))
            sys.exit(1)
        print("PAGER RESULT: OK")
    finally:
        shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    main()
