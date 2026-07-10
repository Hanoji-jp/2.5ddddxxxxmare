#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>

//==========================================================
// Updater
//   GitHub Releases を見て新バージョンがあれば zip を取得し、
//   update.bat 経由で展開・再起動するオートアップデーター。
//   （移植元: TankShooting。本プロジェクト用に owner/repo・終了API・
//     UI を ImGui 化して移植）
//==========================================================

// 現在バージョンを書いたファイル（exe と同じ場所）。"v1.0.0" 形式。
#define VERSION_FILE_PATH "version.txt"

// GitHub リポジトリ情報（リリース専用の公開リポジトリ。ソース本体とは別）
#define GITHUB_OWNER  "Hanoji-jp"
#define GITHUB_REPO   "Corelia"

// ダウンロード先 zip の一時ファイル名
#define UPDATE_ZIP_TMP  "update_tmp.zip"
// 展開先ディレクトリ（実行ファイルと同じ場所）
#define UPDATE_EXTRACT_DIR "."

enum class UpdaterState
{
    Idle,           // 何もしていない
    Checking,       // バージョン確認中
    UpdateAvailable,// 更新あり（ユーザーに通知）
    UpToDate,       // 最新
    Downloading,    // ダウンロード中
    Downloaded,     // ダウンロード完了（確認待ち）
    Error           // エラー
};

class Updater
{
public:
    static Updater& GetInstance()
    {
        static Updater instance;
        return instance;
    }

    // バージョンチェックを非同期で開始
    void StartCheckVersion();

    // ダウンロードを非同期で開始
    void StartDownload();

    // 確認OK後：update.batを起動してゲームを終了
    void ApplyUpdate();

    UpdaterState GetState()   const { return m_state; }
    float        GetProgress()const { return m_progress; }   // 0.0〜1.0
    std::string  GetLatestVersion() const { return m_latestVersion; }
    std::string  GetErrorMessage()  const { return m_errorMsg; }
    std::string  GetDownloadUrl()   const { return m_downloadUrl; }

    // ダウンロードをキャンセルする（Downloading中のみ有効）
    void CancelDownload();

    bool         IsWorking() const
    {
        return m_state == UpdaterState::Checking ||
               m_state == UpdaterState::Downloading;
    }

    // version.txt から現在のバージョンを読み込む（"v0.0.0" 形式）
    // ファイルがなければ "v0.0.0" を返す
    static std::string GetCurrentVersion()
    {
        std::ifstream ifs(VERSION_FILE_PATH);
        if (!ifs) return "v0.0.0";
        std::string v;
        std::getline(ifs, v);
        // 末尾の空白・改行を除去
        while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' '))
            v.pop_back();
        return v.empty() ? "v0.0.0" : v;
    }

private:
    Updater() = default;
    ~Updater();

    void CheckVersionThread();
    void DownloadThread();

    // WinINet ヘルパー
    bool HttpGet(const std::string& host, const std::string& path,
                 std::string& outBody);
    bool DownloadFile(const std::string& url, const std::string& destPath,
                      std::function<void(float)> progressCb);

    // GitHub API から latest release JSON をパースして version / download_url を取得
    bool ParseLatestRelease(const std::string& json,
                            std::string& outVersion,
                            std::string& outUrl);

    std::atomic<UpdaterState> m_state    { UpdaterState::Idle };
    std::atomic<float>        m_progress { 0.0f };
    std::atomic<bool>         m_cancelFlag { false };

    std::string m_latestVersion;
    std::string m_downloadUrl;
    std::string m_errorMsg;

    std::thread m_thread;
    mutable std::mutex m_mutex;
};

#define UPDATER Updater::GetInstance()
