#pragma once
#include "../../Const/GravityRingConst.h"
#include "../../Manager/PlanetGravityManager.h"
#include "../../Manager/ModelManager.h"
#include <array>
#include <vector>

//==========================================================
// GravityRingEffect
// 惑星1つ分の引力ビジュアル
//  - Sphere : GravityRadius 上を回る Box マーカーのリング
//  - Box    : 各面の法線方向に重力モードを示す矢印
//==========================================================
class GravityRingEffect
{
public:
	GravityRingEffect() = default;
	~GravityRingEffect() = default;

	void Update();
	void Draw();

	// ポインタのみ更新（毎フレーム呼ぶ用・モデルロードはしない）
	void SetPlanetPtr(const PlanetData* _pPlanet) { m_pPlanet = _pPlanet; }

	void SetPlanet(const PlanetData* _pPlanet)
	{
		m_pPlanet = _pPlanet;
		// Box矢印モデルをロード
		if (!m_spArrowData)
		{
			m_spArrowData = ModelManager::Instance().GetModel(GravityRingConst::ArrowModelPath);
			if (m_spArrowData) { m_arrowWork.SetModelData(m_spArrowData); }
		}
		// Box.gltf をロード（デブリ・パーティクル共用）
		if (!m_spBoxData)
		{
			m_spBoxData = ModelManager::Instance().GetModel(GravityRingConst::BoxModelPath);
			// デブリの各インスタンスにセット
			for (auto& d : m_debris)
			{
				d.work.SetModelData(m_spBoxData);
			}
			// パーティクルの初期化
			m_particles.resize(GravityRingConst::ParticleMax);
			for (auto& p : m_particles)
			{
				p.work.SetModelData(m_spBoxData);
			}
			// デブリの公転初期角を均等割り
			constexpr float kTwoPi = 6.28318530f;
			for (int i = 0; i < GravityRingConst::DebrisCount; ++i)
			{
				m_debris[i].orbitAngle = kTwoPi / GravityRingConst::DebrisCount * i;
				m_debris[i].driftDir   = (i % 2 == 0) ? 1.0f : -1.0f;
			}
		}
	}

private:
	// Sphere用リング描画
	void DrawSphereRing() const;
	// Box用面矢印描画
	void DrawBoxArrows();
	// 矢印1本を描画するヘルパー
	void DrawArrow(KdDebugWireFrame& _wire,
				   const Math::Vector3& _origin,
				   const Math::Vector3& _dir,
				   const Math::Color&   _col) const;
	// モデル矢印を指定ワールド行列で描画
	void DrawArrowModel(const Math::Matrix& _world);
	// BoxFaceGravityMode → 矢印方向ベクトルと色を返す
	void ResolveFaceArrow(BoxFaceGravityMode  _mode,
						  const Math::Vector3& _faceNormal,
						  Math::Vector3&       _outDir,
						  Math::Color&         _outColor) const;

	// 浮遊デブリ Update/Draw
	void UpdateDebris();
	void DrawDebris();

	// パーティクル Update/Draw
	void UpdateParticles();
	void DrawParticles();

	const PlanetData* m_pPlanet  = nullptr;

	// Sphere リング用回転角
	float             m_rotAngle = 0.0f;

	// Box矢印用モデル（共有データ）
	std::shared_ptr<KdModelData> m_spArrowData;
	KdModelWork                  m_arrowWork;

	// Box.gltf 共用モデルデータ
	std::shared_ptr<KdModelData> m_spBoxData;

	// ── 浮遊デブリ ──────────────────────────────────────────
	struct DebrisInstance
	{
		float         orbitAngle  = 0.0f;
		float         driftOffset = 0.0f;
		float         driftDir    = 1.0f;
		Math::Vector3 cachedPos   = {};
		KdModelWork   work;
	};
	std::array<DebrisInstance, GravityRingConst::DebrisCount> m_debris;

	// ── パーティクル ─────────────────────────────────────────
	struct Particle
	{
		Math::Vector3 pos      = {};
		Math::Vector3 velocity = {};
		float         life     = 0.0f;  // 残り寿命（秒）
		KdModelWork   work;
		bool          active   = false;
	};
	std::vector<Particle> m_particles;
	float                 m_spawnTimer = 0.0f;
};
