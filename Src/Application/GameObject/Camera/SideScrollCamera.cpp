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

	// minY/maxY が FLT_MAX のままの場合はプレイヤーY をそのまま使う
	const bool validY = (cr.minY > -1e+30f && cr.maxY < 1e+30f);
	const float centerY = validY ? (cr.minY + cr.maxY) * 0.5f : _targetPos.y;

	const Math::Vector3 currentCenter =
	{
		(cr.minX + cr.maxX) * 0.5f,
		centerY,
		0.0f
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
		0.0f
	};

	return Math::Vector3::Lerp(currentCenter, nextCenter, std::clamp(m_transitionRate, 0.0f, 1.0f));
}

// 目標カメラ位置（ルーム中心 + 部分追従 + 浮遊ボブ）
Math::Vector3 SideScrollCamera::CalcTargetCamPos(const Math::Vector3& _targetPos, float _floatY, float _floatX, float _offsetZ) const
{
	const auto& cs = CameraSettings::Instance();
	const Math::Vector3 roomCenter = CalcBlendedRoomCenter(_targetPos);

	const float dy = _targetPos.y - roomCenter.y;
	const float wy = std::clamp(cs.FollowWeightY, cs.MinFollowWeightY, 1.0f);

	// Y はプレイヤー直追尾
	return
	{
		_targetPos.x + _floatX,
		_targetPos.y + cs.OffsetY + _floatY,
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

void SideScrollCamera::Update(const Math::Vector3& _targetPos)
{
	// ── 1. ルーム遷移ステート更新 ──────────────────────────────
	constexpr float kDt = 1.0f / 60.0f;
	if (!m_rooms.empty() && m_currentRoom + 1 < static_cast<int>(m_rooms.size()))
	{
		const RoomBounds& cur = m_rooms[m_currentRoom];

		if (!m_isTransitioning && _targetPos.x >= cur.triggerX)
		{
			// triggerX を踏んだ瞬間に遷移開始
			m_nextRoom          = m_currentRoom + 1;
			m_transitionRate    = 0.0f;
			m_isTransitioning   = true;
			m_transitionStartX  = m_pos.x;  // 起点を記録
		}

		if (m_isTransitioning)
		{
			// 時間ベースで transitionRate を 0→1 に進める
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

	// ── 2. 浮遊ボブ ───────────────────────────────────────────
	const auto& cs = CameraSettings::Instance();
	constexpr float kDeltaTime  = 1.0f / 60.0f;
	constexpr float kTwoPi     = 6.28318530f;
	m_floatTimer = std::fmod(m_floatTimer + kDeltaTime * cs.FloatSpeed, kTwoPi);
	const float floatY = std::sinf(m_floatTimer)        * cs.FloatAmplitude * (1.0f - m_wallZoomRate);
	const float floatX = std::sinf(m_floatTimer * 0.7f) * cs.FloatAmplitude * 0.4f * (1.0f - m_wallZoomRate);

	// ── 3. OffsetZ ────────────────────────────────────────────
	const float baseOffsetZ = cs.OffsetZ;

	// ── 4. 目標位置・注視点 ────────────────────────────────────
	Math::Vector3 targetPos    = CalcTargetCamPos(_targetPos, floatY, floatX, baseOffsetZ);
	Math::Vector3 targetLookAt = CalcTargetLookAt(_targetPos);

	// ── 4c. クランプ＋ズーム（クランプに当たったらちょいズーム）────
	if (!m_rooms.empty())
	{
		const RoomBounds& cur = m_rooms[m_currentRoom];

		// 目標クランプ範囲（遷移中は次ルーム、通常は現ルーム）
		const RoomBounds& targetRoom = (m_isTransitioning && m_nextRoom < static_cast<int>(m_rooms.size()))
			? m_rooms[m_nextRoom] : cur;
		const float targetMinX = targetRoom.minX;
		const float targetMaxX = targetRoom.maxX;

		// 初回は即セット、以降は Lerp でじわっと追従（ルーム切替時のテレポート防止）
		if (!m_initialized)
		{
			m_clampMinX = targetMinX;
			m_clampMaxX = targetMaxX;
		}
		else
		{
			m_clampMinX = std::lerp(m_clampMinX, targetMinX, CameraConst::RoomTransitionSpeed * (1.0f / 60.0f));
			m_clampMaxX = std::lerp(m_clampMaxX, targetMaxX, CameraConst::RoomTransitionSpeed * (1.0f / 60.0f));
		}

		// ① baseOffsetZ でのクランプ境界（FOV端が壁を超えない位置）
		const float planeDist0 = std::abs(baseOffsetZ);
		const float halfView0  = (m_mProj._11 > 0.0f) ? (planeDist0 / m_mProj._11) : 0.0f;
		const float clampMin0  = m_clampMinX + halfView0;
		const float clampMax0  = m_clampMaxX - halfView0;

		// ② プレイヤーがクランプ境界を超えた量 → ズーム率を更新
		const float idealX = _targetPos.x;
		float overX = 0.0f;
		if      (idealX > clampMax0) { overX = idealX - clampMax0; }
		else if (idealX < clampMin0) { overX = clampMin0 - idealX; }

		// sqrt カーブでほんのりズーム
		const float targetZoomRate = std::clamp(
			std::sqrtf(overX * cs.WallZoomSensitivity),
			0.0f, cs.WallZoomMaxRatio);
		m_wallZoomRate = std::lerp(m_wallZoomRate, targetZoomRate, cs.WallZoomLerp);

		// ③ ズーム後の actualOffsetZ で halfViewX を再計算
		const float actualOffsetZ = baseOffsetZ * (1.0f - m_wallZoomRate);
		const float planeDist1    = std::abs(actualOffsetZ);
		const float halfView1     = (m_mProj._11 > 0.0f) ? (planeDist1 / m_mProj._11) : 0.0f;
		const float clampMin1     = m_clampMinX + halfView1;
		const float clampMax1     = m_clampMaxX - halfView1;

		// ④ 目標X を確定
		// 遷移中は起点(遷移開始時のカメラX) → 次ルームのminX+FOV左端 へスライド
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

		// ⑤ Z を即反映
		targetPos.z = actualOffsetZ;
	}

	// ── 4b. 初回は即座にセット ─────────────────────────────────
	if (!m_initialized)
	{
		m_pos       = targetPos;
		m_lookAtPos = targetLookAt;
		m_initialized = true;
	}

	// ── 5. 位置・注視点をゆっくり補間（ふわふわ） ──────────────
	// カメラYがプレイヤーより下に落ちると上を向いてしまうため、
	// targetYの最低値をプレイヤーY + 最小オフセットで保証する
	const float minCamY = _targetPos.y + CameraConst::MinCamOffsetY;
	if (targetPos.y < minCamY) { targetPos.y = minCamY; }

	m_pos       = Math::Vector3::Lerp(m_pos,       targetPos,    cs.PosLerp);
	m_lookAtPos = Math::Vector3::Lerp(m_lookAtPos, targetLookAt, cs.LookAtLerp);

	// lerp後も下回っていたら即座に矯正（初回やラグ対策）
	if (m_pos.y < minCamY) { m_pos.y = minCamY; }

	// ── 最終安全ガード ─────────────────────────────────────────
	// 注視点YがカメラYを超えると前方ベクトルが上向きになり空しか映らなくなる
	constexpr float kLookAtMargin = 0.5f;
	if (m_lookAtPos.y >= m_pos.y - kLookAtMargin)
	{
		m_lookAtPos.y = m_pos.y - kLookAtMargin;
	}

	// ── 6. ロール ──────────────────────────────────────────────
	const float offsetX       = _targetPos.x - m_pos.x;
	const float targetRollDeg = std::clamp(
		offsetX * cs.RollSensitivity,
		-cs.RollMaxDeg,
		 cs.RollMaxDeg);
	m_rollDeg = std::lerp(m_rollDeg, targetRollDeg, cs.RollLerp);

	// ── 7. LookAt 基底を構築 ────────────────────────────────────
	const Math::Vector3 worldUp(0.0f, 1.0f, 0.0f);

	Math::Vector3 fwd = m_lookAtPos - m_pos;

	// プレイヤーがカメラに近づきすぎると前方ベクトルが上を向く問題を防ぐ
	// 水平距離に対してピッチ（仰角）を最大 CameraConst::MaxPitchDeg 度に制限
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
	right.Normalize();

	Math::Vector3 up;
	fwd.Cross(right, up);
	up.Normalize();

	const float rollRad         = DirectX::XMConvertToRadians(m_rollDeg);
	const float cosR            = std::cosf(rollRad);
	const float sinR            = std::sinf(rollRad);
	const Math::Vector3 rolledRight = right * cosR + up * (-sinR);
	const Math::Vector3 rolledUp    = right * sinR + up *   cosR;

	// ── 9. カメラワールド行列を確定 ─────────────────────────────
	Math::Matrix mCam;
	mCam._11 = rolledRight.x; mCam._12 = rolledRight.y; mCam._13 = rolledRight.z; mCam._14 = 0.0f;
	mCam._21 = rolledUp.x;    mCam._22 = rolledUp.y;    mCam._23 = rolledUp.z;    mCam._24 = 0.0f;
	mCam._31 = fwd.x;         mCam._32 = fwd.y;         mCam._33 = fwd.z;         mCam._34 = 0.0f;
	mCam._41 = m_pos.x;       mCam._42 = m_pos.y;       mCam._43 = m_pos.z;       mCam._44 = 1.0f;

	SetCameraMatrix(mCam);
}
