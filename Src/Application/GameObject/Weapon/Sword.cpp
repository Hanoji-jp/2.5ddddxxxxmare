#include "../../../Pch.h"
#include "Sword.h"
#include "../../Manager/ModelManager.h"
#include "../../Const/PlayerConst.h"

void Sword::Init()
{
    m_drawType = eDrawTypeLit;

    const auto spData = ModelManager::Instance().GetModel(PlayerConst::SwordPath);
    if (spData)
    {
        m_modelWork.SetModelData(spData);
    }
}

void Sword::AttachTo(const Math::Matrix& _parentWorld)
{
    m_mWorld    = _parentWorld;
    m_isAttached = true;
}

void Sword::DrawLit()
{
    if (!m_isAttached)        { return; }
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
