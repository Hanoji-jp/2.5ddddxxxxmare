#pragma once

//==========================================================
// GravityRingConst.h
// 引力圏リング／重力矢印エフェクト定数
//==========================================================
namespace GravityRingConst
{
	// ── Sphere 用リング ──────────────────────────────────────
	// リングを構成するBoxマーカーの数
	constexpr int   MarkerCount      = 12;
	// 各Boxマーカーのサイズ
	constexpr float MarkerSize       = 0.3f;
	// リングの回転速度（ラジアン/秒）
	constexpr float RotateSpeed      = 0.4f;
	// リングのカラー（RGBA）
	inline const Math::Color MarkerColor = { 0.3f, 0.8f, 1.0f, 0.6f };

	// ── Box 用 重力方向矢印 ──────────────────────────────────
	// 矢印モデルのパス
	constexpr const char* ArrowModelPath = "Asset/Data/Map_Arrow.gltf";
	// Box.gltf のパス（デブリ・パーティクル共用）
	constexpr const char* BoxModelPath   = "Asset/Data/Box.gltf";
	constexpr float ArrowOffset      = 1.2f;
	// 矢印の軸（線）の長さ
	constexpr float ArrowLength      = 1.0f;
	// 矢尻の横幅（V字の広がり）
	constexpr float ArrowHeadWidth   = 0.35f;
	// 矢尻の奥行き
	constexpr float ArrowHeadLength  = 0.4f;

	// ── 浮遊デブリ ───────────────────────────────────────────
	// デブリの個数
	constexpr int   DebrisCount        = 6;
	// デブリのスケール
	constexpr float DebrisScale        = 0.18f;
	// 惑星中心からの初期距離（GravityRadiusに掛ける割合）
	constexpr float DebrisRadiusRatio  = 0.85f;
	// 引力方向への移動速度
	constexpr float DebrisDriftSpeed   = 0.5f;
	// 引力方向への最大移動距離（超えたら元の位置に戻す）
	constexpr float DebrisDriftMax     = 1.2f;
	// デブリの公転速度（ラジアン/秒）
	constexpr float DebrisOrbitSpeed   = 0.35f;

	// ── パーティクル流れ ─────────────────────────────────────
	// パーティクルの最大数
	constexpr int   ParticleMax        = 12;
	// スポーン間隔（秒）
	constexpr float ParticleSpawnInterval = 0.18f;
	// 移動速度
	constexpr float ParticleSpeed      = 2.5f;
	// 寿命（秒）
	constexpr float ParticleLifetime   = 0.9f;
	// スケール
	constexpr float ParticleScale      = 0.12f;
	// スポーン半径（GravityRadiusに掛ける割合）
	constexpr float ParticleSpawnRadiusRatio = 0.75f;

	// 重力モード別カラー
	inline const Math::Color ColorInward  = { 0.2f, 1.0f, 0.4f, 0.85f }; // 引き寄せ：緑
	inline const Math::Color ColorOutward = { 1.0f, 0.3f, 0.3f, 0.85f }; // 弾き飛ばし：赤
	inline const Math::Color ColorInherit = { 0.8f, 0.8f, 0.2f, 0.85f }; // 継承：黄
	inline const Math::Color ColorDown    = { 0.4f, 0.6f, 1.0f, 0.85f }; // 下：青
	inline const Math::Color ColorUp      = { 1.0f, 0.6f, 0.2f, 0.85f }; // 上：オレンジ
	inline const Math::Color ColorLeft    = { 0.8f, 0.3f, 1.0f, 0.85f }; // 左：紫
	inline const Math::Color ColorRight   = { 0.3f, 1.0f, 0.9f, 0.85f }; // 右：シアン
}

