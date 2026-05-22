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

	}
}

void Map::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
