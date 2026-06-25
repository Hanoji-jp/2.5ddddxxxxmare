#pragma once
#include "../../Framework/Shader/KdRenderTargetChange.h"

//==========================================================
// SettingsMenu
// ポーズ／タイトルから開く共通の設定ウィンドウ。
//  ・W/S で項目選択、A/D で変更、TAB または「もどる」で閉じる。
//  ・音量と全画面/窓はその場で反映。解像度は保存（次回起動で反映）。
//==========================================================
class SettingsMenu
{
public:
	SettingsMenu() = default;

	void Open();                 // 開く（SettingsManager の現在値を表示）
	bool IsOpen() const { return m_open; }

	// 毎フレーム呼ぶ（開いている間だけ入力を処理）。閉じたら IsOpen() が false に。
	void Update();

	// パネル描画（スプライトパス内で呼ぶ）
	void Draw();

private:
	void Close();                // 保存して閉じる
	void EnsureFont();           // フォント登録（初回のみ）

	bool  m_open       = false;
	int   m_row        = 0;
	float m_fade       = 0.0f;
	float m_blinkTimer = 0.0f;
	bool  m_fontReady  = false;

	// 背景ぼかし用
	KdRenderTargetPack m_blurRT;
	bool               m_blurInit = false;

	// 入力エッジ用
	bool m_prevUp = false, m_prevDown = false, m_prevLeft = false, m_prevRight = false;
	bool m_prevDecide = false, m_prevBack = false;
};
