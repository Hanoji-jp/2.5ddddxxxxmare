#pragma once
#include "../Const/PlanetConst.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include "../../Framework/Direct3D/KdModel.h"
#include "../../Framework/Math/KdCollider.h"

//==========================================================
// PlanetShape
// 惑星の形状種別
//==========================================================
enum class PlanetShape
{
	Sphere,   // 球形惑星（中心方向へ引力）
	Box,      // 立方体惑星（最近傍面の法線方向へ引力）
};

//==========================================================
// BoxFaceGravityMode
// Box惑星の各面の重力モード
//==========================================================
enum class BoxFaceGravityMode
{
	Inward,   // 面の内向き（面に引き寄せる）
	Outward,  // 面の外向き（面から弾き飛ばされる＝その床から落ちる）
	Inherit,  // 現在の重力を継承（どちら側から来ても同じ方向に落ちる）
	Down,     // 下方向（通常重力）
	Up,       // 上方向
	Left,     // 左方向
	Right,    // 右方向
};

//==========================================================
// PlanetData
// 惑星1個分のデータ
//==========================================================
struct PlanetData
{
	Math::Vector3 Position        = { 0.0f, -20.0f, 0.0f };
	float         SurfaceRadius   = PlanetConst::DefaultSurfaceRadius;
	float         GroundRadius    = PlanetConst::DefaultGroundRadius;
	float         GravityRadius   = PlanetConst::DefaultGravityRadius;
	int           Priority        = PlanetConst::DefaultPriority;
	bool          bNormalGravity  = false;
	float         GravityStrength = 1.0f;  // 重力強度（1.0が標準、大きいほど強い）
	PlanetShape   Shape           = PlanetShape::Sphere;
	Math::Vector3 BoxHalfExtents  = { PlanetConst::DefaultSurfaceRadius,
									  PlanetConst::DefaultSurfaceRadius,
									  PlanetConst::DefaultSurfaceRadius };

	// Box惑星の各面の重力モード（上・下・左・右）
	BoxFaceGravityMode BoxFaceGravityTop    = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode BoxFaceGravityBottom = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode BoxFaceGravityLeft   = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode BoxFaceGravityRight  = BoxFaceGravityMode::Inward;

	// モデル（描画 & コリジョン）
	std::shared_ptr<KdModelWork> modelWork;
	std::unique_ptr<KdCollider>  pCollider;
	Math::Matrix                 mWorld = Math::Matrix::Identity;

	// 草キャップモデル（Box惑星の上面に乗せる薄い板）
	std::shared_ptr<KdModelWork> grassCapWork;
	Math::Matrix                 mGrassCapWorld = Math::Matrix::Identity;

	// unique_ptr を持つため、コピー禁止・ムーブ許可
	PlanetData()                             = default;
	PlanetData(PlanetData&&)                 = default;
	PlanetData& operator=(PlanetData&&)      = default;
	PlanetData(const PlanetData&)            = delete;
	PlanetData& operator=(const PlanetData&) = delete;

	void InitModel();
	void UpdateWorld();
};

//==========================================================
// GravityInfluenceResult
// 重力合成の結果
//==========================================================
struct GravityInfluenceResult
{
	Math::Vector3 totalGravityDir  = { 0.0f, -1.0f, 0.0f }; // 合成重力方向（正規化済み）
	Math::Vector3 dominantUpDir    = { 0.0f,  1.0f, 0.0f }; // 最も影響力が強い惑星の上方向
	int           dominantPlanetIdx = -1;                     // 最も影響力が強い惑星のインデックス
	bool          hasInfluence      = false;                  // 惑星の影響を受けているか
};

//==========================================================
// PlanetGravityManager
// 複数の惑星を管理し、キャラクターへの惑星重力を提供する
// ImGui で編集し CSV で永続化する
//==========================================================
class PlanetGravityManager
{
public:
	static PlanetGravityManager& Instance()
	{
		static PlanetGravityManager s;
		return s;
	}

	void DrawGui();
	void DrawDebugShapes() const;
	void DrawLit() const;   // 惑星モデルを描画
	void PostUpdate();       // 位置変更に追従してワールド行列を更新（Dirty時のみ）

	// 惑星の位置・形状が変わったことを通知（次の PostUpdate で行列を再計算）
	void MarkWorldDirty() { m_worldDirty = true; }

	void Save() const;
	void Load();

	// キャラクター位置から最も近い影響圏内の惑星インデックスを返す（なければ -1）
	int FindNearestPlanetIndex(const Math::Vector3& _charPos) const;

	// キャラクター位置から最も近い影響圏内の惑星データを返す（なければ nullptr）
	const PlanetData* FindNearestPlanet(const Math::Vector3& _charPos) const;

	// 全惑星の重力を合成して影響力を計算（電場方式）
	// currentGravDir: Inherit モード用に現在のキャラクター重力方向を渡す（デフォルトは下向き）
	GravityInfluenceResult ComputeGravityInfluence(const Math::Vector3& _charPos,
		const Math::Vector3& currentGravDir = { 0.0f, -1.0f, 0.0f }) const;

	// インデックスから PlanetData を取得（範囲外なら nullptr）
	const PlanetData* GetPlanet(int index) const
	{
		if (index < 0 || index >= static_cast<int>(m_planets.size())) { return nullptr; }
		return &m_planets[index];
	}

	const std::vector<PlanetData>& GetPlanets() const { return m_planets; }

private:
	PlanetGravityManager()  { Load(); }
	~PlanetGravityManager() {}
	PlanetGravityManager(const PlanetGravityManager&)            = delete;
	PlanetGravityManager& operator=(const PlanetGravityManager&) = delete;

	std::vector<PlanetData> m_planets;

	// 芝生テクスチャ（Box惑星の上面ブレンド用）
	std::shared_ptr<KdTexture> m_spGrassTex;

	// 芝生法線マップ（盛り上がり表現用）
	std::shared_ptr<KdTexture> m_spGrassNormalTex;

	// 草エッジ（境目）テクスチャ（側面上部の土/境目表現）
	std::shared_ptr<KdTexture> m_spGrassEdgeTex;
	int                     m_selectedIndex = -1;
	bool                    m_worldDirty    = true;  // true の間だけ全惑星のワールド行列を再計算
};
