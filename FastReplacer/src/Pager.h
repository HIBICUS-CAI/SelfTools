#pragma once
#include <cstdint>
#include <string>

// git log のような簡易ページャ (UTF-8 テキストを 1 画面ずつ表示)
//   ↑↓ / j k        : 1 行
//   PgUp PgDn / b Space : 1 ページ
//   Home End / g G   : 先頭 / 末尾
//   ← → / h l        : 横スクロール
//   マウスホイール   : 3 行
//   q / Esc / Ctrl+C : 終了
namespace pager
{
// 標準入力・標準出力がともに対話的な端末 (コンソール) か
bool IsInteractive();

// text を表示する。title はステータス行に表示する
void Run(const uint8_t* text, uint64_t size, const std::string& title);
}  // namespace pager
