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

	// ワープ移動の総時間（秒）。速度ではなく時間で制御する。
	// InOutExpo イージングにより始終が遅く中間が速くなる。
	constexpr float WarpTravelDuration  = 1.2f;

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
	// ビジュアル：ローポリファネル（正面向きロート）
	//--------------------------------------------------
	// ファネルのセグメント数（少ないほどローポリ感が強い）
	constexpr int   FunnelSegments      = 14;

	// ファネルのリング枚数（奥行き方向のスライス数）
	constexpr int   FunnelRings         = 9;

	// 口元のリング半径
	constexpr float FunnelOuterRadius   = 3.0f;

	// 奥（中心）の最小半径
	constexpr float FunnelInnerRadius   = 0.3f;

	// ランダム頂点オフセットの最大割合（0.0〜1.0、半径に対する比率）
	constexpr float FunnelJitter        = 0.18f;

	// ポリゴン面の不透明度
	constexpr float FunnelFaceAlpha     = 0.75f;

	// ワイヤーの不透明度
	constexpr float FunnelWireAlpha     = 1.4f;

	// ファネルの奥行き（EntryMouthDirに沿った長さ）
	constexpr float FunnelLength        = 4.0f;

	// 頂点アニメ：波の振幅（半径に対する比率）
	constexpr float FunnelWaveAmp       = 0.12f;

	// 頂点アニメ：奥に向かう波の周波数（リング方向の波の数）
	constexpr float FunnelWaveFreqRing  = 3.0f;

	// 頂点アニメ：周方向の波の周波数（seg方向の波の数）
	constexpr float FunnelWaveFreqSeg   = 2.0f;

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
	// 入口リング色（RGBA） ─ シアン（加算ブレンド・適度な明るさで色差が出る範囲）
	constexpr float EntryColorR         = 0.0f;
	constexpr float EntryColorG         = 0.9f;
	constexpr float EntryColorB         = 1.0f;
	constexpr float EntryColorA         = 1.0f;

	// 出口リング色（RGBA） ─ やや青寄りシアン
	constexpr float ExitColorR          = 0.1f;
	constexpr float ExitColorG          = 0.6f;
	constexpr float ExitColorB          = 1.0f;
	constexpr float ExitColorA          = 1.0f;

	//--------------------------------------------------
	// ビジュアル：アニメーション
	//--------------------------------------------------
	// トンネルスクロール速度（DrawEffect内で使用）
	constexpr float AnimSpeed           = 0.4f;

	//--------------------------------------------------
	// パーティクル（吸い込みBOX）
	//--------------------------------------------------
	// Box.gltfのパス
	constexpr const char* BoxModelPath          = "Asset/Data/Box.gltf";

	// 同時存在するパーティクル数
	constexpr int         ParticleCount         = 12;

	// スポーン半径（EntryPos中心のランダム球面上に生成）
	constexpr float       ParticleSpawnRadius   = 4.5f;

	// BOXのスケール（一辺の長さ）
	constexpr float       ParticleScale         = 0.07f;

	// 引力加速度（EntryPosへの加速。距離が近いほど強くなる）
	constexpr float       ParticleSuckAccel     = 8.0f;

	// 到達判定距離（EntryPosにこの距離以内でリスポーン）
	constexpr float       ParticleReachDist     = 0.4f;

	// 回転速度（ランダム軸、ラジアン/秒）
	constexpr float       ParticleRotSpeed      = 4.0f;

	// Bloom発光強度
	constexpr float       ParticleEmissive      = 4.0f;

	//--------------------------------------------------
	// パーティクル（EXIT 吐き出しBOX）
	//--------------------------------------------------
	// 同時存在するEXITパーティクル数
	constexpr int         ExitParticleCount     = 10;

	// EXITから吐き出される初速（外向き）
	constexpr float       ExitParticleInitSpeed = 3.5f;

	// EXITパーティクルの寿命（秒）
	constexpr float       ExitParticleLifeTime  = 1.8f;

	// EXITパーティクルの生成間隔（秒）
	constexpr float       ExitParticleSpawnInterval = 0.15f;

	// EXITパーティクルの横方向ランダム速度
	constexpr float       ExitParticleSpread    = 1.5f;

	// EXITパーティクルのスケール
	constexpr float       ExitParticleScale     = 0.09f;

	//--------------------------------------------------
	// トンネル wave アニメ
	//--------------------------------------------------
	// パス方向のリング分割数（大きいほど細かい面になる）
	constexpr int         TunnelRings           = 20;

	// トンネル半径に乗せる wave 振幅（半径比率）
	constexpr float       TunnelWaveAmp         = 0.15f;

	// トンネル wave の弧長方向周波数（1単位あたり波の数）
	constexpr float       TunnelWaveFreqAlong   = 0.5f;

	// トンネル wave の周方向周波数（1周あたり波の数）
	constexpr float       TunnelWaveFreqSeg     = 2.0f;

	//--------------------------------------------------
	// 保存
	//--------------------------------------------------
	constexpr const char* SavePath      = "Asset/Data/warp_holes.csv";

	//--------------------------------------------------
	// テレポート型：フェード演出
	//--------------------------------------------------
	// フェードアウト／フェードインの速度（1フレームあたりアルファ増減量）
	constexpr float TeleportFadeSpeed   = 0.04f;   // 約25フレームで完全暗転

	// テレポート後、フェードインが始まるまでの待機時間（秒）
	constexpr float TeleportHoldTime    = 0.1f;

	// ワープ完了後の再トリガー防止クールダウン（秒）
	// Exit 近くに出てもすぐに吸い込まれないようにする
	constexpr float WarpCooldownTime    = 2.5f;
}

