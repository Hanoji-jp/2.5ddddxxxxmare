#pragma once
#include <memory>
#include <deque>

class KdTexture;

//==========================================================
// CursorManager
// 自前のマウスカーソル（OSカーソルは隠す）。
//  ・解像度／画面モードに依存しない位置（MouseInputで変換）。
//  ・ステージ飛来トレイル風の残像を引く。
//  ・F3エディタ中はOSカーソルを使うので自前カーソルは出さない。
//==========================================================
class CursorManager
{
public:
	static CursorManager& Instance()
	{
		static CursorManager instance;
		return instance;
	}

	void Update();   // 位置更新＋トレイル更新（毎フレーム）
	void Draw();     // スプライト最前面に描画（スプライトバッチ内で呼ぶ）

	// 自前カーソルの抑制（showcase・クリア演出など、操作不要な演出中に非表示にする）。
	// true の間は描画・クリック判定・トレイルを止める。毎フレーム設定すること。
	void SetSuppressed(bool s) { m_suppressed = s; }

	// ── UIクリック用（スプライト座標：中心原点・+Y上）──
	bool  IsActive() const { return m_valid; }            // 自前カーソルが有効か
	float PosX()     const { return m_x; }
	float PosY()     const { return m_y; }
	bool  Clicked()  const { return m_clicked; }          // 左クリックした瞬間
	bool  ClickHeld()const { return m_lmb; }              // 左ボタン押下中
	// 中心(cx,cy)・半幅(hx)・半高(hy) の矩形にカーソルが乗っているか
	bool  HitRect(float cx, float cy, float hx, float hy) const
	{
		return m_valid && (m_x >= cx - hx) && (m_x <= cx + hx) && (m_y >= cy - hy) && (m_y <= cy + hy);
	}

	// ── パララックス用：画面中心からの正規化マウス位置（-1..1, 平滑化）。
	//    マウスが無い時は0へ減衰。x:右が+ / y:上が+ ──
	float NormX() const { return m_normX; }
	float NormY() const { return m_normY; }

private:
	CursorManager() = default;
	~CursorManager() = default;
	CursorManager(const CursorManager&) = delete;
	CursorManager& operator=(const CursorManager&) = delete;

	void EnsureTexture();
	void SetOsCursorHidden(bool hide);

	std::shared_ptr<KdTexture> m_tex;
	bool  m_init = false;

	struct Pt { float x = 0.0f; float y = 0.0f; float age = 0.0f; };   // age:経過秒（FPS非依存の寿命管理）
	std::deque<Pt> m_trail;   // 先頭が最新

	float m_x = 0.0f, m_y = 0.0f;   // 現在位置（スプライト座標：中心原点・+Y上）
	bool  m_valid = false;          // この場で描画してよいか
	bool  m_osHidden = false;       // OSカーソルを隠している状態か
	bool  m_suppressed = false;     // 演出中などで自前カーソルを抑制中か

	bool  m_lmb     = false;        // 左ボタン押下中
	bool  m_lmbPrev = false;        // 前フレームの左ボタン
	bool  m_clicked = false;        // この瞬間に押した（エッジ）

	float m_normX = 0.0f, m_normY = 0.0f;   // 画面中心からの正規化マウス位置（平滑化・パララックス用）
};
