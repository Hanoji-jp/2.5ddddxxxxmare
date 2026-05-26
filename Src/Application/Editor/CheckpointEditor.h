#pragma once
#include "../Const/CheckpointConst.h"

//==========================================================
// CheckpointEditor
// ImGui でチェックポイントの位置を配置・編集・保存・読込する
// DrawDebugSpheres() でゴースト表示する
//==========================================================
class CheckpointEditor
{
public:
	CheckpointEditor()  { Load(); }
	~CheckpointEditor() {}

	void DrawGui();

	// エディターモード時にワイヤーフレームスフィアを描画
	void DrawDebugSpheres() const;

	void Save() const;
	void Load();

	const std::vector<Math::Vector3>& GetPositions() const { return m_positions; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

	static constexpr const char* SavePath = "Asset/Data/checkpoints.csv";

private:
	std::vector<Math::Vector3> m_positions;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
