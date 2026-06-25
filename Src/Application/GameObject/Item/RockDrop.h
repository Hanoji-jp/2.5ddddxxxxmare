#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../Const/RockConst.h"

//==========================================================
// RockDrop
// 敵撃破時にドロップする小さな岩石。重力(upDir基準)で散らばって着地し、
// プレイヤーが触れると取得できる通貨アイテム。
//  - 見た目は GravityCore の岩石を小型化したローポリ球（加算描画）
//  - メッシュは全インスタンス共有（静的に1度だけベイク）
//==========================================================
class RockDrop : public KdGameObject
{
public:
	RockDrop()           = default;
	~RockDrop() override = default;

	RockDrop(const RockDrop&)            = delete;
	RockDrop& operator=(const RockDrop&) = delete;

	// spawnPos から up 方向基準で打ち上げて散らばらせる
	void Spawn(const Math::Vector3& spawnPos, const Math::Vector3& upDir);

	void Update()     override;
	void DrawEffect() override;   // 加算でローポリ岩石を描画

	bool IsVisible() const override { return true; }

	void Expire()                   { m_expired = true; }
	bool IsExpired() const override  { return m_expired; }

	// 取得可能か（散らばり猶予を過ぎたら true）
	bool IsPickable() const { return m_age >= RockConst::PickupDelay; }

	// カーソル磁石：lerp 率で target へ吸い寄せる（その場に静止させる）
	void PullTo(const Math::Vector3& target, float lerp)
	{
		m_pos += (target - m_pos) * lerp;
		m_velocity = Math::Vector3::Zero;
		m_landed = true;   // 吸い寄せ中はその場で静止（重力で落ちない）
		m_mWorld = Math::Matrix::CreateTranslation(m_pos);
		SetPos(m_pos);
	}
	// カーソル磁石：start 地点（カメラ付近）から dir 方向へ speed の初速で飛ばす（重力で落ちて再着地する）
	void FlingFrom(const Math::Vector3& start, const Math::Vector3& dir, float speed)
	{
		m_pos       = start;
		m_velocity  = dir * speed;
		m_landed    = false;
		m_hasGround = true;   // 飛んだ先で再着地できるように
		m_spawnUp   = m_pos.Dot(m_upDir);   // 落下消滅判定の基準を更新
		m_mWorld    = Math::Matrix::CreateTranslation(m_pos);
		SetPos(m_pos);
	}

private:
	// 全インスタンス共有のローポリ岩石メッシュ（半径1ユニット）
	static const std::vector<KdPolygon::Vertex>& TriVerts();
	static const std::vector<KdPolygon::Vertex>& WireVerts();
	static void BakeShared();

	Math::Vector3 m_pos       = {};
	Math::Vector3 m_velocity  = {};
	Math::Vector3 m_upDir     = { 0.0f, 1.0f, 0.0f };
	float         m_groundLvl = 0.0f;   // 着地面の up 座標（spawn の up 成分）
	float         m_spawnUp   = 0.0f;   // spawn 時の up 座標（落下消滅判定の基準）
	float         m_rotAngle  = 0.0f;
	float         m_age       = 0.0f;
	bool          m_landed    = false;
	bool          m_hasGround = true;   // 真下に地面があるか（無ければ落下し続ける）
	bool          m_expired   = false;
};
