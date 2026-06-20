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
