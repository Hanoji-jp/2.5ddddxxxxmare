#pragma once
#include "../Const/WarpHoleConst.h"
#include <vector>

//==========================================================
// WarpHoleData
// 入口・出口ペアのデータ＋中継Waypoint列
//==========================================================
struct WarpHoleData
{
	Math::Vector3              EntryPos  = { 0.0f, 0.0f, 0.0f };
	Math::Vector3              ExitPos   = { 10.0f, 0.0f, 0.0f };
	Math::Vector3              ExitDir   = { 1.0f, 0.0f, 0.0f };
	bool                       Enabled   = true;
	// 入口→出口を繋ぐ中継点（順番に通過）
	std::vector<Math::Vector3> Waypoints;

	// トンネルの口元（開口部）が向く方向。
	//   (0,0,0) = 自動（入口⇔出口の直線方向）。
	//   ここを設定すると、中継点（子）の向きに影響されず口元の傾きを固定できる。
	Math::Vector3              EntryMouthDir = { 0.0f, 0.0f, 0.0f };
	Math::Vector3              ExitMouthDir  = { 0.0f, 0.0f, 0.0f };

	// 入口口元の向きを解決（未設定なら入口→出口の直線方向）
	Math::Vector3 GetEntryMouthDir() const
	{
		if (EntryMouthDir.LengthSquared() > 1e-6f)
		{
			Math::Vector3 d = EntryMouthDir; d.Normalize(); return d;
		}
		Math::Vector3 d = ExitPos - EntryPos;
		if (d.LengthSquared() < 1e-6f) { return Math::Vector3::Up; }
		d.Normalize(); return d;
	}

	// 出口口元の向きを解決（未設定なら出口→入口の直線方向）
	Math::Vector3 GetExitMouthDir() const
	{
		if (ExitMouthDir.LengthSquared() > 1e-6f)
		{
			Math::Vector3 d = ExitMouthDir; d.Normalize(); return d;
		}
		Math::Vector3 d = EntryPos - ExitPos;
		if (d.LengthSquared() < 1e-6f) { return Math::Vector3::Up; }
		d.Normalize(); return d;
	}

	// 入口→Waypoints→出口 の全ポイント列を返す
	std::vector<Math::Vector3> GetFullPath() const
	{
		std::vector<Math::Vector3> path;
		path.push_back(EntryPos);
		for (const auto& wp : Waypoints) { path.push_back(wp); }
		path.push_back(ExitPos);
		return path;
	}
};

//==========================================================
// WarpHoleEditor
// ImGui でワープホールの入口・出口ペアを配置・編集・保存・読込する
//==========================================================
class WarpHoleEditor
{
public:
	WarpHoleEditor()  { Load(); }
	~WarpHoleEditor() {}

	void DrawGui();
	void DrawDebug() const;

	void Save() const;
	void Load();

	const std::vector<WarpHoleData>& GetHoles() const { return m_holes; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

private:
	std::vector<WarpHoleData> m_holes;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
