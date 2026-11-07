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
