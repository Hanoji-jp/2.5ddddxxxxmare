#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../../Framework/Direct3D/Polygon/KdPolygon.h"
#include "../../../Framework/Math/KdCollider.h"
#include "../../Const/WindBoxConst.h"
#include "../../Editor/WindBoxEditor.h"

//==========================================================
// WindBox
// 範囲内のプレイヤーに風力を加えるステージギミック。
// 螺旋しながら風向きに流れるビルボードパーティクルを常時描画する。
//==========================================================
class WindBox : public KdGameObject
{
public:
	WindBox() {}
	~WindBox() override {}

	void Init(const WindBoxData& data);
	void Update()     override;
	void DrawLit()    override;
	void DrawEffect() override;
	void DrawDebug()  override;

	bool IsVisible() const override { return true; }

	// プレイヤー位置がこのボックスの範囲内かどうか
	bool IsInRange(const Math::Vector3& pos) const;

	// 風の方向・強さを取得
	const Math::Vector3& GetWindDir()    const { return m_windDir; }
	float                GetPower()      const { return m_power; }
	bool                 IsEnabled()     const { return m_enabled; }

	// 当たり判定アクセサ（Character::CheckWall から使用）
	const KdCollider*    GetCollider()   const { return m_pCollider.get(); }
	const Math::Matrix&  GetWorldMatrix() const { return m_worldMat; }

private:
	// 風向きに対して垂直な平面上のランダム点（ボックス範囲内）を生成
	Math::Vector3 RandomPerp() const;

	Math::Vector3 m_center   = {};
	Math::Vector3 m_halfSize = {};   // size * 0.5f

	Math::Vector3 m_windDir  = { 1.0f, 0.0f, 0.0f };
	float         m_power    = WindBoxConst::DefaultPower;
	float         m_length   = WindBoxConst::DefaultLength;  // COL上端からの風の延長距離
	bool          m_enabled  = true;

	// 風向きに対する右・上基底（Init時に構築）
	Math::Vector3 m_windRight = {};
	Math::Vector3 m_windUp    = {};

	// ボックスの風方向への深さ（spiralTravel の上限）
	float m_windDepth = 4.0f;

	// 螺旋パーティクル1個分
	struct WindParticle
	{
		Math::Vector3 startPerp;   // 風向き垂直面上のスポーン位置（中心相対）
		float         travelDist;  // 風向きに沿った現在進行距離
		float         travelMax;   // このパーティクルの移動上限（ボックス奥行き）
		float         orbitPhase;  // 公転位相（ラジアン）
		float         orbitRadius; // 螺旋半径
		float         selfRot;     // 自転角（ラジアン）
		float         size;        // ポリゴンの一辺サイズ
		float         speed;       // 移動速度（power 比例）
	};

	std::vector<WindParticle>      m_particles;
	std::shared_ptr<KdTexture>     m_spTex;

	// Box モデル（windbox.gltf）
	std::shared_ptr<KdModelWork>   m_boxModel;

	// プロペラの現在回転角（ラジアン、毎フレーム積算）
	float                          m_propellerAngle = 0.0f;

	// プロペラノードの初期ローカル行列（毎フレームこれに回転だけ乗せる）
	Math::Matrix                   m_propellerInitLocal = Math::Matrix::Identity;

	// 毎フレーム更新されるワールド行列（COL判定にも使用）
	Math::Matrix                   m_worldMat = Math::Matrix::Identity;

	// COLノードベースの当たり判定
	std::unique_ptr<KdCollider>    m_pCollider;

	// デバッグ描画用
	std::unique_ptr<KdDebugWireFrame> m_pDebugWire;

	void InitParticle(WindParticle& p, float initialTravel) const;
};
