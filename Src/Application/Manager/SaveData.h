#pragma once

//==========================================================
// SaveData
//   設定(Settings)・合計(totals)・初回フラグ・ステージ記録を
//   1つのバイナリファイル Asset/Data/save.dat にまとめて保存/読込する。
//   （旧: settings.csv / totals.csv / stage_records.csv / launched.flag）
//==========================================================
namespace SaveData
{
    void Load();    // save.dat を読み、SettingsManager / StageManager へ反映（無ければ既定値）
    void Save();    // 両マネージャーの現在値を save.dat へ書き出す
    bool Exists();  // save.dat が存在するか
}
