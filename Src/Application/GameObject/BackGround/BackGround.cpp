#include "../../../Pch.h"
#include "BackGround.h"
#include "../../Manager/ModelManager.h"

static constexpr const char* SkyboxModelPath = "Asset/Data/galaxybox.gltf";
static constexpr float       SkyboxScale     = 6.0f;  // gltf内スケール[92,92,92] × 5.0 = 半径460

void BackGround::Init()
{
    m_drawType = eDrawTypeUnLit;

    auto modelData = ModelManager::Instance().GetModel(SkyboxModelPath);
    m_modelWork.SetModelData(modelData);

    // 均一スケールで初期ワールド行列を設定
    m_mWorld = Math::Matrix::CreateScale(SkyboxScale);
}

void BackGround::Update()
{
    // カメラ位置にスカイボックスを追従させる
    const Math::Vector3& camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
    m_mWorld = Math::Matrix::CreateScale(SkyboxScale)
             * Math::Matrix::CreateTranslation(camPos);
}

void BackGround::DrawUnLit()
{
    if (!m_modelWork.IsEnable()) { return; }

    auto& shaderMgr = KdShaderManager::Instance();

    // スカイボックス用レンダーステート（深度完全無効・カリングなし）
    shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZDisable);
    shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

    shaderMgr.m_StandardShader.DrawModel(m_modelWork, m_mWorld);

    shaderMgr.UndoRasterizerState();
    shaderMgr.UndoDepthStencilState();
}
