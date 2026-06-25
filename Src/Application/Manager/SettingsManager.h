#pragma once

//==========================================================
// SettingsManager
// ユーザー設定（音量・画面モード・解像度）を保持・保存・適用する。
//  ・音量(master/bgm/se)と全画面/窓 … その場で反映可能
//  ・解像度 … 保存して次回起動時に反映（ランタイムのリサイズは非対応）
//==========================================================
class SettingsManager
{
public:
	static SettingsManager& Instance()
	{
		static SettingsManager instance;
		return instance;
	}

	void Load();          // 起動時：ファイルから読み込み（無ければ既定値）
	void Save() const;    // ファイルへ保存

	void ApplyAudio() const;   // 音量を SoundManager へ反映
	void ApplyScreen() const;  // 全画面/窓を Direct3D へ反映（解像度は起動時のみ）
	void ApplyFps() const;     // FPS上限を Application へ反映

	int  FpsValue() const;     // 現在のFPS上限値

	bool HasSavedFile() const { return m_hasFile; }

	// 解像度（起動時のウィンドウ生成に使う）
	int  ResWidth()  const;
	int  ResHeight() const;

	// 値
	float m_master = 1.0f;       // ぜんたい音量(0..1)
	float m_bgm    = 1.0f;       // BGM音量(0..1)
	float m_se     = 1.0f;       // SE音量(0..1)
	bool  m_fullscreen = false;  // true=全画面 / false=ウィンドウ
	int   m_resIndex = 0;        // SettingsConst::Resolutions のインデックス
	int   m_fpsIndex = 1;        // SettingsConst::FpsOptions のインデックス（既定=60）
	bool  m_showFps = false;     // FPSカウンター表示

private:
	SettingsManager() = default;
	~SettingsManager() = default;
	SettingsManager(const SettingsManager&) = delete;
	SettingsManager& operator=(const SettingsManager&) = delete;

	bool m_hasFile = false;      // 保存ファイルが存在したか
};
