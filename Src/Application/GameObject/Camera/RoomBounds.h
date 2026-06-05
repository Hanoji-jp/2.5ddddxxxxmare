#pragma once
#include "../../Const/CameraConst.h"

//==========================================================
// RoomBounds
// カメラが動ける範囲（ルーム単位で設定）
//
// minX / maxX : カメラX座標の最小・最大
// minY / maxY : カメラY座標の最小・最大
// triggerX    : このX座標をプレイヤーが超えたら次のルームへ遷移
//               (-FLT_MAX なら遷移なし)
// blendX      : 部屋の切り替えを始めるX座標の幅（マージン）
// mode        : このルームのカメラ挙動モード
//==========================================================
struct RoomBounds
{
    float minX      = -FLT_MAX;
    float maxX      =  FLT_MAX;
    float minY      = -FLT_MAX;
    float maxY      =  FLT_MAX;
    float triggerX  = FLT_MAX;   // 右方向への遷移トリガーX
    float blendX    = 1.0f;      // 切り替えをふわっと見せる幅
    float cameraZ   = 0.0f;      // カメラの Z 基準位置（ワープホール通過時などに切り替える）
    CameraConst::CameraMode mode = CameraConst::CameraMode::SideScroll;

    // ── フォーカスオフセット ──────────────────────────────────
    // プレイヤー基準・重力ローカル空間でのカメラ注視点オフセット
    //   X = 右方向（重力直交）
    //   Y = 上方向（upDir に沿う）
    //   Z = 手前方向（カメラ前進方向と逆）
    // すべて 0.0 のとき「プレイヤー中心」に戻る
    Math::Vector3 focusOffset   = { 0.0f, 0.0f, 0.0f };
    // このルーム固有の補間速度（0.0f のときはデフォルト速度を使用）
    float         focusLerpSpeed = 0.0f;

    // ── Basic Offset オーバーライド ───────────────────────────
    // true のとき CameraSettings のグローバル値を上書きする
    bool          useOffsetOverride = false;
    float         overrideOffsetX   = 0.0f;   // 右方向オフセット
    float         overrideOffsetY   = 3.0f;   // 上方向オフセット
    float         overrideOffsetZ   = -14.0f; // 奥行きオフセット
};
