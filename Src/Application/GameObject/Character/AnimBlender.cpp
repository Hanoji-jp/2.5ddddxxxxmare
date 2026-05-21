#include "../../../Pch.h"
#include "AnimBlender.h"

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

void AnimBlender::Update(KdModelWork& _modelWork)
{
    if (!_modelWork.IsEnable()) { return; }

    // 書き込み可能なノードリストを取得
    auto& nodes = _modelWork.WorkNodes();

    if (m_isBlending)
    {
        // ----- ブレンド中 -----

        // 前アニメのノード状態をコピーして進める
        std::vector<KdModelWork::Node> fromNodes = nodes;
        m_prevAnimator.AdvanceTime(fromNodes);

        // 新アニメのノード状態をコピーして進める
        std::vector<KdModelWork::Node> toNodes = nodes;
        m_currentAnimator.AdvanceTime(toNodes);

        // ブレンド比率（0→1）
        const float t = m_blendTime / m_blendFrames;

        // ノードをブレンド
        BlendNodes(nodes, fromNodes, toNodes, t);

        m_blendTime += 1.0f;

        if (m_blendTime >= m_blendFrames)
        {
            m_isBlending = false;
        }
    }
    else
    {
        // ----- 通常再生 -----
        m_currentAnimator.AdvanceTime(nodes);
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
        // XMMatrixDecompose は XMVECTOR（16バイト）が必要なので専用変数を使う
        DirectX::XMVECTOR scaleFrom, rotFrom, transFrom;
        DirectX::XMVECTOR scaleTo,   rotTo,   transTo;

        DirectX::XMMatrixDecompose(&scaleFrom, &rotFrom, &transFrom, _from[i].m_localTransform);
        DirectX::XMMatrixDecompose(&scaleTo,   &rotTo,   &transTo,   _to[i].m_localTransform);

        // 各成分をLerp / Slerp
        const DirectX::XMVECTOR blendScale = DirectX::XMVectorLerp(scaleFrom, scaleTo, _t);
        const DirectX::XMVECTOR blendRot   = DirectX::XMQuaternionSlerp(rotFrom, rotTo, _t);
        const DirectX::XMVECTOR blendTrans = DirectX::XMVectorLerp(transFrom, transTo, _t);

        // 行列を再構築
        _dst[i].m_localTransform =
            DirectX::XMMatrixScalingFromVector(blendScale)
            * DirectX::XMMatrixRotationQuaternion(blendRot)
            * DirectX::XMMatrixTranslationFromVector(blendTrans);
    }
}
