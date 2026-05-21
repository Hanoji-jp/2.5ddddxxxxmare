#pragma once
#include "../../Const/MapConst.h"

// マップエディター用フリーカメラ
// 右ドラッグ : 回転（Pitch / Yaw）
// 中ドラッグ : パン（上下左右平行移動）
// ホイール   : ズーム（前後移動）
// WASD       : 前後左右移動
class EditorCamera : public KdCamera
{
public:
    EditorCamera();
    ~EditorCamera() {}

    void Update();

private:
    void RotateByMouse();
    void PanByMouse();
    void ZoomByWheel();
    void MoveByKey();

    // カメラのワールド位置
    Math::Vector3 m_pos   = { 0.0f, 5.0f, -15.0f };

    // 回転角（ラジアン）
    float m_yaw   = 0.0f;
    float m_pitch = 0.0f;

    // 前フレームのマウス座標
    POINT m_prevMousePos = { 0, 0 };
};
