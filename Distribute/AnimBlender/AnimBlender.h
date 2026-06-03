#pragma once
#include <string>
#include <vector>
#include <memory>

class AnimBlender
{
public:
	AnimBlender()  = default;
	~AnimBlender() = default;

	// モデルワークを登録する。ChangeAnimation(name,...) を使う前に必ず呼ぶこと。
	// _pModelWork : 非所有ポインタ。寿命はこのクラスより長くすること。
	void Init(KdModelWork* _pModelWork);

	// アニメーション切り替え ── 名前指定版（ワンクッション）
	// 戻り値 : アニメが見つかり切り替えに成功したら true
	bool ChangeAnimation(const std::string& _animName,
						 bool _isLoop      = true,
						 int  _blendFrames = 8);

	// アニメーション切り替え ── データ直渡し版（低レベル）
	void ChangeAnimation(const std::shared_ptr<KdAnimationData>& _spNext,
						 bool _isLoop      = true,
						 int  _blendFrames = 8);

	// 毎フレーム呼ぶ。モデルワークのノードにアニメ結果を書き込む。
	// speed : 再生速度倍率（1.0 = 等倍）
	void Update(KdModelWork& _modelWork, float speed = 1.0f);

	// 現在のアニメーションが終端に達しているか（ループなし用）
	bool IsAnimationEnd() const;

private:
	// 各ノードの localTransform を Decompose して Lerp/Slerp で再合成する
	static void BlendNodes(std::vector<KdModelWork::Node>&       _dst,
						   const std::vector<KdModelWork::Node>& _from,
						   const std::vector<KdModelWork::Node>& _to,
						   float                                  _t);

	KdModelWork* m_pModelWork    = nullptr; // 非所有ポインタ（Init()で登録）

	KdAnimator   m_currentAnimator;         // 現在再生中のアニメーター
	KdAnimator   m_prevAnimator;            // ブレンド元アニメーター

	float        m_blendTime    = 0.0f;     // 経過ブレンドフレーム数
	float        m_blendFrames  = 0.0f;     // ブレンド総フレーム数
	bool         m_isBlending   = false;    // ブレンド中フラグ
};
