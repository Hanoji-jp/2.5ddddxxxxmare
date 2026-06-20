#pragma once
#include "../../Const/MovingFloorConst.h"
#include "../../Const/PlanetConst.h"
#include "../../Editor/MovingFloorEditor.h"
#include "../../../Framework/Direct3D/KdModel.h"
#include "../../../Framework/Direct3D/KdTexture.h"

//==========================================================
// MovingFloor
// Box.gltf を使って往復移動する床。
// Planet NormalGravity Box と同一の幾何距離方式で判定する。
// KdCollider は不要（Character 側が幾何計算で判定）。
//==========================================================
class MovingFloor : public KdGameObject
{
public:
	// 移動軸
	enum class Axis { X, Y, Z };

	MovingFloor() {}
	~MovingFloor() override {}

	void Init()      override;
	void Update()    override;
	void DrawLit()   override;
	void DrawOutline() override;   // 地形アウトライン（細め）
	void DrawUnLit() override;
	void DrawDebug() override;

	bool IsVisible() const override { return true; }

	// 設定（Init 前に呼ぶ）
	void SetCenter(const Math::Vector3& c) { m_center = c; }
	void SetAxis(Axis a)                   { m_axis   = a; }
	void SetRange(float r)                 { m_range  = r; }
	void SetSize(const Math::Vector3& s)   { m_size   = s; }

	// エディターデータから一括設定（Init 前に呼ぶ）
	void SetData(const MovingFloorData& d)
	{
		m_center         = d.center;
		m_axis           = static_cast<Axis>(static_cast<int>(d.axis));
		m_range          = d.range;
		m_size           = d.size;
		m_speed          = d.speed;
		m_waitDuration   = d.waitTime;
		m_startPhase     = static_cast<int>(d.startPhase);  // -1, 0, 1
		m_bNormalGravity = d.bNormalGravity;
		m_faceTop        = d.faceTop;
		m_faceBottom     = d.faceBottom;
		m_faceLeft       = d.faceLeft;
		m_faceRight      = d.faceRight;
	}

	// 現在の中心位置
	Math::Vector3 GetPos() const override { return m_pos; }

	// 判定・描画用アクセサ
	bool               IsNormalGravity() const { return m_bNormalGravity; }
	BoxFaceGravityMode GetFaceTop()      const { return m_faceTop; }
	BoxFaceGravityMode GetFaceBottom()   const { return m_faceBottom; }
	BoxFaceGravityMode GetFaceLeft()     const { return m_faceLeft; }
	BoxFaceGravityMode GetFaceRight()    const { return m_faceRight; }

	// 判定用：半辺長（Planet Box の BoxHalfExtents 相当）
	const Math::Vector3& GetHalfExtents() const { return m_size; }

	// 今フレームの移動量（キャラクターに加算してもらう用）
	const Math::Vector3& GetDeltaMove() const { return m_deltaMove; }

	// 壁レイキャスト用コライダー & ワールド行列
	const KdCollider*    GetCollider()   const { return m_pCollider.get(); }
	const Math::Matrix&  GetWorldMatrix()const { return m_mWorld; }

private:
	void InitModel();
	void UpdateWorld();

	Math::Vector3 m_center = {};
	Math::Vector3 m_pos    = {};
	Math::Vector3 m_size   = { 
								MovingFloorConst::DefaultSizeX,
							    MovingFloorConst::DefaultSizeY,
							    MovingFloorConst::DefaultSizeZ };
	Axis  m_axis         = Axis::X;
	float m_range        = MovingFloorConst::DefaultRange;
	float m_speed        = MovingFloorConst::MoveSpeed;
	float m_waitDuration = MovingFloorConst::DefaultWaitTime;  // 折り返し一時停止時間（秒）
	float m_waitTimer    = 0.0f;                                // 残り停止時間
	int   m_startPhase   = 0;  // -1=負端, 0=中心, 1=正端
	float m_t            = 0.0f;
	int   m_dir          = 1;

	Math::Vector3 m_deltaMove = {};

	// Planet Box と同じ重力フェイス設定
	bool               m_bNormalGravity = true;
	BoxFaceGravityMode m_faceTop        = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode m_faceBottom     = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode m_faceLeft       = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode m_faceRight      = BoxFaceGravityMode::Inward;

	// 壁レイキャスト用コライダー（TypeBump BoundingBox、ローカル単位 {1,1,1}）
	std::unique_ptr<KdCollider>  m_pCollider;

	// Box 本体モデル
	std::shared_ptr<KdModelWork> m_modelWork;

	// 草キャップモデル（Inward な面それぞれに生成）
	// インデックス: 0=Top, 1=Bottom, 2=Left, 3=Right
	std::shared_ptr<KdModelWork> m_grassCapWork[4];
	Math::Matrix                 m_grassCapWorld[4] = {
		Math::Matrix::Identity, Math::Matrix::Identity,
		Math::Matrix::Identity, Math::Matrix::Identity };

	// テクスチャ
	std::shared_ptr<KdTexture>   m_spGrassTex;
	std::shared_ptr<KdTexture>   m_spGrassNormalTex;
	std::shared_ptr<KdTexture>   m_spGrassEdgeTex;
};

