#pragma once

//==========================================================
// SoundConst.h
// BGM / SE のファイルパスと既定音量。
// 置き場所： Asset/Sound/BGM/ … ループBGM   Asset/Sound/SE/ … 効果音
// ※ファイル名は仮。実ファイルに合わせて変更可。対応形式は wav / mp3 / m4a 等。
//   ファイルが無い場合は SoundManager 側で自動スキップ（無音・assert無し）。
//==========================================================
namespace SoundConst
{
	// ── BGM（ループ）──
	constexpr const char* BgmTitle       = "Asset/Sound/BGM/When_the_flowers_are_quiet.mp3";
	constexpr const char* BgmStory       = "Asset/Sound/BGM/憧れはそのままで.mp3";
	constexpr const char* BgmStageSelect = "Asset/Sound/BGM/忍び寄る邂逅.mp3";
	constexpr const char* BgmShowcase    = "Asset/Sound/BGM/Deep_Exploration.mp3";  // ステージ紹介カメラ中

	// ステージ別BGM（stageId 0始まりで参照。全5ステージ）
	constexpr const char* BgmStage[] =
	{
		"Asset/Sound/BGM/Cloudy_City.mp3",          // ステージ1
		"Asset/Sound/BGM/REACH_FOR_the_FATE.mp3",   // ステージ2
		"Asset/Sound/BGM/Sakura_Mellows.mp3",       // ステージ3
		"Asset/Sound/BGM/Usual_Spot.mp3",           // ステージ4
		"Asset/Sound/BGM/Eggplant_Planet.mp3",      // ステージ5
	};
	constexpr int BgmStageCount = static_cast<int>(sizeof(BgmStage) / sizeof(BgmStage[0]));

	// stageId(0始まり) からステージBGMを取得（範囲外はステージ1）
	inline const char* BgmForStage(int stageId0)
	{
		if (stageId0 < 0 || stageId0 >= BgmStageCount) { stageId0 = 0; }
		return BgmStage[stageId0];
	}

	// ── SE（単発）──
	// ※既定割り当て。実行中は ImGui の「SE Editor」で差し替え可（se_assign.txt に保存）。
	constexpr const char* SeJump       = "Asset/Sound/SE/ポコッ.mp3";                       // ジャンプ
	constexpr const char* SeCoin       = "Asset/Sound/SE/システム決定音_5.mp3";             // コイン取得
	constexpr const char* SePickup     = "Asset/Sound/SE/キラキラ効果音.mp3";               // 緑石(エメラルド)取得
	constexpr const char* SeCheckpoint = "Asset/Sound/SE/ピカーッキラキラという効果音.mp3"; // チェックポイント取得
	constexpr const char* SeDamage     = "Asset/Sound/SE/SFキャンセル音1.mp3";              // 被ダメージ
	constexpr const char* SeDeath      = "Asset/Sound/SE/dead-sound.mp3";                   // 死亡
	constexpr const char* SeStomp      = "Asset/Sound/SE/ポコッ.mp3";                       // 敵踏みつけ撃破
	constexpr const char* SeCore       = "Asset/Sound/SE/エネルギー・パワーチャージ音.mp3"; // 重力コア(ゴール)取得
	constexpr const char* SeClear      = "Asset/Sound/SE/派手目なセレクト音.mp3";           // ステージクリア
	constexpr const char* SeMenuMove   = "Asset/Sound/SE/セレクト音_1.mp3";                 // メニュー移動
	constexpr const char* SeMenuDecide = "Asset/Sound/SE/システム決定音_11.mp3";           // メニュー決定

	// 追加分（既定割り当て。ImGuiで差し替え可）
	constexpr const char* SeLand          = "Asset/Sound/SE/nc133426_【効果音ラボ】ジャンプの着地.mp3"; // 着地
	constexpr const char* SeFootstep      = "Asset/Sound/SE/Walking.mp3";                   // 足音
	constexpr const char* SeStageFlyIn    = "Asset/Sound/SE/StageStartFly.mp3";             // ステージへ飛んでくる
	constexpr const char* SeMenuCancel    = "Asset/Sound/SE/キャンセル音.mp3";              // キャンセル/戻る
	constexpr const char* SePauseOpen     = "Asset/Sound/SE/タップ音.mp3";                   // ポーズ開く
	constexpr const char* SePauseClose    = "Asset/Sound/SE/SFキャンセル音1.mp3";           // ポーズ閉じる
	constexpr const char* SeTitleStart    = "Asset/Sound/SE/システム決定音_12.mp3";         // タイトル開始
	constexpr const char* SeStoryAdvance  = "Asset/Sound/SE/Book01-2(Flip).mp3";            // ストーリー送り
	constexpr const char* SeStorySkip     = "Asset/Sound/SE/キャンセル音.mp3";              // ストーリースキップ
	constexpr const char* SeStageGo       = "Asset/Sound/SE/StageStartFly.mp3";             // ステージへ入る
	constexpr const char* SeStageDeny     = "Asset/Sound/SE/SFキャンセル音1.mp3";           // 拒否
	constexpr const char* SeResultAdvance = "Asset/Sound/SE/システム決定音_6.mp3";          // リザルト送り
	constexpr const char* SeRespawn       = "Asset/Sound/SE/キラキラした音.mp3";            // 復活
	constexpr const char* SeCoreliaTalk   = "Asset/Sound/SE/システム決定音_9.mp3";          // コアリア会話
	constexpr const char* SeUiSend        = "Asset/Sound/SE/タップ音.mp3";                   // UIへ送る
	constexpr const char* SeUiAbsorb      = "Asset/Sound/SE/キラキラ効果音.mp3";             // UI吸収
	constexpr const char* SeParasolGet    = "Asset/Sound/SE/キラキラした音.mp3";            // パラソル取得

	// SE割り当ての保存先（ImGuiで変更→保存するとここへ書き出し、起動時に読み込む）
	constexpr const char* SeAssignFile = "Asset/Sound/SE/se_assign.txt";

	// ── 既定音量 ──
	constexpr float BgmVolume = 0.45f;
	constexpr float SeVolume  = 0.9f;

	// ── BGM フェード ──
	constexpr float BgmFadeSpeed = 1.6f;   // 音量フェード速度（/秒。1.6≒0.6秒で切替）

	// ── ポーズ中のダッキング（音量を下げる倍率。1.0=等倍）──
	constexpr float PauseBgmVolumeScale = 0.55f;

	// ── こもり（別ファイル切替方式）──
	// 通常曲 "Xxx.mp3" に対し、こもり版 "Xxx_muffled.mp3" をクロスフェードで切り替える。
	// （高音を絞ったBGMを用意して置くだけ。無ければ通常のまま）
	constexpr const char* MuffleSuffix = "_muffled";
}
