#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../../Framework/Math/KdCollider.h"
#include "../../Const/SpikeBoxConst.h"
#include "../../Editor/SpikeBoxEditor.h"
#include <memory>

class MovingFloor;

//==========================================================
// SpikeBox
// 棘ボックスのステージギミック（WindBox と同じ ImGui 仕様で管理）。
//  COL            : 通常の押し出し当たり判定（TypeBump | TypeGround）
//  COL_SpikeAttack: 触れると棘ダメージ（TypeDamage）
//==========================================================
class SpikeBox : public KdGameObject
{
public:
	SpikeBox() {}
	~SpikeBox() override {}

	void Init(const SpikeBoxData& data);
	void Update()     override;    // 移動床に追従する（アタッチ時のみ動く）
	void DrawLit()    override;
	void DrawOutline() override;   // 原神式アウトライン
	void DrawDebug()  override;

	bool IsVisible() const override { return true; }

	// 移動床へアタッチ（GameScene が Rebuild 時に設定）
	void SetAttachFloor(const std::weak_ptr<MovingFloor>& floor) { m_attachFloor = floor; }
	const std::weak_ptr<MovingFloor>& GetAttachFloor() const { return m_attachFloor; }

	bool                IsEnabled()     const { return m_enabled; }
	const KdCollider*   GetCollider()   const { return m_pCollider.get(); }
	const Math::Matrix& GetWorldMatrix() const { return m_worldMat; }
	const Math::Vector3& GetCenter()    const { return m_center; }

	// COL ノードのワールドAABB（押し出しは惑星と同じ最近接点方式で行う）
	bool                 HasColAabb()  const { return m_hasColAabb; }
	const Math::Vector3& GetColCenter() const { return m_colCenter; }
	const Math::Vector3& GetColHalf()   const { return m_colHalf; }

	// プレイヤー球が棘(COL_SpikeAttack)に触れているか
	bool IsSpikeHit(const Math::Vector3& pos, float radius) const;

private:
	Math::Vector3 m_center  = {};
	Math::Vector3 m_size    = { 1.0f, 1.0f, 1.0f };
	bool          m_enabled = true;
	SpikeBoxConst::SpikeDir m_dir = SpikeBoxConst::SpikeDir::Up;   // 棘の向き

	std::shared_ptr<KdModelWork> m_boxModel;
	Math::Matrix                 m_worldMat = Math::Matrix::Identity;
	std::unique_ptr<KdCollider>  m_pCollider;

	// COL ノードのワールドAABB（押し出し用）
	bool          m_hasColAabb = false;
	Math::Vector3 m_colCenter  = {};
	Math::Vector3 m_colHalf    = {};

	// 移動床への追従（アタッチ）
	std::weak_ptr<MovingFloor> m_attachFloor;     // 追従先（空なら静止）
	Math::Vector3              m_baseCenter = {};  // 配置時の中心（オフセット基準）

	// 中心を指定位置へ移す（ワールド行列・COL AABB・SetPos をまとめて更新）
	void ApplyCenter(const Math::Vector3& newCenter);
};
