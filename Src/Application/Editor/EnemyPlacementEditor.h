#pragma once
#include "EnemyPlacementData.h"

//==========================================================
// EnemyPlacementEditor
// ImGui で敵の種類・位置を配置・編集・保存・読込する
// DrawDebugSpheres() でゴーストスフィアを描画する
//==========================================================
class EnemyPlacementEditor
{
public:
	EnemyPlacementEditor()  { Load(); }
	~EnemyPlacementEditor() {}

	void DrawGui();

	// エディターモード時にワイヤーフレームスフィアを描画
	void DrawDebugSpheres() const;

	void Save() const;
	void Load();

	const std::vector<EnemyPlacementData>& GetPlacements() const { return m_placements; }

	bool IsDirty()   const { return m_dirty; }
	void ClearDirty()      { m_dirty = false; }

	static constexpr const char* SavePath = "Asset/Data/enemy_placements.csv";

private:
	std::vector<EnemyPlacementData> m_placements;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
