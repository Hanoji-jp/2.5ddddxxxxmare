#pragma once
#include "../../Const/GameConst.h"

// 2.5D横スクロール用カメラ
// プレイヤーをX軸方向に追従し、Z座標は固定
class SideScrollCamera : public KdCamera
{
public:
    SideScrollCamera()  {}
    ~SideScrollCamera() {}

    // ターゲットのワールド座標に追従して更新する
    void Update(const Math::Vector3& _targetPos);

private:
    // カメラの現在位置（スムーズ追従用）
    Math::Vector3 m_pos = { 0.0f, GameConst::CameraOffsetY, GameConst::CameraOffsetZ };
};
