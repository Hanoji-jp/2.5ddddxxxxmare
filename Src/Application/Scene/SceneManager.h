#pragma once

class BaseScene;

class SceneManager
{
public :

	// シーン情報
	enum class SceneType
	{
		Title,
		Story,
		StageSelect,
		Game,
		Editor,
	};

	void PreUpdate();
	void Update();
	void PostUpdate();

	void PreDraw();
	void Draw();
	void DrawSprite();
	void DrawDebug();

	// 次のシーンをセット (次のフレームから切り替わる)
	void SetNextScene(SceneType _nextScene)
	{
		m_nextSceneType = _nextScene;
	}

	// 現在と同じシーンを最初から再読込する（もういちど/やりなおす用）。
	// SetNextScene(同一型) だけだと「型が同じ＝切替なし」で何も起きないため専用に用意。
	void RestartScene()
	{
		m_nextSceneType = m_currentSceneType;
		m_forceReload   = true;
	}

	// シーン切替直後の入力ロック中か（前シーンの押しっぱなし入力が
	// 新シーンで即発火する“連打/持ち越し”を防ぐため、各UIの決定入力はこれを見る）
	// ・一定フレーム経過 かつ 決定キー(Enter/Space/Tab)を一度離す までロック継続。
	bool IsInputLocked() const { return m_inputLockFrames > 0 || !m_confirmReleased; }

	// 現在のシーンのオブジェクトリストを取得
	const std::list<std::shared_ptr<KdGameObject>>& GetObjList();

	// 現在のシーンにオブジェクトを追加
	void AddObject(const std::shared_ptr<KdGameObject>& _obj);

private :

	// マネージャーの初期化
	// インスタンス生成(アプリ起動)時にコンストラクタで自動実行
	void Init()
	{
		// 開始シーンに切り替え
		ChangeScene(m_currentSceneType);
	}

	// シーン切り替え関数
	void ChangeScene(SceneType _sceneType);

	// 現在のシーンのインスタンスを保持しているポインタ
	std::shared_ptr<BaseScene> m_currentScene = nullptr;

	// 現在のシーンの種類を保持している変数
	SceneType m_currentSceneType = SceneType::Title;
	
	// 次のシーンの種類を保持している変数
	SceneType m_nextSceneType = m_currentSceneType;

	// 同一シーンの強制再読込フラグ（RestartScene 用）
	bool m_forceReload = false;

	// シーン切替直後に入力を無視するフレーム数（持ち越し/連打防止）
	static constexpr int kInputLockFrames = 12;   // 約0.2秒(60fps)
	int  m_inputLockFrames  = kInputLockFrames;
	bool m_confirmReleased  = false;   // 切替後に決定キーが一度離されたか

private:

	SceneManager() { Init(); }
	~SceneManager() {}

public:

	// シングルトンパターン
	// 常に存在する && 必ず1つしか存在しない(1つしか存在出来ない)
	// どこからでもアクセスが可能で便利だが
	// 何でもかんでもシングルトンという思考はNG
	static SceneManager& Instance()
	{
		static SceneManager instance;
		return instance;
	}
};
