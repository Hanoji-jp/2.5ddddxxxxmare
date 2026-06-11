#pragma once
#include "../Const/GravityCoreConst.h"
#include <vector>
#include <string>

//==========================================================
// GravityCoreData
// 重力コア 1 個の配置データ
//==========================================================
struct GravityCoreData
{
	Math::Vector3 pos     = { 0.0f, 0.0f, 0.0f };
	float         radius  = GravityCoreConst::DefaultRadius;
	CoreType      type    = CoreType::Rock;
	bool          enabled = true;
};

//==========================================================
// GravityCoreEditor
// ImGui で重力コアの配置・半径を編集・保存・読込する
//==========================================================
class GravityCoreEditor
{
public:
	GravityCoreEditor()  { Load(); }
	~GravityCoreEditor() {}

	void DrawGui();
	void DrawDebug() const;

	void Save() const;
	void Load();

	const std::vector<GravityCoreData>& GetCores() const { return m_cores; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

private:
	std::vector<GravityCoreData> m_cores;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
