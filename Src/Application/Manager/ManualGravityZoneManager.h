#pragma once
#include "../Const/GameConst.h"
#include "../../Framework/Direct3D/KdModel.h"

//==========================================================
// ManualGravityZone
// 手動重力を使用可能なエリア（矩形）
//==========================================================
struct ManualGravityZone
{
	Math::Vector3 Center = { 0.0f, 0.0f, 0.0f };  // エリア中心
	Math::Vector3 HalfExtents = { 10.0f, 10.0f, 10.0f };  // 半分のサイズ
	bool bEnabled = true;  // このゾーンが有効か

	// 描画用
	std::shared_ptr<KdModelWork> modelWork;
	Math::Matrix mWorld = Math::Matrix::Identity;

	void UpdateWorld();
};

//==========================================================
// ManualGravityZoneManager
// 手動重力エリアを管理し、プレイヤーが使用可能か判定する
//==========================================================
class ManualGravityZoneManager
{
public:
	static ManualGravityZoneManager& Instance()
	{
		static ManualGravityZoneManager s;
		return s;
	}

	// プレイヤーが手動重力を使用可能な位置にいるか
	bool CanUseManualGravity(const Math::Vector3& _pos) const;

	// エリア追加
	void AddZone(const ManualGravityZone& _zone) { m_zones.push_back(_zone); }

	// 全エリアクリア
	void ClearZones() { m_zones.clear(); }

	// エリア数取得
	size_t GetZoneCount() const { return m_zones.size(); }

	// エリア取得
	ManualGravityZone* GetZone(int _index)
	{
		if (_index < 0 || _index >= static_cast<int>(m_zones.size())) { return nullptr; }
		return &m_zones[_index];
	}

	// デバッグ描画
	void DrawDebugShapes() const;

	// 背景Box描画
	void DrawUnLit() const;

	// ImGui エディタ
	void DrawGui();

	// 保存・読み込み
	void Save() const;
	void Load();

private:
	ManualGravityZoneManager() = default;

	std::vector<ManualGravityZone> m_zones;
	int m_selectedIndex = -1;  // GUI で選択中のゾーン
};
