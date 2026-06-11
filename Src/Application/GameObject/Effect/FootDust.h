#pragma once
#include "../../Const/JuiceConst.h"

//==========================================================
// FootDust
// 歩き・ダッシュ時に足元から後方へ煙パーティクルを生成する。
// GameScene の m_objList に追加して使う（自動消滅）。
//==========================================================
class FootDust : public KdGameObject
{
public:
	// パーティクルを1つ生成。
	//   pos       : 足元の座標
	//   backDir   : 進行方向と逆向きの単位ベクトル（後方散布に使用）
	//   upDir     : プレイヤーの上方向
	//   modelData : Box.gltf のデータ
	void Spawn(const Math::Vector3& pos,
			   const Math::Vector3& backDir,
			   const Math::Vector3& upDir,
			   const std::shared_ptr<KdModelData>& modelData);

	void Update()     override;
	void DrawEffect() override;

	bool IsVisible() const override { return true; }

private:
	KdModelWork   m_modelWork;
	Math::Vector3 m_pos;
	Math::Vector3 m_vel;          // 粒ごとのランダム速度ベクトル（上昇＋横流れ）
	float         m_scale       = 0.0f;
	float         m_scaleShrink = 0.0f;  // 縮み速度（粒ごとランダム）
	float         m_alpha       = JuiceConst::DustAlphaStart;
	float         m_lifetime    = JuiceConst::DustLifetimeMax;
	float         m_gray        = 1.0f;
};

