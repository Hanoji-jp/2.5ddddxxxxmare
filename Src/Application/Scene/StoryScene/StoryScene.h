#pragma once

#include "../BaseScene/BaseScene.h"

//==========================================================
// StoryScene
// ゲーム開始時（タイトルでスタート後）に主人公の道のりを描いた
// ストーリーページを1枚ずつ表示する。Enter/Spaceでめくり、
// 最後まで見るか ESC でスキップするとステージセレクトへ進む。
// めくりは2Dの短冊分割による柔らかいカール（高画質維持）。
//==========================================================
class StoryScene : public BaseScene
{
public:

	StoryScene()  { Init(); }
	~StoryScene() {}

private:

	void Event() override;
	void Init()  override;

	void DrawSpriteExtra() override;   // ページ画像（2Dソフトカール）＋プロンプト

	// 16:9 維持の表示矩形サイズ（contain＝レターボックス）を求める
	void CalcPageSize(int& outW, int& outH) const;

	// ページ遷移フェーズ
	enum class Phase { In, Hold, Out };

	std::vector<std::shared_ptr<KdTexture>> m_pages;

	int   m_page        = 0;
	Phase m_phase       = Phase::In;
	float m_fade        = 0.0f;     // 1ページ目フェードイン用 0→1
	float m_turn        = 0.0f;     // めくり進行 0→1
	float m_timer       = 0.0f;
	bool  m_advancePrev = false;    // Enter/Space のエッジ検出
	bool  m_skipPrev    = false;    // ESC のエッジ検出
};
