#pragma once
#include "../../Const/CameraConst.h"
#include "RoomBounds.h"

// 2.5D横スクロール用カメラ
// ルーム単位でカメラ中心を基準に、LookAt + ロール + 浮遊でリトルナイトメア風に動く
class SideScrollCamera : public KdCamera
{
public:
    SideScrollCamera()  {}
    ~SideScrollCamera() {}

    void Update(const Math::Vector3& _targetPos);
    void SetRooms(const std::vector<RoomBounds>& _rooms);
    int  GetCurrentRoomIndex() const { return m_currentRoom; }

private:
    Math::Vector3 CalcBlendedRoomCenter(const Math::Vector3& _targetPos) const;
    Math::Vector3 CalcTargetCamPos(const Math::Vector3& _targetPos, float _floatY, float _floatX, float _offsetZ) const;
    Math::Vector3 CalcTargetLookAt(const Math::Vector3& _targetPos) const;

    // 補間後のカメラ位置
    Math::Vector3 m_pos        = { 0.0f, CameraConst::OffsetY, CameraConst::OffsetZ };
    // 補間後の注視点
    Math::Vector3 m_lookAtPos  = { 0.0f, 0.0f, 0.0f };
    // 現在のロール角（度）
    float         m_rollDeg    = 0.0f;
    // 浮遊タイマー
    float         m_floatTimer = 0.0f;
    // 現在のズーム率（0=通常 1=最大ズームイン）
    float         m_wallZoomRate = 0.0f;
    // 初期化済みフラグ
    bool          m_initialized = false;
    // クランプ範囲（Lerp で滑らかに切り替え）
    float         m_clampMinX = -FLT_MAX;
    float         m_clampMaxX =  FLT_MAX;
    // 遷移後クランプ猶予タイマー（0になったらクランプ有効）
    float         m_postTransitionTimer = 0.0f;
    // 遷移開始時のカメラX位置（スライドの起点）
    float         m_transitionStartX   = 0.0f;

    int           m_currentRoom     = 0;
    int           m_nextRoom        = 0;
    float         m_transitionRate  = 0.0f;
    bool          m_isTransitioning = false;
    std::vector<RoomBounds> m_rooms;
};
