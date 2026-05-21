#pragma once
#include "../../Const/CameraConst.h"
#include "RoomBounds.h"

// 2.5D横スクロール用カメラ
// ルーム単位でカメラ中心を固定し、境界でのみ切り替える
class SideScrollCamera : public KdCamera
{
public:
    SideScrollCamera()  {}
    ~SideScrollCamera() {}

    // ターゲットのワールド座標に追従して更新する
    void Update(const Math::Vector3& _targetPos);

    // ルーム一覧を設定する
    void SetRooms(const std::vector<RoomBounds>& _rooms);

    // 現在のルームインデックス取得
    int GetCurrentRoomIndex() const { return m_currentRoom; }

private:
    // 現在ルームのカメラ中心を取得
    Math::Vector3 GetRoomCenter() const;

    // プレイヤーを少し追うカメラ中心を取得
    Math::Vector3 GetFollowRoomCenter(const Math::Vector3& _targetPos) const;

    // ルーム遷移をゆるくブレンドした目標位置を取得
    Math::Vector3 GetBlendedRoomCenter(const Math::Vector3& _targetPos) const;

    // カメラの注視点を取得
    Math::Vector3 GetLookAtPoint(const Math::Vector3& _targetPos) const;

    // カメラの現在位置
    Math::Vector3 m_pos        = { 0.0f, CameraConst::OffsetY, CameraConst::OffsetZ };

    // カメラの注視点
    Math::Vector3 m_lookAtPos  = { 0.0f, 0.0f, 0.0f };

    // 遷移先ルーム
    int           m_nextRoom = 0;

    // 遷移ブレンド量
    float         m_transitionRate = 0.0f;

    // ルーム一覧
    std::vector<RoomBounds> m_rooms;

    // 現在のルームインデックス
    int           m_currentRoom = 0;

    // 遷移中か
    bool          m_isTransitioning = false;

};
