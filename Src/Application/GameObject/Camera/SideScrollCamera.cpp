#include "../../../Pch.h"
#include "SideScrollCamera.h"
#include "../../Camera/CameraSettings.h"

void SideScrollCamera::SetRooms(const std::vector<RoomBounds>& _rooms)
{
	m_rooms         = _rooms;
	m_currentRoom   = 0;
	m_nextRoom      = 0;
	m_isTransitioning = false;
	m_transitionRate  = 0.0f;
	m_initialized     = false;
}

// ルーム遷移ブレンド後の基準中心を返す
Math::Vector3 SideScrollCamera::CalcBlendedRoomCenter(const Math::Vector3& _targetPos) const
{
	if (m_rooms.empty()) { return _targetPos; }

	const RoomBounds& cr = m_rooms[m_currentRoom];

	const bool validY = (cr.minY > -1e+30f && cr.maxY < 1e+30f);
	const float centerY = validY ? (cr.minY + cr.maxY) * 0.5f : _targetPos.y;

	const Math::Vector3 currentCenter =
	{
		(cr.minX + cr.maxX) * 0.5f,
		centerY,
		cr.cameraZ
	};

	if (!m_isTransitioning || m_nextRoom == m_currentRoom ||
		m_nextRoom >= static_cast<int>(m_rooms.size()))
	{
		return currentCenter;
	}

	const RoomBounds& nr = m_rooms[m_nextRoom];
	const bool validYN = (nr.minY > -1e+30f && nr.maxY < 1e+30f);
	const float nextCenterY = validYN ? (nr.minY + nr.maxY) * 0.5f : _targetPos.y;

	const Math::Vector3 nextCenter =
	{
		(nr.minX + nr.maxX) * 0.5f,
		nextCenterY,
		nr.cameraZ
	};

	return Math::Vector3::Lerp(currentCenter, nextCenter, std::clamp(m_transitionRate, 0.0f, 1.0f));
}

// 目標カメラ位置（ルーム中心 + 部分追従 + 浮遊ボブ）
Math::Vector3 SideScrollCamera::CalcTargetCamPos(const Math::Vector3& _targetPos, float _floatY, float _floatX, float _offsetZ, float _upSign, float _offsetX, float _offsetY) const
{
	const Math::Vector3 roomCenter = CalcBlendedRoomCenter(_targetPos);

	// 重力方向に応じてオフセット方向を反転
	constexpr float kInvertedExtraOffset = 5.0f;
	const float offsetY = _offsetY * _upSign - (1.0f - _upSign) * 0.5f * kInvertedExtraOffset;
	float camY = _targetPos.y + offsetY + _floatY;

	constexpr float kViewHalfHeight = 10.0f;
	constexpr float kScreenMargin   = 0.5f;
	const float minCamY = _targetPos.y - kViewHalfHeight + kScreenMargin;
	const float maxCamY = _targetPos.y + kViewHalfHeight - kScreenMargin;
	if (camY < minCamY) { camY = minCamY; }
	else if (camY > maxCamY) { camY = maxCamY; }

	return
	{
		_targetPos.x + _floatX + _offsetX,
		camY,
		_offsetZ
	};
}

// 目標注視点（プレイヤーの少し上 + 先読み）
Math::Vector3 SideScrollCamera::CalcTargetLookAt(const Math::Vector3& _targetPos) const
{
	const auto& cs = CameraSettings::Instance();
	return
	{
		_targetPos.x + cs.LookAheadX,
		_targetPos.y + cs.LookAtHeight + cs.LookAheadY,
		_targetPos.z
	};
}

void SideScrollCamera::Update(const Math::Vector3& _targetPos, const Math::Vector3& _upDir)
{
	// upDirをSlerpで滑らかに追従（急な切り替えを防ぐ）
	constexpr float kUpSlerpSpeed = 0.05f;
	m_upDir = Math::Vector3::Lerp(m_upDir, _upDir, kUpSlerpSpeed);
	m_upDir.Normalize();

	// upDir.yが負 → 重力反転中（上下逆）
	const float upSign = (m_upDir.y >= 0.0f) ? 1.0f : -1.0f;

	// ── 0. フォーカスオフセット補間（重力ローカル→ワールド）────────
	// ルームのfocusOffsetをupDir基底でワールド空間へ変換してLerp補間
	{
		// 現在ルームのfocusOffset（重力ローカル）を取得
		Math::Vector3 localFocus = { 0.0f, 0.0f, 0.0f };
		float focusLerp = CameraConst::FocusOffsetLerp;

		if (!m_rooms.empty())
		{
			const RoomBounds& cur = m_rooms[m_currentRoom];
			localFocus  = cur.focusOffset;
			focusLerp   = (cur.focusLerpSpeed > 0.0f) ? cur.focusLerpSpeed : CameraConst::FocusOffsetLerp;

			// 遷移中は次ルームのオフセットへ線形ブレンド
			if (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
			{
				const RoomBounds& nxt = m_rooms[m_nextRoom];
				const float t = std::clamp(m_transitionRate, 0.0f, 1.0f);
				localFocus = Math::Vector3::Lerp(cur.focusOffset, nxt.focusOffset, t);
				const float nxtLerp = (nxt.focusLerpSpeed > 0.0f) ? nxt.focusLerpSpeed : CameraConst::FocusOffsetLerp;
				focusLerp = std::lerp(focusLerp, nxtLerp, t);
			}
		}

		// 重力ローカル基底を構築（upDir = Y軸、Z軸 = カメラ奥行き方向）
		const Math::Vector3 gravUp    = m_upDir;                         // ローカルY
		Math::Vector3 gravRight = { gravUp.y, -gravUp.x, 0.0f };        // upDirに直交する水平右方向（XY面内）
		if (gravRight.LengthSquared() < 1e-6f)
		{
			gravRight = { 0.0f, 0.0f, 1.0f };
		}
		gravRight.Normalize();
		Math::Vector3 gravForward;
		gravUp.Cross(gravRight, gravForward);
		gravForward.Normalize();

		// ローカル→ワールド変換
		const Math::Vector3 targetFocusWorld =
			gravRight   * localFocus.x +
			gravUp      * localFocus.y +
			gravForward * localFocus.z;

		// スムーズに補間
		m_focusOffsetWorld = Math::Vector3::Lerp(m_focusOffsetWorld, targetFocusWorld, focusLerp);
	}

	// ── 1. ルーム遷移ステート更新（全モード共通） ──────────────
	constexpr float kDt = 1.0f / 60.0f;
	if (!m_rooms.empty() && m_currentRoom + 1 < static_cast<int>(m_rooms.size()))
	{
		const RoomBounds& cur = m_rooms[m_currentRoom];

		if (!m_isTransitioning && _targetPos.x >= cur.triggerX)
		{
			m_nextRoom          = m_currentRoom + 1;
			m_transitionRate    = 0.0f;
			m_isTransitioning   = true;
			m_transitionStartX  = m_pos.x;
		}

		if (m_isTransitioning)
		{
			m_transitionRate += kDt * CameraConst::RoomTransitionSpeed;
			if (m_transitionRate >= 1.0f)
			{
				m_transitionRate  = 0.0f;
				m_currentRoom     = m_nextRoom;
				m_isTransitioning = false;
				m_clampMinX       = m_rooms[m_currentRoom].minX;
				m_clampMaxX       = m_rooms[m_currentRoom].maxX;
			}
		}
	}

	// 現在のルームのモードを取得
	const CameraConst::CameraMode mode = m_rooms.empty()
		? CameraConst::CameraMode::SideScroll
		: m_rooms[m_currentRoom].mode;

	// ── 惑星到達ズームアウト→戻り（全モード共通）──────────────
	constexpr float kDeltaTime = 1.0f / 60.0f;
	float planetZoomOffset = 0.0f;
	if (m_planetZoomTimer > 0.0f)
	{
		constexpr float kDecay = 1.5f;
		m_planetZoomTimer -= kDeltaTime * kDecay;
		m_planetZoomTimer  = std::max(m_planetZoomTimer, 0.0f);
		const float zoomCurve = std::sinf(m_planetZoomTimer * 3.14159f);
		planetZoomOffset = zoomCurve * JuiceConst::PlanetZoomOutAmount;
	}

	// ── 2. モード別のターゲット位置・注視点を計算 ──────────────
	const auto& cs = CameraSettings::Instance();
	Math::Vector3 targetPos;
	Math::Vector3 targetLookAt;
	float         targetRollDeg = 0.0f;

	// ── effective offset: ルームのオーバーライドがあれば使い、なければグローバル値 ──
	float effectiveOffsetX = cs.OffsetX;
	float effectiveOffsetY = cs.OffsetY;
	float effectiveOffsetZ = cs.OffsetZ;
	if (!m_rooms.empty())
	{
		const RoomBounds& cur = m_rooms[m_currentRoom];
		if (cur.useOffsetOverride)
		{
			effectiveOffsetX = cur.overrideOffsetX;
			effectiveOffsetY = cur.overrideOffsetY;
			effectiveOffsetZ = cur.overrideOffsetZ;
		}

		// 遷移中: 次ルームのオーバーライドへブレンド
		if (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
		{
			const RoomBounds& nxt = m_rooms[m_nextRoom];
			const float t = std::clamp(m_transitionRate, 0.0f, 1.0f);
			const float nxtX = nxt.useOffsetOverride ? nxt.overrideOffsetX : cs.OffsetX;
			const float nxtY = nxt.useOffsetOverride ? nxt.overrideOffsetY : cs.OffsetY;
			const float nxtZ = nxt.useOffsetOverride ? nxt.overrideOffsetZ : cs.OffsetZ;
				effectiveOffsetX = std::lerp(effectiveOffsetX, nxtX, t);
					effectiveOffsetY = std::lerp(effectiveOffsetY, nxtY, t);
					effectiveOffsetZ = std::lerp(effectiveOffsetZ, nxtZ, t);
				}
			}

			// minY/maxY が設定されていないルームはプレイヤーY中心（offsetY=0）
			// 設定されている場合のみ effectiveOffsetY を使って上下にずらす
			{
				const bool hasMinY = !m_rooms.empty() && (m_rooms[m_currentRoom].minY > -1e+30f);
				const bool hasMaxY = !m_rooms.empty() && (m_rooms[m_currentRoom].maxY <  1e+30f);
				if (!hasMinY && !hasMaxY)
				{
					effectiveOffsetY = 0.0f;
				}
			}

	if (mode == CameraConst::CameraMode::TopDown)
	{
		// ── TopDown : プレイヤー（またはルーム中心）の真上から見下ろす ──
		const Math::Vector3 roomCenter = CalcBlendedRoomCenter(_targetPos);

		// カメラはルーム中心の真上、注視点はルーム中心（ズームは高さに加算）
		targetPos    = { roomCenter.x, roomCenter.y + CameraConst::TopDownHeight + planetZoomOffset, roomCenter.z + CameraConst::TopDownOffsetZ };
		targetLookAt = { roomCenter.x, roomCenter.y, roomCenter.z };

		targetRollDeg = 0.0f;
	}
	else if (mode == CameraConst::CameraMode::Fixed2D)
	{
		// ── Fixed2D : マリギャラ2Dモード風 ──────────────────────
		const float currentCameraZ = m_rooms.empty() ? 0.0f : m_rooms[m_currentRoom].cameraZ;
		const float targetCameraZ  = (!m_rooms.empty() && m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
			? m_rooms[m_nextRoom].cameraZ : currentCameraZ;
		const float blendedCameraZ = std::lerp(currentCameraZ, targetCameraZ,
			std::clamp(m_transitionRate, 0.0f, 1.0f));

		// 底面に立っているとき（upSign=-1）はカメラを逆方向（下）にオフセット
		// effectiveOffsetY を使うことでルーム別 or グローバル値が反映される
		const float offsetY = effectiveOffsetY * upSign;

		// フォーカスオフセットをプレイヤー位置に加算
		const Math::Vector3 focusedTarget = _targetPos + m_focusOffsetWorld;

		targetPos    = { focusedTarget.x + effectiveOffsetX, focusedTarget.y + offsetY, blendedCameraZ + effectiveOffsetZ + planetZoomOffset };
		// 注視点はプレイヤーの実座標を直接向く
		targetLookAt = focusedTarget;
		targetRollDeg = 0.0f;
	}
	else
	{
		// ── SideScroll : 既存の横スクロール挙動 ──────────────────
		constexpr float kTwoPi    = 6.28318530f;
		m_floatTimer = std::fmod(m_floatTimer + kDeltaTime * cs.FloatSpeed, kTwoPi);
		const float floatY = std::sinf(m_floatTimer)        * cs.FloatAmplitude * (1.0f - m_wallZoomRate);
		const float floatX = std::sinf(m_floatTimer * 0.7f) * cs.FloatAmplitude * 0.4f * (1.0f - m_wallZoomRate);

		const float baseOffsetZ = m_useOffsetZOverride ? m_offsetZOverride : effectiveOffsetZ;

		// フォーカスオフセットをプレイヤー位置に加算してSideScroll基準位置を決定
		const Math::Vector3 focusedTarget = _targetPos + m_focusOffsetWorld;

		targetPos    = CalcTargetCamPos(focusedTarget, floatY, floatX, baseOffsetZ + planetZoomOffset, upSign, effectiveOffsetX, effectiveOffsetY);
		// 注視点はプレイヤーの実座標を直接向く
		targetLookAt = focusedTarget;

		if (!m_rooms.empty())
		{
			const RoomBounds& cur = m_rooms[m_currentRoom];
			const RoomBounds& targetRoom = (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
				? m_rooms[m_nextRoom] : cur;

			if (!m_initialized)
			{
				m_clampMinX = targetRoom.minX;
				m_clampMaxX = targetRoom.maxX;
			}
			else
			{
				m_clampMinX = std::lerp(m_clampMinX, targetRoom.minX, CameraConst::RoomTransitionSpeed * (1.0f / 60.0f));
				m_clampMaxX = std::lerp(m_clampMaxX, targetRoom.maxX, CameraConst::RoomTransitionSpeed * (1.0f / 60.0f));
			}

			const float planeDist0 = std::abs(baseOffsetZ);
			const float halfView0  = (m_mProj._11 > 0.0f) ? (planeDist0 / m_mProj._11) : 0.0f;
			const float clampMin0  = m_clampMinX + halfView0;
			const float clampMax0  = m_clampMaxX - halfView0;

			const float idealX = _targetPos.x;
			float overX = 0.0f;
			if      (idealX > clampMax0) { overX = idealX - clampMax0; }
			else if (idealX < clampMin0) { overX = clampMin0 - idealX; }

			const float targetZoomRate = std::clamp(
				std::sqrtf(overX * cs.WallZoomSensitivity),
				0.0f, cs.WallZoomMaxRatio);
			m_wallZoomRate = std::lerp(m_wallZoomRate, targetZoomRate, cs.WallZoomLerp);

			const float actualOffsetZ = (baseOffsetZ + planetZoomOffset) * (1.0f - m_wallZoomRate);
			const float planeDist1    = std::abs(actualOffsetZ);
			const float halfView1     = (m_mProj._11 > 0.0f) ? (planeDist1 / m_mProj._11) : 0.0f;
			const float clampMin1     = m_clampMinX + halfView1;
			const float clampMax1     = m_clampMaxX - halfView1;

			if (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
			{
				const RoomBounds& nxt  = m_rooms[m_nextRoom];
				const float nextEntryX = nxt.minX + halfView1;
				const float t          = m_transitionRate;
				const float smooth     = t * t * (3.0f - 2.0f * t);
				targetPos.x    = std::lerp(m_transitionStartX, nextEntryX, smooth);
				targetLookAt.x = targetPos.x;
			}
			else if (clampMin1 <= clampMax1)
			{
				targetPos.x    = std::clamp(idealX,         clampMin1, clampMax1);
				targetLookAt.x = std::clamp(targetLookAt.x, clampMin1, clampMax1);
			}
			else
			{
				const float center = (m_clampMinX + m_clampMaxX) * 0.5f;
				targetPos.x    = center;
				targetLookAt.x = center;
			}

			// cameraZ: 遷移中は currentRoom→nextRoom をブレンド
			const float currentCameraZ = m_rooms[m_currentRoom].cameraZ;
			const float targetCameraZ  = (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
				? m_rooms[m_nextRoom].cameraZ : currentCameraZ;
			const float blendedCameraZ = std::lerp(currentCameraZ, targetCameraZ,
				std::clamp(m_transitionRate, 0.0f, 1.0f));
			targetPos.z = actualOffsetZ + blendedCameraZ;
		}

		// ロール計算
		const float offsetX = _targetPos.x - m_pos.x;
		targetRollDeg = std::clamp(offsetX * cs.RollSensitivity, -cs.RollMaxDeg, cs.RollMaxDeg);
	}

	// ── 3. 初回は即座にセット ──────────────────────────────────
	if (!m_initialized)
	{
		m_pos       = targetPos;
		m_lookAtPos = targetLookAt;
		m_initialized = true;
	}

	// ── 3.5 minY/maxY クランプ（ルーム設定が有効な場合のみ） ───
	if (!m_rooms.empty())
	{
		const RoomBounds& cur = m_rooms[m_currentRoom];
		// 遷移中はブレンドされた minY/maxY を使用
		float blendedMinY = cur.minY;
		float blendedMaxY = cur.maxY;
		if (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
		{
			const RoomBounds& nxt = m_rooms[m_nextRoom];
			const float t = std::clamp(m_transitionRate, 0.0f, 1.0f);
			blendedMinY = std::lerp(cur.minY, nxt.minY, t);
			blendedMaxY = std::lerp(cur.maxY, nxt.maxY, t);
		}
		if (blendedMinY > -1e+30f) { targetPos.y = std::max(targetPos.y, blendedMinY); }
		if (blendedMaxY <  1e+30f) { targetPos.y = std::min(targetPos.y, blendedMaxY); }
	}

	// ── 4. 補間（TopDown / Fixed2D でも Lerp で滑らかに切り替わる） ──
	if (mode == CameraConst::CameraMode::SideScroll)
	{
		const float minCamY = _targetPos.y + CameraConst::MinCamOffsetY;
		if (targetPos.y < minCamY) { targetPos.y = minCamY; }
	}

	if (mode == CameraConst::CameraMode::Fixed2D)
	{
		m_pos       = Math::Vector3::Lerp(m_pos,       targetPos,    CameraConst::Fixed2DPosLerp);
		m_lookAtPos = Math::Vector3::Lerp(m_lookAtPos, targetLookAt, CameraConst::Fixed2DLookLerp);
	}
	else
	{
		m_pos       = Math::Vector3::Lerp(m_pos,       targetPos,    cs.PosLerp);
		m_lookAtPos = Math::Vector3::Lerp(m_lookAtPos, targetLookAt, cs.LookAtLerp);

		if (mode != CameraConst::CameraMode::TopDown)
		{
			const float minCamY = _targetPos.y + CameraConst::MinCamOffsetY;
			if (m_pos.y < minCamY) { m_pos.y = minCamY; }

			constexpr float kLookAtMargin = 0.5f;
			if (m_lookAtPos.y >= m_pos.y - kLookAtMargin)
			{
				m_lookAtPos.y = m_pos.y - kLookAtMargin;
			}
		}
	}

	// ── 5. ロール ─────────────────────────────────────────────
	m_rollDeg = std::lerp(m_rollDeg, targetRollDeg, cs.RollLerp);

	// ── 6. LookAt 基底を構築 ────────────────────────────────────
	Math::Vector3 worldUp;
	if (mode == CameraConst::CameraMode::TopDown)
	{
		// TopDown は真下を向くため、Forward が -Y → Up 基底に +Z を使う
		worldUp = { 0.0f, 0.0f, 1.0f };
	}
	else
	{
		worldUp = { 0.0f, 1.0f, 0.0f };
	}

	Math::Vector3 fwd = m_lookAtPos - m_pos;

	if (mode == CameraConst::CameraMode::SideScroll)
	{
		const float horizLen = std::sqrtf(fwd.x * fwd.x + fwd.z * fwd.z);
		const float maxTan   = std::tanf(DirectX::XMConvertToRadians(CameraConst::MaxPitchDeg));
		if (horizLen > 0.0f && fwd.y > horizLen * maxTan)
		{
			fwd.y = horizLen * maxTan;
		}
	}

	fwd.Normalize();

	Math::Vector3 right;
	worldUp.Cross(fwd, right);
	if (right.LengthSquared() < 1e-6f)
	{
		// fwd が worldUp と平行（真下向きなど）→ 代替ベクトルで安定化
		right = { 1.0f, 0.0f, 0.0f };
	}
	else
	{
		right.Normalize();
	}

	Math::Vector3 up;
	fwd.Cross(right, up);
	up.Normalize();

	const float rollRad             = DirectX::XMConvertToRadians(m_rollDeg);
	const float cosR                = std::cosf(rollRad);
	const float sinR                = std::sinf(rollRad);
	const Math::Vector3 rolledRight = right * cosR + up * (-sinR);
	const Math::Vector3 rolledUp    = right * sinR + up *   cosR;

	// ── 7. カメラワールド行列を確定 ─────────────────────────────

	// ── 8. カメラシェイク適用 ────────────────────────────────
	Math::Vector3 shakeOffset = { 0.0f, 0.0f, 0.0f };
	if (m_shakeStrength > 0.0f)
	{
		m_shakeTimer   += kDt * 37.0f;  // ノイズ周波数
		m_shakeStrength = std::max(0.0f,
			m_shakeStrength - JuiceConst::ShakeDecay * kDt);

		const float sx = std::sinf(m_shakeTimer * 1.00f) + std::sinf(m_shakeTimer * 2.31f);
		const float sy = std::sinf(m_shakeTimer * 0.87f) + std::sinf(m_shakeTimer * 1.73f);
		shakeOffset.x = sx * 0.5f * m_shakeStrength;
		shakeOffset.y = sy * 0.5f * m_shakeStrength;
	}

	Math::Matrix mCam;
	mCam._11 = rolledRight.x; mCam._12 = rolledRight.y; mCam._13 = rolledRight.z; mCam._14 = 0.0f;
	mCam._21 = rolledUp.x;    mCam._22 = rolledUp.y;    mCam._23 = rolledUp.z;    mCam._24 = 0.0f;
	mCam._31 = fwd.x;         mCam._32 = fwd.y;         mCam._33 = fwd.z;         mCam._34 = 0.0f;
	mCam._41 = m_pos.x + shakeOffset.x;
	mCam._42 = m_pos.y + shakeOffset.y;
	mCam._43 = m_pos.z;
	mCam._44 = 1.0f;

	SetCameraMatrix(mCam);
}
