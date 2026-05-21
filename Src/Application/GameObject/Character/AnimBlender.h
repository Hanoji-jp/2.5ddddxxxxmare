#pragma once

//==========================================================
// AnimBlender
// 2つのアニメーションを指定フレーム数かけてブレンドするクラス
//
// 使い方:
//   ChangeAnimation(newAnimData, blendFrames) で切り替え開始
//   Update(modelWork) を毎フレーム呼ぶ
//==========================================================
class AnimBlender
{
public:
    AnimBlender()  {}
    ~AnimBlender() {}

    // アニメーション切り替え（ブレンド開始）
    // _blendFrames : ブレンドにかけるフレーム数（0なら即切り替え）
    void ChangeAnimation(const std::shared_ptr<KdAnimationData>& _spNext,
                         bool  _isLoop       = true,
                         int   _blendFrames  = 8);

    // 毎フレーム呼ぶ更新
    void Update(KdModelWork& _modelWork);

    // 現在のアニメーションが終了しているか（ループなしのとき有効）
    bool IsAnimationEnd() const { return m_currentAnimator.IsAnimationEnd(); }

private:
    // ノードリストのローカル行列を行列分解してLerpし直す
    static void BlendNodes(std::vector<KdModelWork::Node>&       _dst,
                           const std::vector<KdModelWork::Node>& _from,
                           const std::vector<KdModelWork::Node>& _to,
                           float                                  _t);

    KdAnimator m_currentAnimator;   // 現在再生中
    KdAnimator m_prevAnimator;      // ブレンド元（前のアニメ）

    // ブレンド用ノードスナップショット
    std::vector<KdModelWork::Node> m_prevNodes;

    float m_blendTime    = 0.0f;    // 経過ブレンドフレーム
    float m_blendFrames  = 0.0f;    // ブレンド総フレーム数
    bool  m_isBlending   = false;   // ブレンド中か
};
