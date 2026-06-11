#pragma once
#include "../../Const/WarpHoleConst.h"
#include "../../Editor/WarpHoleEditor.h"

//==========================================================
// WarpHole
// 実行時に入口に近づいたプレイヤーを吸い込み出口に射出する
// DrawEffect でワールド空間にドーナツリング＋スポークを加算描画
//==========================================================
class WarpHole : public KdGameObject
{
public:
	explicit WarpHole(const WarpHoleData& data);
	~WarpHole() override = default;

	void Init()        override;
	void Update()      override;
	void DrawLit()     override;   // BloomBOXパーティクル描画
	void DrawEffect()  override;   // 加算ブレンドで3Dリング描画
	void DrawBright()  override;   // Bloomパス：ファネルをにじませる
	void DrawDebug()   override;   // エディタ用ワイヤーフレーム

	// Entry/Exit 両端を包む球でカリング
	bool CheckInScreen(const DirectX::BoundingFrustum& frustum) const override
	{
		const Math::Vector3 center = (m_data.EntryPos + m_data.ExitPos) * 0.5f;
		const float radius = (m_data.EntryPos - m_data.ExitPos).Length() * 0.5f + WarpHoleConst::FunnelLength + WarpHoleConst::FunnelOuterRadius;
		return frustum.Intersects(DirectX::BoundingSphere(center, radius));
	}

	// プレイヤーが吸い込み範囲に入ったか判定
	bool CheckWarpTrigger(const Math::Vector3& pPos) const;

	const WarpHoleData& GetData() const { return m_data; }
	void SetData(const WarpHoleData& d) { m_data = d; m_centerPathDirty = true; }

	std::vector<Math::Vector3> BuildTunnelCenterPath() const;
	std::vector<Math::Vector3> BuildTeleportEntryPath() const;
	std::vector<Math::Vector3> BuildTeleportExitPath() const;
	std::vector<Math::Vector3> BuildTeleportExitPathReverse() const;
	std::vector<Math::Vector3> BuildTeleportEntryPathReverse() const;

private:
	//--------------------------------------------------
	// 吸い込みBOXパーティクル
	//--------------------------------------------------
	struct BoxParticle
	{
		Math::Vector3 Pos         = {};
		Math::Vector3 Velocity    = {};
		Math::Vector3 RotAxis     = { 0.0f, 1.0f, 0.0f };
		float         RotAngle    = 0.0f;   // 累積回転角（ラジアン）
	};

	// EXIT吐き出しパーティクル
	struct ExitParticle
	{
		Math::Vector3 Pos         = {};
		Math::Vector3 Velocity    = {};
		Math::Vector3 RotAxis     = { 0.0f, 1.0f, 0.0f };
		float         RotAngle    = 0.0f;
		float         Life        = 0.0f;   // 残り寸命（秒）
		bool          Active      = false;
	};

	void InitParticles();
	void SpawnParticle(BoxParticle& p) const;
	void UpdateParticles();

	// EXIT吐き出しパーティクル
	void InitExitParticles();
	void SpawnExitParticle(ExitParticle& p) const;
	void UpdateExitParticles();

	// ファネル奥端とトンネル端点リングを三角形でブリッジ接続する
	void DrawFunnelTunnelBridge(
		const Math::Vector3& funnelCenter,
		const Math::Vector3& funnelMouthDir,
		const Math::Vector3& tunnelEndPos,
		const Math::Vector3& tunnelTangent,
		const Math::Vector3& tunnelBitangent,
		const Math::Color&   col) const;

	// ローポリファネル（正面向きロート形状）描画
	// invertGradient=true のとき奥ほど明るく・口元でフェードアウト（Bloomパス用）
	void DrawFunnelFace(const Math::Vector3& center,
						const Math::Vector3& mouthDir,
						const Math::Color&   col,
						const std::vector<float>& jitterOffsets,
						float animTime,
						bool invertGradient = false) const;

	// パス上を面＋ワイヤーで繋ぐチューブ描画（ファネルと同じ面スタイル）
	void DrawTunnelFace(const std::vector<Math::Vector3>& path,
						const Math::Color& entryCol,
						const Math::Color& exitCol,
						const std::vector<float>& jitterOffsets,
						float animTime) const;

	// Init時に固定シードで生成するランダム頂点オフセット
	// サイズ = FunnelRings * FunnelSegments
	std::vector<float>   m_entryJitter;
	std::vector<float>   m_exitJitter;
	std::vector<float>   m_tunnelJitter; // トンネル用（TunnelRings * FunnelSegments）

	std::vector<BoxParticle>  m_particles;
	std::vector<ExitParticle> m_exitParticles;
	float                     m_exitSpawnTimer = 0.0f;
	KdModelWork               m_boxModel;

	void DrawTunnelAlongPath(const std::vector<Math::Vector3>& path,
							 const Math::Color& entryCol,
							 const Math::Color& exitCol,
							 float animOffset,
							 bool entryMouthOnly = false,
							 float animTime = 0.0f) const;

	static void MakeBasis(const Math::Vector3& normal,
						  Math::Vector3& outTangent,
						  Math::Vector3& outBitangent);

	static std::vector<Math::Vector3> ResamplePath(
		const std::vector<Math::Vector3>& src, float spacing);

	static std::vector<Math::Vector3> BuildSpline(
		const std::vector<Math::Vector3>& points, int subdivPerSegment);

	// パスの両端から trimDist 分の点を弧長ベースで削除する
	static std::vector<Math::Vector3> TrimTunnelPath(
		const std::vector<Math::Vector3>& path, float trimDist);

	WarpHoleData m_data;
	float        m_animOffset = 0.0f;

	mutable std::vector<Math::Vector3> m_centerPathCache;
	mutable bool                       m_centerPathDirty = true;

	// 頂点バッファ再利用（毎フレームの動的確保を回避）
	mutable std::vector<KdPolygon::Vertex> m_vtxBuf;
};

