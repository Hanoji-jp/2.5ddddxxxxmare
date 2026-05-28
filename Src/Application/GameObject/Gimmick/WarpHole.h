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

	// プレイヤーを吸い込み範囲に入ったか判定して射出
	// pPos : 現在位置  pVel : 速度（上書きする）
	// 戻り値 : 射出した場合 true
	bool TryWarp(Math::Vector3& pPos, Math::Vector3& pVel) const;

	const WarpHoleData& GetData() const { return m_data; }
	void SetData(const WarpHoleData& d) { m_data = d; }

private:
	// ワイヤーフレームトンネルを描画（LINE_LIST）
	void DrawTunnel(const Math::Vector3& origin,
					const Math::Vector3& axis,
					const Math::Color& col,
					float animOffset) const;

	// 軸ベクトル計算ヘルパー
	static void MakeBasis(const Math::Vector3& normal,
						  Math::Vector3& outTangent,
						  Math::Vector3& outBitangent);

	WarpHoleData m_data;
	float        m_animOffset = 0.0f;  // トンネルスクロール位相（0..1）
};
