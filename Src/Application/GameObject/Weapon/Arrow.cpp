#include "../../../Pch.h"
#include "Arrow.h"
#include "../../Manager/ModelManager.h"
#include "../../Const/PlayerConst.h"
#include "../../Const/ArrowConst.h"

void Arrow::Init()
{
    m_drawType = eDrawTypeLit;

    const auto spData = ModelManager::Instance().GetModel(PlayerConst::ArrowPath);
    if (spData)
    {
        m_modelWork.SetModelData(spData);
    }
}

void Arrow::Launch(const Math::Vector3& _pos, const Math::Vector3& _dir)
{
    SetPos(_pos);

    const Math::Vector3 normDir = DirectX::XMVector3Normalize(_dir);
    m_velocity    = normDir * ArrowConst::Speed;
    m_travelDist  = 0.0f;
    m_isExpired   = false;
}

void Arrow::Update()
{
    Math::Vector3 pos = GetPos();
    pos          += m_velocity;
    m_travelDist += m_velocity.Length();
    SetPos(pos);

    // XZ平面の向きからY軸回転を求めてワールド行列を構築する
    if (m_velocity.LengthSquared() > 0.0f)
    {
        const float yaw = std::atan2f(m_velocity.x, m_velocity.z);
        m_mWorld = DirectX::XMMatrixRotationY(yaw)
                 * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
    }

    if (m_travelDist >= ArrowConst::MaxTravelDist)
    {
        m_isExpired = true;
    }
}

void Arrow::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
