#include "../../../Pch.h"
#include "BackGround.h"
#include "../../Manager/ModelManager.h"

static constexpr const char* SkyboxModelPath = "Asset/Data/galaxybox.gltf";
static constexpr float       SkyboxScale     = 5.0f;  // 少し小さく（カメラが遠ざかってクリップ／描画外になるのを防ぐ）

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

    // 描画直前に必ず現在のカメラ位置へ再追従（ポーズ／showcase中で Update が
    // 止まっていてもカメラが球から出て描画外にならないようにする）
    const Math::Vector3& camPos = KdShaderManager::Instance().GetCameraCB().CamPos;
    m_mWorld = Math::Matrix::CreateScale(SkyboxScale)
             * Math::Matrix::CreateTranslation(camPos);

    auto& shaderMgr = KdShaderManager::Instance();

    // スカイボックス用レンダーステート（深度完全無効・カリングなし）
    shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZDisable);
    shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

    shaderMgr.m_StandardShader.DrawModel(m_modelWork, m_mWorld);

    shaderMgr.UndoRasterizerState();
    shaderMgr.UndoDepthStencilState();
}
