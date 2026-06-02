#pragma once

namespace WarpHoleConst
{
	//--------------------------------------------------
	// 判定
	//--------------------------------------------------
	// 吸い込み判定半径
	constexpr float SuckRadius          = 2.0f;

	// ワープ完了後の射出速度
	constexpr float LaunchSpeed         = 8.0f;

	//--------------------------------------------------
	// Waypoint 移動
	//--------------------------------------------------
	// Waypointを移動する速度（単位：ワールド/秒）
	constexpr float WarpMoveSpeed       = 25.0f;

	//--------------------------------------------------
	// 発射速度プロファイル（マリオギャラクシー ランチスター風）
	//   発射直後にドンッと最高速 → 緩やかに巡航速度へ落ち着く
	//--------------------------------------------------
	// 発射直後の最高速倍率（WarpMoveSpeed に対する倍率）
	constexpr float WarpLaunchSpeedMul  = 2.6f;

	// 巡航時の速度倍率（終盤に落ち着く速さ。WarpMoveSpeed に対する倍率）
	constexpr float WarpCruiseSpeedMul  = 1.0f;

	// 最高速→巡航速へ減衰しきるまでの距離割合（0〜1、曲線全長に対する割合）
	constexpr float WarpLaunchBlendDist = 0.35f;

	// Waypoint到達判定距離
	constexpr float WaypointReachDist   = 0.3f;

	// ワープ中の向き Slerp 速度（Traveling フェーズ）
	constexpr float WarpRotSlerpSpeed        = 0.12f;

	// 吸い込みフェーズの向き Slerp 速度（位置と一緒に動くよう速め）
	constexpr float WarpRotSlerpSpeedSucking = 0.35f;

	//--------------------------------------------------
	// 吸い込み演出
	//--------------------------------------------------
	// 吸い込み検知半径（この距離以内に入ったら演出開始）
	constexpr float SuckPullRadius      = 4.0f;

	// 吸い込み演出にかかる時間（秒）
	constexpr float SuckDuration        = 1.8f;

	// 吸い込み中の回転数（1.5 = 1.5周しながら入口へ）
	constexpr float SuckSpinRevolutions = 1.2f;

	// 螺旋の固定初期半径（プレイヤーの接近角度に関係なく常にこの半径で旋回する）
	constexpr float SuckSpiralRadius    = 2.0f;

	// 吸い込みフェーズ中のスケール縮小速度（1秒あたり）
	constexpr float SuckShrinkSpeed     = 0.15f;

	// ワープ移動中の縦方向ストレッチ倍率（進行方向に伸びる）
	constexpr float WarpStretchScale    = 1.6f;

	//--------------------------------------------------
	// ビジュアル：ワイヤーフレームトンネル
	//--------------------------------------------------
	// 同心円リングの分割数（1リングの頂点数）
	constexpr int   RingSegments        = 32;

	// 口元のリング半径
	constexpr float RingOuterRadius     = 2.5f;

	// トンネルの奥行き
	constexpr float FunnelDepth         = 8.0f;

	// 半径プロファイル制御点（口元→奥、線形補間で使用）
	// 1.0 → 0.7 → 0.4 → 0.45 → 0.40 → 0.40
	// ここを変えるだけでトンネルの形が変わる

	// 描画するリング枚数（奥行き方向のスライス数）
	constexpr int   RingLayers          = 12;

	// 放射スポーク（縦の格子線）の本数
	constexpr int   SpokeCount          = 16;

	// アニメ用：1秒に何リング分流れるか
	constexpr float TunnelScrollSpeed   = 1.5f;

	// プレイヤー移動用の中心線リサンプリング間隔（ワールド単位）
	//   トンネル部と中継点部の点密度を均一化し、繋ぎ目のガクつきを防ぐ
	constexpr float CenterPathSpacing   = 1.0f;

	// 中心線スプライン（Catmull-Rom）の1区間あたり分割数
	//   大きいほど中継点間が滑らかな曲線になる（トンネル・移動軌道共通）
	constexpr int   CenterSplineSubdiv  = 12;

	// 口元の向き固定の影響範囲（端からこの弧長距離までは口元方向に寄せる）
	//   この距離を超えると本来の軌道接線へ滑らかにブレンドする
	constexpr float MouthAlignDist      = 4.0f;

	//--------------------------------------------------
	// ビジュアル：色・ブレンド
	//--------------------------------------------------
	// 入口リング色（RGBA） ─ 青紫系
	constexpr float EntryColorR         = 0.3f;
	constexpr float EntryColorG         = 0.0f;
	constexpr float EntryColorB         = 1.0f;
	constexpr float EntryColorA         = 0.85f;

	// 出口リング色（RGBA） ─ マゼンタ系
	constexpr float ExitColorR          = 1.0f;
	constexpr float ExitColorG          = 0.0f;
	constexpr float ExitColorB          = 0.8f;
	constexpr float ExitColorA          = 0.85f;

	//--------------------------------------------------
	// ビジュアル：アニメーション
	//--------------------------------------------------
	// トンネルスクロール速度（DrawEffect内で使用）
	constexpr float AnimSpeed           = 0.4f;

	//--------------------------------------------------
	// 保存
	//--------------------------------------------------
	constexpr const char* SavePath      = "Asset/Data/warp_holes.csv";
}
