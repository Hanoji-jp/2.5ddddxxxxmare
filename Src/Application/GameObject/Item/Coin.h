#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../Const/ItemConst.h"
#include "../Effect/ItemEffect.h"

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
	void DrawOutline() override;   // 原神式アウトライン
	void DrawEffect() override;   // 星きらめき

	void SetSpawnPos(const Math::Vector3& _pos) { m_spawnPos = _pos; SetPos(_pos); }
	const Math::Vector3& GetSpawnPos() const    { return m_spawnPos; }

	// 削除（エディタで配置を消す）：保存対象から外れ、リストからも除去される
	void Expire() { m_isExpired = true; }
	bool IsExpired() const override { return m_isExpired; }

	// 取得（プレイ中に拾う）：この場では隠すが配置データは残す（保存される）
	void Collect()           { m_collected = true; }
	bool IsCollected() const { return m_collected; }
	void ResetCollected()    { m_collected = false; }   // ステージやり直しで未取得に戻す

private:
	KdModelWork   m_modelWork;
	Math::Vector3 m_spawnPos    = { 0.0f, 0.0f, 0.0f };
	float         m_bobTimer    = 0.0f;
	float         m_rotAngle    = 0.0f;
	bool          m_collected   = false;   // プレイ中に取得済み（保存・配置は維持）
	ItemEffect    m_effect;       // 星きらめき（コインは Effekseer なし）
};
