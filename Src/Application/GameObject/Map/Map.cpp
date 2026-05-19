#include "../../../Pch.h"
#include "Map.h"
#include "../../Manager/ModelManager.h"

void Map::Init()
{
    m_drawType = eDrawTypeLit;

    const auto spData = ModelManager::Instance().GetModel("Assets/Map/testmap.gltf");
    if (spData)
    {
        m_modelWork.SetModelData(spData);
    }
}

void Map::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
