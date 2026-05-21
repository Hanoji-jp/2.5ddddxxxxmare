#include "../../../Pch.h"
#include "MapObject.h"
#include "../../Manager/ModelManager.h"

void MapObject::Init(const MapObjectData& _data)
{
    m_drawType = eDrawTypeLit;

    // モデルロード
    const auto spData = ModelManager::Instance().GetModel(_data.modelPath);
    if (spData)
    {
        m_modelWork.SetModelData(spData);
    }

    // ワールド行列を構築
    const Math::Matrix mScale = Math::Matrix::CreateScale(_data.scale);
    const Math::Matrix mRot   =
        Math::Matrix::CreateRotationX(_data.rotation.x) *
        Math::Matrix::CreateRotationY(_data.rotation.y) *
        Math::Matrix::CreateRotationZ(_data.rotation.z);
    const Math::Matrix mTrans = Math::Matrix::CreateTranslation(_data.position);

    m_mWorld = mScale * mRot * mTrans;
}

void MapObject::DrawLit()
{
    if (!m_modelWork.IsEnable()) { return; }
    KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, m_mWorld);
}
