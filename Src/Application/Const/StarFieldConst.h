#pragma once

// 星空（スカイボックス内にランダム配置する星）に関わる定数
namespace StarFieldConst
{
	// 星モデルのパス（小さなBoxを星として使う）
	constexpr const char* StarModelPath = "Asset/Data/Box.gltf";

	// 配置する星の数
	constexpr int StarNum = 300;

	// 星を配置する球殻の半径範囲（カメラ中心からの距離）
	constexpr float StarRadiusMin = 60.0f;
	constexpr float StarRadiusMax = 400.0f;

	// 星1個のスケール範囲（ランダム）
	constexpr float StarScaleMin = 0.15f;
	constexpr float StarScaleMax = 0.6f;

	// 星の発光（エミッシブ）強度範囲（大きいほどブルームで強く光る）
	constexpr float StarEmissiveMin = 2.0f;
	constexpr float StarEmissiveMax = 6.0f;

	// 星の色バリエーション（白〜青白〜淡黄。RGBそれぞれの最低値）
	constexpr float StarColorMinR = 0.7f;
	constexpr float StarColorMinG = 0.7f;
	constexpr float StarColorMinB = 0.8f;

	// 乱数シード（毎回同じ星空にしたい場合は固定値を使う）
	constexpr unsigned int RandomSeed = 20240601u;

	// 星のゆっくりした瞬き（明滅）速度・強さ
	constexpr float TwinkleSpeed     = 1.5f;   // 明滅の速さ
	constexpr float TwinkleStrength  = 0.35f;  // 明滅の振れ幅（0で明滅なし）

	// 多重スクロール（パララックス）
	//   カメラ移動に対して星がどれだけ「ついてくる」かの割合。
	//   1.0 = 完全追従（動いて見えない）、0.0 = 全く追従しない（無限遠で固定）。
	//   近い星ほど追従を弱く（よく動く）、遠い星ほど追従を強く（あまり動かない）する。
	constexpr float NearParallax = 0.30f;  // 近い星：カメラ移動の20%動いて見える
	constexpr float FarParallax  = 0.985f; // 遠い星：ほぼ追従（わずかに動く）
}

