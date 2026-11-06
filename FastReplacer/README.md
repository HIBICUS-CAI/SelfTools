# FastReplacer

指定フォルダ内の全ファイルを UTF-8 テキストとして扱い、文字列を高速に検索・置換するコマンドラインツール (Windows x64 / C++20)。
数 GB 以上の巨大ファイルを想定し、速度を最優先にしている (メモリ使用量は二の次)。

## 使い方

```
FastReplacer <フォルダ> -f <検索文字列> -r <置換文字列> [オプション]   置換モード
FastReplacer <フォルダ> -f <検索文字列> -s [オプション]                検索のみ (レポート出力)
```

| オプション | 内容 |
|---|---|
| `-f, --find <text>` | 検索文字列 (必須) |
| `-r, --replace <text>` | 置換文字列 (空文字列 `""` で削除) |
| `-s, --search-only` | 検索のみ。結果をレポートファイルに書き出す |
| `-c, --match-case` | 大文字/小文字を区別する (既定: 区別しない) |
| `-w, --whole-word` | 単語単位で検索する (既定: 無効) |
| `-R, --recursive` | サブフォルダも対象にする (既定: 直下のみ) |
| `-e, --escape` | `\\ \n \r \t \0 \xHH \uXXXX \UXXXXXXXX` を解釈する |
| `-o, --report <path>` | レポートのパス。既定はカレントの `FastReplacer_report_<日時>.txt` (置換モードは指定時のみ出力) |
| `-t, --threads <n>` | スレッド数 (既定: 論理コア数) |

終了コード: `0`=成功 / `1`=一部ファイルでエラー / `2`=引数エラー

※ cmd/PowerShell では `"D:\data\"` のように `\"` で終わる引数はクォートが壊れるため、末尾の `\` を付けないこと。

### 例

```bat
FastReplacer D:\logs -f "ERROR" -s -c -w -o D:\report.txt
FastReplacer D:\data -f "旧名称" -r "新名称" -R
FastReplacer D:\data -f "\r\n" -r "\n" -e
```

### レポート (検索モード)

UTF-8 (BOM 付き)。一致ごとに 1 行:

```
=== D:\logs\a.txt  (3 件, 3.20 GiB)
  L120:C5  @48213  前の文脈【一致】後ろの文脈
```

`L` = 行番号 (LF 区切り), `C` = 行頭からの文字数 (コードポイント, 1 始まり), `@` = バイトオフセット (0 始まり)。
文脈は前後 100 バイトまで。制御文字や不正な UTF-8 は `\r` `\xHH` のようにエスケープ表示する。

## 照合仕様

- バイト列として照合する (BOM・改行コードは変換しない。Unicode 正規化もしない)。不正な UTF-8 バイトはどの文字とも一致しない。
- 一致は先頭から貪欲・重なりなし (並列処理でも逐次処理と完全に同じ結果になる)。
- **大小区別なし (既定)**: Unicode 単純ケースフォールディング (CaseFolding.txt の C+S) で同一視する。
  `Ａ`/`ａ`、`Σ`/`σ`/`ς` なども一致。ただし ASCII と非 ASCII 同士は一致させない (`K`(U+212A)≠`k`, `ſ`≠`s`)。
- **単語単位**: パターンの先頭 (末尾) が単語文字の場合、その直前 (直後) が単語文字でないこと。
  単語文字は .NET 正規表現の `\w` と同じ `[\p{L}\p{Mn}\p{Nd}\p{Pc}]`。日本語の漢字・かなも単語文字として扱う。

## 置換の動作

- 一致があったファイルのみ、同じフォルダに `<元ファイル名>.frtmp` を書き出してから `ReplaceFileW` で置き換える
  (元ファイルの属性・ACL・作成日時を維持)。途中で失敗しても元ファイルは壊れない。
- 中断 (Ctrl+C 等) で `.frtmp` が残った場合は削除してよい (`.frtmp` は処理対象から除外される)。
- 一致の無いファイルは一切書き換えない。

## 高速化の要点

- 入力はメモリマップ + `PrefetchVirtualMemory` による先読み。ファイルを 32 MiB 単位に分割して全スレッドで並列検索。
- 検索は AVX2 (非対応 CPU は SSE2) で、パターン中の出現頻度が低い 2 バイト位置を同時に比較して候補を絞り込む。
- 置換出力は 32 MiB ブロックを並列に直接生成し、`FILE_FLAG_NO_BUFFERING` で先頭から順に書き込む
  (キャッシュを汚さず、NTFS の VDL ゼロ埋めも発生しない)。
- レポートも並列に整形しながら順に書き出す。

## ビルド

- Visual Studio: `FastReplacer.vcxproj` (v143 / x64 / C++20 / `/utf-8`)
- CMake: `cmake -S . -B build && cmake --build build --config Release`
- MinGW (クロス): `x86_64-w64-mingw32-g++-posix -std=c++20 -O2 -municode -static -o FastReplacer.exe src/*.cpp`

`src/UnicodeData.inc` は `python tools/gen_unicode.py` で生成 (Python の unicodedata を使用)。

## テスト

```
python tests/test_fastreplacer.py <FastReplacer 実行ファイル> --cases 300
```

ランダムなテキスト (日本語・ギリシャ文字・特殊ケース文字・不正 UTF-8・CRLF を含む) に対し、
チャンクサイズ 1 バイト〜1 MiB、スレッド数 1〜8 で、Python の素朴な参照実装と検索結果 (行・列・オフセット) と置換結果を突き合わせる。
