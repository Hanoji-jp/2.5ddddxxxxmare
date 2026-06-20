#pragma once
#include "../Const/GameConst.h"
#include "../../Framework/Direct3D/KdModel.h"

//==========================================================
// ZoneType
// ゾーンの種別
//==========================================================
enum class ZoneType
{
	ManualGravity,  // 手動重力使用可能エリア
	NormalGravity,  // 固定方向重力エリア
};

//==========================================================
// ZoneGravityDir
// NormalGravityゾーンの重力方向
//==========================================================
enum class ZoneGravityDir
{
	Down,   // -Y（通常）
	Up,     // +Y
	Left,   // -X
	Right,  // +X
};

//==========================================================
// ManualGravityZone
// 手動重力を使用可能なエリア（矩形）
//==========================================================
struct ManualGravityZone
{
	Math::Vector3 Center = { 0.0f, 0.0f, 0.0f };
	Math::Vector3 HalfExtents = { 10.0f, 10.0f, 10.0f };
	bool bEnabled = true;
	ZoneType Type = ZoneType::ManualGravity;
	ZoneGravityDir GravityDir = ZoneGravityDir::Down;  // NormalGravity時の重力方向

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

	// プレイヤーがNormalGravityゾーン内にいるか（true時はgravDirOutに方向を返す）
	bool IsInNormalGravityZone(const Math::Vector3& _pos, Math::Vector3& gravDirOut) const;

	// エリア追加
	void AddZone(const ManualGravityZone& _zone) { m_zones.push_back(_zone); }

	// 全エリアクリア
	void ClearZones() { m_zones.clear(); }

	// エリア数取得
	size_t GetZoneCount() const { return m_zones.size(); }

	// エディタ操作用：選択中インデックスを設定
	void SetSelected(int _i) { m_selectedIndex = _i; }

	// エディタ操作用：末尾のゾーンを削除（生成のアンドゥ用）
	void RemoveLastZone() { if (!m_zones.empty()) { m_zones.pop_back(); } }

	// エリア取得
	ManualGravityZone* GetZone(int _index)
	{
		if (_index < 0 || _index >= static_cast<int>(m_zones.size())) { return nullptr; }
		return &m_zones[_index];
	}

	// 背景Boxモデルのカメラ側の面のZ座標（重力矢印をこの面に合わせるのに使う）
	float GetWallFrontZ(int _index) const;

	// デバッグ描画
	void DrawDebugShapes() const;

	// 背景Box描画。showNormalGravity=false なら NormalGravity ゾーンの箱は描かない。
	// manualGravityUp=true で ManualGravity ゾーンの箱を赤、false で青にする。
	void DrawUnLit(bool showNormalGravity = true, bool manualGravityUp = false) const;

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
