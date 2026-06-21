#pragma once
// 日本語リザルト文。UTF-8 BOM付きで保存（/utf-8 未指定のため、ナロー文字列リテラルは
// コンパイル時に CP932(Shift-JIS) へ変換され、GDIフォント描画で正しく表示される）。
namespace ResultText
{
    constexpr const char* CoreBack  = "重力コアを取り戻した！";
    constexpr const char* NextGo    = "次のステージに進もう！";
    constexpr const char* TimeLabel = "クリアタイム";
}