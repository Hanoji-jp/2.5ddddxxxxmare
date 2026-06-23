#pragma once
#include "../../Const/JuiceConst.h"

//==========================================================
// DamageBurst
// プレイヤーが被ダメージしたとき、体全体から赤系のboxパーティクルを
// 全方向へ「パーん」と弾けさせる（FootDust/StarBurst と同じ自動消滅方式）。
//
// 使い方:
//   auto burst = std::make_shared<DamageBurst>();
//   burst->Spawn(playerPos, playerUpDir, boxModelData);
//   AddObject(burst);
//==========================================================
class DamageBurst : public KdGameObject
{
public:
	void Spawn(const Math::Vector3&              center,
			   const Math::Vector3&              upDir,
			   const std::shared_ptr<KdModelData>& modelData);

	void Update()     override;
	void DrawEffect() override;

	bool IsVisible() const override { return true; }

private:
	struct Particle
	{
		KdModelWork   modelWork;
		Math::Vector3 pos;
		Math::Vector3 vel;
		float         scale       = 0.0f;
		float         scaleShrink = 0.0f;
		float         alpha       = 1.0f;
		float         lifetime    = 0.0f;
		float         spin        = 0.0f;   // 自転角
		float         spinSpeed   = 0.0f;
	};

	Math::Vector3         m_up = { 0.0f, 1.0f, 0.0f };
	std::vector<Particle> m_particles;
};
