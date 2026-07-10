#include "../main.h"
#include "SettingsManager.h"
#include "SoundManager.h"
#include "SaveData.h"
#include "../Const/SettingsConst.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void SettingsManager::Load()
{
	// 統合バイナリ save.dat から読み込む（設定・合計・記録をまとめて）
	SaveData::Load();
	m_hasFile = SaveData::Exists();
}

void SettingsManager::Save() const
{
	// 統合バイナリ save.dat へ保存（設定・合計・記録をまとめて）
	SaveData::Save();
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
