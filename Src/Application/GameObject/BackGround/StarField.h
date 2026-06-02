#pragma once

// スカイボックス内にランダム配置する星空オブジェクト
// DrawBright() で発光描画し、既存のブルームで光らせる
class StarField : public KdGameObject
{
public:
	StarField()          { Init(); }
	virtual ~StarField() {}

	void Init()       override;
	void Update()     override;
	void DrawBright() override;

	bool IsVisible() const override { return true; }

private:
	// 1つの星の情報
	struct Star
	{
		Math::Vector3 BasePos;   // 星の基準ワールド座標（生成時のカメラ基準）
		float         Scale;     // 星の大きさ
		float         Emissive;  // 発光強度
		Math::Vector3 Color;     // 星の色
		float         TwinklePhase; // 明滅の位相
		float         Parallax;  // 追従割合（近い星=小さい, 遠い星=1に近い）
	};

	KdModelWork       m_modelWork;
	std::vector<Star> m_stars;
	float             m_time = 0.0f; // 明滅用の経過時間
	Math::Vector3     m_originCamPos{ 0.0f, 0.0f, 0.0f }; // 生成時のカメラ位置
};
