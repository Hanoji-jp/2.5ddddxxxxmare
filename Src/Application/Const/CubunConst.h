#pragma once

namespace CubunConst
{
	// モデルパス
	inline constexpr const char* ModelPath = "Asset/Data/Cubunglb.gltf";

	// HP
	constexpr int   Hp              = 3;

	// 移動速度（ワールド単位/秒）
	constexpr float MoveSpeed       = 2.5f;

	// 巡回折り返し距離（スポーン位置から左右）
	constexpr float PatrolRange     = 4.0f;

	// ジャンプ力（毎秒の初速）
	constexpr float JumpPower       = 7.0f;

	// ジャンプ間隔（秒）：着地してからこの時間後に再ジャンプ
	constexpr float JumpInterval    = 0.4f;

	// コリジョンサイズ（幅・高さ・奥行き の半径）
	constexpr float CollisionRadius = 0.5f;
	constexpr float CollisionHeight = 1.0f;

	// モデルのピボット補正（足元が原点のモデルなら CollisionHeight/2、中心原点なら 0）
	constexpr float ModelOffsetY    = 0.5f;

	// 棘ダメージ判定の有効距離（体の下面から下方向）
	constexpr float SpikeOffset     = 0.6f;   // 下面からさらにこの距離
	constexpr float SpikeRadius     = 0.5f;

	// プレイヤーへの体当たりダメージ（棘に当たった場合は 2）
	constexpr int   ContactDamage   = 1;
	constexpr int   SpikeDamage     = 2;

	// 攻撃射程（接近型なので近距離）
	constexpr float AttackRange     = 1.2f;

	// 追跡開始距離
	constexpr float ChaseRange      = 6.0f;

	// 重力スケール（Character の重力設定と合わせる）
	constexpr float GravityScale    = 1.0f;

	// 体の「上」方向が重力と逆転したとき体を回転させないため、
	// 視覚的 upDir を固定する（true = 常にモデルを上向きで描画）
	constexpr bool  FixVisualUp     = true;
}
