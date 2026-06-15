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
	// PlayerConst::JumpPower = 0.4f と同スケール。MaxFallSpeed = 0.5f 系の物理に合わせる
	constexpr float JumpPower       = 0.2f;

	// ジャンプ間隔（秒）：着地してからこの時間後に再ジャンプ
	constexpr float JumpInterval    = 0.4f;

	// 本体コリジョン：モデルの COL ノード実寸に一致させた値。
	// （COL ノードのメッシュ境界 × ModelScale(0.3) ＋ ModelOffsetY(0.5)。GetPos は足元基準）
	//   横半径 CollisionRadius、縦半径 CollisionHalfH、中心高さ CollisionCenterY。
	//   本体は y ∈ [CollisionCenterY - CollisionHalfH, CollisionCenterY + CollisionHalfH]。
	constexpr float CollisionRadius  = 1.32f;  // 横半径（x,z half-extent）
	constexpr float CollisionHalfH   = 1.0f;   // 縦半径（y half-extent）
	constexpr float CollisionCenterY = 1.99f;  // 本体中心の高さ（GetPos からのオフセット）

	// モデルのピボット補正（CalcVisualMatrix で pos.y に加算）
	constexpr float ModelOffsetY    = 0.5f;

	// 棘ダメージ判定の有効距離（体の下面から下方向）
	constexpr float SpikeOffset     = 0.4f;   // 下面からさらにこの距離
	constexpr float SpikeRadius     = 0.7f;

	// COL_Attack（叩きつけ判定）：モデル内のコリジョンノード名
	inline constexpr const char* AttackNodeName = "COL_Attack";
	// COL_Attack スラブのデバッグ表示用サイズ（実判定はレイキャスト。COL_Attack ノード実寸）
	constexpr float AttackCenterY = 0.49f;  // スラブ中心の高さ（GetPos からのオフセット）
	constexpr float AttackHalfH   = 0.17f;  // 縦半径
	constexpr float AttackRadius  = 1.12f;  // 横半径
	// 叩きつけ判定レイ：プレイヤー背後へのオフセットと接触有効距離（ワールド単位）
	constexpr float AttackRayBackOffset = 0.5f;
	constexpr float AttackContactRange  = 1.0f;

	// プレイヤーへの体当たりダメージ（棘に当たった場合は 2）
	constexpr int   ContactDamage   = 1;
	constexpr int   SpikeDamage     = 2;
	// 叩きつけ（COL_Attack）ダメージ：即死ではなくダメージ＋ノックバック
	constexpr int   CrushDamage     = 2;

	// ── 踏みつけ判定（頭側＝安全面。重力＝足元基準）──
	// 上面からこの高さまでに足があれば踏みつけ成立（プレイヤーの足が乗る範囲）
	constexpr float StompReach        = 0.7f;
	// 上面より少し内側まで許容（めり込み時も拾う）
	constexpr float StompTolerance    = 0.3f;
	// 水平方向の許容半径（CollisionRadius に加算。プレイヤーの体の太さ分）
	constexpr float StompHorizMargin  = 0.4f;
	// この値以下の接近速度（m_upDir 方向成分）なら踏みつけ成立（降下中 = 負）
	constexpr float StompApproachMax  = 0.05f;
	// 踏みつけ後のプレイヤー跳ね返り速度（Cubun の上方向へ。Player::JumpPower=0.4 より控えめ）
	constexpr float StompBounceVel    = 0.32f;

	// ── ぺちゃんこ演出（クリボー踏みつけ）──
	// 踏まれてから消えるまでの時間（秒）
	constexpr float SquashDuration   = 0.2f;
	// 潰れ切ったときの Y スケール比率（1.0 = 元サイズ、小さいほど薄い）
	constexpr float SquashScaleY     = 0.04f;
	// 潰れ切ったときの XZ スケール比率（横方向の広がり）
	constexpr float SquashScaleXZ    = 1.6f;

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

