#pragma once
#include "../../Framework/Direct3D/KdCamera.h"
#include "../../Framework/Direct3D/KdModel.h"
#include "../../Framework/Shader/KdRenderTargetChange.h"
#include "../Manager/ModelManager.h"
#include "../Const/ItemConst.h"
#include "../Const/UIConst.h"
#include "../Const/OutlineConst.h"
#include <memory>

//==========================================================
// CoinModelIcon
// UI用にコインの3Dモデル(coin.gltf)を専用オフスクリーンRTへ描き、
// その結果テクスチャを HUD/リザルト等へスプライト貼りするための共通ヘルパー。
//   ・本体(Lit/UnLit) ＋ 原神式アウトライン(背面押し出し)で描画
//   ・背景は透明(α0)。回転・サイズ等は UIConst の Coin3D* で調整
//   ・Render() は「3D描画パス内」で毎フレーム呼ぶこと（StandardShader が使える文脈）。
//     呼び出し後にシーンカメラを復帰させる（restoreCam を渡す）。
//==========================================================
class CoinModelIcon
{
public:
	// 3Dパス内で毎フレーム呼ぶ。描画後 restoreCam でシーンカメラを元へ戻す。
	void Render(float dt, const std::shared_ptr<KdCamera>& restoreCam)
	{
		// 初期化（RT＋モデル＋射影）
		if (!m_init)
		{
			const int sz = UIConst::Coin3DRTSize;
			m_rt.CreateRenderTarget(sz, sz, true);   // α＋深度あり
			auto data = ModelManager::Instance().GetModel(ItemConst::CoinModelPath);
			if (data) { m_model.SetModelData(data); }
			m_cam.SetProjectionMatrix(UIConst::Coin3DFovDeg, 100.0f, 0.01f, 1.0f); // 正方RT=アスペクト1
			// 原点のコインを -Z 側から見る（Kdカメラ前方＝+Z）
			m_cam.SetCameraMatrix(Math::Matrix::CreateTranslation(0.0f, 0.0f, -UIConst::Coin3DCamDist));
			m_init = true;
		}
		if (!m_model.GetData()) { return; }

		m_spin += dt * UIConst::Coin3DSpinSpeed;

		const Math::Matrix world =
			Math::Matrix::CreateScale(UIConst::Coin3DScale) *
			Math::Matrix::CreateRotationY(m_spin) *
			Math::Matrix::CreateRotationX(DirectX::XMConvertToRadians(UIConst::Coin3DTiltDeg));

		auto& sm = KdShaderManager::Instance();
		KdRenderTargetChanger changer;
		changer.ChangeRenderTarget(m_rt);
		m_rt.ClearTexture(Math::Color(0.0f, 0.0f, 0.0f, 0.0f));   // 背景透明
		m_cam.SetToShader();

		sm.ChangeDepthStencilState(KdDepthStencilState::ZEnable);
		sm.ChangeBlendState(KdBlendState::Alpha);

		auto& ss = sm.m_StandardShader;

		// 本体
		if (UIConst::Coin3DLit)
		{
			ss.BeginLit();
			ss.DrawModel(m_model, world);
			ss.EndLit();
		}
		else
		{
			ss.BeginUnLit();
			ss.DrawModel(m_model, world);
			ss.EndUnLit();
		}

		// アウトライン（背面押し出し。本体の後に描く前提）
		ss.BeginOutline();
		ss.SetOutlineWidth(OutlineConst::Width);
		const Math::Color oc(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
		ss.DrawModel(m_model, world, oc, Math::Vector3::Zero);
		ss.EndOutline();

		sm.UndoBlendState();
		sm.UndoDepthStencilState();
		changer.UndoRenderTarget();

		// シーンカメラへ復帰（この後の3D描画/ブルームパスが正しいカメラで動くように）
		if (restoreCam) { restoreCam->SetToShader(); }
	}

	// 生成済みなら結果テクスチャ、未生成なら nullptr
	KdTexture* GetTexture() const
	{
		if (m_init && m_rt.m_RTTexture && m_rt.m_RTTexture->WorkSRView())
		{
			return m_rt.m_RTTexture.get();
		}
		return nullptr;
	}

private:
	KdRenderTargetPack m_rt;
	KdCamera           m_cam;
	KdModelWork        m_model;
	bool               m_init = false;
	float              m_spin = 0.0f;
};
