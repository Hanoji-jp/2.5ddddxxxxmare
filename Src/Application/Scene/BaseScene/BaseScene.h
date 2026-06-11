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

protected:
	virtual void DrawGui()        {}
	virtual void DrawDebugExtra() {}
	virtual void DrawUnLitExtra() {}  // 背景など objList 外の UnLit 描画用
	virtual void DrawLitExtra()   {}  // 惑星など objList 外のモデル描画用
	virtual void DrawSpriteExtra() {} // フルスクリーンオーバーレイ等の追加スプライト描画

protected :

	// 継承先シーンで必要ならオーバーライドする
	virtual void Event();
	virtual void Init();

	std::shared_ptr<KdCamera> m_camera = nullptr;

	// フラスタムカリング用・PreDraw で毎フレーム更新
	DirectX::BoundingFrustum m_frustum;

	// 全オブジェクトのアドレスをリストで管理
	std::list<std::shared_ptr<KdGameObject>> m_objList;
};

