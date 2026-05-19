#include "../../../Pch.h"
#include "BackGround.h"

void BackGround::Init()
{
    m_drawType = eDrawTypeLit;
}

void BackGround::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
