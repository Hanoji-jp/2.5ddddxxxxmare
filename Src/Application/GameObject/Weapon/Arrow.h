#pragma once

// 矢モデル（発射体）クラス
// 発射後は飛翔して一定距離で消滅する
class Arrow : public KdGameObject
{
public:
    Arrow()          {}
    virtual ~Arrow() {}

    void Init()    override;
    void Update()  override;
    void DrawLit() override;

    bool IsVisible() const override { return true; }

    // 発射：位置と進行方向を設定する
    void Launch(const Math::Vector3& _pos, const Math::Vector3& _dir);

private:
    KdModelWork  m_modelWork;
    Math::Vector3 m_velocity  = { 0.0f, 0.0f, 0.0f };
    float        m_travelDist = 0.0f;         // 飛距離累計
};
