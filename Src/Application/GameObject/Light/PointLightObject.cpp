#include "../../../Pch.h"
#include "PointLightObject.h"

void PointLightObject::Init()
{
	// 点光源はモデルを持たないため描画タイプなし
	m_drawType = 0;
}

void PointLightObject::Update()
{
	// 毎フレーム AmbientController に登録（Update後にクリアされるため毎回必要）
	KdShaderManager::Instance().WorkAmbientController().AddPointLight(
		m_color,
		m_radius,
		m_mWorld.Translation()
	);
}

void PointLightObject::DrawDebug()
{
    const Math::Vector3 pos = m_mWorld.Translation();
    const Math::Color   col = { m_color.x, m_color.y, m_color.z, 1.0f };

    KdDebugWireFrame wire;
    wire.AddDebugSphere(pos, m_radius, col);
    wire.Draw();
}

void PointLightObject::DrawGui()
{
	const std::string label = "PointLight [" + std::to_string(reinterpret_cast<uintptr_t>(this)) + "]";
	if (ImGui::TreeNode(label.c_str()))
	{
		float pos[3] = { m_mWorld.Translation().x, m_mWorld.Translation().y, m_mWorld.Translation().z };
		if (ImGui::DragFloat3("Position", pos, 0.1f))
		{
			m_mWorld.Translation({ pos[0], pos[1], pos[2] });
		}

		float col[3] = { m_color.x, m_color.y, m_color.z };
		if (ImGui::ColorEdit3("Color", col))
		{
			m_color = { col[0], col[1], col[2] };
		}

		ImGui::DragFloat("Radius", &m_radius, 0.5f, 0.0f, 500.0f);

		ImGui::TreePop();
	}
}
