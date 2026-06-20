#pragma once
#include "../Const/SpikeBoxConst.h"
#include <vector>
#include <string>

//==========================================================
// SpikeBoxData
// 棘ボックス1つ分の配置データ
//==========================================================
struct SpikeBoxData
{
	Math::Vector3 center = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 size   = { SpikeBoxConst::DefaultSizeX,
							 SpikeBoxConst::DefaultSizeY,
							 SpikeBoxConst::DefaultSizeZ };
	bool          enabled = true;
};

//==========================================================
// SpikeBoxEditor
// ImGui で棘ボックスの位置・サイズを編集・保存・読込する（WindBox と同じ仕様）
//==========================================================
class SpikeBoxEditor
{
public:
	SpikeBoxEditor()  { Load(); }
	~SpikeBoxEditor() {}

	void DrawGui();
	void DrawDebug() const;

	void Save() const;
	void Load();

	const std::vector<SpikeBoxData>& GetBoxes() const { return m_boxes; }

	// エディタ操作（マウスピッキング／生成／コピー）用のアクセサ
	std::vector<SpikeBoxData>& WorkBoxes()       { return m_boxes; }
	void MarkDirty()                             { m_dirty = true; }
	void SetSelected(int _i)                     { m_selectedIndex = _i; }
	int  GetSelected() const                     { return m_selectedIndex; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

private:
	std::vector<SpikeBoxData> m_boxes;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
