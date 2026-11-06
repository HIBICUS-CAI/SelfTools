#!/usr/bin/env python3
"""FastReplacer の結果を Python の素朴な参照実装と突き合わせるランダムテスト。

usage: python3 test_fastreplacer.py <FastReplacer 実行ファイル> [--cases N] [--wine]
"""
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata


def fold(cp):
    ch = chr(cp)
    f = ch.casefold()
    if len(f) == 1:
        return ord(f)
    lo = ch.lower()
    if len(lo) == 1:
        return ord(lo)
    return cp


def is_word(cp):
    if cp is None or 0xD800 <= cp <= 0xDFFF:
        return False
    cat = unicodedata.category(chr(cp))
    return cat[0] == "L" or cat in ("Mn", "Nd", "Pc")


def eq_cp(a, b, match_case):
    if 0xD800 <= a <= 0xDFFF:
        return False
    if match_case:
        return a == b
    return (a < 0x80) == (b < 0x80) and fold(a) == fold(b)


def units(data: bytes):
    """(cp, byte_offset) の列。不正バイトはサロゲートエスケープ。"""
    s = data.decode("utf-8", errors="surrogateescape")
    out, off = [], 0
    for ch in s:
        cp = ord(ch)
        if 0xDC80 <= cp <= 0xDCFF:
            out.append((cp, off))
            off += 1
        else:
            out.append((cp, off))
            off += len(ch.encode("utf-8"))
    assert off == len(data)
    return out


def ref_find(data: bytes, pat: str, match_case: bool, whole_word: bool):
    us = units(data)
    p = [ord(c) for c in pat]
    n = len(us)
    w_start, w_end = is_word(p[0]), is_word(p[-1])
    res = []
    i = 0
    while i + len(p) <= n:
        ok = all(eq_cp(us[i + k][0], p[k], match_case) for k in range(len(p)))
        if ok and whole_word:
            if w_start and i > 0 and is_word(us[i - 1][0]):
                ok = False
            if w_end and i + len(p) < n and is_word(us[i + len(p)][0]):
                ok = False
        if ok:
            b = us[i][1]
            e = us[i + len(p)][1] if i + len(p) < n else len(data)
            res.append((b, e - b))
            i += len(p)
        else:
            i += 1
    return res


def line_col(data: bytes, pos: int):
    line = data.count(b"\n", 0, pos) + 1
    ls = data.rfind(b"\n", 0, pos) + 1
    col = sum(1 for x in data[ls:pos] if (x & 0xC0) != 0x80) + 1
    return line, col


def ref_replace(data: bytes, matches, rep: bytes):
    out, p = [], 0
    for b, l in matches:
        out.append(data[p:b])
        out.append(rep)
        p = b + l
    out.append(data[p:])
    return b"".join(out)


# ---------------------------------------------------------------------------
PIECES = [
    "a", "A", "aa", "ab", "abab", "the", "The", "THE", "word", "Word_1", "foo", "bar", "Straße", "STRASSE",
    "k", "K", "\u212a", "s", "S", "\u017f", "\u0131", "\u0130", "i", "I",
    "σ", "ς", "Σ", "θ", "ϑ", "Θ", "ϴ", "ι", "\u0345", "Ι", "\u1fbe", "Ω", "ω", "\u2126", "å", "Å", "\u212b",
    "こんにちは", "日本語", "テスト", "東京都", "ａｂｃ", "ＡＢＣ", "😀", "𝐀",
    " ", " ", "  ", "\n", "\r\n", "\t", ".", ",", "-", "_", "1", "23",
]
BAD = [b"\xff", b"\xe3\x81", b"\x80", b"\xf0\x9f\x98", b"\xc0\xaf", b"\xed\xa0\x80"]


def rand_text(rng, nbytes):
    parts, size = [], 0
    while size < nbytes:
        if rng.random() < 0.02:
            b = rng.choice(BAD)
        else:
            b = rng.choice(PIECES).encode("utf-8")
        parts.append(b)
        size += len(b)
    return b"".join(parts)


def rand_pattern(rng, data: bytes):
    if rng.random() < 0.3:
        pat = "".join(rng.choice(PIECES) for _ in range(rng.randint(1, 3)))
    else:
        s = data.decode("utf-8", errors="replace")
        if not s:
            return "a"
        a = rng.randrange(len(s))
        pat = s[a:a + rng.randint(1, 8)]
        pat = "".join(c.upper() if rng.random() < 0.3 else c for c in pat if c != "\ufffd")
    pat = pat.replace("\ufffd", "")
    return pat or "a"


def esc(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


REPORT_RE = re.compile(r"^  L(\d+):C(\d+)  @(\d+)  ")


def run_tool(cmd_prefix, args):
    r = subprocess.run(cmd_prefix + args, capture_output=True)
    return r


def to_tool_path(p, wine):
    if not wine:
        return p
    return "Z:" + p.replace("/", "\\")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("exe")
    ap.add_argument("--cases", type=int, default=300)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--wine", action="store_true")
    a = ap.parse_args()
    rng = random.Random(a.seed)
    prefix = (["/usr/lib/wine/wine64", a.exe] if a.wine else [a.exe])
    env_fail = 0
    total_matches = 0
    root = tempfile.mkdtemp(prefix="frtest_")
    try:
        for case in range(a.cases):
            d = os.path.join(root, "c%d" % case)
            os.makedirs(os.path.join(d, "sub"))
            files = {}
            for k in range(rng.randint(1, 3)):
                name = os.path.join(d, ("sub" if k == 2 else ""), "f%d.txt" % k)
                files[name] = rand_text(rng, rng.choice([0, 1, 5, 50, 300, 3000, 40000]))
            for name, data in files.items():
                with open(name, "wb") as f:
                    f.write(data)
            sample = max(files.values(), key=len)
            pat = rand_pattern(rng, sample)
            rep = rng.choice(["", "X", "置換後", "longer-replacement", "\n", "a"])
            mc = rng.random() < 0.4
            ww = rng.random() < 0.4
            rec = rng.random() < 0.5
            chunk = rng.choice([1, 2, 3, 7, 16, 33, 64, 1000, 1 << 20])
            threads = rng.choice([1, 2, 3, 8])
            opts = ["-e", "-t", str(threads), "--chunk-size", str(chunk)]
            if mc:
                opts.append("-c")
            if ww:
                opts.append("-w")
            if rec:
                opts.append("-R")
            targets = [n for n in files if rec or os.path.dirname(n) == d]
            expect = {n: ref_find(files[n], pat, mc, ww) for n in targets}
            total_matches += sum(len(v) for v in expect.values())

            # --- 検索 ---
            report = os.path.join(root, "rep%d.txt" % case)
            r = run_tool(prefix, [to_tool_path(d, a.wine), "-f", esc(pat), "-s", "-o", to_tool_path(report, a.wine)] + opts)
            ctx = "case=%d pat=%r mc=%s ww=%s chunk=%d threads=%d" % (case, pat, mc, ww, chunk, threads)
            if r.returncode != 0:
                print("FAIL(search rc=%d) %s\n%s" % (r.returncode, ctx, r.stderr.decode("utf-8", "replace")))
                env_fail += 1
                continue
            got, cur = {}, None
            with open(report, "rb") as f:
                for raw in f.read().decode("utf-8").splitlines():
                    if raw.startswith("=== "):
                        path = raw[4:raw.rindex("  (")]
                        cur = path.replace("Z:", "").replace("\\", "/") if a.wine else path
                        got[cur] = []
                    m = REPORT_RE.match(raw)
                    if m:
                        got[cur].append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
            for n in targets:
                exp = [(line_col(files[n], b) + (b,)) for b, _ in expect[n]]
                if got.get(n, []) != exp:
                    print("FAIL(search) %s file=%s\n exp=%s\n got=%s" % (ctx, n, exp[:10], got.get(n, [])[:10]))
                    env_fail += 1
                    break

            # --- 置換 ---
            r = run_tool(prefix, [to_tool_path(d, a.wine), "-f", esc(pat), "-r", esc(rep)] + opts)
            if r.returncode != 0:
                print("FAIL(replace rc=%d) %s\n%s" % (r.returncode, ctx, r.stdout.decode("utf-8", "replace")))
                env_fail += 1
                continue
            for n, data in files.items():
                with open(n, "rb") as f:
                    now = f.read()
                want = ref_replace(data, expect[n], rep.encode("utf-8")) if n in expect else data
                if now != want:
                    print("FAIL(replace) %s file=%s" % (ctx, n))
                    env_fail += 1
                    break
            leftovers = [x for x in os.listdir(d) if x.endswith(".frtmp")]
            if leftovers:
                print("FAIL(tmp left) %s" % ctx)
                env_fail += 1
            if (case + 1) % 50 == 0:
                print("... %d cases" % (case + 1), flush=True)
    finally:
        shutil.rmtree(root, ignore_errors=True)
    print("RESULT: %s (%d failures / %d cases, %d expected matches)" % ("OK" if env_fail == 0 else "NG", env_fail, a.cases, total_matches))
    sys.exit(1 if env_fail else 0)


if __name__ == "__main__":
    main()
