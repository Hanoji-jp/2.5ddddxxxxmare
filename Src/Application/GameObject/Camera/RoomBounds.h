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
    CameraConst::CameraMode mode = CameraConst::CameraMode::SideScroll;
};
