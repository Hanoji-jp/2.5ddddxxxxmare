#pragma once
#include <vector>
#include <string>

// プレイヤーに関わる定数
namespace PlayerConst
{
    // 移動速度
    constexpr float MoveSpeed        = 0.08f;

    // 加速度（入力あり）
    constexpr float Acceleration     = 0.012f;

    // 減速度（入力なし）
    constexpr float Deceleration     = 0.018f;

    // 向き補間速度（1.0fで即座、小さいほど緩やか）
    constexpr float RotationSpeed    = 0.15f;

    // ジャンプ初速
    constexpr float JumpPower        = 0.4f;

    // 最大HP
    constexpr int   MaxHp            = 5;

    // 無敵時間（フレーム数）
    constexpr int   InvincibleFrame  = 60;

    // 被ダメージ時に本体を赤くする時間（フレーム数）
    constexpr int   DamageFlashFrame = 18;

    // 赤フラッシュの強さ（0=変化なし, 1=真っ赤）
    constexpr float DamageFlashStrength = 0.85f;

    // 復活直後の無敵時間（フレーム数）。明転(0.6s)＋逃げる猶予を確保し、
    // 敵の近くに復活しても即再死亡＝復活ループに陥らないようにする。
    constexpr int   RespawnInvincibleFrame = 150;

    // ノックバック（マリオギャラクシー風：水平に吹き飛び＋少し上）
    constexpr float KnockbackSpeed   = 0.28f;  // 水平方向速度
    constexpr float KnockbackUpSpeed = 0.22f;  // 上方向速度（m_upDir 方向）

    // モデルの拡大率
    constexpr float ModelScale       = 0.3f;

    // モデルのピボット補正（足元がずれている場合に調整）
    constexpr float ModelOffsetY     = -0.56f;

    // 近接攻撃クールダウン（フレーム）
    constexpr int   MeleeCooldown    = 30;

    // 遠距離攻撃クールダウン（フレーム）
    constexpr int   RangedCooldown   = 45;

    // アニメーションブレンドフレーム数
    constexpr int   AnimBlendFrames  = 8;

    // 矢の発射オフセット
    constexpr float ArrowOffsetY     = 1.0f;

    // ダッシュ倍率（Shift 長押し）
    constexpr float DashSpeedMul     = 1.85f;  // 移動速度の倍率
    constexpr float DashAccelMul     = 1.6f;   // 加速度の倍率
    constexpr float DashAnimSpeedMul = 1.7f;   // アニメ再生速度の倍率

    // 傘スロー落下の重力スケール（1.0=通常、小さいほど遅い）
    constexpr float ParasolGravityScale = 0.25f;

    // 上半身ノード名リスト（Walk 中 Attack を上半身だけ上書きするのに使用）
    // Body より上の階層に属するノードをすべて列挙する
    inline const std::vector<std::string>& UpperBodyNodes()
    {
        static const std::vector<std::string> s_nodes = {
            "Body", "Body.001", "Head",
            "shoulder.R", "arm.R", "ParasolHand.R",
            "shoulder.L", "arm.L", "SwordHand.L",
        };
        return s_nodes;
    }

    // 左腕チェーン（Fall 中 Attack で shoulder.L 以降だけ上書きするのに使用）
    inline const std::vector<std::string>& LeftArmNodes()
    {
        static const std::vector<std::string> s_nodes = {
            "shoulder.L", "arm.L", "SwordHand.L",
        };
        return s_nodes;
    }

    // モデルパス
    constexpr const char* ModelPath  = "Asset/Data/Player.gltf";
    constexpr const char* SwordPath  = "Asset/Data/Sword.gltf";
    constexpr const char* BowPath    = "Asset/Data/Bow.gltf";
    constexpr const char* ArrowPath  = "Asset/Data/Bow_Arrow.gltf";
    constexpr const char* DustPath   = "Asset/Data/Box.gltf";  // 足元煙パーティクル用
}

