#include "../../../Pch.h"
#include "AnimBlender.h"

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

void AnimBlender::ChangeAnimation(const std::shared_ptr<KdAnimationData>& _spNext,
                                  bool _isLoop,
                                  int  _blendFrames)
{
    if (!_spNext) { return; }

    // ブレンドフレームが0なら即切り替え
    if (_blendFrames <= 0)
    {
        m_currentAnimator.SetAnimation(_spNext, _isLoop);
        m_isBlending  = false;
        m_blendTime   = 0.0f;
        m_blendFrames = 0.0f;
        return;
    }

    // 前のアニメーターを退避（prevNodesは呼び出し元のUpdateで最新状態が入っている）
    m_prevAnimator = m_currentAnimator;

    // 新しいアニメーションをセット
    m_currentAnimator.SetAnimation(_spNext, _isLoop);

    // ブレンド開始
    m_blendFrames = static_cast<float>(_blendFrames);
    m_blendTime   = 0.0f;
    m_isBlending  = true;
}

void AnimBlender::Update(KdModelWork& _modelWork, float speed)
{
	// m_pModelWork が Init() 済みで、渡された modelWork と一致しているか確認
	// ポインタがズレている場合は再登録して続行
	if (m_pModelWork != &_modelWork) { m_pModelWork = &_modelWork; }

	if (!_modelWork.IsEnable()) { return; }

	// 書き込み可能なノードリストを取得
	auto& nodes = _modelWork.WorkNodes();
	if (nodes.empty()) { return; }

	// フレームレート非依存：60fps基準のフレーム換算でアニメを進める
	// （高FPSでアニメが速くなる／低FPSで遅くなるのを防ぐ）
	const float frameScale = KdFPSController::GetDt() * 60.0f;
	speed *= frameScale;

	if (m_isBlending)
	{

		//ここに関しては京ちゃんのコードと仕様が全く違うからあまり参考にはならんかも
		//おいどんはmodelの中にアニメーションをぶち込んでいるので
		// ----- ブレンド中 -----

		// 前アニメのノード状態をコピーして進める
		std::vector<KdModelWork::Node> fromNodes = nodes;
		m_prevAnimator.AdvanceTime(fromNodes, speed);

		// 新アニメのノード状態をコピーして進めるｄ
		std::vector<KdModelWork::Node> toNodes = nodes;
		m_currentAnimator.AdvanceTime(toNodes, speed);

		// ブレンド比率（0→1）
		const float t = m_blendTime / m_blendFrames;

		// ノードをブレンド
		BlendNodes(nodes, fromNodes, toNodes, t);

		m_blendTime += frameScale;

		if (m_blendTime >= m_blendFrames)
		{
			m_isBlending = false;
		}
	}
	else
	{
		// ----- 通常再生 -----
		m_currentAnimator.AdvanceTime(nodes, speed);
	}

	// ---- 上半身アニメ上書き ----
	if (m_upperBodyActive)
	{
		// 上半身だけ別アニメで進めてノードを上書き
		std::vector<KdModelWork::Node> upperNodes = nodes;
		m_upperBodyAnimator.AdvanceTime(upperNodes, speed);

		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const std::string& name = nodes[i].m_name;
			for (const auto& target : m_upperBodyNodeNames)
			{
				if (name == target)
				{
					nodes[i].m_localTransform = upperNodes[i].m_localTransform;
					break;
				}
			}
		}

		if (m_upperBodyAnimator.IsAnimationEnd()) { m_upperBodyActive = false; }
	}
}

void AnimBlender::BlendNodes(std::vector<KdModelWork::Node>&       _dst,
                             const std::vector<KdModelWork::Node>& _from,
                             const std::vector<KdModelWork::Node>& _to,
                             float                                  _t)
{
    const size_t count = _dst.size();
    if (_from.size() != count || _to.size() != count) { return; }

    for (size_t i = 0; i < count; ++i)
    {
        // XMMatrixDecompose は１６バイトが必要なので専用変数をつかえよ
        DirectX::XMVECTOR scaleFrom, rotFrom, transFrom;
        DirectX::XMVECTOR scaleTo,   rotTo,   transTo;

        DirectX::XMMatrixDecompose(&scaleFrom, &rotFrom, &transFrom, _from[i].m_localTransform);
        DirectX::XMMatrixDecompose(&scaleTo,   &rotTo,   &transTo,   _to[i].m_localTransform);

        // 各Lerp / Slerp
        const DirectX::XMVECTOR blendScale = DirectX::XMVectorLerp(scaleFrom, scaleTo, _t);
        const DirectX::XMVECTOR blendRot   = DirectX::XMQuaternionSlerp(rotFrom, rotTo, _t);
        const DirectX::XMVECTOR blendTrans = DirectX::XMVectorLerp(transFrom, transTo, _t);

        // 行列のやつ
        _dst[i].m_localTransform =
            DirectX::XMMatrixScalingFromVector(blendScale)
            * DirectX::XMMatrixRotationQuaternion(blendRot)
            * DirectX::XMMatrixTranslationFromVector(blendTrans);
    }
}

void AnimBlender::SetUpperBodyAnim(const std::shared_ptr<KdAnimationData>& _spAnim,
                                   const std::vector<std::string>& _nodeNames,
                                   bool _isLoop)
{
    if (!_spAnim) { return; }
    m_upperBodyAnimator.SetAnimation(_spAnim, _isLoop);
    m_upperBodyNodeNames = _nodeNames;
    m_upperBodyActive    = true;
}

void AnimBlender::ClearUpperBodyAnim()
{
    m_upperBodyActive = false;
    m_upperBodyNodeNames.clear();
}

