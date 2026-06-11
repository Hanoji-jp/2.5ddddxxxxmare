#include "../../../Pch.h"
#include "StarField.h"
#include "../../Manager/ModelManager.h"
#include "../../Const/StarFieldConst.h"

#include <random>

// kFixedDeltaTime は削除 → KdFPSController::GetDt() を使用

void StarField::Init()
{
	m_drawType = eDrawTypeBright;

	auto modelData = ModelManager::Instance().GetModel(StarFieldConst::StarModelPath);
	m_modelWork.SetModelData(modelData);

	// 固定シードで毎回同じ星空を生成
	std::mt19937 engine(StarFieldConst::RandomSeed);
	std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
	std::uniform_real_distribution<float> distAngle(0.0f, DirectX::XM_2PI);

	m_stars.reserve(StarFieldConst::StarNum);

	// 生成時のカメラ位置を星空の原点とする
	m_originCamPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	for (int i = 0; i < StarFieldConst::StarNum; ++i)
	{
		Star star;

		// 球面上に一様分布する方向を生成
		const float z     = dist01(engine) * 2.0f - 1.0f;          // -1〜1
		const float theta = distAngle(engine);                      // 0〜2π
		const float r     = std::sqrtf(std::max(0.0f, 1.0f - z * z));
		const Math::Vector3 dir = { r * std::cosf(theta), r * std::sinf(theta), z };

		// 距離をランダムに決める
		const float distRate = dist01(engine); // 0=最も近い, 1=最も遠い
		const float distance = StarFieldConst::StarRadiusMin +
			distRate * (StarFieldConst::StarRadiusMax - StarFieldConst::StarRadiusMin);

		// 基準ワールド座標（生成時カメラ基準の球殻上）
		star.BasePos = m_originCamPos + dir * distance;

		// スケールは距離に連動：近い星ほど小さく、遠い星ほど大きく
		star.Scale = StarFieldConst::StarScaleMin +
			distRate * (StarFieldConst::StarScaleMax - StarFieldConst::StarScaleMin);

		// パララックス：近い星ほど追従が弱く（よく動く）、遠い星ほど追従が強い
		star.Parallax = StarFieldConst::NearParallax +
			distRate * (StarFieldConst::FarParallax - StarFieldConst::NearParallax);

		star.Emissive = StarFieldConst::StarEmissiveMin +
			dist01(engine) * (StarFieldConst::StarEmissiveMax - StarFieldConst::StarEmissiveMin);

		// 色：最低値〜1.0 の範囲でランダム（白〜青白〜淡黄）
		star.Color = {
			StarFieldConst::StarColorMinR + dist01(engine) * (1.0f - StarFieldConst::StarColorMinR),
			StarFieldConst::StarColorMinG + dist01(engine) * (1.0f - StarFieldConst::StarColorMinG),
			StarFieldConst::StarColorMinB + dist01(engine) * (1.0f - StarFieldConst::StarColorMinB),
		};

		// 明滅の初期位相をばらけさせる
		star.TwinklePhase = distAngle(engine);

		m_stars.push_back(star);
	}
}

void StarField::Update()
{
	m_time += KdFPSController::GetDt();
}

void StarField::DrawBright()
{
	if (!m_modelWork.IsEnable()) { return; }

	auto& shaderMgr = KdShaderManager::Instance();

	// 星は実ワールド座標を持つため、手前の足場などには隠れてほしい。
	// ZWriteDisable: 手前の物には隠れる○ / 深度書き込みはしない（背景扱い）
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	shaderMgr.ChangeRasterizerState(KdRasterizerState::CullNone);

	const Math::Vector3& camPos = shaderMgr.GetCameraCB().CamPos;

	// 生成時カメラからの移動量（パララックスの基準）
	const Math::Vector3 camDelta = camPos - m_originCamPos;

	for (const auto& star : m_stars)
	{
		// 明滅（ゆっくりした輝度の揺らぎ）
		const float twinkle = 1.0f + StarFieldConst::TwinkleStrength *
			std::sinf(m_time * StarFieldConst::TwinkleSpeed + star.TwinklePhase);

		// 多重スクロール：カメラ移動量にパララックス係数を掛けて追従させる。
		//   Parallax=1.0 → 完全追従（動いて見えない）
		//   Parallax<1.0 → カメラより遅れて動く＝近い星ほど大きく流れる
		const Math::Vector3 pos = star.BasePos + camDelta * star.Parallax;

		const Math::Matrix world =
			Math::Matrix::CreateScale(star.Scale) *
			Math::Matrix::CreateTranslation(pos);

		// 発光色（ブルームで光らせる）
		const Math::Vector3 emissive = star.Color * (star.Emissive * twinkle);

		shaderMgr.m_StandardShader.DrawModel(m_modelWork, world, kWhiteColor, emissive);
	}

	shaderMgr.UndoRasterizerState();
	shaderMgr.UndoDepthStencilState();
}

