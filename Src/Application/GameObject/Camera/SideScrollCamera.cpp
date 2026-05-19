#include "../../../Pch.h"
#include "SideScrollCamera.h"

void SideScrollCamera::Update(const Math::Vector3& _targetPos)
{
    // 追従の滑らかさ（線形補間係数）
    constexpr float FollowLerp = 0.1f;

    const Math::Vector3 targetCamPos =
    {
        _targetPos.x,
        _targetPos.y + GameConst::CameraOffsetY,
        GameConst::CameraOffsetZ
    };

    m_pos = Math::Vector3::Lerp(m_pos, targetCamPos, FollowLerp);

    Math::Matrix mCam = Math::Matrix::CreateTranslation(m_pos);
    SetCameraMatrix(mCam);
}
