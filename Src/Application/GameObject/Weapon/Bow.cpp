#include "../../../Pch.h"
#include "Bow.h"
#include "../../Manager/ModelManager.h"
#include "../../Const/PlayerConst.h"

void Bow::Init()
{
    m_drawType = eDrawTypeLit;

    const auto spData = ModelManager::Instance().GetModel(PlayerConst::BowPath);
    if (spData)
    {
        m_modelWork.SetModelData(spData);
    }
}

void Bow::AttachTo(const Math::Matrix& _parentWorld)
{
    m_mWorld    = _parentWorld;
    m_isAttached = true;
}

void Bow::DrawLit()
{
    if (!m_isAttached)           { return; }
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
