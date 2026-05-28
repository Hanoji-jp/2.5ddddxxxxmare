#pragma once
#include "../Const/WarpHoleConst.h"

//==========================================================
// WarpHoleData
// 入口・出口ペアのデータ
//==========================================================
struct WarpHoleData
{
	Math::Vector3 EntryPos  = { 0.0f, 0.0f, 0.0f };  // 入口
	Math::Vector3 ExitPos   = { 10.0f, 0.0f, 0.0f }; // 出口
	Math::Vector3 ExitDir   = { 1.0f, 0.0f, 0.0f };  // 射出方向（正規化）
	bool          Enabled   = true;
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
