#include "../main.h"
#include "SettingsManager.h"
#include "SoundManager.h"
#include "../Const/SettingsConst.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void SettingsManager::Load()
{
	std::ifstream ifs(SettingsConst::FilePath);
	if (!ifs) { m_hasFile = false; return; }
	m_hasFile = true;

	std::string line;
	while (std::getline(ifs, line))
	{
		if (line.empty()) { continue; }
		const size_t comma = line.find(',');
		if (comma == std::string::npos) { continue; }
		const std::string key = line.substr(0, comma);
		const std::string val = line.substr(comma + 1);

		if      (key == "master")     { m_master     = std::stof(val); }
		else if (key == "bgm")        { m_bgm        = std::stof(val); }
		else if (key == "se")         { m_se         = std::stof(val); }
		else if (key == "fullscreen") { m_fullscreen = (std::stoi(val) != 0); }
		else if (key == "resIndex")   { m_resIndex   = std::stoi(val); }
		else if (key == "fpsIndex")   { m_fpsIndex   = std::stoi(val); }
		else if (key == "showFps")    { m_showFps    = (std::stoi(val) != 0); }
	}

	// クランプ
	m_master = std::clamp(m_master, 0.0f, 1.0f);
	m_bgm    = std::clamp(m_bgm,    0.0f, 1.0f);
	m_se     = std::clamp(m_se,     0.0f, 1.0f);
	if (m_resIndex < 0 || m_resIndex >= SettingsConst::ResolutionCount) { m_resIndex = 0; }
	if (m_fpsIndex < 0 || m_fpsIndex >= SettingsConst::FpsCount)        { m_fpsIndex = 1; }
}

void SettingsManager::Save() const
{
	std::ofstream ofs(SettingsConst::FilePath);
	if (!ofs) { return; }
	ofs << "master,"     << m_master << "\n";
	ofs << "bgm,"        << m_bgm    << "\n";
	ofs << "se,"         << m_se     << "\n";
	ofs << "fullscreen," << (m_fullscreen ? 1 : 0) << "\n";
	ofs << "resIndex,"   << m_resIndex << "\n";
	ofs << "fpsIndex,"   << m_fpsIndex << "\n";
	ofs << "showFps,"    << (m_showFps ? 1 : 0) << "\n";
}

void SettingsManager::ApplyAudio() const
{
	auto& s = SoundManager::Instance();
	s.SetMasterVolume(m_master);
	s.SetBgmUserVolume(m_bgm);
	s.SetSeUserVolume(m_se);
}

void SettingsManager::ApplyScreen() const
{
	// ボーダレス全画面方式：解像度（画面モード）は変えず、枠なしウィンドウで画面いっぱいにする。
	// （DXGI専有フルスクリーンは解像度を強制変更してしまうため使わない）
	HWND hwnd = Application::Instance().GetWindowHandle();
	if (!hwnd) { return; }

	if (m_fullscreen)
	{
		// 枠なしにして、ウィンドウのあるモニタいっぱいへ広げる
		HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi{}; mi.cbSize = sizeof(mi);
		GetMonitorInfo(mon, &mi);
		const int mx = mi.rcMonitor.left;
		const int my = mi.rcMonitor.top;
		const int mw = mi.rcMonitor.right  - mi.rcMonitor.left;
		const int mh = mi.rcMonitor.bottom - mi.rcMonitor.top;

		SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(hwnd, HWND_TOP, mx, my, mw, mh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
	else
	{
		// 枠あり（タイトルバー）に戻し、レンダー解像度サイズのウィンドウにする
		const LONG_PTR style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME);
		SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_VISIBLE);

		RECT rc{ 0, 0, ResWidth(), ResHeight() };
		AdjustWindowRect(&rc, static_cast<DWORD>(style), FALSE);
		SetWindowPos(hwnd, HWND_TOP, 80, 60,
			rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}
}

int SettingsManager::FpsValue() const
{
	const int i = (m_fpsIndex >= 0 && m_fpsIndex < SettingsConst::FpsCount) ? m_fpsIndex : 1;
	return SettingsConst::FpsOptions[i].fps;
}

void SettingsManager::ApplyFps() const
{
	Application::Instance().SetMaxFps(FpsValue());
}

int SettingsManager::ResWidth() const
{
	const int i = (m_resIndex >= 0 && m_resIndex < SettingsConst::ResolutionCount) ? m_resIndex : 0;
	return SettingsConst::Resolutions[i].w;
}

int SettingsManager::ResHeight() const
{
	const int i = (m_resIndex >= 0 && m_resIndex < SettingsConst::ResolutionCount) ? m_resIndex : 0;
	return SettingsConst::Resolutions[i].h;
}
