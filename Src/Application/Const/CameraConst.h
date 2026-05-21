#pragma once

namespace CameraConst
{
	constexpr float OffsetY        = 3.0f;
	constexpr float OffsetZ        = -15.0f;
	constexpr float FollowLerp     = 0.1f;
	constexpr float FloatHeight    = 6.0f;
	constexpr float LookAtHeight   = 1.2f;
  constexpr float FollowWeightX  = 0.30f;
	constexpr float FollowWeightY  = 0.18f;
	constexpr float LookAheadX     = 0.9f;
	constexpr float LookAheadY     = 0.05f;
	constexpr float TiltPitchDeg   = 1.0f;
	constexpr float TiltRollDeg    = 0.5f;

	constexpr float DeadZoneX      = 1.5f;  // カメラの横デッドゾーン幅
	constexpr float DeadZoneY      = 0.75f; // カメラの縦デッドゾーン幅
	constexpr float TransitionLerp = 0.07f;
	constexpr float LeadOffset     = 2.0f;
	constexpr float LeadLerp       = 0.05f;
}