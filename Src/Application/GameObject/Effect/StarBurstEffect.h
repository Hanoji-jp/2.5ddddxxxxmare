#pragma once
#include "../../Const/JuiceConst.h"

//==========================================================
// StarBurstEffect
// 敵撃破時に青白い星パーティクルをバースト放出する。
// 複数の Particle を一括スポーンして使う（自動消滅）。
//
// 使い方:
//   auto burst = std::make_shared<StarBurstEffect>();
//   burst->Spawn(pos, upDir, modelData);
//   AddObject(burst);
//==========================================================
class StarBurstEffect : public KdGameObject
{
public:
	// countMul : 粒数の倍率（1.0=既定）。scaleMul : 大きさ・飛散速度の倍率。
	void Spawn(const Math::Vector3&              pos,
			   const Math::Vector3&              upDir,
			   const std::shared_ptr<KdModelData>& modelData,
			   float countMul = 1.0f,
			   float scaleMul = 1.0f);

	void Update()     override;
	void DrawEffect() override;

	bool IsVisible() const override { return true; }

private:
	// 粒1つ分のデータ
	struct Particle
	{
		KdModelWork   modelWork;
		Math::Vector3 pos;
		Math::Vector3 vel;
		float         scale       = 0.0f;
		float         scaleShrink = 0.0f;
		float         alpha       = 1.0f;
		float         lifetime    = 0.0f;
	};

	std::vector<Particle> m_particles;
};

