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

	void Update()      override;
	void DrawEffect()  override;   // 加算ブレンドで3Dリング描画
	void DrawDebug()   override;   // エディタ用ワイヤーフレーム

	// プレイヤーが吸い込み範囲に入ったか判定
	// 戻り値 : ワープ開始すべき場合 true（位置や速度は GameScene 側で制御する）
	bool CheckWarpTrigger(const Math::Vector3& pPos) const;

	const WarpHoleData& GetData() const { return m_data; }
	void SetData(const WarpHoleData& d) { m_data = d; m_centerPathDirty = true; }

	// トンネルのビジュアル中心線を点列で返す。
	// プレイヤーをこの線に沿って動かせば、見た目と移動が完全一致する。
	// 並び順：入口の口元 → 入口の奥 → 出口の奥 → 出口の口元
	std::vector<Math::Vector3> BuildTunnelCenterPath() const;

private:
	// 中心線（点列）に沿ってワイヤーフレームトンネルを一本描画する。
	//   両端を口元として太く、中間を細くするトランペット半径プロファイル。
	void DrawTunnelAlongPath(const std::vector<Math::Vector3>& path,
							 const Math::Color& entryCol,
							 const Math::Color& exitCol,
							 float animOffset) const;

	// 軸ベクトル計算ヘルパー
	static void MakeBasis(const Math::Vector3& normal,
						  Math::Vector3& outTangent,
						  Math::Vector3& outBitangent);

	// ポリラインを一定間隔で等間隔リサンプリングする（密度の急変を防ぐ）
	static std::vector<Math::Vector3> ResamplePath(
		const std::vector<Math::Vector3>& src, float spacing);

	// 制御点列を Catmull-Rom スプラインで密な滑らか曲線に変換する。
	//   各制御点を必ず通過しつつ、中継点の間を曲線で繋ぐ。
	static std::vector<Math::Vector3> BuildSpline(
		const std::vector<Math::Vector3>& points, int subdivPerSegment);

	WarpHoleData m_data;
	float        m_animOffset = 0.0f;  // トンネルスクロール位相（0..1）

	// 中心線キャッシュ（入口/出口/中継点が変わらない限り再計算しない）
	mutable std::vector<Math::Vector3> m_centerPathCache;
	mutable bool                       m_centerPathDirty = true;
};
