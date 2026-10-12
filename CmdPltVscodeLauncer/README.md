# CmdPltVscodeLauncer

PowerToys Run の「VS Code Workspaces」プラグイン（アクションキーワード `{`）を、PowerToys Command Palette (CmdPal) 拡張機能として移植したもの。

移植元: [microsoft/PowerToys `Community.PowerToys.Run.Plugin.VSCodeWorkspaces`](https://github.com/microsoft/PowerToys/tree/main/src/modules/launcher/Plugins/Community.PowerToys.Run.Plugin.VSCodeWorkspaces)

## 移植元と同一の仕様

- VS Code の検出: `PATH`（ユーザー → プロセス）の `VS Code` / `VSCodium` / `vscode` を含むフォルダから `code` / `code-insiders` / `code-exploration` / `codium` / `codium-insiders` を検出（起動時に 1 回、ポータブル版 `data` にも対応）
- 履歴の取得: `storage.json`（`openedPathsList`）、`User/globalStorage/state.vscdb`、共有ストレージ `state.vscdb` の `history.recentlyOpenedPathsList`（検索のたびに再読込、重複除去も同一）
- 環境: Local / SSH / WSL / Codespaces / Dev Container / Tunnel、UNC パス、ローカルパスの存在確認
- SSH リモートマシン: `settings.json` の `remote.SSH.configFile` から ssh config を解析
- タイトル / サブタイトル / アイコン（folder.png・monitor.png に VS Code アイコンを合成）
- 絞り込み（タイトル部分一致）・スコア計算・並び順（空検索時はタイトル順）
- PowerToys Run 本体の検索文字列正規化（前後空白除去・連続空白の圧縮）と、選択回数によるスコア加算（`Score + 選択回数 × 5`）
- 起動引数: `--folder-uri` / `--file-uri`、SSH は `--new-window --enable-proposed-api ms-vscode-remote.remote-ssh --remote ssh-remote+"host"`
- コンテキストメニュー（ワークスペースのみ）: Copy path (Ctrl+C) / Open in Explorer (Ctrl+Shift+F) / Open in console (Ctrl+Shift+C)
- 成功時はウィンドウを隠し、失敗時は開いたままエラーを表示

## CmdPal の制約による差異

| 項目 | PowerToys Run | 本拡張 |
| --- | --- | --- |
| `{` キーワード | plugin.json で既定設定 | 外部拡張は既定エイリアスを持てないため手動設定（下記） |
| ツールチップ | タイトル / サブタイトルと同内容 | CmdPal に API がないため無し |
| エラー表示 | PowerToys Run のメッセージ | CmdPal のステータストースト |
| 選択履歴 | PowerToys Run 全体で共有 | 本拡張専用（`LocalState\UserSelectedRecord.json`） |
| 表示言語 | PowerToys の翻訳版 | 英語のみ（`Properties/Resources.resx`） |

## 動作環境

- PowerToys v0.100.2 以降（同梱テンプレートと同じ Extension SDK `0.11.260520004` / .NET 10 を使用）
- ビルド: Visual Studio 2026（.NET 10 のため VS 2022 は不可）

## ビルド / インストール

1. Visual Studio 2026 で `CmdPltVscodeLauncer.sln` を開く（不足ワークロードは `.vsconfig` から追加される）
2. プラットフォーム `x64`（または `ARM64`）で **配置 (Deploy)** を実行
3. Command Palette で「Reload」を実行（または CmdPal を再起動）

## `{` で起動する設定

1. Command Palette の設定 → 拡張機能 → **Visual Studio Code Workspaces**
2. コマンド「Visual Studio Code Workspaces」のエイリアスに `{` を入力し、**直接（Direct）** を有効にする

以後、PowerToys Run と同じく `{` を入力するとワークスペース一覧に切り替わる。
