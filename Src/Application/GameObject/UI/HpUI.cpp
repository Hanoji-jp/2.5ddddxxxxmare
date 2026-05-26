#include "../../../Pch.h"
#include "HpUI.h"
#include "../Character/Character.h"

void HpUI::DrawGui()
{
	const auto spPlayer = m_wpPlayer.lock();
	if (!spPlayer) { return; }

	const auto* pChar = dynamic_cast<const Character*>(spPlayer.get());
	if (!pChar) { return; }

	const int hp    = pChar->GetHp();
	const int maxHp = 3;

	ImGui::SetNextWindowPos(ImVec2(UIConst::HpIconStartX, UIConst::HpIconStartY), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("##HpUI", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize   |
		ImGuiWindowFlags_NoMove     |
		ImGuiWindowFlags_NoInputs   |
		ImGuiWindowFlags_NoScrollbar);

	for (int i = 0; i < maxHp; ++i)
	{
		if (i > 0) { ImGui::SameLine(0.0f, UIConst::HpIconSpacing - UIConst::HpIconSize); }

		if (i < hp)
			ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "HP");
		else
			ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "HP");
	}

	ImGui::End();
}

