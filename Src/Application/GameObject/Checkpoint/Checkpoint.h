#pragma once
#include "../../Const/CheckpointConst.h"

//==========================================================
// Checkpoint
// プレイヤーが触れると「最後のチェックポイント」として記録
// GameScene 側が GetPos() を参照してリスポーン座標に使う
//==========================================================
class Checkpoint : public KdGameObject
{
public:
	Checkpoint()          { Init(); }
	virtual ~Checkpoint() {}

	void Init()       override;
	void Update()     override;
	void DrawDebug()  override;

	bool IsVisible()  const override { return false; }

	// プレイヤーの参照をセット
	void SetPlayer(const std::weak_ptr<KdGameObject>& _wp) { m_wpPlayer = _wp; }

	// このチェックポイントが有効（最後に踏んだ）かどうか
	bool IsActivated() const { return m_activated; }
	void Deactivate()        { m_activated = false; }

private:
	std::weak_ptr<KdGameObject> m_wpPlayer;
	bool m_activated = false;
};
