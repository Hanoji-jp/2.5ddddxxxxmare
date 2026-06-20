#include "StageSelectScene.h"
#include "../SceneManager.h"
#include "../../GameObject/BackGround/BackGround.h"
#include "../../GameObject/BackGround/StarField.h"
#include "../../GameObject/Light/PointLightObject.h"
#include "../../GameObject/Effect/EffectBase.h"
#include "../../Manager/ModelManager.h"
#include "../../Manager/StageManager.h"
#include "../../Const/StageSelectConst.h"
#include "../../Const/OutlineConst.h"

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

		if (!m_entering && !m_moving)
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
		if (adv && !m_advPrev && !m_moving && !m_entering
			&& !SceneManager::Instance().IsInputLocked())   // 切替直後の持ち越し/連打を無視
		{
			m_entering = true;
			// 選択ノードのステージ番号を設定（stageIdは0始まり→フォルダはStage01から）
			StageManager::Instance().SetStageIndex(m_nodes[m_current].stageId + 1);
		}
		m_advPrev = adv;   // ロック中もエッジは更新（押しっぱなしは解除後まで無効）
	}

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
		sprite.DrawFont(pos, &col, "%s", text);
	};

	// 見出し（上）
	{
		const Math::Color col(1.0f, 1.0f, 1.0f, 0.9f);
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
		const Math::Color col(0.85f, 0.95f, 1.0f, 1.0f);
		drawCentered(name, sh * StageNameYRatio, col);
	}
	// 操作ヒント（下・点滅）
	{
		const float blink = 0.5f + 0.5f * std::sinf(m_timer * 3.0f);
		const Math::Color col(1.0f, 1.0f, 1.0f, 0.4f + 0.5f * blink);
		drawCentered(HintText, -sh * HintYRatio, col);
	}

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
