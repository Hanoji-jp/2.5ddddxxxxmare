#pragma once
#include "../../Const/LightConst.h"

//==========================================================
// PointLightObject
// 点光源ゲームオブジェクト
// 毎フレーム AmbientController に点光源を登録する
//==========================================================
class PointLightObject : public KdGameObject
{
public:
	PointLightObject() { Init(); }
	virtual ~PointLightObject() {}

	void Init()       override;
	void Update()     override;
	void DrawDebug()  override;
	void DrawGui();

	// 光源パラメータのセッター
	void SetColor(const Math::Vector3& color)   { m_color  = color; }
	void SetRadius(float radius)                { m_radius = radius; }
	void SetPos(const Math::Vector3& pos)       { m_mWorld.Translation(pos); }

	Math::Vector3 GetPos() const { return m_mWorld.Translation(); }

private:
	Math::Vector3 m_color  = LightConst::PointLightDefaultColor;
	float         m_radius = LightConst::PointLightDefaultRadius;
};
