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

	// Y はプレイヤー直追尾（roomCenter 経由をやめて誤差をなくす）
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
	if (!m_rooms.empty() && m_currentRoom + 1 < static_cast<int>(m_rooms.size()))
	{
		const int         nextIdx  = m_currentRoom + 1;
		const RoomBounds& cur      = m_rooms[m_currentRoom];
		const float       startX   = cur.triggerX - cur.blendX;
		const float       endX     = cur.triggerX + cur.blendX;

		if (_targetPos.x <= startX)
		{
			m_isTransitioning = false;
			m_transitionRate  = 0.0f;
			m_nextRoom        = m_currentRoom;
		}
		else if (_targetPos.x >= endX)
		{
			m_currentRoom     = nextIdx;
			m_nextRoom        = m_currentRoom;
			m_isTransitioning = false;
			m_transitionRate  = 0.0f;
		}
		else
		{
			const float span  = endX - startX;
			m_nextRoom        = nextIdx;
			m_transitionRate  = (span > 0.0f) ? ((_targetPos.x - startX) / span) : 1.0f;
			m_isTransitioning = true;
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

		// ① baseOffsetZ でのクランプ境界（FOV端が壁を超えない位置）
		const float planeDist0 = std::abs(baseOffsetZ);
		const float halfView0  = (m_mProj._11 > 0.0f) ? (planeDist0 / m_mProj._11) : 0.0f;
		const float clampMin0  = cur.minX + halfView0;
		const float clampMax0  = cur.maxX - halfView0;

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
		//    ズームで寄った分だけ halfViewX が縮み、clampMax が広がってプレイヤーを追える
		const float actualOffsetZ = baseOffsetZ * (1.0f - m_wallZoomRate);
		const float planeDist1    = std::abs(actualOffsetZ);
		const float halfView1     = (m_mProj._11 > 0.0f) ? (planeDist1 / m_mProj._11) : 0.0f;
		const float clampMin1     = cur.minX + halfView1;
		const float clampMax1     = cur.maxX - halfView1;

		// ④ 目標をズーム後クランプ範囲に収める（MAX は絶対超えない）
		if (clampMin1 <= clampMax1)
		{
			targetPos.x    = std::clamp(idealX,          clampMin1, clampMax1);
			targetLookAt.x = std::clamp(targetLookAt.x,  clampMin1, clampMax1);
		}
		else
		{
			const float center = (cur.minX + cur.maxX) * 0.5f;
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
	m_pos       = Math::Vector3::Lerp(m_pos,       targetPos,    cs.PosLerp);
	m_lookAtPos = Math::Vector3::Lerp(m_lookAtPos, targetLookAt, cs.LookAtLerp);

	// Lerp 後も壁外に出ないよう再クランプ（FOV漏れ防止）
	if (!m_rooms.empty())
	{
		const RoomBounds& cur    = m_rooms[m_currentRoom];
		const float planeDist    = std::abs(m_pos.z);
		const float halfViewX    = (m_mProj._11 > 0.0f) ? (planeDist / m_mProj._11) : 0.0f;
		const float clampMin     = cur.minX + halfViewX;
		const float clampMax     = cur.maxX - halfViewX;
		if (clampMin <= clampMax)
		{
			m_pos.x       = std::clamp(m_pos.x,       clampMin, clampMax);
			m_lookAtPos.x = std::clamp(m_lookAtPos.x, clampMin, clampMax);
		}
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
