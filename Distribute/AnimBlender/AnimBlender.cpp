/*
================================================================================
  AnimBlender  ──  アニメーションブレンダー（配布用）
  AnimBlender.h の実装ファイル
================================================================================
*/

// kd framework の必要なヘッダを環境に合わせてインクルードしてください
// 例:
// #include "../../Framework/Direct3D/KdModel.h"
// #include "../../Framework/Math/KdAnimation.h"
// または Pch.h でまとめてインクルードしている場合はそちらを使用
#include "AnimBlender.h"

// ─────────────────────────────────────────────────────────────────────────────
void AnimBlender::Init(KdModelWork* _pModelWork)
{
	m_pModelWork = _pModelWork;
}

// ─────────────────────────────────────────────────────────────────────────────
bool AnimBlender::ChangeAnimation(const std::string& _animName,
								  bool _isLoop,
								  int  _blendFrames)
{
	if (!m_pModelWork) { return false; }

	// モデルワーク経由でワンクッション取得（存在しない名前は nullptr）
	const auto spAnim = m_pModelWork->GetAnimation(_animName);
	if (!spAnim) { return false; }

	ChangeAnimation(spAnim, _isLoop, _blendFrames);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void AnimBlender::ChangeAnimation(const std::shared_ptr<KdAnimationData>& _spNext,
								  bool _isLoop,
								  int  _blendFrames)
{
	if (!_spNext) { return; }

	if (_blendFrames <= 0)
	{
		// 即切り替え
		m_currentAnimator.SetAnimation(_spNext, _isLoop);
		m_isBlending  = false;
		m_blendTime   = 0.0f;
		m_blendFrames = 0.0f;
		return;
	}

	// 前アニメーターを退避してブレンド開始
	m_prevAnimator = m_currentAnimator;
	m_currentAnimator.SetAnimation(_spNext, _isLoop);
	m_blendFrames = static_cast<float>(_blendFrames);
	m_blendTime   = 0.0f;
	m_isBlending  = true;
}

// ─────────────────────────────────────────────────────────────────────────────
void AnimBlender::Update(KdModelWork& _modelWork, float speed)
{
	if (!_modelWork.IsEnable()) { return; }

	auto& nodes = _modelWork.WorkNodes();

	if (m_isBlending)
	{
		// 前アニメ・新アニメそれぞれ1フレーム進めてブレンド
		std::vector<KdModelWork::Node> fromNodes = nodes;
		m_prevAnimator.AdvanceTime(fromNodes, speed);

		std::vector<KdModelWork::Node> toNodes = nodes;
		m_currentAnimator.AdvanceTime(toNodes, speed);

		const float t = m_blendTime / m_blendFrames;
		BlendNodes(nodes, fromNodes, toNodes, t);

		m_blendTime += 1.0f;
		if (m_blendTime >= m_blendFrames) { m_isBlending = false; }
	}
	else
	{
		m_currentAnimator.AdvanceTime(nodes, speed);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
bool AnimBlender::IsAnimationEnd() const
{
	return m_currentAnimator.IsAnimationEnd();
}

// ─────────────────────────────────────────────────────────────────────────────
void AnimBlender::BlendNodes(std::vector<KdModelWork::Node>&       _dst,
							 const std::vector<KdModelWork::Node>& _from,
							 const std::vector<KdModelWork::Node>& _to,
							 float                                  _t)
{
	const size_t count = _dst.size();
	if (_from.size() != count || _to.size() != count) { return; }

	for (size_t i = 0; i < count; ++i)
	{
		// XMMatrixDecompose はスタック上の XMVECTOR が必要（アラインメント）
		DirectX::XMVECTOR scaleFrom, rotFrom, transFrom;
		DirectX::XMVECTOR scaleTo,   rotTo,   transTo;

		DirectX::XMMatrixDecompose(&scaleFrom, &rotFrom, &transFrom,
								   _from[i].m_localTransform);
		DirectX::XMMatrixDecompose(&scaleTo,   &rotTo,   &transTo,
								   _to[i].m_localTransform);

		const DirectX::XMVECTOR blendScale =
			DirectX::XMVectorLerp(scaleFrom, scaleTo, _t);
		const DirectX::XMVECTOR blendRot =
			DirectX::XMQuaternionSlerp(rotFrom, rotTo, _t);
		const DirectX::XMVECTOR blendTrans =
			DirectX::XMVectorLerp(transFrom, transTo, _t);

		_dst[i].m_localTransform =
			DirectX::XMMatrixScalingFromVector(blendScale)
			* DirectX::XMMatrixRotationQuaternion(blendRot)
			* DirectX::XMMatrixTranslationFromVector(blendTrans);
	}
}
