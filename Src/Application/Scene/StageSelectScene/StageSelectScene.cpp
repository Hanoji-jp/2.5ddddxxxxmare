#include "StageSelectScene.h"
#include "../SceneManager.h"
#include "../../GameObject/BackGround/BackGround.h"
#include "../../GameObject/BackGround/StarField.h"
#include "../../GameObject/Light/PointLightObject.h"
#include "../../GameObject/Effect/EffectBase.h"
#include "../../Manager/ModelManager.h"
#include "../../Manager/StageManager.h"
#include "../../Const/StageSelectConst.h"
#include "../../Const/ResultConst.h"
#include "../../Const/ResultTextConst.h"
#include "../../Const/OutlineConst.h"
#include "../../Util/TextFx.h"

using namespace StageSelectConst;

namespace
{
	// ノードの色（ステージごとの色味）
	const Math::Vector3 kNodeColors[] = {
		{ 0.5f, 0.85f, 1.0f },   // シアン
		{ 1.0f, 0.8f,  0.45f },  // 暖色
		{ 0.7f, 1.0f,  0.6f },   // 緑
		{ 1.0f, 0.6f,  0.8f },   // ピンク
		{ 0.8f, 0.7f,  1.0f },   // 紫
		{ 1.0f, 0.95f, 0.55f },  // 黄
	};
}

//----------------------------------------------------------
void StageSelectScene::Init()
{
	// ── カメラ（固定の見下ろし気味アイソメ。Event で微ゆらぎ）──
	auto cam = std::make_shared<KdCamera>();
	cam->SetProjectionMatrix(CamFov);
	m_camera = cam;

	// ── ノード配置（2行×3列のグリッド＋道で接続）──
	//   row0(画面下/手前 z=-6): 0,1,2 ／ row1(画面上/奥 z=+6): 3,4,5
	const Math::Vector3 layout[6] = {
		{ -12.0f, 0.0f, -6.0f }, { 0.0f, 0.0f, -6.0f }, { 12.0f, 0.0f, -6.0f },
		{ -12.0f, 0.0f,  6.0f }, { 0.0f, 0.0f,  6.0f }, { 12.0f, 0.0f,  6.0f },
	};
	m_nodes.clear();
	for (int i = 0; i < 6; ++i)
	{
		Node n;
		n.pos     = layout[i];
		n.stageId = i;
		n.color   = kNodeColors[i % (sizeof(kNodeColors) / sizeof(kNodeColors[0]))];
		m_nodes.push_back(n);
	}
	// 道（描画用：隣接の横＋縦）
	m_links = {
		{0,1},{1,2}, {3,4},{4,5},   // 横（各行）
		{0,3},{1,4},{2,5},          // 縦（行間）
	};
	m_current = 0;   // 左下スタート（W で上の行へ行けるように）
	m_camFocus = m_nodes[m_current].pos;   // カメラ初期注視点

	// ── ノードの箱モデル（共有）──
	{
		const auto spData = ModelManager::Instance().GetModel(NodeModel);
		if (spData) { m_nodeModel.SetModelData(spData); }
	}

	// ── マーカー（プレイヤー：物理なし・モデルのみ）──
	m_marker.SetModelData(MarkerModel);
	m_markerAnim.Init(&m_marker);
	m_markerAnim.ChangeAnimation("Idle", true, 0);
	// 装備は非表示
	m_marker.SetNodeVisible("HandledSword",  false);
	m_marker.SetNodeVisible("BackSword",     false);
	m_marker.SetNodeVisible("OpenedParasol", false);
	m_marker.SetNodeVisible("ClosedParasol", false);

	// ── 背景の回転惑星 ──
	{
		const auto spData = ModelManager::Instance().GetModel(BgPlanetModel);
		if (spData) { m_bgPlanet.SetModelData(spData); }
	}

	// ── グロー素材（連結ドット＆後光）──
	m_dotTex = std::make_shared<KdTexture>();
	if (m_dotTex->Load(DotTex))
	{
		m_dotPoly.SetMaterial(m_dotTex);
		m_dotPoly.SetScale(1.0f);
	}

	// ── 漂う光の粒 ──
	{
		auto rnd01 = []() { return static_cast<float>(std::rand()) / RAND_MAX; };
		m_motes.resize(MoteCount);
		for (auto& m : m_motes)
		{
			m.pos = {
				(rnd01() * 2.0f - 1.0f) * MoteAreaX,
				-MoteAreaY * 0.3f + rnd01() * MoteAreaY,
				(rnd01() * 2.0f - 1.0f) * MoteAreaZ };
			m.size = MoteSizeMin + rnd01() * (MoteSizeMax - MoteSizeMin);
			m.spin = rnd01() * 6.28f;
		}
	}

	// ── 宇宙背景＋ライト ──
	AddObject(std::make_shared<BackGround>());
	AddObject(std::make_shared<StarField>());
	{
		auto light = std::make_shared<PointLightObject>();
		light->SetPos({ 0.0f, 12.0f, -8.0f });
		light->SetColor({ 1.0f, 0.95f, 0.85f });
		light->SetRadius(80.0f);
		AddObject(light);
	}

	// ── フォント ──
	KdFontManager::Instance().AddFont(FontNo, FontName, FontHeight);
	// リザルト用（バナー/値=大、ラベル=小）
	KdFontManager::Instance().AddFont(ResultConst::FontBigNo,   FontConst::GameFontName, ResultConst::FontBigH);
	KdFontManager::Instance().AddFont(ResultConst::FontSmallNo, FontConst::GameFontName, ResultConst::FontSmallH);
	KdFontManager::Instance().AddFont(ResultConst::FontMidNo,   FontConst::GameFontName, ResultConst::FontMidH);

	// コイン/rock アイコン（リザルト・HUD・タリー共用）
	m_coinTex = std::make_shared<KdTexture>();
	m_coinTex->Load(ResultConst::CoinIconPath);
	m_rockTex = std::make_shared<KdTexture>();
	m_rockTex->Load(ResultConst::RockIconPath);

	// ── クリア後リザルト：直前にステージをクリアして戻ってきたか ──
	{
		auto res = StageManager::Instance().ConsumeResult();
		if (res.pending)
		{
			m_resultActive = true;
			m_resultZoom   = 0.0f;
			m_resStageId   = res.stageId;
			m_resCoins     = res.coins;
			m_resRocks     = res.rocks;
			m_resDeaths    = res.deaths;
			m_resTime      = res.time;
			m_resStep      = 1;        // まずメッセージ
			m_resCardAnim  = 0.0f;

			// マーカー（プレイヤー）をクリアしたステージのノードへ置く
			for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
			{
				if (m_nodes[i].stageId == res.stageId) { m_current = i; break; }
			}
			m_camFocus = m_nodes[m_current].pos;
		}
	}
}

//----------------------------------------------------------
// 指定ノードへホップ移動を開始
void StageSelectScene::StartMove(int target)
{
	if (m_moving) { return; }
	if (target < 0 || target >= static_cast<int>(m_nodes.size())) { return; }

	m_moving   = true;
	m_moveFrom = m_current;
	m_moveTo   = target;
	m_moveT    = 0.0f;

	// 進む向きへマーカーを向ける
	Math::Vector3 to = m_nodes[target].pos - m_nodes[m_current].pos;
	m_markerYaw = std::atan2f(to.x, to.z);
}

//----------------------------------------------------------
Math::Vector3 StageSelectScene::MarkerPos() const
{
	if (m_nodes.empty()) { return Math::Vector3::Zero; }

	// 選択中ノードの上下ふわふわ（箱の描画と同じ式）。マーカーも一緒に揺らす。
	const float nodeBob = std::sinf(m_timer * NodeBobSpeed) * NodeBobAmp;

	if (m_moving)
	{
		const float t = m_moveT * m_moveT * (3.0f - 2.0f * m_moveT);   // smoothstep
		Math::Vector3 p = Math::Vector3::Lerp(m_nodes[m_moveFrom].pos, m_nodes[m_moveTo].pos, t);
		// 移動先（選択中＝揺れる箱）に着くにつれてふわふわを合わせる
		p.y += MarkerYOffset + std::sinf(t * 3.14159265f) * HopHeight + nodeBob * t;
		return p;
	}
	Math::Vector3 p = m_nodes[m_current].pos;
	p.y += MarkerYOffset + nodeBob;   // 箱と同じ上下動
	return p;
}

//----------------------------------------------------------
void StageSelectScene::Event()
{
	const float dt = KdFPSController::GetDt();
	m_timer += dt;

	// ライト
	{
		auto& ambient = KdShaderManager::Instance().WorkAmbientController();
		Math::Vector3 dir(0.3f, -1.0f, 0.5f);
		dir.Normalize();
		ambient.SetDirLight(dir, Math::Vector3(2.0f, 2.0f, 2.0f));
		ambient.SetAmbientLight(Math::Vector4(0.45f, 0.48f, 0.58f, 1.0f));
	}

	// マーカーのアニメ更新
	m_markerAnim.Update(m_marker, 1.0f);

	// 光の粒：ゆっくり上昇、上端で巻き戻し
	{
		auto rnd01 = []() { return static_cast<float>(std::rand()) / RAND_MAX; };
		const float topY = MoteAreaY * 0.7f;
		for (auto& m : m_motes)
		{
			m.pos.y += MoteRise * dt;
			if (m.pos.y > topY)
			{
				m.pos.y = -MoteAreaY * 0.3f;
				m.pos.x = (rnd01() * 2.0f - 1.0f) * MoteAreaX;
				m.pos.z = (rnd01() * 2.0f - 1.0f) * MoteAreaZ;
			}
		}
	}

	// 開始の黒フェードイン
	if (m_introFade > 0.0f)
	{
		m_introFade -= IntroFadeSpeed * dt;
		if (m_introFade < 0.0f) { m_introFade = 0.0f; }
	}

	// ── 移動の進行 ──
	if (m_moving)
	{
		m_moveT += dt / MoveDuration;
		if (m_moveT >= 1.0f)
		{
			m_moveT  = 0.0f;
			m_moving = false;
			m_current = m_moveTo;
		}
	}

	// ── 入力（WASDで上下左右に選択・Enter/Spaceで決定）──
	// グリッド隣接で確実に移動：W=上の行 / S=下の行 / A=左 / D=右
	{
		const bool kW = (GetAsyncKeyState('W') & 0x8000) != 0;
		const bool kA = (GetAsyncKeyState('A') & 0x8000) != 0;
		const bool kS = (GetAsyncKeyState('S') & 0x8000) != 0;
		const bool kD = (GetAsyncKeyState('D') & 0x8000) != 0;
		// 押した瞬間（エッジ）
		const bool eW = kW && !m_prevW;
		const bool eA = kA && !m_prevA;
		const bool eS = kS && !m_prevS;
		const bool eD = kD && !m_prevD;
		m_prevW = kW; m_prevA = kA; m_prevS = kS; m_prevD = kD;

		// リザルト/タリー中はマップ移動・入場を止める（決定で進めるだけ）
		if (!m_resultActive && !m_tallyActive && !m_entering && !m_moving)
		{
			const int row = m_current / kCols;
			const int col = m_current % kCols;
			int target = -1;
			if      (eW) { if (row < kRows - 1) { target = m_current + kCols; } }  // 上の行へ（画面奥）
			else if (eS) { if (row > 0)         { target = m_current - kCols; } }  // 下の行へ（画面手前）
			else if (eD) { if (col < kCols - 1) { target = m_current + 1; } }      // 右
			else if (eA) { if (col > 0)         { target = m_current - 1; } }      // 左
			if (target >= 0) { StartMove(target); }
		}

		// 決定
		const bool adv = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
		              || ((GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0);
		if (m_resultActive)
		{
			// キーで次のカードを出す。3枚出たら次のキーで閉じる（カメラは俯瞰へ）
			if (adv && !m_resultAdvPrev && !SceneManager::Instance().IsInputLocked())
			{
				if (m_resStep < ResultConst::Pages) { ++m_resStep; m_resCardAnim = 0.0f; }
				else { m_resultActive = false; StartTally(); }   // 見終わり→入手分を UI へ飛ばす
			}
		}
		else if (adv && !m_advPrev && !m_moving && !m_entering && !m_tallyActive
			&& !SceneManager::Instance().IsInputLocked())   // 切替直後の持ち越し/連打を無視
		{
			m_entering = true;
			// 選択ノードのステージ番号を設定（stageIdは0始まり→フォルダはStage01から）
			StageManager::Instance().SetStageIndex(m_nodes[m_current].stageId + 1);
		}
		m_advPrev      = adv;   // ロック中もエッジは更新（押しっぱなしは解除後まで無効）
		m_resultAdvPrev = adv;
	}

	// リザルト/タリー中はカメラを寄せたまま（0=俯瞰 ⇔ 1=寄り）
	const bool zoomIn = m_resultActive || m_tallyActive;
	m_resultZoom += (zoomIn ? ResultConst::ZoomSpeed : -ResultConst::ZoomSpeed) * dt;
	m_resultZoom  = std::clamp(m_resultZoom, 0.0f, 1.0f);
	m_resCardAnim += dt;   // ページ入れ替えポップ
	UpdateTally(dt);       // 入手分の UI ホーミング
	if (m_coinHudPop > 0.0f) { m_coinHudPop -= dt; }
	if (m_rockHudPop > 0.0f) { m_rockHudPop -= dt; }

	// 入場フェード（白へ）→ ゲームへ
	if (m_entering)
	{
		m_fadeAlpha += FadeOutSpeed * dt;
		if (m_fadeAlpha >= 1.0f)
		{
			m_fadeAlpha = 1.0f;
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
		}
	}

	// ── カメラ（選択中ノードへフォーカス＝追従パン＋待機ゆらぎ）──
	{
		// 選択中（移動中は移動先）のノードへ注視点を「マージン（遊び）」を残してゆったり寄せる。
		const int sel = m_moving ? m_moveTo : m_current;
		if (sel >= 0 && sel < static_cast<int>(m_nodes.size()))
		{
			Math::Vector3 to = m_nodes[sel].pos - m_camFocus;
			to.y = 0.0f;
			const float dist = std::sqrtf(to.x * to.x + to.z * to.z);
			if (dist > CamFocusMargin)
			{
				// マージンを超えたぶんだけ追う（デッドゾーン）。手前 CamFocusMargin で止まる。
				const Math::Vector3 dir     = to / dist;
				const Math::Vector3 desired = m_nodes[sel].pos - dir * CamFocusMargin;
				m_camFocus = Math::Vector3::Lerp(m_camFocus, desired,
					std::min(CamFocusLerp * dt * 60.0f, 1.0f));
			}
		}

		// カメラ位置は固定（マップ全体を見渡す定位置）。向きだけ「ほんの少し」選択ノード側へ。
		Math::Vector3 eye    = {
			std::sinf(m_timer * CamSwaySpeed) * CamSwayX,
			CamEyeY + std::cosf(m_timer * CamSwaySpeed * 0.8f) * CamSwayY,
			CamEyeZ };
		// 注視点はマップ中心と選択ノードの中間（控えめ）。中心(0,CamTargetY,0)から少しだけ寄せる。
		Math::Vector3 target = {
			m_camFocus.x * CamFocusAmount,
			CamTargetY + m_camFocus.y * CamFocusAmount,
			m_camFocus.z * CamFocusAmount };

		// リザルト中：マーカー（プレイヤー）へ寄ったクローズアップへ補間
		if (m_resultZoom > 0.0f)
		{
			const Math::Vector3 mk = MarkerPos();
			// CamPanX を eye/target 両方の X に足す＝右へパン（被写体が左へ寄る）
			const Math::Vector3 closeEye = mk + Math::Vector3(
				ResultConst::CamEyeOffX + ResultConst::CamPanX, ResultConst::CamEyeOffY, ResultConst::CamEyeOffZ);
			const Math::Vector3 closeTgt = mk + Math::Vector3(
				ResultConst::CamPanX, ResultConst::CamFocusUp, 0.0f);
			const float z  = m_resultZoom;
			const float ez = z * z * (3.0f - 2.0f * z);   // smoothstep
			eye    = Math::Vector3::Lerp(eye,    closeEye, ez);
			target = Math::Vector3::Lerp(target, closeTgt, ez);
		}

		Math::Vector3 f = target - eye; f.Normalize();
		Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize();
		Math::Vector3 u = f.Cross(r);
		Math::Matrix world;
		world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
		world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
		world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
		world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
		m_camera->SetCameraMatrix(world);
	}
}

//----------------------------------------------------------
void StageSelectScene::DrawLitExtra()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 背景でゆっくり回る惑星
	if (m_bgPlanet.IsEnable())
	{
		const Math::Matrix w =
			Math::Matrix::CreateScale(BgPlanetScale) *
			Math::Matrix::CreateRotationY(m_timer * BgPlanetSpin) *
			Math::Matrix::CreateTranslation(BgPlanetX, BgPlanetY, BgPlanetZ);
		shader.DrawModel(m_bgPlanet, w);
	}

	// ステージノード（箱。選択中は拡大＋ふわふわ）
	const int sel = m_moving ? m_moveTo : m_current;
	if (m_nodeModel.IsEnable())
	{
		for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
		{
			const auto& n = m_nodes[i];
			const bool  isSel = (i == sel);
			const float scale = NodeScale * (isSel ? NodeSelectScale : 1.0f);
			const float bob   = isSel ? std::sinf(m_timer * NodeBobSpeed) * NodeBobAmp : 0.0f;
			const Math::Matrix w =
				Math::Matrix::CreateScale(scale) *
				Math::Matrix::CreateTranslation(n.pos.x, n.pos.y + bob, n.pos.z);
			shader.DrawModel(m_nodeModel, w,
				Math::Color(n.color.x, n.color.y, n.color.z, 1.0f), Math::Vector3::Zero);
		}
	}

	// マーカー（プレイヤー）
	if (m_marker.IsEnable())
	{
		const Math::Matrix w =
			Math::Matrix::CreateScale(MarkerScale) *
			Math::Matrix::CreateRotationY(m_markerYaw) *
			Math::Matrix::CreateTranslation(MarkerPos());
		shader.DrawModel(m_marker, w);
	}
}

//----------------------------------------------------------
void StageSelectScene::DrawOutlineExtra()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);

	// ノード（地形扱いの太さ）
	if (m_nodeModel.IsEnable())
	{
		shader.SetOutlineWidth(OutlineConst::TerrainWidth);
		const int sel = m_moving ? m_moveTo : m_current;
		for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
		{
			const auto& n = m_nodes[i];
			const bool  isSel = (i == sel);
			const float scale = NodeScale * (isSel ? NodeSelectScale : 1.0f);
			const float bob   = isSel ? std::sinf(m_timer * NodeBobSpeed) * NodeBobAmp : 0.0f;
			const Math::Matrix w =
				Math::Matrix::CreateScale(scale) *
				Math::Matrix::CreateTranslation(n.pos.x, n.pos.y + bob, n.pos.z);
			shader.DrawModel(m_nodeModel, w, c, Math::Vector3::Zero);
		}
	}

	// マーカー（キャラの太さ）
	if (m_marker.IsEnable())
	{
		shader.SetOutlineWidth(OutlineConst::Width);
		const Math::Matrix w =
			Math::Matrix::CreateScale(MarkerScale) *
			Math::Matrix::CreateRotationY(m_markerYaw) *
			Math::Matrix::CreateTranslation(MarkerPos());
		shader.DrawModel(m_marker, w, c, Math::Vector3::Zero);
	}
}

//----------------------------------------------------------
void StageSelectScene::DrawEffectExtra()
{
	auto& sm = KdShaderManager::Instance();
	sm.ChangeBlendState(KdBlendState::Add);
	sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	sm.m_StandardShader.SetDissolve(0.0f);

	// 光の粒
	if (m_dotTex)
	{
		for (const auto& mt : m_motes)
		{
			const float tw = 0.6f + 0.4f * std::sinf(m_timer * 1.5f + mt.spin);
			const float a  = 0.3f * tw;
			const Math::Color   col{ MoteColR, MoteColG, MoteColB, a };
			const Math::Vector3 em { MoteColR * a, MoteColG * a, MoteColB * a };
			EffectBase::DrawBillboard(m_dotPoly, mt.pos, mt.size * tw, mt.spin, col, em);
		}

		// 連結ドット（ノード間の道）
		const float dotPulse = 0.7f + 0.3f * std::sinf(m_timer * 2.0f);
		for (const auto& link : m_links)
		{
			const Math::Vector3 a = m_nodes[link.first].pos;
			const Math::Vector3 b = m_nodes[link.second].pos;
			const Math::Vector3 seg = b - a;
			const float len = seg.Length();
			const int   num = std::max(1, static_cast<int>(len / DotSpacing));
			for (int k = 1; k < num; ++k)
			{
				const float f = static_cast<float>(k) / num;
				Math::Vector3 p = a + seg * f;
				p.y += 0.2f;
				const Math::Color   col{ DotColR, DotColG, DotColB, 0.6f };
				const Math::Vector3 em { DotColR * 0.6f, DotColG * 0.6f, DotColB * 0.6f };
				EffectBase::DrawBillboard(m_dotPoly, p, DotSize * dotPulse, 0.0f, col, em);
			}
		}

		// 選択中ノードの後光
		const int sel = m_moving ? m_moveTo : m_current;
		if (sel >= 0 && sel < static_cast<int>(m_nodes.size()))
		{
			const auto& n = m_nodes[sel];
			const float bob   = std::sinf(m_timer * NodeBobSpeed) * NodeBobAmp;
			const float pulse = 0.85f + 0.15f * std::sinf(m_timer * HaloPulse);
			const Math::Vector3 haloPos = { n.pos.x, n.pos.y + bob + 0.5f, n.pos.z };
			const Math::Color   col{ n.color.x, n.color.y, n.color.z, 0.5f };
			const Math::Vector3 em { n.color.x * 0.6f, n.color.y * 0.6f, n.color.z * 0.6f };
			EffectBase::DrawBillboard(m_dotPoly, haloPos, HaloSize * pulse, 0.0f, col, em);
		}
	}

	sm.UndoDepthStencilState();
	sm.UndoBlendState();
}

//----------------------------------------------------------
void StageSelectScene::DrawSpriteExtra()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	auto drawCentered = [&](const char* text, float yRatioFromCenter, const Math::Color& col)
	{
		auto measure = KdFontManager::Instance().CreateFontTexture(FontNo, text, false);
		float textW = 0.0f, textH = 0.0f;
		if (measure)
		{
			for (const auto& d : measure->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				textW += static_cast<float>(d->FontTex->GetInfo().Width);
				textH  = std::max(textH, static_cast<float>(d->FontTex->GetInfo().Height));
			}
		}
		const Math::Vector2 pos(-textW * 0.5f, yRatioFromCenter - textH * 0.5f);
		TextFx::DrawShadowed(sprite, pos, col, text);
	};

	// 通常UI（見出し/ステージ名/ヒント）はリザルト中はフェードアウト
	const float normalUiAlpha = 1.0f - m_resultZoom;
	if (normalUiAlpha > 0.01f)
	{
		// 見出し（上）
		{
			const Math::Color col(1.0f, 1.0f, 1.0f, 0.9f * normalUiAlpha);
			drawCentered(TitleText, sh * TitleYRatio, col);
		}
		// 選択中のステージ名（仮表示）
		{
			int sid = 0;
			if (m_current >= 0 && m_current < static_cast<int>(m_nodes.size()))
			{
				sid = m_nodes[m_current].stageId;
			}
			const char* name = (sid >= 0 && sid < StageNameCount) ? StageNames[sid] : StageNameFallback;
			const Math::Color col(0.85f, 0.95f, 1.0f, normalUiAlpha);
			drawCentered(name, sh * StageNameYRatio, col);
		}
		// 操作ヒント（下・点滅）
		{
			const float blink = 0.5f + 0.5f * std::sinf(m_timer * 3.0f);
			const Math::Color col(1.0f, 1.0f, 1.0f, (0.4f + 0.5f * blink) * normalUiAlpha);
			drawCentered(HintText, -sh * HintYRatio, col);
		}
	}

	// 合計コイン/rock HUD（常時）＋タリーの飛行アイコン
	DrawTotalsHud();

	// リザルトパネル（表示中のみ。タリー中は消えてアイコンが飛ぶ）
	if (m_resultActive && m_resultZoom > 0.0f) { DrawResultPanel(); }

	// 開始の黒フェードイン
	if (m_introFade > 0.0f)
	{
		const Math::Color black(0.0f, 0.0f, 0.0f, m_introFade);
		sprite.DrawBox(0, 0, sw, sh, &black, true);
	}
	// 入場の白フェード（GameScene の白フェードインへ繋ぐ）
	if (m_fadeAlpha > 0.0f)
	{
		const Math::Color flash(1.0f, 1.0f, 1.0f, m_fadeAlpha);
		sprite.DrawBox(0, 0, sw, sh, &flash, true);
	}
}

//----------------------------------------------------------
// クリア後リザルトパネル：プレイヤー横にステージ名/タイム/コイン/死亡回数
//----------------------------------------------------------
void StageSelectScene::DrawResultPanel()
{
	using namespace ResultConst;
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);
	const float a  = std::clamp(m_resultZoom, 0.0f, 1.0f);   // 寄りに合わせてフェードイン

	// ── マーカー（プレイヤー）のワールド位置をスクリーンへ投影 ──
	const Math::Vector3 wp = MarkerPos() + Math::Vector3(0.0f, AnchorWorldUp, 0.0f);
	const Math::Matrix  vp = KdShaderManager::Instance().GetCameraCB().mView
	                       * KdShaderManager::Instance().GetCameraCB().mProj;
	Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(wp.x, wp.y, wp.z, 1.0f), vp);
	if (clip.w < 0.001f) { return; }   // カメラ後方なら描かない
	const float mx = (clip.x / clip.w) * sw * 0.5f;   // 中心原点・+Xが右
	const float my = (clip.y / clip.w) * sh * 0.5f;   // +Yが上

	// ── パネル中心位置（マーカーの右）。画面内に収めるようクランプ ──
	float px = mx + PanelOffsetX;
	float py = my;
	const float halfW = PanelW * 0.5f;
	const float halfH = PanelH * 0.5f;
	const float limX  = sw * 0.5f - ScreenMargin - halfW;
	const float limY  = sh * 0.5f - ScreenMargin - halfH;
	px = std::clamp(px, -limX, limX);
	py = std::clamp(py, -limY, limY);

	// ── パネル本体＋金縁 ──
	const Math::Color edge (EdgeR,  EdgeG,  EdgeB,  EdgeA  * a);
	const Math::Color body (PanelR, PanelG, PanelB, PanelA * a);
	sprite.DrawBox(static_cast<int>(px), static_cast<int>(py),
		static_cast<int>(halfW + EdgeThickness), static_cast<int>(halfH + EdgeThickness), &edge, true);
	sprite.DrawBox(static_cast<int>(px), static_cast<int>(py),
		static_cast<int>(halfW), static_cast<int>(halfH), &body, true);

	const float top  = py + halfH;
	const float bot  = py - halfH;
	const float left = px - halfW;

	// 中央揃え＋右下シャドウのテキスト描画（フォントスロット指定）
	auto drawCenter = [&](int slot, const char* t, float cx2, float cy2, const Math::Color& col)
	{
		auto fs = KdFontManager::Instance().CreateFontTexture(slot, t, false);
		if (!fs) { return; }
		float w = 0.0f, h = 0.0f;
		for (const auto& d : fs->GetTexList())
		{
			if (d && d->FontTex)
			{
				w += static_cast<float>(d->FontTex->GetInfo().Width);
				h  = std::max(h, static_cast<float>(d->FontTex->GetInfo().Height));
			}
		}
		const Math::Vector2 pos(cx2 - w * 0.5f, cy2 - h * 0.5f);
		const Math::Color sh(0.0f, 0.0f, 0.0f, col.w * TextFxConst::ShadowAlphaMul);
		sprite.DrawFont(fs, Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY), &sh, 0);
		sprite.DrawFont(fs, pos, &col, 0);
	};

	char buf[96];

	// ── 上部バナー（金帯）＋タイトル「STAGE N CLEAR!」 ──
	const float bannerCY = top - BannerH * 0.5f;
	sprite.DrawBox(static_cast<int>(px), static_cast<int>(bannerCY),
		static_cast<int>(halfW), static_cast<int>(BannerH * 0.5f), &edge, true);
	{
		std::snprintf(buf, sizeof(buf), "STAGE %d  %s", m_resStageId + 1, ClearWord);
		const Math::Color title(TitleR, TitleG, TitleB, a);
		drawCenter(FontBigNo, buf, px, bannerCY, title);
	}

	// タイム文字列
	const int totalCs = static_cast<int>(m_resTime * 100.0f);
	const int cs  = totalCs % 100;
	const int sec = (totalCs / 100) % 60;
	const int min = totalCs / 6000;
	char timeBuf[32]; std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d.%02d", min, sec, cs);

	const Math::Color cardBody(CardR, CardG, CardB, CardA * a);
	const Math::Color cardEdge(EdgeR, EdgeG, EdgeB, CardEdgeA * a);

	// ── 進捗ドット（●○ …）：今どのページか。バナー直下 ──
	const float dotRowCY = top - BannerH - ResultConst::DotRowH * 0.5f;
	{
		const float totalW = ResultConst::DotGap * (ResultConst::Pages - 1);
		for (int i = 0; i < ResultConst::Pages; ++i)
		{
			const float dx = px - totalW * 0.5f + i * ResultConst::DotGap;
			const bool  on = (i <= m_resStep - 1);
			const Math::Color dc = on
				? Math::Color(EdgeR, EdgeG, EdgeB, ResultConst::DotOnA * a)
				: Math::Color(ResultConst::DotOffR, ResultConst::DotOffG, ResultConst::DotOffB, ResultConst::DotOffA * a);
			sprite.DrawBox(static_cast<int>(dx), static_cast<int>(dotRowCY),
				static_cast<int>(ResultConst::DotSize), static_cast<int>(ResultConst::DotSize), &dc, true);
		}
	}

	// ── コンテンツ枠（ページ入れ替え時にポップ）──
	const float p  = std::clamp(m_resCardAnim / ResultConst::CardPopTime, 0.0f, 1.0f);
	const float pm = p - 1.0f;
	const float eb = 1.0f + (ResultConst::CardPopOvershoot + 1.0f) * pm * pm * pm
		+ ResultConst::CardPopOvershoot * pm * pm;            // easeOutBack
	const float scale = std::lerp(ResultConst::CardPopStart, 1.0f, eb);
	const float ca    = a * p;

	const float contentTop = top - BannerH - DotRowH;
	const float contentBot = bot + FooterH;
	const float contentCY  = (contentTop + contentBot) * 0.5f;
	const float contentH   = (contentTop - contentBot) - CardVPad * 2.0f;
	const float innerW2    = PanelW - SidePad * 2.0f;
	const float hcw = innerW2 * 0.5f * scale;
	const float hch = contentH * 0.5f * scale;

	sprite.DrawBox(static_cast<int>(px), static_cast<int>(contentCY),
		static_cast<int>(hcw + CardEdgeThickness), static_cast<int>(hch + CardEdgeThickness), &cardEdge, true);
	sprite.DrawBox(static_cast<int>(px), static_cast<int>(contentCY),
		static_cast<int>(hcw), static_cast<int>(hch), &cardBody, true);

	const Math::Color labelCol(LabelR, LabelG, LabelB, ca);
	const Math::Color valueCol(ValueR, ValueG, ValueB, ca);
	const Math::Color msgCol  (ValueR, ValueG, ValueB, ca);

	// アイコン＋数（コイン/rockページ用）
	auto drawIconValue = [&](const std::shared_ptr<KdTexture>& tex, int amount)
	{
		char vb[16]; std::snprintf(vb, sizeof(vb), "x%d", amount);
		const int isz = static_cast<int>(ResultConst::PageIconSize * scale);
		if (tex)
		{
			const Math::Color ic(1.0f, 1.0f, 1.0f, ca);
			sprite.DrawTex(tex.get(), static_cast<int>(px), static_cast<int>(contentCY + ResultConst::PageIconSize * 0.4f),
				isz, isz, nullptr, &ic);
		}
		drawCenter(FontBigNo, vb, px, contentCY - ResultConst::FontBigH * 0.55f, valueCol);
	};

	if (m_resStep <= 1)
	{
		// ページ1：ステージ名 ＋ メッセージ（3行）
		const char* stageName = (m_resStageId >= 0 && m_resStageId < StageNameCount)
			? StageNames[m_resStageId] : StageNameFallback;
		const Math::Color nameCol(LabelR, LabelG, LabelB, ca);   // ステージ名は金色で強調
		const float lineH = ResultConst::FontMidH * 1.35f;
		drawCenter(ResultConst::FontMidNo, stageName,            px, contentCY + lineH, nameCol);
		drawCenter(ResultConst::FontMidNo, ResultText::CoreBack, px, contentCY,         msgCol);
		drawCenter(ResultConst::FontMidNo, ResultText::NextGo,   px, contentCY - lineH, msgCol);
	}
	else if (m_resStep == 2)
	{
		// ページ2：クリアタイム
		drawCenter(FontSmallNo, ResultText::TimeLabel, px, contentCY + contentH * 0.5f - LabelInset, labelCol);
		drawCenter(FontBigNo,   timeBuf,               px, contentCY - ValueDrop,                    valueCol);
	}
	else if (m_resStep == 3)
	{
		// ページ3：入手コイン
		drawCenter(FontSmallNo, CoinLabel, px, contentCY + contentH * 0.5f - LabelInset, labelCol);
		drawIconValue(m_coinTex, m_resCoins);
	}
	else
	{
		// ページ4：入手 rock
		drawCenter(FontSmallNo, "ROCKS", px, contentCY + contentH * 0.5f - LabelInset, labelCol);
		drawIconValue(m_rockTex, m_resRocks);
	}

	// ── ヒント（下・点滅）。途中は NEXT、最後は OK ──
	const float blink = 0.5f + 0.5f * std::sinf(m_timer * 3.0f);
	const Math::Color hint(1.0f, 1.0f, 1.0f, (0.4f + 0.5f * blink) * a);
	const char* hintText = (m_resStep < ResultConst::Pages) ? NextHint : CloseHint;
	drawCenter(FontSmallNo, hintText, px, bot + FooterH * 0.5f, hint);
}

//----------------------------------------------------------
// HUDアイコンのスクリーン位置（中心原点・+Yが上）。左上に縦並び
//----------------------------------------------------------
Math::Vector3 StageSelectScene::HudCoinPos() const
{
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);
	const float x = -sw * 0.5f + ResultConst::HudMargin + ResultConst::HudIconSize * 0.5f;
	const float y =  sh * 0.5f - ResultConst::HudMargin - ResultConst::HudIconSize * 0.5f;
	return Math::Vector3(x, y, 0.0f);
}
Math::Vector3 StageSelectScene::HudRockPos() const
{
	Math::Vector3 p = HudCoinPos();
	p.y -= ResultConst::HudRowGap;   // コインの下
	return p;
}

//----------------------------------------------------------
// クリア入手分（コイン/rock）をプレイヤーから UI へ飛ばす演出を開始
//----------------------------------------------------------
void StageSelectScene::StartTally()
{
	m_flyers.clear();
	m_tallyActive = false;

	if (m_resCoins <= 0 && m_resRocks <= 0) { return; }

	// プレイヤーのワールド位置をスクリーンへ投影（開始点）
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);
	const Math::Vector3 wp = MarkerPos() + Math::Vector3(0.0f, ResultConst::AnchorWorldUp, 0.0f);
	const Math::Matrix  vp = KdShaderManager::Instance().GetCameraCB().mView
	                       * KdShaderManager::Instance().GetCameraCB().mProj;
	Math::Vector4 clip = Math::Vector4::Transform(Math::Vector4(wp.x, wp.y, wp.z, 1.0f), vp);
	Math::Vector3 startBase(0.0f, 0.0f, 0.0f);
	if (clip.w > 0.001f)
	{
		startBase.x = (clip.x / clip.w) * sw * 0.5f;
		startBase.y = (clip.y / clip.w) * sh * 0.5f;
	}

	auto rnd = []() { return static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f; };

	float delay = 0.0f;
	auto spawn = [&](bool isRock, int amount)
	{
		if (amount <= 0) { return; }
		const int count = std::min(amount, ResultConst::TallyMaxPerKind);
		const int base  = amount / count;
		const int rem   = amount % count;
		for (int i = 0; i < count; ++i)
		{
			TallyFlyer f;
			f.isRock = isRock;
			f.start  = startBase + Math::Vector3(rnd() * ResultConst::TallySpread, rnd() * ResultConst::TallySpread * 0.5f, 0.0f);
			f.delay  = delay;
			f.t      = 0.0f;
			f.add    = base + (i < rem ? 1 : 0);
			f.done   = false;
			m_flyers.push_back(f);
			delay += ResultConst::TallyStagger;
		}
	};
	spawn(false, m_resCoins);   // コイン先
	spawn(true,  m_resRocks);   // rock後

	m_tallyActive = !m_flyers.empty();
}

//----------------------------------------------------------
// タリー更新：各アイコンを HUD へホーミングさせ、到達で合計へ加算
//----------------------------------------------------------
void StageSelectScene::UpdateTally(float dt)
{
	if (!m_tallyActive) { return; }

	bool anyLeft = false;
	for (auto& f : m_flyers)
	{
		if (f.done) { continue; }
		if (f.delay > 0.0f) { f.delay -= dt; anyLeft = true; continue; }

		f.t += dt / ResultConst::TallyFlyTime;
		if (f.t >= 1.0f)
		{
			f.t = 1.0f;
			f.done = true;
			if (f.isRock)
			{
				StageManager::Instance().AddTotalRocks(f.add);
				m_rockHudPop = ResultConst::HudPopTime;
			}
			else
			{
				StageManager::Instance().AddTotalCoins(f.add);
				m_coinHudPop = ResultConst::HudPopTime;
			}
		}
		else { anyLeft = true; }
	}

	if (!anyLeft) { m_tallyActive = false; m_flyers.clear(); }
}

//----------------------------------------------------------
// 左上の合計コイン/rock HUD ＋ タリー飛行アイコン
//----------------------------------------------------------
void StageSelectScene::DrawTotalsHud()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	// アイコン＋数字（ポップ拡大つき）
	auto drawRow = [&](const std::shared_ptr<KdTexture>& tex, const Math::Vector3& pos, int total, float pop)
	{
		const float k   = (pop > 0.0f) ? (pop / ResultConst::HudPopTime) : 0.0f;
		const float scl = 1.0f + ResultConst::HudPopScale * k;
		const int   isz = static_cast<int>(ResultConst::HudIconSize * scl);
		const Math::Color ic(1.0f, 1.0f, 1.0f, 1.0f);
		if (tex) { sprite.DrawTex(tex.get(), static_cast<int>(pos.x), static_cast<int>(pos.y), isz, isz, nullptr, &ic); }

		char nb[24]; std::snprintf(nb, sizeof(nb), "x%d", total);
		auto fs = KdFontManager::Instance().CreateFontTexture(ResultConst::HudFontNo, nb, false);
		if (fs)
		{
			float h = 0.0f;
			for (const auto& d : fs->GetTexList()) { if (d && d->FontTex) { h = std::max(h, static_cast<float>(d->FontTex->GetInfo().Height)); } }
			const float tx = pos.x + ResultConst::HudIconSize * 0.5f + ResultConst::HudTextGap;
			const float ty = pos.y - h * 0.5f;
			const Math::Color sh(0.0f, 0.0f, 0.0f, TextFxConst::ShadowAlphaMul);
			sprite.DrawFont(fs, Math::Vector2(tx + TextFxConst::ShadowOffX, ty - TextFxConst::ShadowOffY), &sh, 0);
			const Math::Color tc(1.0f, 1.0f, 1.0f, 1.0f);
			sprite.DrawFont(fs, Math::Vector2(tx, ty), &tc, 0);
		}
	};

	drawRow(m_coinTex, HudCoinPos(), StageManager::Instance().GetTotalCoins(), m_coinHudPop);
	drawRow(m_rockTex, HudRockPos(), StageManager::Instance().GetTotalRocks(), m_rockHudPop);

	// タリー飛行アイコン（プレイヤー→HUDへ弧を描いてホーミング）
	if (m_tallyActive)
	{
		for (const auto& f : m_flyers)
		{
			if (f.done || f.delay > 0.0f) { continue; }
			const Math::Vector3 target = f.isRock ? HudRockPos() : HudCoinPos();
			const float te = f.t * f.t * (3.0f - 2.0f * f.t);   // smoothstep
			const Math::Vector3 ctrl = (f.start + target) * 0.5f + Math::Vector3(0.0f, ResultConst::TallyArcUp, 0.0f);
			const float u = 1.0f - te;
			const Math::Vector3 pos = f.start * (u * u) + ctrl * (2.0f * u * te) + target * (te * te);
			const auto& tex = f.isRock ? m_rockTex : m_coinTex;
			if (tex)
			{
				const Math::Color ic(1.0f, 1.0f, 1.0f, 1.0f);
				sprite.DrawTex(tex.get(), static_cast<int>(pos.x), static_cast<int>(pos.y),
					ResultConst::TallyFlySize, ResultConst::TallyFlySize, nullptr, &ic);
			}
		}
	}
}
