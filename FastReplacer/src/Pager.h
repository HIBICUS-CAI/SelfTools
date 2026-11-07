#pragma once
#include <cstdint>
#include <string>

// ---- 表示用ユーティリティ ----
namespace view
{
int CharWidth(char32_t c);  // 端末上の表示幅 (全角 2 / 結合文字 0 / それ以外 1)
int TextWidth(const std::string& s);  // 表示用に無害化済みの UTF-8 文字列の表示幅
int TerminalWidth();  // 端末の桁数 (取得できなければ 0)

// 1 項目分の前後文。検索文字を中央に置き、左右を budget 桁に収める
//   pad  : 左側の空白 (検索文字の位置を揃えるため)
//   left / match / right : 制御文字・不正 UTF-8 をエスケープ済みの表示用文字列 (行をまたがない)
struct Context
{
    int pad = 0;
    std::string left, match, right;
};
void BuildContext(const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget, Context& out);
// 前後文をテキスト出力用に out へ直接追加する (一致部分を【】で囲む。BuildContext と同じ範囲)
void AppendContextText(std::string& out, const uint8_t* t, uint64_t n, uint64_t pos, uint32_t len, int budget);

// s の表示桁 [hscroll, hscroll + cols) の部分だけを out に追加する。col は s の手前までの累計桁 (更新される)
void AppendClipped(std::string& out, const std::string& s, uint64_t& col, uint64_t hscroll, uint64_t cols);
}  // namespace view

// ---- git log のような簡易ページャ ----
//   ↑↓ / j k / ホイール      : 1 行 (ホイールは 3 行)
//   PgUp PgDn / b Space       : 1 ページ
//   Home End / g G            : 先頭 / 末尾
//   ← → / h l                 : 横スクロール
//   q / Esc / Ctrl+C          : 終了
namespace pager
{
class Source
{
public:
    virtual ~Source() = default;
    virtual uint64_t Count() = 0;
    // row 行目を表示桁 [hscroll, hscroll + cols) に収めて out へ追加する (色付けの制御シーケンスは可)
    virtual void Render(uint64_t row, int cols, uint64_t hscroll, std::string& out) = 0;
};

// 標準入力・標準出力がともに対話的な端末 (コンソール) か
bool IsInteractive();

void Run(Source& src, const std::string& title);
}  // namespace pager
