#include "../main.h"
#include "CursorManager.h"
#include "../Util/MouseInput.h"
#include <algorithm>
#include <cmath>

namespace
{
	// トレイル/カーソルの見た目
	constexpr const char* kTexPath    = "Asset/Texture/Glow.png";   // 飛来トレイルと同じグロー
	constexpr int   kTrailMax    = 160;    // 残像の最大数（補間点を含むキャップ）
	constexpr float kTrailLife   = 0.18f;  // 残像の寿命(秒)。FPSに依らず一定時間で消える
	constexpr float kTrailStep   = 6.0f;   // 補間点の間隔(px)。小さいほど滑らか
	constexpr int   kTrailStepMax= 40;     // 1フレームに足す補間点の上限
	constexpr float kHeadSize    = 30.0f;  // 先頭（カーソル本体）の大きさ(px)
	constexpr float kTrailSize   = 24.0f;  // 残像の最大サイズ(px)
	constexpr float kHeadAlpha   = 1.0f;
	constexpr float kTrailAlpha  = 0.55f;  // 残像の最大アルファ
	// 色（ステージ飛来トレイル風：シアン〜白）
	constexpr float kHeadR = 1.0f,  kHeadG = 1.0f,  kHeadB = 1.0f;
	constexpr float kTailR = 0.45f, kTailG = 0.85f, kTailB = 1.0f;
}

void CursorManager::EnsureTexture()
{
	if (m_init) { return; }
	m_init = true;
	m_tex = std::make_shared<KdTexture>();
	if (!m_tex->Load(kTexPath)) { m_tex = nullptr; }
}

void CursorManager::SetOsCursorHidden(bool hide)
{
	if (hide == m_osHidden) { return; }
	m_osHidden = hide;
	ShowCursor(hide ? FALSE : TRUE);
}

void CursorManager::Update()
{
	EnsureTexture();

	// 抑制フラグは「毎フレーム要求」方式：ここで読み取って即リセットする。
	// こうすると要求元のシーンを抜ければ自動でカーソルが復帰する（フラグが残り続けない）。
	const bool suppressed = m_suppressed;
	m_suppressed = false;

	// 左クリックのエッジ（UIクリック用）。位置が無効なフレームは押下扱いしない。
	const bool lmbNow = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	// F3エディタ（ImGuiビューポート）中はOSカーソルを使う＝自前カーソルは出さない
	if (KdDebugGUI::Instance().IsGameViewport())
	{
		m_valid = false;
		m_clicked = false;
		m_lmb = lmbNow; m_lmbPrev = lmbNow;
		m_normX *= 0.9f; m_normY *= 0.9f;   // パララックスを中心へ戻す
		SetOsCursorHidden(false);
		m_trail.clear();
		return;
	}

	// 演出中（showcase・クリア演出など）は自前カーソルを完全に非表示にする。
	// 操作不要なので描画・クリック・トレイルを止め、OSカーソルも隠したままにする。
	if (suppressed)
	{
		m_valid = false;
		m_clicked = false;
		m_lmb = lmbNow; m_lmbPrev = lmbNow;
		m_normX *= 0.9f; m_normY *= 0.9f;
		SetOsCursorHidden(true);
		m_trail.clear();
		return;
	}

	HWND hwnd = Application::Instance().GetWindowHandle();
	float nx = 0.0f, ny = 0.0f;
	if (!MouseInput::ClientNDC(hwnd, nx, ny))
	{
		m_valid = false;
		m_clicked = false;
		m_lmb = lmbNow; m_lmbPrev = lmbNow;
		m_normX *= 0.9f; m_normY *= 0.9f;
		return;
	}

	// クリックエッジ更新
	m_clicked = lmbNow && !m_lmbPrev;
	m_lmb     = lmbNow;
	m_lmbPrev = lmbNow;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float bw = static_cast<float>(bb->GetInfo().Width);
	const float bh = static_cast<float>(bb->GetInfo().Height);

	// クライアント正規化 → 描画解像度のスプライト座標（中心原点・+Y上）
	m_x = nx * bw - bw * 0.5f;
	m_y = bh * 0.5f - ny * bh;
	m_valid = true;

	// パララックス用：画面中心からの正規化(-1..1)を平滑化
	const float tnx = (nx - 0.5f) * 2.0f;
	const float tny = -(ny - 0.5f) * 2.0f;   // 上が+
	m_normX += (tnx - m_normX) * 0.15f;
	m_normY += (tny - m_normY) * 0.15f;

	SetOsCursorHidden(true);

	// トレイル更新（時間ベース＝FPSに依らず一定時間で消える）
	const float dt = KdFPSController::GetDt();
	for (auto& p : m_trail) { p.age += dt; }
	while (!m_trail.empty() && m_trail.back().age >= kTrailLife) { m_trail.pop_back(); }

	// 前回位置→今回位置の間を一定間隔で補間して点を足す（高速移動でも途切れず滑らか）
	if (!m_trail.empty())
	{
		const Pt prev = m_trail.front();
		const float ddx = m_x - prev.x, ddy = m_y - prev.y;
		const float dist = std::sqrt(ddx * ddx + ddy * ddy);
		const int   n = std::min(static_cast<int>(dist / kTrailStep), kTrailStepMax);
		for (int k = 1; k <= n; ++k)
		{
			const float t = static_cast<float>(k) / static_cast<float>(n + 1);
			m_trail.push_front({ prev.x + ddx * t, prev.y + ddy * t, 0.0f });
		}
	}
	m_trail.push_front({ m_x, m_y, 0.0f });
	while (static_cast<int>(m_trail.size()) > kTrailMax) { m_trail.pop_back(); }
}

void CursorManager::Draw()
{
	if (!m_valid || !m_tex) { return; }

	auto& sm     = KdShaderManager::Instance();
	auto& sprite = sm.m_spriteShader;

	// 残像（加算で尾を引く。経過時間で薄く小さく＝FPS非依存）
	sm.ChangeBlendState(KdBlendState::Add);
	const int n = static_cast<int>(m_trail.size());
	for (int i = n - 1; i >= 1; --i)   // 古い→新しい（先頭=本体は別途）
	{
		const float fade = 1.0f - std::clamp(m_trail[i].age / kTrailLife, 0.0f, 1.0f);   // 1=新しい,0=寿命
		const float sz = kTrailSize * (0.35f + 0.65f * fade);
		const Math::Color c(kTailR, kTailG, kTailB, kTrailAlpha * fade * fade);
		sprite.DrawTex(m_tex.get(), static_cast<int>(m_trail[i].x), static_cast<int>(m_trail[i].y),
			static_cast<int>(sz), static_cast<int>(sz), nullptr, &c);
	}
	sm.UndoBlendState();

	// 本体（先頭）
	const Math::Color head(kHeadR, kHeadG, kHeadB, kHeadAlpha);
	sprite.DrawTex(m_tex.get(), static_cast<int>(m_x), static_cast<int>(m_y),
		static_cast<int>(kHeadSize), static_cast<int>(kHeadSize), nullptr, &head);
}
