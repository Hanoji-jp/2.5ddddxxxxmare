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
