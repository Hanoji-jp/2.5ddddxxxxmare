#pragma once
#include "../Const/MovingFloorConst.h"
#include "../Manager/PlanetGravityManager.h"
#include <vector>
#include <string>

//==========================================================
// MovingFloorData
// 移動床1つ分の配置データ
//==========================================================
struct MovingFloorData
{
	// 移動軸
	enum class Axis { X, Y, Z };

	// 開始位置： -1=負端（Left/Bottom） 0=中心 1=正端（Right/Top）
	enum class StartPhase { NegEnd = -1, Center = 0, PosEnd = 1 };

	Math::Vector3 center = { 0.0f, 0.0f, 0.0f };
	Axis          axis   = Axis::X;
	float         range  = MovingFloorConst::DefaultRange;
	Math::Vector3 size   = { MovingFloorConst::DefaultSizeX,
											  MovingFloorConst::DefaultSizeY,
											  MovingFloorConst::DefaultSizeZ };

	// 移動速度（ワールド単位/秒）
	float         speed  = MovingFloorConst::MoveSpeed;

	// 折り返し地点での一時停止時間（秒）
	float         waitTime = MovingFloorConst::DefaultWaitTime;

	// 開始位置
	StartPhase    startPhase = StartPhase::Center;

	// Planet Box と同じ重力フェイス設定
	bool               bNormalGravity         = true;
	BoxFaceGravityMode faceTop    = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode faceBottom = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode faceLeft   = BoxFaceGravityMode::Inward;
	BoxFaceGravityMode faceRight  = BoxFaceGravityMode::Inward;
};

//==========================================================
// MovingFloorEditor
// ImGui で移動床の位置・軸・範囲・サイズを編集・保存・読込する
//==========================================================
class MovingFloorEditor
{
public:
	MovingFloorEditor()  { Load(); }
	~MovingFloorEditor() {}

	void DrawGui();
	void DrawDebug() const;

	void Save() const;
	void Load();

	const std::vector<MovingFloorData>& GetFloors() const { return m_floors; }

	// エディタ操作（マウスピッキング／生成／コピー）用のアクセサ
	std::vector<MovingFloorData>& WorkFloors()       { return m_floors; }
	void MarkDirty()                                 { m_dirty = true; }
	void SetSelected(int _i)                         { m_selectedIndex = _i; }
	int  GetSelected() const                         { return m_selectedIndex; }

	bool IsDirty()    const { return m_dirty; }
	void ClearDirty()       { m_dirty = false; }

	static constexpr const char* SavePath = "Asset/Data/moving_floors.csv";

private:
	std::vector<MovingFloorData> m_floors;
	int  m_selectedIndex = -1;
	bool m_dirty         = false;
};
