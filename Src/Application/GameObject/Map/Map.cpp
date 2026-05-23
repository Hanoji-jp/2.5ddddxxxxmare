#include "../../../Pch.h"
#include "Map.h"
#include "../../Manager/ModelManager.h"

void Map::Init()
{
	m_drawType = eDrawTypeLit;

	const auto spData = ModelManager::Instance().GetModel("Asset/Data/Map.gltf");
	if (spData)
	{
		m_modelWork.SetModelData(spData);
		//ワールドを右に回転
		Math::Matrix mapScale = Math::Matrix::CreateScale(1.5f);
		Math::Matrix mapRot   = Math::Matrix::CreateRotationY(DirectX::XMConvertToRadians(90.f));
		m_mWorld = mapScale * mapRot;

		// コライダーを生成
		// ノード名で TypeGround / TypeBump を分けて個別登録する
		m_pCollider = std::make_unique<KdCollider>();

		const auto& colIndices = spData->GetCollisionMeshNodeIndices();
		KdDebugGUI::Instance().AddLog("Map COL nodes: %d", (int)colIndices.size());
		for (int idx : colIndices)
		{
			const std::string& name = spData->GetOriginalNodes()[idx].m_name;
			KdDebugGUI::Instance().AddLog("  COL node[%d]: %s", idx, name.c_str());

			const bool isFloor = (name.find("Floor") != std::string::npos || name.find("floor") != std::string::npos);
			const UINT type    = isFloor ? (KdCollider::TypeGround | KdCollider::TypeBump) : KdCollider::TypeBump;

			auto spShape = std::make_unique<KdModelCollision>(
				std::shared_ptr<KdModelWork>(&m_modelWork, [](KdModelWork*) {}), type);
			spShape->SetNodeFilter({ idx });
			m_pCollider->RegisterCollisionShape(name, std::move(spShape));
		}

		// 初回のノード行列を確定させる
		m_modelWork.CalcNodeMatrices();

		// デバッグ：COL_Wall ノードのワールド行列位置を確認
		auto& workNodes = m_modelWork.GetNodes();
		auto& dataNodes = spData->GetOriginalNodes();
		for (int idx : spData->GetCollisionMeshNodeIndices())
		{
			Math::Vector3 t = workNodes[idx].m_worldTransform.Translation();
			KdDebugGUI::Instance().AddLog("COL node[%d]=%s worldPos=(%.1f,%.1f,%.1f)",
				idx, dataNodes[idx].m_name.c_str(), t.x, t.y, t.z);
		}
	}
}

void Map::PostUpdate()
{
    // コリジョン判定で使用するノード行列を毎フレーム更新
    if (m_modelWork.NeedCalcNodeMatrices())
    {
        m_modelWork.CalcNodeMatrices();
    }
}

void Map::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);

    // デバッグ：COLノードを半透明で描画
    const auto spData = m_modelWork.GetData();
    if (!spData) { return; }
    auto& workNodes = m_modelWork.GetNodes();
    auto& dataNodes = spData->GetOriginalNodes();
    for (int idx : spData->GetCollisionMeshNodeIndices())
    {
        KdShaderManager::Instance().m_StandardShader.DrawMesh(
            dataNodes[idx].m_spMesh.get(),
            workNodes[idx].m_worldTransform * m_mWorld,
            spData->GetMaterials(),
            { 1.0f, 0.0f, 0.0f, 0.5f },
            Math::Vector3::Zero);
    }
}
