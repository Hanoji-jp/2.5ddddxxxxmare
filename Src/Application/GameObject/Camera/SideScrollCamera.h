#pragma once
#include "../../Const/CameraConst.h"
#include "../../Const/JuiceConst.h"
#include "RoomBounds.h"

// 2.5D横スクロール用カメラ
// ルーム単位でカメラ中心を基準に、LookAt + ロール + 浮遊でリトルナイトメア風に動く
class SideScrollCamera : public KdCamera
{
public:
    SideScrollCamera()  {}
    ~SideScrollCamera() {}

    void Update(const Math::Vector3& _targetPos, const Math::Vector3& _upDir = { 0.0f, 1.0f, 0.0f });
    void SetRooms(const std::vector<RoomBounds>& _rooms);
    int  GetCurrentRoomIndex() const { return m_currentRoom; }

    // カメラシェイクを開始する（強さ 0〜1 程度）
    void TriggerShake(float strength) { m_shakeStrength = std::max(m_shakeStrength, strength); }

    // 惑星到達ズームをトリガー
    void TriggerPlanetZoom() { m_planetZoomTimer = 1.0f; }

    // WarpHole通過後のZ基準オフセットを上書きする（0.0fで解除）
    void SetOffsetZOverride(float z) { m_offsetZOverride = z; m_useOffsetZOverride = true; }
    void ClearOffsetZOverride()      { m_useOffsetZOverride = false; }

private:
    Math::Vector3 CalcBlendedRoomCenter(const Math::Vector3& _targetPos) const;
    Math::Vector3 CalcTargetCamPos(const Math::Vector3& _targetPos, float _floatY, float _floatX, float _offsetZ, float _upSign, float _offsetX, float _offsetY) const;
    Math::Vector3 CalcTargetLookAt(const Math::Vector3& _targetPos) const;

    // 補間後のカメラ位置
    Math::Vector3 m_pos        = { 0.0f, CameraConst::OffsetY, CameraConst::OffsetZ };
    // 補間後の注視点
    Math::Vector3 m_lookAtPos  = { 0.0f, 0.0f, 0.0f };
    // 現在のロール角（度）
    float         m_rollDeg    = 0.0f;
    // 現在のupDir（重力方向に合わせてカメラ位置を調整）
    Math::Vector3 m_upDir      = { 0.0f, 1.0f, 0.0f };
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

    // カメラシェイク
    float         m_shakeStrength = 0.0f;
    float         m_shakeTimer    = 0.0f;

    // 惑星到達ズームアウト→戻り（0=無効、1=開始）
    float         m_planetZoomTimer = 0.0f;

    // WarpHole通過後のZ基準オーバーライド
    float         m_offsetZOverride     = 0.0f;
    bool          m_useOffsetZOverride  = false;

    // ── フォーカスオフセット補間（重力ローカル空間）────────────
    // 現在の補間済みオフセット（ワールド空間）
    Math::Vector3 m_focusOffsetWorld    = { 0.0f, 0.0f, 0.0f };
};

