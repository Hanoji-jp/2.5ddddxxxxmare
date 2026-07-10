#include "Updater.h"
#include "../main.h"
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <fstream>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")

Updater::~Updater()
{
    // 終了時：ダウンロード中なら中断フラグを立て、スレッドを切り離す
    m_cancelFlag = true;
    try
    {
        if (m_thread.joinable())
            m_thread.detach();
    }
    catch (...) {}
}

// ──────────────────────────────────────────────────────────────
// 公開 API
// ──────────────────────────────────────────────────────────────

void Updater::StartCheckVersion()
{
    if (IsWorking()) return;
    m_state    = UpdaterState::Checking;
    m_errorMsg = "";
    // スレッドの join／生成失敗（std::system_error）でアプリを巻き込まない
    try
    {
        if (m_thread.joinable()) m_thread.join();
        m_thread = std::thread(&Updater::CheckVersionThread, this);
    }
    catch (...)
    {
        m_state    = UpdaterState::Error;
        m_errorMsg = "Failed to start update check.";
    }
}

void Updater::StartDownload()
{
    if (IsWorking()) return;
    m_state    = UpdaterState::Downloading;
    m_progress = 0.0f;
    m_errorMsg = "";
    try
    {
        if (m_thread.joinable()) m_thread.join();
        m_thread = std::thread(&Updater::DownloadThread, this);
    }
    catch (...)
    {
        m_state    = UpdaterState::Error;
        m_errorMsg = "Failed to start download.";
    }
}

void Updater::ApplyUpdate()
{
    // update.bat を生成して起動 → ゲーム終了
    // バッチは「ゲームPID終了待ち → zip解凍 → ゲーム再起動」を行う
    DWORD pid = GetCurrentProcessId();

    // bat の内容を生成
    // PowerShell の Expand-Archive で zip を展開し上書き
    std::ofstream bat("update.bat");
    if (!bat.is_open()) return;

    bat <<
        "@echo off\n"
        "echo Waiting for game to exit...\n"
        ":wait\n"
        "tasklist /FI \"PID eq " << pid << "\" 2>NUL | find /I \"" << pid << "\" >NUL\n"
        "if not errorlevel 1 (\n"
        "    timeout /t 1 /nobreak >NUL\n"
        "    goto wait\n"
        ")\n"
        "echo Extracting update...\n"
        "powershell -NoProfile -Command \""
            "Expand-Archive -Path 'update_tmp.zip' -DestinationPath '.' -Force\"\n"
        "echo Cleaning up...\n"
        "del update_tmp.zip\n"
        "echo Update complete! Starting game...\n"
        "start Project.exe\n"
        "del \"%~f0\"\n";   // bat 自己削除
    bat.close();

    // バッチを最小化ウィンドウで起動
    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_MINIMIZE;
    PROCESS_INFORMATION pi = {};
    CreateProcessA(nullptr, (LPSTR)"cmd.exe /c update.bat",
                   nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread)  CloseHandle(pi.hThread);

    // ゲームを終了
    Application::Instance().End();
}

// ──────────────────────────────────────────────────────────────
// スレッド処理
// ──────────────────────────────────────────────────────────────

void Updater::CheckVersionThread()
{
  try
  {
    std::string body;
    // GitHub API: GET /repos/{owner}/{repo}/releases/latest
    bool ok = HttpGet("api.github.com",
                      "/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest",
                      body);
    if (!ok)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_errorMsg = "Failed to connect to GitHub API.";
        m_state    = UpdaterState::Error;
        return;
    }

    std::string version, url;
    if (!ParseLatestRelease(body, version, url))
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_errorMsg = "Failed to parse release info.";
        m_state    = UpdaterState::Error;
        return;
    }

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_latestVersion = version;
        m_downloadUrl   = url;
    }

    if (version == GetCurrentVersion())
        m_state = UpdaterState::UpToDate;
    else
        m_state = UpdaterState::UpdateAvailable;
  }
  catch (...)
  {
    // ワーカースレッドから例外を絶対に逃がさない（逃がすと std::terminate）
    m_errorMsg = "Update check failed.";
    m_state    = UpdaterState::Error;
  }
}

void Updater::CancelDownload()
{
    if (m_state == UpdaterState::Downloading)
    {
        m_cancelFlag = true;
        m_state = UpdaterState::Idle;
    }
}

void Updater::DownloadThread()
{
  try
  {
    std::string url;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        url = m_downloadUrl;
    }

    m_cancelFlag = false;
    bool ok = DownloadFile(url, UPDATE_ZIP_TMP, [this](float p){
        m_progress = p;
    });

    // キャンセルされた場合
    if (m_cancelFlag)
    {
        // 不完全なzipを削除
        ::DeleteFileA(UPDATE_ZIP_TMP);
        return;
    }

    if (!ok)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_errorMsg = "Download failed.";
        m_state    = UpdaterState::Error;
        return;
    }

    m_progress = 1.0f;
    m_state    = UpdaterState::Downloaded;
  }
  catch (...)
  {
    // ワーカースレッドから例外を絶対に逃がさない（逃がすと std::terminate）
    m_errorMsg = "Download failed.";
    m_state    = UpdaterState::Error;
  }
}

// ──────────────────────────────────────────────────────────────
// WinINet ヘルパー
// ──────────────────────────────────────────────────────────────

bool Updater::HttpGet(const std::string& host, const std::string& path, std::string& outBody)
{
    HINTERNET hNet = InternetOpenA("CoreliaUpdater/1.0",
                                   INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hNet) return false;

    HINTERNET hConn = InternetConnectA(hNet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
                                        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return false; }

    const char* accepts[] = { "*/*", nullptr };
    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path.c_str(),
                                       nullptr, nullptr, accepts,
                                       INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return false; }

    // GitHub API requires User-Agent
    HttpAddRequestHeadersA(hReq,
        "User-Agent: CoreliaUpdater\r\nAccept: application/vnd.github+json\r\n",
        (DWORD)-1, HTTP_ADDREQ_FLAG_ADD);

    if (!HttpSendRequestA(hReq, nullptr, 0, nullptr, 0))
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        return false;
    }

    char buf[4096];
    DWORD read = 0;
    while (InternetReadFile(hReq, buf, sizeof(buf) - 1, &read) && read > 0)
    {
        buf[read] = '\0';
        outBody += buf;
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return true;
}

bool Updater::DownloadFile(const std::string& url, const std::string& destPath,
                            std::function<void(float)> progressCb)
{
    // URL をホスト・パスに分解
    URL_COMPONENTSA uc = {};
    uc.dwStructSize     = sizeof(uc);
    char hostBuf[256] = {}, pathBuf[1024] = {};
    uc.lpszHostName     = hostBuf;  uc.dwHostNameLength  = sizeof(hostBuf);
    uc.lpszUrlPath      = pathBuf;  uc.dwUrlPathLength   = sizeof(pathBuf);
    if (!InternetCrackUrlA(url.c_str(), 0, 0, &uc)) return false;

    HINTERNET hNet = InternetOpenA("CoreliaUpdater/1.0",
                                   INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hNet) return false;

    HINTERNET hConn = InternetConnectA(hNet, hostBuf, uc.nPort,
                                        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hNet); return false; }

    DWORD flags = INTERNET_FLAG_RELOAD;
    if (uc.nScheme == INTERNET_SCHEME_HTTPS) flags |= INTERNET_FLAG_SECURE;

    const char* accepts[] = { "*/*", nullptr };
    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", pathBuf,
                                       nullptr, nullptr, accepts, flags, 0);
    if (!hReq) { InternetCloseHandle(hConn); InternetCloseHandle(hNet); return false; }

    HttpAddRequestHeadersA(hReq, "User-Agent: CoreliaUpdater\r\n",
                            (DWORD)-1, HTTP_ADDREQ_FLAG_ADD);

    if (!HttpSendRequestA(hReq, nullptr, 0, nullptr, 0))
    {
        // リダイレクト先を手動で取得（GitHub releases は CDN へリダイレクトする）
        char location[1024] = {};
        DWORD locLen = sizeof(location);
        DWORD statusCode = 0;
        DWORD scLen = sizeof(statusCode);
        HttpQueryInfoA(hReq, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &statusCode, &scLen, nullptr);

        if (statusCode == 302 || statusCode == 301)
        {
            HttpQueryInfoA(hReq, HTTP_QUERY_LOCATION, location, &locLen, nullptr);
            InternetCloseHandle(hReq);
            InternetCloseHandle(hConn);
            InternetCloseHandle(hNet);
            return DownloadFile(std::string(location), destPath, progressCb);
        }
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        return false;
    }

    // Content-Length 取得（進捗計算用）
    DWORD contentLength = 0;
    {
        char clBuf[32] = {};
        DWORD clLen = sizeof(clBuf);
        HttpQueryInfoA(hReq, HTTP_QUERY_CONTENT_LENGTH, clBuf, &clLen, nullptr);
        contentLength = (DWORD)atol(clBuf);
    }

    std::ofstream ofs(destPath, std::ios::binary);
    if (!ofs.is_open())
    {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hNet);
        return false;
    }

    char buf[65536];
    DWORD read = 0, totalRead = 0;
    while (InternetReadFile(hReq, buf, sizeof(buf), &read) && read > 0)
    {
        // キャンセルチェック
        if (m_cancelFlag)
        {
            ofs.close();
            InternetCloseHandle(hReq);
            InternetCloseHandle(hConn);
            InternetCloseHandle(hNet);
            return false;
        }
        ofs.write(buf, read);
        totalRead += read;
        if (contentLength > 0 && progressCb)
            progressCb((float)totalRead / (float)contentLength);
    }
    ofs.close();

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hNet);
    return totalRead > 0;
}

// ──────────────────────────────────────────────────────────────
// GitHub API JSON の簡易パース
// "tag_name": "v1.0.0"  と  assets[0].browser_download_url を取得
// ──────────────────────────────────────────────────────────────
bool Updater::ParseLatestRelease(const std::string& json,
                                  std::string& outVersion,
                                  std::string& outUrl)
{
    // tag_name
    {
        auto pos = json.find("\"tag_name\"");
        if (pos == std::string::npos) return false;
        auto q1 = json.find('"', pos + 10);
        if (q1 == std::string::npos) return false;
        auto q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) return false;
        outVersion = json.substr(q1 + 1, q2 - q1 - 1);
    }

    // browser_download_url（最初のアセット）
    {
        auto pos = json.find("\"browser_download_url\"");
        if (pos == std::string::npos) return false;
        auto q1 = json.find('"', pos + 22);
        if (q1 == std::string::npos) return false;
        auto q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) return false;
        outUrl = json.substr(q1 + 1, q2 - q1 - 1);
    }

    return !outVersion.empty() && !outUrl.empty();
}
