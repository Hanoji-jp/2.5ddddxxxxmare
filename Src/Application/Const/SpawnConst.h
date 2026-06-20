#pragma once

// スポーン関連の定数
namespace SpawnConst
{
    // プレイヤー初期スポーン座標のデフォルト値
    constexpr float DefaultX = 0.0f;
    constexpr float DefaultY = 2.0f;
    constexpr float DefaultZ = 0.0f;

    // スポーン設定の保存先
    constexpr const char* SavePath = "Asset/Data/spawn_settings.csv";

    // 奈落落下の保険：スポーンよりこの距離以上下に落ちたら静かにスポーンへ戻す（死亡なし）
    constexpr float FallResetBelow = 150.0f;
}

