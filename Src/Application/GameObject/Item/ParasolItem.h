#pragma once
#include "../../Const/ItemConst.h"
#include "../Effect/ItemEffect.h"

//==========================================================
// ParasolItem
// プレイヤーに傘を与えるアイテム。
// 傾けた状態で上下ボブ＋Y回転しながら浮遊する。
// Object_Ring_Glow.efk をループ再生して存在感を出す。
//==========================================================
class ParasolItem : public KdGameObject
{
public:
	ParasolItem()           = default;
	~ParasolItem() override = default;

	ParasolItem(const ParasolItem&)            = delete;
	ParasolItem& operator=(const ParasolItem&) = delete;

	void Init()    override;
	void Update()  override;
	void DrawLit() override;
	void DrawEffect() override;   // 星きらめき

	void SetSpawnPos(const Math::Vector3& pos) { m_spawnPos = pos; SetPos(pos); }
	const Math::Vector3& GetSpawnPos() const   { return m_spawnPos; }

	void Expire() { m_isExpired = true; }
	bool IsExpired() const override { return m_isExpired; }

	// 取得済みフラグ（GameScene から読む）
	bool IsPickedUp() const { return m_pickedUp; }
	void MarkPickedUp();    // エフェクト停止 + Expire

private:
	KdModelWork   m_modelWork;
	Math::Vector3 m_spawnPos  = {};
	float         m_bobTimer  = 0.0f;
	float         m_rotAngle  = 0.0f;
	bool          m_pickedUp  = false;

	// 星きらめき＋Effekseerループを統合管理
	ItemEffect    m_effect;
};
