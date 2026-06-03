#pragma once
#include "../../Const/ItemConst.h"

//==========================================================
// HitBox
// 「当てる側」の球形ヒットボックスラッパー
//
// 使い方:
//   1. Init(radius) で半径を設定
//   2. Update(worldPos) で毎フレーム中心座標を更新
//   3. GetSphereInfo() で KdCollider::SphereInfo を取得し
//      相手の KdGameObject::Intersects() に渡す
//==========================================================
class HitBox
{
public:
	HitBox()  = default;
	~HitBox() = default;

	HitBox(const HitBox&)            = delete;
	HitBox& operator=(const HitBox&) = delete;

	void Init(float _radius, UINT _type = KdCollider::TypeEvent)
	{
		m_radius = _radius;
		m_type   = _type;
	}

	// 毎フレーム呼ぶ：ヒットボックス中心座標を更新
	void Update(const Math::Vector3& _worldPos)
	{
		m_center = _worldPos;
	}

	// 当たり判定情報を返す
	KdCollider::SphereInfo GetSphereInfo() const
	{
		return KdCollider::SphereInfo(m_type, m_center, m_radius);
	}

	float             GetRadius() const { return m_radius; }
	Math::Vector3     GetCenter() const { return m_center; }

private:
	Math::Vector3 m_center = { 0.0f, 0.0f, 0.0f };
	float         m_radius = 1.0f;
	UINT          m_type   = KdCollider::TypeEvent;
};
