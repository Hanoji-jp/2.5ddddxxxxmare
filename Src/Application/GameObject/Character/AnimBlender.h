#pragma once

//==========================================================
// AnimBlender
// 2つのアニメーションを指定フレーム数かけてブレンドするクラス
//
// 使い方:
//   Init(modelWork) でモデルを登録しておく
//   ChangeAnimation(animName, ...) で切り替え開始（ワンクッション版）
//   ChangeAnimation(spAnim, ...)   で直接データを渡す版
//   Update(modelWork) を毎フレーム呼ぶ
//==========================================================
class AnimBlender
{
public:
    AnimBlender()  {}
    ~AnimBlender() {}

    // m_pModelWork は外部オブジェクトの生アドレスのためコピー・ムーブ禁止
    AnimBlender(const AnimBlender&)            = delete;
    AnimBlender& operator=(const AnimBlender&) = delete;
    AnimBlender(AnimBlender&&)                 = delete;
    AnimBlender& operator=(AnimBlender&&)      = delete;

    // モデルワークを登録（ChangeAnimation(name) を使う前に呼ぶ）
    void Init(KdModelWork* _pModelWork) { m_pModelWork = _pModelWork; }

    // アニメーション切り替え ── 名前指定版（ワンクッション）
    // モデルからアニメを取得してから渡す。存在しない名前は無視。
    bool ChangeAnimation(const std::string& _animName,
                         bool  _isLoop      = true,
                         int   _blendFrames = 8);

    // アニメーション切り替え ── データ直渡し版（低レベル）
    void ChangeAnimation(const std::shared_ptr<KdAnimationData>& _spNext,
                         bool  _isLoop       = true,
                         int   _blendFrames  = 8);

    // 毎フレーム呼ぶ更新（speed: アニメ再生速度倍率）
    void Update(KdModelWork& _modelWork, float speed = 1.0f);

    // 現在のアニメーションが終了しているか（ループなしのとき有効）
    bool IsAnimationEnd() const { return m_currentAnimator.IsAnimationEnd(); }

    // ---- 上半身アニメ上書き ----
    // 指定ノード名集合だけ別アニメで上書きする（Walk 中 Attack など）
    void SetUpperBodyAnim(const std::shared_ptr<KdAnimationData>& _spAnim,
                          const std::vector<std::string>& _nodeNames,
                          bool _isLoop = false);
    void ClearUpperBodyAnim();
    bool IsUpperBodyAnimEnd() const { return !m_upperBodyActive || m_upperBodyAnimator.IsAnimationEnd(); }

private:
    // ノードリストのローカル行列を行列分解してLerpし直す
    static void BlendNodes(std::vector<KdModelWork::Node>&       _dst,
                           const std::vector<KdModelWork::Node>& _from,
                           const std::vector<KdModelWork::Node>& _to,
                           float                                  _t);

    KdModelWork* m_pModelWork = nullptr; // 登録モデル（非所有）

    KdAnimator m_currentAnimator;   // 現在再生中
    KdAnimator m_prevAnimator;      // ブレンド元（前のアニメ）

    // ブレンド用ノードスナップショット
    std::vector<KdModelWork::Node> m_prevNodes;

    float m_blendTime    = 0.0f;    // 経過ブレンドフレーム
    float m_blendFrames  = 0.0f;    // ブレンド総フレーム数
    bool  m_isBlending   = false;   // ブレンド中か

    // 上半身アニメ上書き
    KdAnimator               m_upperBodyAnimator;
    std::vector<std::string> m_upperBodyNodeNames; // 上書き対象ノード名
    bool                     m_upperBodyActive = false;
};

