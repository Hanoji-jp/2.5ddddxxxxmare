#pragma once
#include "../Const/PlanetConst.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"

//==========================================================
// PlanetData
// 惑星1個分のデータ
//==========================================================
struct PlanetData
{
	Math::Vector3 Position      = { 0.0f, -20.0f, 0.0f };
	float         SurfaceRadius = PlanetConst::DefaultSurfaceRadius;
	float         GravityRadius = PlanetConst::DefaultGravityRadius;
};

//==========================================================
// PlanetGravityManager
// 複数の惑星を管理し、キャラクターへの惑星重力を提供する
// ImGui で編集し CSV で永続化する
//==========================================================
class PlanetGravityManager
{
public:
	static PlanetGravityManager& Instance()
	{
		static PlanetGravityManager s;
		return s;
	}

	void DrawGui();
	void DrawDebugSpheres() const;

	void Save() const;
	void Load();

	// キャラクター位置から最も近い影響圏内の惑星を返す（なければ nullptr）
	const PlanetData* FindNearestPlanet(const Math::Vector3& _charPos) const;

	const std::vector<PlanetData>& GetPlanets() const { return m_planets; }

private:
	PlanetGravityManager()  { Load(); }
	~PlanetGravityManager() {}
	PlanetGravityManager(const PlanetGravityManager&)            = delete;
	PlanetGravityManager& operator=(const PlanetGravityManager&) = delete;

	std::vector<PlanetData> m_planets;
	int                     m_selectedIndex = -1;
};
