#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <array>
#include <functional>

class KdSoundInstance;
class KdBgmVoice;

//----------------------------------------------------------
// ゲーム内SEの論理ID。実ファイルは SoundManager の差し替え可能テーブルが持つ
// （ImGuiの「SE Editor」でホットリロード）。
//----------------------------------------------------------
enum class SeId
{
	// ── ゲームプレイ ──
	Jump, Coin, Pickup, Checkpoint, Damage, Death, Stomp, Core, Clear, MenuMove, MenuDecide,
	// ── プレイヤー操作 ──
	Land,          // 着地
	Footstep,      // 足音（歩行ループ）
	StageFlyIn,    // ステージへ飛んでくる（イントロのフライイン）
	// ── 追加：メニュー/UI共通 ──
	MenuCancel,    // キャンセル/戻る(TAB)
	PauseOpen,     // ポーズを開く
	PauseClose,    // ポーズを閉じる
	// ── 追加：シーン遷移・各画面 ──
	TitleStart,    // タイトル開始(Enter)
	StoryAdvance,  // ストーリー送り
	StorySkip,     // ストーリースキップ(TAB)
	StageGo,       // ステージへ入る(GO)
	StageDeny,     // 未解放ノードへ拒否
	ResultAdvance, // リザルトのカード送り
	// ── 追加：その他イベント ──
	Respawn,       // 復活
	CoreliaTalk,   // コアリアに話しかける
	// ── 追加：リザルトのUIホーミング ──
	UiSend,        // 取得アイテムをUIへ送り出す音
	UiAbsorb,      // UIが取得アイテムを吸収する音（カウント加算）
	ParasolGet,    // パラソル取得
	MaxNum,
};

//==========================================================
// SoundManager
// アプリ側の音声窓口。KdAudioManager を薄くラップする。
//  ・SE     : PlaySE / PlaySE3D（単発）
//  ・BGM    : PlayBGM（ループ・1曲のみ。曲が変わると自動でクロスフェード）/ StopBGM
//  ・フェード: 音量を毎フレーム補間（Update を呼ぶ必要あり）
//  ・こもり : SetBgmMuffle で「Xxx_muffled」版へクロスフェード（戻すと通常へ）
//  ・ファイルが無い場合は自動スキップ（無音・assert回避）。
//==========================================================
class SoundManager
{
public:
	static SoundManager& Instance()
	{
		static SoundManager instance;
		return instance;
	}

	// 毎フレーム呼ぶ（フェードの補間）
	void Update(float dt);

	// 事前ロード（デコードしてキャッシュ）。再生時のカクつき防止に Init 等で呼ぶ。
	// ファイルが無ければ何もしない。
	void Preload(std::string_view path);

	// 効果音（論理ID指定。テーブルで差し替え可能）
	void PlaySE(SeId id, float vol = 1.0f);
	void PlaySE3D(SeId id, const Math::Vector3& pos, float vol = 1.0f);
	// 効果音（パス直指定。汎用）
	void PlaySE(std::string_view path, float vol = 1.0f);
	void PlaySE3D(std::string_view path, const Math::Vector3& pos, float vol = 1.0f);

	// 登録SEを全部プリロード（カクつき防止。Init等で呼ぶ）
	void PreloadAllSE();

	// 全BGM（通常版＋こもり版）を事前ロード。起動時に一度呼ぶ（着地時ロードを無くす）。
	void PreloadAllBgm();

	// 全音声(BGM＋SE)を1つずつ事前ロードし、各段階で進捗(0..1)を通知（ロード画面用）。
	void PreloadAllWithProgress(const std::function<void(float)>& onProgress);

	// 再生中のSEをすべて一時停止／再開（BGMは対象外。ポーズ用）
	void PauseAllSE();
	void ResumeAllSE();

	// ImGui の SE 編集パネル（ファイル差し替え＝ホットリロード／試聴／保存）
	void DrawSeEditorGui();

	// SE割り当ての保存／読み込み
	void SaveSeAssign();
	void LoadSeAssign();

	// BGM（ループ）。同じ曲が流れていれば何もしない。曲が変わるとクロスフェード。
	void PlayBGM(std::string_view path, float vol = 1.0f);

	// BGM 停止（フェードアウト）
	void StopBGM();

	// BGM をこもらせる/戻す（"_muffled" 版へクロスフェード切替）
	void SetBgmMuffle(bool on);

	// BGM の音量倍率（ダッキング）。1.0=等倍、0.45などで下げる。ポーズ中などに使う。
	void SetBgmVolumeScale(float scale);

	// ── ユーザー設定の音量（0..1）。設定画面から反映する ──
	void  SetMasterVolume(float v) { m_masterVol  = v; }
	void  SetBgmUserVolume(float v){ m_bgmUserVol = v; }
	void  SetSeUserVolume(float v) { m_seUserVol  = v; }
	float GetMasterVolume()  const { return m_masterVol;  }
	float GetBgmUserVolume() const { return m_bgmUserVol; }
	float GetSeUserVolume()  const { return m_seUserVol;  }

private:
	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	static bool        Exists(std::string_view path);
	static std::string MuffledOf(std::string_view basePath);   // "Xxx.ext" → "Xxx_muffled.ext"

	void TrackSe(const std::shared_ptr<KdSoundInstance>& inst);   // 再生中SEを追跡リストへ
	void InitSeTable();                       // SeId 表を既定値＋保存ファイルで初期化（初回のみ）
	void RefreshSeFileList();                 // SEフォルダのファイル一覧を取得（ImGui用）
	const std::string& SePath(SeId id);       // 現在割り当てられたファイルパス

	// SEの差し替え可能テーブル
	struct SeSlot
	{
		std::string label;   // 表示名（UTF-8。ImGui用）
		std::string path;    // 実ファイルパス（CP932ネイティブ。filesystem/再生はこれ）
	};
	std::array<SeSlot, static_cast<size_t>(SeId::MaxNum)> m_seSlots;
	bool m_seInit = false;

	bool m_seEditorVisible = false;   // SE EditorのImGui表示（F5でトグル・既定は非表示）
	bool m_f5Prev          = false;   // F5キーのエッジ検出

	std::vector<std::string> m_seFiles;       // SEフォルダ内ファイル（CP932パス。ImGuiの選択肢）
	std::vector<std::string> m_seFilesU8;     // 同・UTF-8表示名（ImGui描画用）

	// 再生中SEの追跡（ポーズで一時停止/再開するため。BGMは含めない）
	std::vector<std::weak_ptr<KdSoundInstance>> m_activeSe;

	// 実際に1曲を（クロスフェードで）鳴らす。m_bgmBasePath は変えない。
	void StartTrack(std::string_view path);

	std::shared_ptr<KdBgmVoice> m_bgm;       // 現在のBGM（自前ボイス。こもりはローパスで実現）
	std::shared_ptr<KdBgmVoice> m_bgmOld;    // 切替前のBGM（フェードアウト中）

	std::string m_bgmPath;      // 実際に再生中のファイル（通常 or こもり）
	std::string m_bgmBasePath;  // 論理的な曲（こもり解除で戻る先）
	bool        m_muffled = false;

	float m_bgmVol    = 1.0f;    // PlayBGM 指定の基準音量
	float m_duck      = 1.0f;    // 音量倍率（ダッキング。ポーズ中などで下げる）
	float m_masterVol = 1.0f;    // 設定：マスター音量(0..1)
	float m_bgmUserVol= 1.0f;    // 設定：BGM音量(0..1)
	float m_seUserVol = 1.0f;    // 設定：SE音量(0..1)
	float m_curVol    = 0.0f;    // m_bgm の現在音量
	float m_targetVol = 0.0f;    // 旧：未使用（互換のため残置）
	float m_oldVol    = 0.0f;    // m_bgmOld の現在音量
};
