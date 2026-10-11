# Hider

画面の一部を単色ウィンドウで隠すツール。終了時の位置・サイズを保存し、次回起動時に復元する。
Avalonia + .NET 10 製で、Windows 専用の旧 WPF 版は `../Hider_Wpf` にある。

対応ターゲット: `win-x64` / `linux-x64` / `linux-arm64`（Ubuntu 26.04+）

## 必要なもの

- ビルド: .NET 10 SDK
- スクリプト: Python 3.8+（標準ライブラリのみ、pip 不要）
- 実行: 不要（self-contained でランタイム同梱）

## ビルド

```sh
python scripts/build.py              # 実行中のマシン向け
python scripts/build.py linux-arm64  # ターゲット指定（複数可）
python scripts/build.py all          # 3 ターゲットすべて
```

どのホストからでも全ターゲットをビルドできる。出力先は `bin/publish/<target>/`。

## インストール

先に同じマシン向けのビルドを済ませておくこと。既存ファイルは上書きされる。

```sh
python scripts/install.py            # 既定のフォルダへ
python scripts/install.py -d <フォルダ>
```

| OS | 既定のインストール先 | 追加で行うこと |
| --- | --- | --- |
| Windows | `%LOCALAPPDATA%\Programs\Hider` | デスクトップに `Hider.lnk` を作成 |
| Ubuntu | `$XDG_DATA_HOME/Hider`（通常 `~/.local/share/Hider`） | `~/.local/share/applications/Hider.desktop` を作成（Super キー検索・ドックへのピン留め用） |

Windows で Hider 起動中はファイルを上書きできないため、終了してから実行すること。

## 設定ファイル

`<LocalApplicationData>/hider_setting/setting.txt`（Windows: `%LOCALAPPDATA%`、Ubuntu: `~/.local/share`）。
Left/Top は物理ピクセル、Width/Height は DIP。

## 備考

- Ubuntu（Wayland セッション）では XWayland 経由で動作する。
