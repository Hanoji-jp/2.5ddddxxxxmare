#include "../../../Pch.h"
#include "SideScrollCamera.h"

void SideScrollCamera::SetRooms(const std::vector<RoomBounds>& _rooms)
{
	m_rooms = _rooms;
	m_currentRoom = 0;
	m_nextRoom = 0;
	m_isTransitioning = false;
	m_transitionRate = 0.0f;
}

void SideScrollCamera::Update(const Math::Vector3& _targetPos)
{
	if (m_rooms.empty())
	{
        m_lookAtPos = _targetPos + Math::Vector3(0.0f, CameraConst::LookAtHeight, 0.0f);
		Math::Matrix mCam = Math::Matrix::CreateTranslation(m_pos);
		SetCameraMatrix(mCam);
		return;
	}

	const RoomBounds& currentRoom = m_rooms[m_currentRoom];

	if (m_currentRoom + 1 < static_cast<int>(m_rooms.size()))
	{
		const int candidateNextRoom = m_currentRoom + 1;
		const RoomBounds& nextRoom = m_rooms[candidateNextRoom];
		const float startBlendX = currentRoom.triggerX - currentRoom.blendX;
		const float endBlendX = currentRoom.triggerX + currentRoom.blendX;

		if (_targetPos.x <= startBlendX)
		{
			m_isTransitioning = false;
			m_transitionRate = 0.0f;
			m_nextRoom = m_currentRoom;
		}
		else if (_targetPos.x >= endBlendX)
		{
			m_currentRoom = candidateNextRoom;
			m_nextRoom = m_currentRoom;
			m_isTransitioning = false;
			m_transitionRate = 0.0f;
		}
		else
		{
			const float blendSpan = endBlendX - startBlendX;
			m_nextRoom = candidateNextRoom;
			m_transitionRate = (blendSpan > 0.0f) ? ((_targetPos.x - startBlendX) / blendSpan) : 1.0f;
			m_isTransitioning = true;
		}
	}
	else
	{
		m_nextRoom = m_currentRoom;
		m_transitionRate = 0.0f;
		m_isTransitioning = false;
	}

  const Math::Vector3 roomCenter = GetBlendedRoomCenter(_targetPos);
	const Math::Vector3 followCenter = GetFollowRoomCenter(_targetPos);
	const Math::Vector3 lookAtPoint = GetLookAtPoint(_targetPos);

     const Math::Vector3 cameraCenter = Math::Vector3::Lerp(roomCenter, followCenter, 0.5f);
	 m_pos = cameraCenter + Math::Vector3(0.0f, CameraConst::FloatHeight, CameraConst::OffsetZ);
	m_lookAtPos = lookAtPoint;

	// 3Dの向きは固定に近くし、見え方は位置と注視点で作る
	Math::Matrix mCam = Math::Matrix::CreateTranslation(m_pos);
	SetCameraMatrix(mCam);
}

Math::Vector3 SideScrollCamera::GetRoomCenter() const
{
	if (m_rooms.empty())
	{
		return m_pos;
	}

	const RoomBounds& room = m_rooms[m_currentRoom];
	return
	{
		(room.minX + room.maxX) * 0.5f,
		(room.minY + room.maxY) * 0.5f,
		0.0f
	};
}

Math::Vector3 SideScrollCamera::GetBlendedRoomCenter(const Math::Vector3& _targetPos) const
{
	if (m_rooms.empty())
	{
		return m_pos;
	}

	const Math::Vector3 currentCenter = GetRoomCenter();
	if (!m_isTransitioning || m_nextRoom == m_currentRoom || m_nextRoom >= static_cast<int>(m_rooms.size()))
	{
		return currentCenter;
	}

	const RoomBounds& nextRoom = m_rooms[m_nextRoom];
	const Math::Vector3 nextCenter =
	{
		(nextRoom.minX + nextRoom.maxX) * 0.5f,
		(nextRoom.minY + nextRoom.maxY) * 0.5f,
		0.0f
	};

	return Math::Vector3::Lerp(currentCenter, nextCenter, std::clamp(m_transitionRate, 0.0f, 1.0f));
}

Math::Vector3 SideScrollCamera::GetFollowRoomCenter(const Math::Vector3& _targetPos) const
{
	const Math::Vector3 roomCenter = GetBlendedRoomCenter(_targetPos);
	const Math::Vector3 followOffset =
	{
		(_targetPos.x - roomCenter.x) * CameraConst::FollowWeightX,
		(_targetPos.y - roomCenter.y) * CameraConst::FollowWeightY,
		0.0f
	};

	return roomCenter + followOffset;
}

Math::Vector3 SideScrollCamera::GetLookAtPoint(const Math::Vector3& _targetPos) const
{
	const Math::Vector3 forwardLook =
	{
		_targetPos.x + CameraConst::LookAheadX,
		_targetPos.y + CameraConst::LookAheadY,
		0.0f
	};

  return forwardLook;
}
