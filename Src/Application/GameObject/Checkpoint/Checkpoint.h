#pragma once
#include "../../Const/CheckpointConst.h"

//==========================================================
// Checkpoint
// プレイヤーが触れると「最後のチェックポイント」として記録。
// GameScene 側が GetPos() を参照してリスポーン座標に使う。
// 見た目は DrawVertices で自作した「旗」（はためく布＋ポール）。
//==========================================================
class Checkpoint : public KdGameObject
{
public:
	Checkpoint()          { Init(); }
	virtual ~Checkpoint() {}

	void Init()       override;
	void Update()     override;
	void DrawLit()    override;   // 旗本体（ポール＋はためく布）
	void DrawDebug()  override;

	bool IsVisible()  const override { return true; }

	// プレイヤーの参照をセット
	void SetPlayer(const std::weak_ptr<KdGameObject>& _wp) { m_wpPlayer = _wp; }

	// このチェックポイントが有効（最後に踏んだ）かどうか
	bool IsActivated() const { return m_activated; }
	void Deactivate()        { m_activated = false; }

private:
	std::weak_ptr<KdGameObject> m_wpPlayer;
	bool  m_activated = false;
	float m_time      = 0.0f;   // はためきアニメ用
};
