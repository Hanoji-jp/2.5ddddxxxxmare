#pragma once
#include "../../Const/UIConst.h"
#include "../../Const/JuiceConst.h"

//==========================================================
// HpUI
// プレイヤーのHPをImGuiでハート（●）表示する
//==========================================================
class HpUI : public KdGameObject
{
public:
	HpUI()          {}
	virtual ~HpUI() {}

	void SetPlayer(const std::weak_ptr<KdGameObject>& _wp) { m_wpPlayer = _wp; }

	// ダメージ時にシェイクを開始する
	void TriggerShake() { m_shakeTimer = JuiceConst::HpShakeDuration; }

	// GameScene::DrawGui() から呼ぶ（ImGuiフレーム内限定）
	void DrawGui();

	bool IsVisible() const override { return false; }

private:
	std::weak_ptr<KdGameObject> m_wpPlayer;

	// HP ゲージヒットシェイク
	float m_shakeTimer = 0.0f;  // 残り時間
};
