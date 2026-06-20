#pragma once
#include "Framework/Utility/ThreadPool/KdThreadPool.h"

class BaseScene
{
public :

	BaseScene()			 { Init(); }
	virtual ~BaseScene() {}

	void PreUpdate();
	void Update();
	void PostUpdate();

	void PreDraw();
	void Draw();
	void DrawSprite();
	void DrawDebug();

	// オブジェクトリストを取得
	const std::list<std::shared_ptr<KdGameObject>>& GetObjList()
	{
		return m_objList;
	}

	// オブジェクトリストに追加
	void AddObject(const std::shared_ptr<KdGameObject>& _obj)
	{
		m_objList.push_back(_obj);
	}

	// ヒットストップ：一瞬だけ更新を止める（秒）
	void TriggerHitStop(float sec) { if (sec > m_hitStopTimer) { m_hitStopTimer = sec; } }
	bool IsHitStopped() const { return m_hitStopTimer > 0.0f; }

protected:
	virtual void DrawGui()        {}
	virtual void DrawDebugExtra() {}
	virtual void DrawUnLitExtra() {}  // 背景など objList 外の UnLit 描画用
	virtual void DrawLitExtra()   {}  // 惑星など objList 外のモデル描画用
	virtual void DrawOutlineExtra() {} // objList 外のアウトライン追加描画用
	virtual void DrawEffectExtra() {} // アイテム星など objList 外のエフェクト描画用
	virtual void DrawBrightExtra() {} // objList 外のブルーム源（発光）追加描画用
	virtual void DrawSpriteExtra() {} // フルスクリーンオーバーレイ等の追加スプライト描画

protected :

	// 継承先シーンで必要ならオーバーライドする
	virtual void Event();
	virtual void Init();
	// ヒットストップ中でも動かしたい演出の更新（取得バースト等）
	virtual void UpdateDuringHitStop() {}

	// true の間はオブジェクト更新・物理を止める（ポーズメニュー等）。Event は動く。
	virtual bool IsUpdatePaused() const { return false; }

	std::shared_ptr<KdCamera> m_camera = nullptr;

	// フラスタムカリング用・PreDraw で毎フレーム更新
	DirectX::BoundingFrustum m_frustum;

	// 全オブジェクトのアドレスをリストで管理
	std::list<std::shared_ptr<KdGameObject>> m_objList;

	// ヒットストップ残り時間（秒）。> 0 の間は Update / PostUpdate を止める
	float m_hitStopTimer = 0.0f;
};

