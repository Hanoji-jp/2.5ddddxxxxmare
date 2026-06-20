#include "../../../Pch.h"
#include "SpikeBox.h"
#include "../../Const/OutlineConst.h"

//----------------------------------------------------------
void SpikeBox::Init(const SpikeBoxData& data)
{
	m_center  = data.center;
	m_size    = data.size;
	m_enabled = data.enabled;

	// モデル読込
	m_boxModel = std::make_shared<KdModelWork>();
	m_boxModel->SetModelData(SpikeBoxConst::BoxModelPath);
	m_boxModel->CalcNodeMatrices();   // コライダーのノード行列を確定

	// 影生成パス(GenerateDepthMapFromLight)で DrawLit を描かせる
	m_drawType = eDrawTypeLit;

	// ワールド行列：スケール → 平行移動（コライダー・描画で共用）
	m_worldMat = Math::Matrix::CreateScale(m_size)
			   * Math::Matrix::CreateTranslation(m_center);
	SetPos(m_center);

	// フラスタムカリング半径
	m_cullingRadius = m_size.Length() * 2.0f;

	// コライダー登録：COL=押し出し / COL_SpikeAttack=棘ダメージ をノードごとに分離。
	// 球の押し出しは workNode の行列を使うため、必ず KdModelWork ベースで作る（WindBox/惑星と同じ）。
	m_pCollider = std::make_unique<KdCollider>();
	if (const auto spData = m_boxModel->GetData())
	{
		const auto& nodes = spData->GetOriginalNodes();

		// ノード名 → GetOriginalNodes() のインデックスを引く
		auto findNode = [&](const char* name) -> int
		{
			for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
			{
				if (nodes[i].m_name == name) { return i; }
			}
			return -1;
		};

		// COL（押し出し：床＋壁）
		const int colIdx = findNode(SpikeBoxConst::ColNodeName);
		if (colIdx >= 0)
		{
			auto shape = std::make_unique<KdModelCollision>(
				m_boxModel, KdCollider::TypeGround | KdCollider::TypeBump);
			shape->SetNodeFilter({ colIdx });
			m_pCollider->RegisterCollisionShape(SpikeBoxConst::ColNodeName, std::move(shape));
		}

		// COL_SpikeAttack（棘ダメージ）
		const int atkIdx = findNode(SpikeBoxConst::AttackNodeName);
		if (atkIdx >= 0)
		{
			auto shape = std::make_unique<KdModelCollision>(m_boxModel, KdCollider::TypeDamage);
			shape->SetNodeFilter({ atkIdx });
			m_pCollider->RegisterCollisionShape(SpikeBoxConst::AttackNodeName, std::move(shape));
		}

		// COL ノードのワールドAABB を算出（押し出しは惑星と同じ最近接点方式で行うため）
		const auto& workNodes = m_boxModel->GetNodes();
		if (colIdx >= 0 && nodes[colIdx].m_spMesh)
		{
			const DirectX::BoundingBox& localBB = nodes[colIdx].m_spMesh->GetBoundingBox();
			const Math::Matrix nodeWorld = workNodes[colIdx].m_worldTransform * m_worldMat;

			DirectX::BoundingBox worldBB;
			localBB.Transform(worldBB, nodeWorld);   // 回転があってもAABBへ展開
			m_colCenter  = { worldBB.Center.x,  worldBB.Center.y,  worldBB.Center.z };
			m_colHalf    = { worldBB.Extents.x, worldBB.Extents.y, worldBB.Extents.z };
			m_hasColAabb = true;
		}
	}
}

//----------------------------------------------------------
void SpikeBox::DrawLit()
{
	if (!m_enabled || !m_boxModel || !m_boxModel->IsEnable()) { return; }
	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_boxModel, m_worldMat);
}

//----------------------------------------------------------
void SpikeBox::DrawOutline()
{
	if (!m_enabled || !m_boxModel || !m_boxModel->IsEnable()) { return; }
	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::TerrainWidth);   // 大きいので太め
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(*m_boxModel, m_worldMat, c, Math::Vector3::Zero);
}

//----------------------------------------------------------
void SpikeBox::DrawDebug()
{
	if (m_pDebugWire) { m_pDebugWire->Draw(); }
}

//----------------------------------------------------------
// プレイヤー球が棘(COL_SpikeAttack=TypeDamage)に触れているか
//----------------------------------------------------------
bool SpikeBox::IsSpikeHit(const Math::Vector3& pos, float radius) const
{
	if (!m_enabled || !m_pCollider) { return false; }

	const KdCollider::SphereInfo sphere(KdCollider::TypeDamage, pos, radius);
	return m_pCollider->Intersects(sphere, m_worldMat, nullptr);
}
