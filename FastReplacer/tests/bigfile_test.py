#!/usr/bin/env python3
"""4GiB 超ファイルでの検索/置換の検証 (オフセット・行・列・置換結果をストリーミングで照合)。

usage: python3 bigfile_test.py <FastReplacer 実行ファイル> <作業フォルダ> [--size-mib N]
"""
import argparse
import os
import random
import re
import subprocess
import sys
import time

PAT = b"NEEDLE"
REP = "置換済みREPLACED".encode("utf-8")
BLOCK = 1 << 20
CHUNK = 64 << 20


def make_block():
    rng = random.Random(1)
    words = ["the", "quick", "brown", "fox", "日本語", "テキスト", "こんにちは", "東京", "ｎｅｅｄ", "σίσυφος", "\n", "\r\n", " ", "  "]
    b = bytearray()
    while len(b) < BLOCK + 64:
        b += (rng.choice(words) + " ").encode("utf-8")
    b = bytes(b[:BLOCK])
    assert PAT not in b and PAT.lower() not in b.lower()
    return b


def plants(size):
    g = 1 << 30
    offs = [0, 4095, (32 << 20) - 3, (64 << 20) - 1, 2 * g + 7, 4 * g - 3, 4 * g + 5, size - len(PAT)]
    return sorted(set(o for o in offs if 0 <= o <= size - len(PAT)))


def original_range(block, offs, a, b):
    """元ファイルの [a, b) を生成"""
    out = bytearray()
    p = a
    while p < b:
        q = min(b, (p // BLOCK + 1) * BLOCK)
        out += block[p % BLOCK:p % BLOCK + (q - p)]
        p = q
    for o in offs:
        lo, hi = max(a, o), min(b, o + len(PAT))
        if lo < hi:
            out[lo - a:hi - a] = PAT[lo - o:hi - o]
    return bytes(out)


def expected_line_col(block, offs, size):
    """各一致の (行, 列) をストリーミングで計算"""
    res = {}
    line, col = 1, 0
    pos = 0
    targets = list(offs)
    ti = 0
    while pos < size and ti < len(targets):
        end = min(size, pos + CHUNK)
        data = original_range(block, offs, pos, end)
        while ti < len(targets) and targets[ti] < end:
            t = targets[ti] - pos
            seg = data[:t]
            nl = seg.count(b"\n")
            if nl:
                ls = seg.rfind(b"\n") + 1
                c = sum(1 for x in seg[ls:] if (x & 0xC0) != 0x80)
                res[targets[ti]] = (line + nl, c + 1)
            else:
                c = sum(1 for x in seg if (x & 0xC0) != 0x80)
                res[targets[ti]] = (line, col + c + 1)
            ti += 1
        nl = data.count(b"\n")
        if nl:
            ls = data.rfind(b"\n") + 1
            col = sum(1 for x in data[ls:] if (x & 0xC0) != 0x80)
        else:
            col += sum(1 for x in data if (x & 0xC0) != 0x80)
        line += nl
        pos = end
    return res


def run(cmd):
    t0 = time.time()
    r = subprocess.run(cmd, capture_output=True)
    out = r.stdout.decode("utf-8", "replace")
    print(out.strip())
    print("  (%.2f s, rc=%d)" % (time.time() - t0, r.returncode), flush=True)
    if r.returncode != 0:
        print(r.stderr.decode("utf-8", "replace"))
        sys.exit("FAIL: rc=%d" % r.returncode)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("workdir")
    ap.add_argument("--size-mib", type=int, default=4 * 1024 + 96)
    a = ap.parse_args()
    size = a.size_mib << 20
    d = os.path.join(a.workdir, "big")
    os.makedirs(d, exist_ok=True)
    path = os.path.join(d, "big.txt")
    block = make_block()
    offs = plants(size)

    t0 = time.time()
    with open(path, "wb") as f:
        pos = 0
        while pos < size:
            end = min(size, pos + CHUNK)
            f.write(original_range(block, offs, pos, end))
            pos = end
    print("generated %d bytes in %.1f s, plants=%s" % (size, time.time() - t0, offs), flush=True)

    exp_lc = expected_line_col(block, offs, size)
    rx = re.compile(r"^  L(\d+):C(\d+)  @(\d+)")
    # 大小区別あり / なし × RAM 直接読み込み / メモリマップ × 文脈なし / あり
    for extra in (["-c"], [], ["-c", "--mmap"], ["-x"]):
        report = os.path.join(a.workdir, "big_report.txt")
        run([a.exe, d, "-f", "NEEDLE" if "-c" in extra else "needle", "-s", "-o", report] + extra)
        got = []
        with open(report, encoding="utf-8-sig") as f:
            for ln in f:
                m = rx.match(ln)
                if m:
                    got.append((int(m.group(3)), int(m.group(1)), int(m.group(2))))
        exp = [(o,) + exp_lc[o] for o in offs]
        if got != exp:
            sys.exit("FAIL search %s\n exp=%s\n got=%s" % (extra, exp, got))
        print("search %s OK: %s" % (extra, got), flush=True)
        os.remove(report)

    run([a.exe, d, "-f", "NEEDLE", "-r", REP.decode("utf-8"), "-c"])
    new_size = size + len(offs) * (len(REP) - len(PAT))
    if os.path.getsize(path) != new_size:
        sys.exit("FAIL size %d != %d" % (os.path.getsize(path), new_size))
    # 期待出力をストリーミング生成して比較
    with open(path, "rb") as f:
        prev = 0
        cmp_pos = 0
        for o in offs + [size]:
            p = prev
            while p < o:
                q = min(o, p + CHUNK)
                want = original_range(block, offs, p, q)
                got = f.read(len(want))
                if got != want:
                    sys.exit("FAIL replace content near output offset %d" % cmp_pos)
                cmp_pos += len(want)
                p = q
            if o < size:
                got = f.read(len(REP))
                if got != REP:
                    sys.exit("FAIL replace at output offset %d" % cmp_pos)
                cmp_pos += len(REP)
                prev = o + len(PAT)
        if f.read(1):
            sys.exit("FAIL trailing data")
    if [x for x in os.listdir(d) if x.endswith(".frtmp")]:
        sys.exit("FAIL tmp left")
    os.remove(path)
    print("BIGFILE RESULT: OK")


if __name__ == "__main__":
    main()
