#pragma once

namespace MovingFloorConst
{
	// 往復移動の速度（ワールド単位/秒）
	constexpr float MoveSpeed       = 3.0f;

	// デフォルトの移動距離（片道）
	constexpr float DefaultRange    = 5.0f;

	// 床の標準サイズ（幅・高さ・奥行）
	constexpr float DefaultSizeX    = 4.0f;
	constexpr float DefaultSizeY    = 0.4f;
	constexpr float DefaultSizeZ    = 4.0f;

	// 着地判定に使うレイの長さ
	constexpr float RayLength       = 0.6f;

	// 折り返し地点でのデフォルト一時停止時間（秒）
	constexpr float DefaultWaitTime = 1.0f;
}
