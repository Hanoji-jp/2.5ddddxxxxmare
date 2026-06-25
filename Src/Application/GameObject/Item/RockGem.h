#pragma once
#include "../../../Framework/GameObject/KdGameObject.h"
#include "../../Const/RockGemConst.h"

//==========================================================
// RockGem
// コインエディタで配置する「カラフル岩（マリギャラのスターピース風）」。
//  - 形状は RockDrop（緑エメラルド）と同じローポリ岩を流用するが、
//    インスタンスごとにレインボー色で着色する。
//  - 配置アイテムなので Coin と同様に Collect（取得しても配置データは残す）。
//  - 取得すると右上カウンターに加算されるだけ（回復はしない）。
//==========================================================
class RockGem : public KdGameObject
{
public:
	RockGem()           = default;
	~RockGem() override = default;

	RockGem(const RockGem&)            = delete;
	RockGem& operator=(const RockGem&) = delete;

	void Init()       override;    // コライダー登録＋配置位置から色を決定
	void Update()     override;    // その場でふわふわ＋自転
	void DrawEffect() override;    // 加算でローポリ岩石をレインボー着色して描画

	bool IsVisible() const override { return true; }

	void SetSpawnPos(const Math::Vector3& pos) { m_spawnPos = pos; SetPos(pos); }
	const Math::Vector3& GetSpawnPos() const   { return m_spawnPos; }

	// 削除（エディタで配置を消す）：保存対象から外れ、リストからも除去される
	void Expire()                   { m_expired = true; }
	bool IsExpired() const override { return m_expired; }

	// 取得（プレイ中に拾う）：この場では隠すが配置データは残す（保存される）
	void Collect()           { m_collected = true; }
	bool IsCollected() const { return m_collected; }
	void ResetCollected()    { m_collected = false; m_basePos = m_spawnPos; m_vel = Math::Vector3::Zero; }   // ステージやり直しで未取得に戻す

	// 取得演出やバーストに使う、このジェムの代表色
	const Math::Color& GetColor() const { return m_color; }

	// カーソル磁石：lerp 率で target へ吸い寄せる（位置を直接移動）
	void PullTo(const Math::Vector3& target, float lerp)
	{
		m_basePos += (target - m_basePos) * lerp;
		m_vel = Math::Vector3::Zero;
	}
	// カーソル磁石：start 地点（カメラ付近）から dir 方向へ speed の初速で飛ばす
	void FlingFrom(const Math::Vector3& start, const Math::Vector3& dir, float speed)
	{
		m_basePos = start;
		m_vel     = dir * speed;
	}

	// 撃ち出した投擲物として寿命を設定（>=0 で有効。0以下になったら消滅）
	void SetLife(float sec) { m_life = sec; }

private:
	// 全インスタンス共有のローポリ岩石メッシュ（半径1ユニット・グレースケール）
	static const std::vector<KdPolygon::Vertex>& TriVerts();
	static const std::vector<KdPolygon::Vertex>& WireVerts();

	Math::Vector3 m_spawnPos  = { 0.0f, 0.0f, 0.0f };   // 配置（保存）位置
	Math::Vector3 m_basePos   = { 0.0f, 0.0f, 0.0f };   // 浮遊の基準位置（吸い寄せ・飛ばしで動く）
	Math::Vector3 m_vel       = { 0.0f, 0.0f, 0.0f };   // 飛ばした後の速度
	Math::Color   m_color     = { 1.0f, 1.0f, 1.0f, 1.0f };   // レインボーから1色（配置位置で決定）
	float         m_bobTimer  = 0.0f;
	float         m_rotAngle  = 0.0f;
	float         m_life      = -1.0f;   // 投擲物の寿命(秒)。-1=無限（配置物）
	bool          m_collected = false;   // プレイ中に取得済み（保存・配置は維持）
	bool          m_expired   = false;
};
