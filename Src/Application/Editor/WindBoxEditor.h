#pragma once
#include "../Const/WindBoxConst.h"
#include <vector>
#include <string>

//==========================================================
// WindBoxData
// 風ボックス1つ分の配置データ
//==========================================================
struct WindBoxData
{
	Math::Vector3 center   = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 size     = { WindBoxConst::DefaultSizeX,
							   WindBoxConst::DefaultSizeY,
							   WindBoxConst::DefaultSizeZ };

	// 風の向き（正規化ベクトル、エディタでは角度入力して変換）
	// windbox.gltf の風口は +Y なのでデフォルトは上向き
	Math::Vector3 windDir  = { 0.0f, 1.0f, 0.0f };

	// 風の上昇力（毎フレームの加速量、重力加速度と同スケール）
	float power            = WindBoxConst::DefaultPower;

	// 風が届く距離（COL範囲上端からさらに延長する長さ）
	float length           = WindBoxConst::DefaultLength;

	// 有効フラグ
	bool  enabled          = true;

	// 追従する移動床のindex（-1=なし）
	int   attachFloor      = -1;
};

//==========================================================
// WindBoxEditor
// ImGui で風ボックスの位置・サイズ・向き・強さを編集・保存・読込する
//==========================================================
class WindBoxEditor
{
public:
	WindBoxEditor()  { Load(); }
	~WindBoxEditor() {}

	void DrawGui();
	void DrawDebug() const;

	void Save() const;
	void Load();

	const std::vector<WindBoxData>& GetBoxes() const { return m_boxes; }

	// エディタ操作（マウスピッキング／生成／コピー）用のアクセサ
	std::vector<WindBoxData>& WorkBoxes()       { return m_boxes; }
	void MarkDirty()                            { m_dirty = true; }
	void SetSelected(int _i)                    { m_selectedIndex = _i; }
	int  GetSelected() const                    { return m_selectedIndex; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

private:
	std::vector<WindBoxData> m_boxes;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
