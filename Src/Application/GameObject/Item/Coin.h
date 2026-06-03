#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../Const/ItemConst.h"

//==========================================================
// Coin
// 取得可能なコインアイテム
// KdCollider(TypeEvent) で取得判定を持つ
//==========================================================
class Coin : public KdGameObject
{
public:
	Coin()          { Init(); }
	~Coin() override = default;

	Coin(const Coin&)            = delete;
	Coin& operator=(const Coin&) = delete;

	void Init()   override;
	void Update() override;
	void DrawLit() override;

	void SetSpawnPos(const Math::Vector3& _pos) { m_spawnPos = _pos; SetPos(_pos); }
	const Math::Vector3& GetSpawnPos() const    { return m_spawnPos; }

	bool IsExpired() const override { return m_isExpired; }

private:
	KdModelWork   m_modelWork;
	Math::Vector3 m_spawnPos    = { 0.0f, 0.0f, 0.0f };
	float         m_bobTimer    = 0.0f;
	float         m_rotAngle    = 0.0f;
};
