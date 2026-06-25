#pragma once

//==========================================================
// ItemMagnetConst
// マウスカーソルで岩（RockGem / RockDrop）を操作する「磁石」挙動の定数。
//  ・ホバー：カーソルに近い岩を吸い寄せて取得（マリギャラのスターピース風）。
//  ・左クリック：クリックしたワールド地点へ周囲の岩を飛ばす。
//==========================================================
namespace ItemMagnetConst
{
	constexpr float AttractRadius = 4.5f;   // カーソルへ吸い寄せ始めるXYワールド距離
	constexpr float CollectRadius = 0.9f;   // これより近いと取得する距離
	constexpr float PullLerp      = 0.20f;  // 吸い寄せの追従率（60fps基準・dtで補正）
	constexpr float FlingSpeed    = 34.0f;  // 飛ばす初速（units/秒）
	constexpr float FlingStartAhead = 3.0f; // 発射開始点をカメラからレイ方向へずらす距離（画面外start防止）
	constexpr float ThrownLife    = 4.0f;   // 撃ち出した岩の寿命(秒)。これを過ぎたら消える
	constexpr float GemDrag       = 0.985f; // 撃ち出した岩の速度減衰（60fps基準。勢いを保つ）
}
