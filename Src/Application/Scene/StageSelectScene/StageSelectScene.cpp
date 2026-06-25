#include "StageSelectScene.h"
#include "../SceneManager.h"
#include "../../Manager/SoundManager.h"
#include "../../Manager/CursorManager.h"
#include "../../Const/SoundConst.h"
#include "../../GameObject/BackGround/BackGround.h"
#include "../../GameObject/BackGround/StarField.h"
#include "../../GameObject/Light/PointLightObject.h"
#include "../../GameObject/Effect/EffectBase.h"
#include "../../Manager/ModelManager.h"
#include "../../Manager/StageManager.h"
#include "../../Const/StageSelectConst.h"
#include "../../Const/ResultConst.h"
#include "../../Const/PauseMenuConst.h"
#include "../../Util/CoreIcon.h"
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
	// BGM（ステージセレクト。ファイル未配置なら無音）
	SoundManager::Instance().PlayBGM(SoundConst::BgmStageSelect, SoundConst::BgmVolume);

	// ── カメラ（固定の見下ろし気味アイソメ。Event で微ゆらぎ）──
	auto cam = std::make_shared<KdCamera>();
	cam->SetProjectionMatrix(CamFov);
	m_camera = cam;

	// ── ノード配置（全5ステージ。stageId 順に繋がるスネーク経路）──
	//   手前(z=-6) に 3個(0,1,2)を等間隔、奥(z=+6) に 2個(3,4)を左右対称に置く。
	//   奥の2個は手前の隙間の上(x=±6)に来て、台形状にきれいに収まる。
	//   経路は 0→1→2(手前右) →3(奥右) →4(奥左) と折り返す。
	const Math::Vector3 layout[5] = {
		{ -12.0f, 0.0f, -6.0f }, { 0.0f, 0.0f, -6.0f }, { 12.0f, 0.0f, -6.0f },
		{   6.0f, 0.0f,  6.0f }, { -6.0f, 0.0f,  6.0f },
	};
	m_nodes.clear();
	for (int i = 0; i < 5; ++i)
	{
		Node n;
		n.pos     = layout[i];
		n.stageId = i;
		n.color   = kNodeColors[i % (sizeof(kNodeColors) / sizeof(kNodeColors[0]))];
		m_nodes.push_back(n);
	}
	// 道（描画＆移動の隣接。図の通り：手前3個は横で繋ぎ、奥2個へは斜めで繋ぐ）
	//   0(手前左)―1(手前中)―2(手前右)、4(奥左)―3(奥右)、
	//   斜め：1―4 / 1―3 / 2―3。これで奥への移動は斜め入力(W+A / W+D)になる。
	m_links = {
		{0,1},{1,2},{3,4},{1,4},{1,3},{2,3},
	};
	// 直前にプレイ/選択していたステージにカーソルを合わせる（戻ってきたとき1に戻らないように）
	m_current = 0;
	{
		const int lastStageId = StageManager::Instance().GetStageIndex() - 1;   // 0始まり
		for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
		{
			if (m_nodes[i].stageId == lastStageId) { m_current = i; break; }
		}
	}
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

	// 選択詳細の GO / BACK ボタン
	m_btnGo.Set(StageSelectConst::SelGoHint,   ResultConst::FontMidNo);
	m_btnBack.Set(StageSelectConst::SelBackHint, ResultConst::FontMidNo);

	// コイン/rock アイコン（リザルト・HUD・タリー共用）
	m_coinTex = std::make_shared<KdTexture>();
	m_coinTex->Load(ResultConst::CoinIconPath);
	m_rockTex = std::make_shared<KdTexture>();
	m_rockTex->Load(ResultConst::RockIconPath);
	m_lifeIcoTex = std::make_shared<KdTexture>();
	if (!m_lifeIcoTex->Load("Asset/Texture/LifeIco.png")) { m_lifeIcoTex = nullptr; }

	// ステージのサムネ画像を stageId 別に読み込む（無いステージは nullptr のまま）
	m_stageThumbs.resize(StageSelectConst::StageNameCount);
	for (int i = 0; i < StageSelectConst::StageNameCount; ++i)
	{
		char path[128];
		std::snprintf(path, sizeof(path), StageSelectConst::ThumbPathFmt, i + 1);
		auto tex = std::make_shared<KdTexture>();
		// forceLinear=true：sRGB変換を無効化し、エクスプローラーで見える明るさのまま表示する
		if (tex->Load(path, false, false, true, true)) { m_stageThumbs[i] = tex; }
	}

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
			m_resCores     = res.cores;
			m_resDeaths    = res.deaths;
			m_resTime      = res.time;
			m_resStep      = 1;        // まずメッセージ
			m_resCardAnim  = 0.0f;
			m_resultOutAnim = 0.0f;    // 箱から出てくる登場演出を最初から

			// マーカー（プレイヤー）をクリアしたステージのノードへ置く
			for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
			{
				if (m_nodes[i].stageId == res.stageId) { m_current = i; break; }
			}
			m_camFocus = m_nodes[m_current].pos;

			// 次ステージのノードを「色・大きさを取り戻す」演出の対象にする
			const int nextStage = res.stageId + 1;
			for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
			{
				if (m_nodes[i].stageId == nextStage) { m_unlockNode = i; m_unlockAnim = 0.0f; break; }
			}
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

	// ノード間ホップのSE
	SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume);

	// 進む向きへマーカーを向ける
	Math::Vector3 to = m_nodes[target].pos - m_nodes[m_current].pos;
	m_markerYaw = std::atan2f(to.x, to.z);

	// ホップ中はジャンプアニメ
	m_markerAnim.ChangeAnimation("Jump", false, 4);
}

//----------------------------------------------------------
// ステージ解放判定：ステージ0は常に解放。以降は前ステージをクリア済みなら解放。
bool StageSelectScene::IsUnlocked(int stageId) const
{
	if (stageId <= 0) { return true; }
	if (StageManager::Instance().IsDebugUnlockAll()) { return true; }   // デバッグ：全解放
	return StageManager::Instance().GetRecord(stageId - 1).cleared;
}

//----------------------------------------------------------
// ノードの見た目：未解放はくすんだ色＋小さめ。解放アニメ中はロック→通常へ補間。
void StageSelectScene::NodeAppearance(int nodeIdx, float& outScale, Math::Color& outColor) const
{
	using namespace StageSelectConst;
	const auto& n = m_nodes[nodeIdx];

	// lockT: 1=完全ロック見た目, 0=通常。解放アニメ中のノードは 1→0 へ。
	float lockT = IsUnlocked(n.stageId) ? 0.0f : 1.0f;
	if (nodeIdx == m_unlockNode)
	{
		const float p = std::clamp(m_unlockAnim / UnlockAnimTime, 0.0f, 1.0f);
		lockT = 1.0f - (p * p * (3.0f - 2.0f * p));   // smoothstep で 1→0
	}

	outScale = NodeScale * std::lerp(1.0f, LockedNodeScaleMul, lockT);

	// 未解放はほぼ黒（完全じゃない黒）。解放で元の色へ戻る。
	const Math::Vector3 c = n.color;
	const Math::Vector3 locked(LockedColorV, LockedColorV, LockedColorV);
	const Math::Vector3 cc = Math::Vector3::Lerp(c, locked, lockT);
	outColor = Math::Color(cc.x, cc.y, cc.z, 1.0f);
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

	// 入場演出：ぴょんと上に跳ねてから箱の中へ沈む
	if (m_entering)
	{
		const float pe   = std::clamp(m_enterAnim / EnterHopTime, 0.0f, 1.0f);
		const float hop  = std::sinf(pe * 3.14159265f) * EnterHopHeight;  // 上にぴょん（戻る）
		const float sink = pe * pe * EnterSink;                           // 終盤ほど箱へ沈む
		p.y += hop - sink;
	}

	// リザルト登場演出：入場(GO)の完全な逆再生。pe を 1→0 に動かして同じ式を使う。
	if (m_resultActive)
	{
		const float prog = std::clamp((m_resultOutAnim - ResultOutDelay) / EnterHopTime, 0.0f, 1.0f);
		const float pe   = 1.0f - prog;                                  // 入場の終端→始端
		const float hop  = std::sinf(pe * 3.14159265f) * EnterHopHeight; // 入場と同じ
		const float sink = pe * pe * EnterSink;                          // 入場と同じ
		p.y += hop - sink;
	}
	return p;
}

//----------------------------------------------------------
// 描画用スケール：入場演出で箱に吸い込まれるように縮む
//----------------------------------------------------------
float StageSelectScene::MarkerDrawScale() const
{
	if (m_entering)
	{
		const float pe = std::clamp(m_enterAnim / EnterHopTime, 0.0f, 1.0f);
		const float s  = 1.0f - pe * pe;   // 序盤は大きいまま、終盤で一気に縮む
		return MarkerScale * std::max(s, 0.0f);
	}

	// リザルト登場：入場(GO)の完全な逆再生。pe を 1→0 にして同じスケール式(1-pe^2)を使う。
	if (m_resultActive)
	{
		const float prog = std::clamp((m_resultOutAnim - ResultOutDelay) / EnterHopTime, 0.0f, 1.0f);
		const float pe   = 1.0f - prog;
		const float s    = 1.0f - pe * pe;   // 入場と同じ式（pe=1で0→pe=0で1）
		return MarkerScale * std::max(s, 0.0f);
	}

	return MarkerScale;
}

//----------------------------------------------------------
// 描画用のマーカー向き：リザルト/選択詳細でズーム中はカメラ（こちら）を向く。
// それ以外は移動方向に設定された m_markerYaw を使う。
//----------------------------------------------------------
float StageSelectScene::MarkerDrawYaw() const
{
	if (m_selZoom > 0.01f || m_resultZoom > 0.01f)
	{
		const Math::Vector3 cam = KdShaderManager::Instance().GetCameraCB().CamPos;
		const Math::Vector3 mp  = MarkerPos();
		const float dx = cam.x - mp.x;
		const float dz = cam.z - mp.z;
		if (dx * dx + dz * dz > 1e-6f) { return std::atan2f(dx, dz); }
	}
	return m_markerYaw;
}

//----------------------------------------------------------
void StageSelectScene::Event()
{
	const float dt = KdFPSController::GetDt();
	m_timer += dt;

	// ── デバッグ：F9 で全ステージ解放をトグル ──
	{
		const bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
		if (f9 && !m_dbgUnlockPrev)
		{
			auto& sm = StageManager::Instance();
			sm.SetDebugUnlockAll(!sm.IsDebugUnlockAll());
			SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
		}
		m_dbgUnlockPrev = f9;
	}

	// ── TABメニュー（マップ閲覧中のみ開閉。背景ぼかし付き）──
	{
		const bool tab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
		const bool canToggle = !m_resultActive && !m_tallyActive && !m_selecting && !m_entering;
		// 設定ウィンドウが開いている間は TAB を設定側へ（メニューは閉じない）
		if (tab && !m_menuTabPrev && !m_settingsMenu.IsOpen() && (m_menuOpen || canToggle))
		{
			m_menuOpen = !m_menuOpen;
			m_menuIndex = 0;
			SoundManager::Instance().PlaySE(m_menuOpen ? SeId::PauseOpen : SeId::PauseClose,
				SoundConst::SeVolume);
		}
		m_menuTabPrev = tab;
	}
	if (m_menuOpen) { UpdateMenu(); return; }

	// 解放アニメ（次ステージが色・大きさを取り戻す）はリザルト/タリーを見終えてから進める
	if (m_unlockNode >= 0 && !m_resultActive && !m_tallyActive
		&& m_unlockAnim < StageSelectConst::UnlockAnimTime)
	{
		m_unlockAnim += dt;
	}

	// 「行けない！」赤点滅のタイマー
	if (m_denyTimer > 0.0f) { m_denyTimer -= dt; }

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
			m_markerAnim.ChangeAnimation("Idle", true, 6);   // 着地→待機
		}
	}

	// ── 入力（WASDでノード選択・Enter/Spaceで決定）──
	// 道で繋がった隣ノードへ、入力方向に最も近い1個へホップ。
	// 手前⇔奥は斜め配置なので W+A / W+D など斜め入力で行き先を選ぶ。
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

		// リザルト/タリー/選択詳細中はマップ移動を止める
		if (!m_resultActive && !m_tallyActive && !m_selecting && !m_entering && !m_moving)
		{
			// 入力方向ベクトル（画面：W=奥(+Z) / S=手前(-Z) / A=左(-X) / D=右(+X)。
			// 同時押しで斜めも作れる。例：W+D で右奥へ）。
			Math::Vector3 inDir = Math::Vector3::Zero;
			if (kW) { inDir.z += 1.0f; }
			if (kS) { inDir.z -= 1.0f; }
			if (kD) { inDir.x += 1.0f; }
			if (kA) { inDir.x -= 1.0f; }

			const bool anyEdge = (eW || eA || eS || eD);
			if (anyEdge && inDir.LengthSquared() > 0.0f)
			{
				inDir.Normalize();

				// リンクで繋がった隣ノードのうち、入力方向に最も近いものを選ぶ。
				// 最良と次点が近すぎる（方向が曖昧）なら動かさない＝斜め入力で確定させる。
				int   best = -1, second = -1;
				float bestScore = -2.0f, secondScore = -2.0f;
				for (const auto& link : m_links)
				{
					int j = -1;
					if      (link.first  == m_current) { j = link.second; }
					else if (link.second == m_current) { j = link.first; }
					if (j < 0) { continue; }

					Math::Vector3 v = m_nodes[j].pos - m_nodes[m_current].pos;
					v.y = 0.0f;
					if (v.LengthSquared() < 1e-4f) { continue; }
					v.Normalize();

					const float score = inDir.Dot(v);
					if (score > bestScore) { second = best; secondScore = bestScore; best = j; bestScore = score; }
					else if (score > secondScore) { second = j; secondScore = score; }
				}

				const bool clear = (best >= 0)
					&& (bestScore >= StageSelectConst::DirDotThreshold)
					&& (second < 0 || (bestScore - secondScore) >= StageSelectConst::DirAmbiguityMargin);

				if (clear)
				{
					// 未解放（前ステージ未クリア）のノードへは移動しない。赤点滅で拒否。
					if (IsUnlocked(m_nodes[best].stageId)) { StartMove(best); }
					else
					{
						m_denyNode = best; m_denyTimer = StageSelectConst::DenyFlashTime;
						SoundManager::Instance().PlaySE(SeId::StageDeny, SoundConst::SeVolume);
					}
				}
			}
		}

		// ── マウス：boxノードをクリックで選択 ──
		//   別の解放済みノード→そこへ移動／いまのノード→詳細を開く／未解放→拒否
		if (!m_resultActive && !m_tallyActive && !m_selecting && !m_entering && !m_moving)
		{
			auto& cur = CursorManager::Instance();
			if (cur.IsActive() && cur.Clicked())
			{
				const auto& bbm = KdDirect3D::Instance().GetBackBuffer();
				const float sw = static_cast<float>(bbm->GetInfo().Width);
				const float sh = static_cast<float>(bbm->GetInfo().Height);
				const Math::Matrix vp = KdShaderManager::Instance().GetCameraCB().mView
					* KdShaderManager::Instance().GetCameraCB().mProj;
				constexpr float kHitR = 80.0f;   // クリック許容半径(px)
				int hit = -1; float bestD = kHitR;
				for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
				{
					const Math::Vector4 clip = Math::Vector4::Transform(
						Math::Vector4(m_nodes[i].pos.x, m_nodes[i].pos.y, m_nodes[i].pos.z, 1.0f), vp);
					if (clip.w <= 0.001f) { continue; }
					const float sx = (clip.x / clip.w) * sw * 0.5f;
					const float sy = (clip.y / clip.w) * sh * 0.5f;
					const float dx = cur.PosX() - sx, dy = cur.PosY() - sy;
					const float d = std::sqrt(dx * dx + dy * dy);
					if (d < bestD) { bestD = d; hit = i; }
				}
				if (hit >= 0)
				{
					if (IsUnlocked(m_nodes[hit].stageId))
					{
						if (hit == m_current)
						{
							// いまのノードをクリック → 詳細（GO/BACK）を開く
							m_selecting = true; m_selChoice = 0;
							m_advPrev = true; m_resultAdvPrev = true;
							SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
						}
						else
						{
							StartMove(hit);   // 別ノードへ移動
						}
					}
					else
					{
						m_denyNode = hit; m_denyTimer = StageSelectConst::DenyFlashTime;
						SoundManager::Instance().PlaySE(SeId::StageDeny, SoundConst::SeVolume);
					}
				}
			}
		}

		// 決定
		const bool adv = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
		              || ((GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0);
		if (m_resultActive)
		{
			// キー（Enter/Space）またはマウス左クリックで次のカードへ（コアリア会話と同じ感覚）
			const bool advEdge = (adv && !m_resultAdvPrev);
			const bool clickEdge = CursorManager::Instance().IsActive() && CursorManager::Instance().Clicked();
			if ((advEdge || clickEdge) && !SceneManager::Instance().IsInputLocked())
			{
				SoundManager::Instance().PlaySE(SeId::ResultAdvance, SoundConst::SeVolume);
				if (m_resStep < ResultConst::Pages) { ++m_resStep; m_resCardAnim = 0.0f; }
				else { m_resultActive = false; StartTally(); }   // 見終わり→入手分を UI へ飛ばす
			}
		}
		else if (m_selecting)
		{
			// 詳細表示中：A/D で GO/BACK を選択、Enterで決定。TABでも即戻れる
			if (eA) { m_selChoice = 1; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }        // 左 = BACK
			else if (eD) { m_selChoice = 0; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }   // 右 = GO
			const bool back = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

			// マウス：GO/BACK ボタンのホバー＆クリック（描画と同じレイアウトで判定）
			bool mouseGo = false, mouseBack = false;
			{
				auto& cur = CursorManager::Instance();
				if (cur.IsActive())
				{
					const auto& bbm = KdDirect3D::Instance().GetBackBuffer();
					const float sw = static_cast<float>(bbm->GetInfo().Width);
					const float sh = static_cast<float>(bbm->GetInfo().Height);
					const float boxHalfH = StageSelectConst::SelHintBoxH * 0.5f;
					const float btnCY    = -sh * 0.5f + StageSelectConst::SelBarMargin + boxHalfH;
					const float goHalfW   = m_btnGo.HalfWidth(StageSelectConst::SelHintBoxPadX);
					const float backHalfW = m_btnBack.HalfWidth(StageSelectConst::SelHintBoxPadX);
					const float backCX = -sw * 0.5f + StageSelectConst::SelBarMargin + backHalfW;
					const float goCX   =  sw * 0.5f - StageSelectConst::SelBarMargin - goHalfW;
					if (cur.HitRect(goCX, btnCY, goHalfW, boxHalfH))
					{
						if (m_selChoice != 0) { m_selChoice = 0; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
						if (cur.Clicked()) { mouseGo = true; }
					}
					else if (cur.HitRect(backCX, btnCY, backHalfW, boxHalfH))
					{
						if (m_selChoice != 1) { m_selChoice = 1; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
						if (cur.Clicked()) { mouseBack = true; }
					}
				}
			}

			// 入場演出中は決定を受け付けない（GO連打でぴょんぴょん再生されるのを防ぐ）
			const bool keyDecide = adv && !m_advPrev && !m_entering && !SceneManager::Instance().IsInputLocked();
			const bool goNow   = (keyDecide && m_selChoice == 0) || (mouseGo   && !m_entering);
			const bool backNow = (keyDecide && m_selChoice == 1) || (mouseBack && !m_entering) || (back && !m_selBackPrev);

			if (goNow)
			{
				// 未解放ステージへは入れない（前ステージ未クリア）。GOは無視。
				if (IsUnlocked(m_nodes[m_current].stageId))
				{
					m_entering  = true;
					m_enterAnim = 0.0f;   // ぴょん入場アニメ開始
					StageManager::Instance().SetStageIndex(m_nodes[m_current].stageId + 1);
					SoundManager::Instance().PlaySE(SeId::StageGo, SoundConst::SeVolume);
				}
				else
				{
					SoundManager::Instance().PlaySE(SeId::StageDeny, SoundConst::SeVolume);
				}
			}
			else if (backNow)
			{
				m_selecting = false;   // BACK → 一覧へ戻る
				SoundManager::Instance().PlaySE(SeId::MenuCancel, SoundConst::SeVolume);
			}
			m_selBackPrev = back;
		}
		else if (adv && !m_advPrev && !m_moving && !m_entering && !m_tallyActive
			&& !SceneManager::Instance().IsInputLocked())   // 切替直後の持ち越し/連打を無視
		{
			// 決定 → いきなり入場せず、まずカメラを寄せて詳細表示（ワンクッション）
			m_selecting = true;
			m_selChoice = 0;   // 既定は GO
				SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
		}
		m_advPrev      = adv;   // ロック中もエッジは更新（押しっぱなしは解除後まで無効）
		m_resultAdvPrev = adv;
	}

	// リザルト/タリー中はカメラを寄せたまま（0=俯瞰 ⇔ 1=寄り）
	const bool zoomIn = m_resultActive || m_tallyActive;
	m_resultZoom += (zoomIn ? ResultConst::ZoomSpeed : -ResultConst::ZoomSpeed) * dt;
	m_resultZoom  = std::clamp(m_resultZoom, 0.0f, 1.0f);
	// 選択詳細のカメラ寄り（入場演出中も寄せたままにして、ぴょん入場を見せる）
	m_selZoom += (m_selecting ? ResultConst::ZoomSpeed : -ResultConst::ZoomSpeed) * dt;
	m_selZoom  = std::clamp(m_selZoom, 0.0f, 1.0f);
	m_resCardAnim += dt;   // ページ入れ替えポップ
	if (m_resultActive) { m_resultOutAnim += dt; }   // 箱から出てくる登場演出
	UpdateTally(dt);       // 入手分の UI ホーミング
	if (m_coinHudPop > 0.0f) { m_coinHudPop -= dt; }
	if (m_rockHudPop > 0.0f) { m_rockHudPop -= dt; }

	// 入場演出：ぴょん→縮んで箱へ入る → 入り切ったら白フェード → ゲームへ
	if (m_entering)
	{
		m_enterAnim += dt;
		// ぴょん入場が終わってから白へフェード
		if (m_enterAnim >= EnterHopTime)
		{
			m_fadeAlpha += FadeOutSpeed * dt;
			if (m_fadeAlpha >= 1.0f)
			{
				m_fadeAlpha = 1.0f;
				SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
			}
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
			// プレイヤーを画面中央に：横パン(CamPanX)は入れない
			const Math::Vector3 closeEye = mk + Math::Vector3(
				ResultConst::CamEyeOffX, ResultConst::CamEyeOffY, ResultConst::CamEyeOffZ);
			const Math::Vector3 closeTgt = mk + Math::Vector3(
				0.0f, ResultConst::CamFocusUp, 0.0f);
			const float z  = m_resultZoom;
			const float ez = z * z * (3.0f - 2.0f * z);   // smoothstep
			eye    = Math::Vector3::Lerp(eye,    closeEye, ez);
			target = Math::Vector3::Lerp(target, closeTgt, ez);
		}

		// 選択詳細中：マーカー（プレイヤー）へ寄ったクローズアップへ補間
		if (m_selZoom > 0.0f && !m_resultActive)
		{
			const Math::Vector3 nd = MarkerPos();
			// プレイヤーを画面中央に：横パン(CamPanX)は入れない
			const Math::Vector3 closeEye = nd + Math::Vector3(
				ResultConst::CamEyeOffX, ResultConst::CamEyeOffY, ResultConst::CamEyeOffZ);
			const Math::Vector3 closeTgt = nd + Math::Vector3(
				0.0f, ResultConst::CamFocusUp, 0.0f);
			const float z  = m_selZoom;
			const float ez = z * z * (3.0f - 2.0f * z);
			eye    = Math::Vector3::Lerp(eye,    closeEye, ez);
			target = Math::Vector3::Lerp(target, closeTgt, ez);
		}

		// マウスパララックス：視点を左右/上下へずらして奥行きを出す（寄り/リザルト中も有効）
		{
			eye.x += CursorManager::Instance().NormX() * StageSelectConst::ParallaxX;
			eye.y += CursorManager::Instance().NormY() * StageSelectConst::ParallaxY;
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

	// ステージノード（箱。選択中は拡大＋ふわふわ）
	const int sel = m_moving ? m_moveTo : m_current;
	if (m_nodeModel.IsEnable())
	{
		for (int i = 0; i < static_cast<int>(m_nodes.size()); ++i)
		{
			const auto& n = m_nodes[i];
			const bool  isSel = (i == sel);
			float baseScale; Math::Color col;
			NodeAppearance(i, baseScale, col);   // 未解放はくすんだ色＋小さめ
			const float scale = baseScale * (isSel ? NodeSelectScale : 1.0f);
			const float bob   = isSel ? std::sinf(m_timer * NodeBobSpeed) * NodeBobAmp : 0.0f;
			const Math::Matrix w =
				Math::Matrix::CreateScale(scale) *
				Math::Matrix::CreateTranslation(n.pos.x, n.pos.y + bob, n.pos.z);
			shader.DrawModel(m_nodeModel, w, col, Math::Vector3::Zero);
		}
	}

	// マーカー（プレイヤー）
	if (m_marker.IsEnable())
	{
		const Math::Matrix w =
			Math::Matrix::CreateScale(MarkerDrawScale()) *
			Math::Matrix::CreateRotationY(MarkerDrawYaw()) *
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
			float baseScale; Math::Color col;
			NodeAppearance(i, baseScale, col);   // ロック状態でアウトラインの大きさも合わせる
			const float scale = baseScale * (isSel ? NodeSelectScale : 1.0f);
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
			Math::Matrix::CreateScale(MarkerDrawScale()) *
			Math::Matrix::CreateRotationY(MarkerDrawYaw()) *
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
			// 未解放ノードへ繋がる道は描かない（どちらかがロックなら線なし）
			if (!IsUnlocked(m_nodes[link.first].stageId)
				|| !IsUnlocked(m_nodes[link.second].stageId)) { continue; }

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

		// 「行けない！」演出：未解放ノードへの道を赤く点滅
		if (m_denyTimer > 0.0f && m_denyNode >= 0 && m_denyNode < static_cast<int>(m_nodes.size()))
		{
			const float blink = 0.5f + 0.5f * std::sinf(m_timer * DenyFlashSpeed);
			const float fade  = std::clamp(m_denyTimer / DenyFlashTime, 0.0f, 1.0f);
			const float aRed  = blink * fade;
			const Math::Vector3 a = m_nodes[m_current].pos;
			const Math::Vector3 b = m_nodes[m_denyNode].pos;
			const Math::Vector3 seg = b - a;
			const float len = seg.Length();
			const int   num = std::max(1, static_cast<int>(len / DotSpacing));
			for (int k = 1; k < num; ++k)
			{
				const float f = static_cast<float>(k) / num;
				Math::Vector3 p = a + seg * f;
				p.y += 0.2f;
				const Math::Color   col{ DenyColR, DenyColG, DenyColB, aRed };
				const Math::Vector3 em { DenyColR * aRed, DenyColG * aRed, DenyColB * aRed };
				EffectBase::DrawBillboard(m_dotPoly, p, DotSize * (0.9f + 0.4f * blink), 0.0f, col, em);
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

	// UI用コイン3D（別RTへ。結果パネル/合計HUDで使用）。3Dパス内で描き、カメラを戻す。
	m_coinIcon.Render(KdFPSController::GetDt(), m_camera);
}

//----------------------------------------------------------
void StageSelectScene::DrawSpriteExtra()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// ── TABメニューを開いている間は、背景ぼかしを「先に」描く。
	//    こうすると以降のUI（見出し/合計HUD等）はぼかしの上に残り、メニュー中もUIが見える。
	if (m_menuOpen) { DrawMenuBackground(); }

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

	// 通常UI（見出し/ステージ名の吹き出し/ヒント）はリザルト中はフェードアウト。
	// TABメニュー中は出さない（ぼかしの上に鮮明な吹き出しが残らないように）。
	const float normalUiAlpha = 1.0f - std::max(m_resultZoom, m_selZoom);
	if (normalUiAlpha > 0.01f && !m_menuOpen)
	{
		// 見出し（上）
		{
			const Math::Color col(1.0f, 1.0f, 1.0f, 0.9f * normalUiAlpha);
			drawCentered(TitleText, sh * TitleYRatio, col);
		}
		// 選択中のステージ名を吹き出しでマーカー（プレイヤー）の上に表示
		{
			int sid = 0;
			if (m_current >= 0 && m_current < static_cast<int>(m_nodes.size()))
			{
				sid = m_nodes[m_current].stageId;
			}
			const char* name = (sid >= 0 && sid < StageNameCount) ? StageNames[sid] : StageNameFallback;
			// 「ステージN　〇〇」形式なら名前部分（〇〇）だけ（実行時SJISの全角スペース 0x81 0x40）
			const char* disp = name;
			if (const char* sp = std::strstr(name, "\x81\x40")) { disp = sp + 2; }

			auto fs = KdFontManager::Instance().CreateFontTexture(FontNo, disp, false);
			if (fs)
			{
				float tw = 0.0f, th = 0.0f;
				for (const auto& d : fs->GetTexList())
				{
					if (!d || !d->FontTex) { continue; }
					tw += static_cast<float>(d->FontTex->GetInfo().Width);
					th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
				}
				// 立っている箱（ノード）を投影し、スクリーン上で固定px上へ置く
				// （ワールドの縦オフセットだと画面端で斜めにずれるため、スクリーン空間でずらす）
				const Math::Vector3 nodePos = (m_current >= 0 && m_current < static_cast<int>(m_nodes.size()))
					? m_nodes[m_current].pos : MarkerPos();
				const Math::Matrix  vp = KdShaderManager::Instance().GetCameraCB().mView
					* KdShaderManager::Instance().GetCameraCB().mProj;
				const Math::Vector4 clip = Math::Vector4::Transform(
					Math::Vector4(nodePos.x, nodePos.y, nodePos.z, 1.0f), vp);
				if (clip.w > 0.001f)
				{
					constexpr float kScreenUpPx = 200.0f;   // 箱の画面位置から上へずらす量(px)
					const float sx = (clip.x / clip.w) * sw * 0.5f;
					const float sy = (clip.y / clip.w) * sh * 0.5f + kScreenUpPx;
					const int ex = static_cast<int>(tw * 0.5f) + 24;   // 左右余白
					const int ey = static_cast<int>(th * 0.5f) + 14;   // 上下余白
					// 吹き出し（薄紫・角丸＋下向き尻尾）
					const Math::Color body(0.93f, 0.91f, 0.99f, normalUiAlpha);
					sprite.DrawRoundedBubble(static_cast<int>(sx), static_cast<int>(sy),
						ex, ey, static_cast<float>(ey), 12, 16, &body, 8);
					// 文字（濃い色・中央）
					const Math::Color txt(0.28f, 0.22f, 0.38f, normalUiAlpha);
					sprite.DrawFont(fs, Math::Vector2(sx - tw * 0.5f, sy - th * 0.5f), &txt, 0);
				}
			}
		}
		// 操作ヒント（下・点滅）
		{
			const float blink = 0.5f + 0.5f * std::sinf(m_timer * 3.0f);
			const Math::Color col(1.0f, 1.0f, 1.0f, (0.4f + 0.5f * blink) * normalUiAlpha);
			drawCentered(HintText, -sh * HintYRatio, col);
		}
	}

	// 合計コイン/rock HUD（タリーの飛行アイコン含む）。TABメニュー中は出さない（ぼかしの前に残らないように）
	if (!m_menuOpen) { DrawTotalsHud(); }

	// リザルトパネル（表示中のみ。タリー中は消えてアイコンが飛ぶ）。メニュー中は出さない
	if (!m_menuOpen && m_resultActive && m_resultZoom > 0.0f) { DrawResultPanel(); }

	// ステージ選択の詳細パネル（カメラが寄ったら表示）
	if (!m_resultActive && m_selZoom > 0.0f) { DrawSelectPanel(); }

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

	// ── TABメニューのパネル（最前面）。背景ぼかしは上で先に描いてあるのでUIが残る ──
	if (m_menuOpen) { DrawMenu(); }
}

//----------------------------------------------------------
// TABメニュー：操作（W/Sで選択、Enter/Spaceで決定。TABで閉じる）
//----------------------------------------------------------
void StageSelectScene::UpdateMenu()
{
	m_menuBlink += KdFPSController::GetDt();
	constexpr int kCount = 3;   // 0=つづける / 1=せってい / 2=タイトルへ

	// 設定ウィンドウが開いている間はそちらに入力を渡す（メニュー操作は止める＝貫通防止）
	if (m_settingsMenu.IsOpen())
	{
		m_settingsMenu.Update();
		const bool navHeld =
			((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0) ||
			((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
		const bool decideHeld =
			((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
		m_menuNavPrev = navHeld;
		m_menuConfPrev = decideHeld;
		return;
	}

	const bool up   = ((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0);
	const bool down = ((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
	const bool nav  = up || down;
	if (nav && !m_menuNavPrev)
	{
		if (up) { m_menuIndex = (m_menuIndex + kCount - 1) % kCount; }
		else    { m_menuIndex = (m_menuIndex + 1) % kCount; }
		SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume);
	}
	m_menuNavPrev = nav;

	// マウス：ホバーで選択／クリックで決定
	bool mouseConfirm = false;
	{
		using namespace PauseMenuConst;
		auto& cur = CursorManager::Instance();
		if (cur.IsActive())
		{
			const float panelH = BannerH + ContentPadTop + 3 * ItemRowH + ContentPadBottom;
			const float hh = panelH * 0.5f;
			const float bannerCY = hh - BannerH * 0.5f;
			const float firstItemY = bannerCY - BannerH * 0.5f - ContentPadTop - ItemRowH * 0.5f;
			const float barHalfW = (PanelFullW - SidePad * 2.0f) * 0.5f;
			for (int i = 0; i < kCount; ++i)
			{
				const float y = firstItemY - i * ItemRowH;
				if (!cur.HitRect(0.0f, y, barHalfW, ItemRowH * 0.5f)) { continue; }
				if (m_menuIndex != i) { m_menuIndex = i; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
				if (cur.Clicked()) { mouseConfirm = true; }
				break;
			}
		}
	}

	const bool conf = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	if ((conf && !m_menuConfPrev) || mouseConfirm)
	{
		SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
		if (m_menuIndex == 0)
		{
			m_menuOpen = false;   // つづける
			// 同じ決定キーがマップ側の「決定」に流れてステージ選択が暴発するのを防ぐ。
			// 押しっぱなしを「既に押下済み」とみなしてエッジを消費する。
			m_advPrev       = true;
			m_resultAdvPrev = true;
		}
		else if (m_menuIndex == 1)
		{
			m_settingsMenu.Open();   // せってい
		}
		else { SceneManager::Instance().SetNextScene(SceneManager::SceneType::Title); }
	}
	m_menuConfPrev = conf;
}

//----------------------------------------------------------
// TABメニュー：描画（背景をぼかして角丸パネル＋項目）
//----------------------------------------------------------
// TABメニューの背景ぼかし＋暗幕（パネルより先に描く＝UIをこの上に残せる）
void StageSelectScene::DrawMenuBackground()
{
	using namespace PauseMenuConst;
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	auto& pp     = KdShaderManager::Instance().m_postProcessShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// 背景ぼかし：シーンRT(3D)をぼかして全画面合成
	if (!m_menuBlurInit)
	{
		m_menuBlurRT.CreateRenderTarget(bb->GetWidth(), bb->GetHeight());
		m_menuBlurInit = (m_menuBlurRT.m_RTTexture != nullptr);
	}
	auto srcRT = pp.GetSceneRT();
	if (m_menuBlurInit && srcRT && srcRT->WorkRTView())
	{
		sprite.End();
		pp.GenerateBlurTexture(srcRT, m_menuBlurRT.m_RTTexture, m_menuBlurRT.m_viewPort, BlurRadius);
		sprite.Begin();
		const Math::Color white(1.0f, 1.0f, 1.0f, 1.0f);
		sprite.DrawTex(m_menuBlurRT.m_RTTexture.get(), 0, 0, sw, sh, nullptr, &white);
	}
	// うっすら暗幕
	{
		const Math::Color dim(0.0f, 0.0f, 0.0f, DimAlpha);
		sprite.DrawBox(0, 0, sw, sh, &dim, true);
	}
}

void StageSelectScene::DrawMenu()
{
	using namespace PauseMenuConst;
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);
	(void)sw; (void)sh;

	// 中央寄せ＋影テキスト（指定スロット）
	auto drawCentered = [&](int fontNo, const char* text, float cx, float y, const Math::Color& col)
	{
		auto fs = KdFontManager::Instance().CreateFontTexture(fontNo, text, false);
		if (!fs) { return; }
		float tw = 0.0f, th = 0.0f;
		for (const auto& d : fs->GetTexList())
		{
			if (!d || !d->FontTex) { continue; }
			tw += static_cast<float>(d->FontTex->GetInfo().Width);
			th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
		}
		const Math::Vector2 pos(cx - tw * 0.5f, y - th * 0.5f);
		const Math::Color shc(TextFxConst::ShadowR, TextFxConst::ShadowG, TextFxConst::ShadowB,
			col.w * TextFxConst::ShadowAlphaMul);
		sprite.DrawFont(fs, Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY), &shc, 0);
		sprite.DrawFont(fs, pos, &col, 0);
	};

	// パネル（影→金縁→紺本体、角丸）
	const float hw = PanelFullW * 0.5f;
	const float panelH = BannerH + ContentPadTop + 3 * ItemRowH + ContentPadBottom;
	const float hh = panelH * 0.5f;
	const float top = hh;
	{
		const int ct = PanelEdgeThickness;
		const Math::Color shadow(0.0f, 0.0f, 0.0f, PanelShadowA);
		sprite.DrawRoundedBox(6, -6, static_cast<int>(hw) + ct, static_cast<int>(hh) + ct, PanelRadius + ct, &shadow, PanelCornerSegs);
		const Math::Color edge(PanelEdgeR, PanelEdgeG, PanelEdgeB, PanelEdgeA);
		sprite.DrawRoundedBox(0, 0, static_cast<int>(hw) + ct, static_cast<int>(hh) + ct, PanelRadius + ct, &edge, PanelCornerSegs);
		const Math::Color body(PanelBodyR, PanelBodyG, PanelBodyB, PanelBodyA);
		sprite.DrawRoundedBox(0, 0, static_cast<int>(hw), static_cast<int>(hh), PanelRadius, &body, PanelCornerSegs);
	}

	// 上部バナー（金）＋タイトル
	const float bannerCY = top - BannerH * 0.5f;
	{
		const Math::Color banner(PanelEdgeR, PanelEdgeG, PanelEdgeB, 1.0f);
		sprite.DrawRoundedBox(0, static_cast<int>(bannerCY), static_cast<int>(hw), static_cast<int>(BannerH * 0.5f), PanelRadius, &banner, PanelCornerSegs);
		const Math::Color titleCol(BannerTextR, BannerTextG, BannerTextB, 1.0f);
		drawCentered(ResultConst::FontBigNo, "メニュー", 0.0f, bannerCY, titleCol);
	}

	// 項目
	const float firstItemY = bannerCY - BannerH * 0.5f - ContentPadTop - ItemRowH * 0.5f;
	const float barHalfW   = (PanelFullW - SidePad * 2.0f) * 0.5f;
	const char* items[3] = { "つづける", "せってい", "タイトルへ" };
	const float blink = 0.5f + 0.5f * std::sinf(m_menuBlink * BlinkSpeed);
	for (int i = 0; i < 3; ++i)
	{
		const bool  sel = (i == m_menuIndex);
		const float y   = firstItemY - i * ItemRowH;
		if (sel)
		{
			const Math::Color bar(PanelEdgeR, PanelEdgeG, PanelEdgeB, HighlightA + HighlightBlinkA * blink);
			sprite.DrawRoundedBox(0, static_cast<int>(y), static_cast<int>(barHalfW), static_cast<int>(HighlightH * 0.5f), PanelRadius, &bar, PanelCornerSegs);
		}
		const Math::Color col = sel ? Math::Color(1.0f, 0.97f, 0.7f, 1.0f) : Math::Color(0.75f, 0.78f, 0.85f, 0.85f);
		drawCentered(ResultConst::FontMidNo, items[i], 0.0f, y, col);
	}

	// 設定ウィンドウ（開いていればメニューの上に重ねる）
	m_settingsMenu.Draw();
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

	// ── パネル本体＋金縁（角丸）──
	const Math::Color edge (EdgeR,  EdgeG,  EdgeB,  EdgeA  * a);
	const Math::Color body (PanelR, PanelG, PanelB, PanelA * a);
	DrawRoundedFrame(px, py, halfW, halfH, StageSelectConst::SelHintBoxRadius, body, edge);

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
	sprite.DrawRoundedBox(static_cast<int>(px), static_cast<int>(bannerCY),
		static_cast<int>(halfW), static_cast<int>(BannerH * 0.5f), StageSelectConst::SelHintBoxRadius, &edge, 8);
	{
		std::snprintf(buf, sizeof(buf), "ステージ%d　%s", m_resStageId + 1, ClearWord);
		const Math::Color title(TitleR, TitleG, TitleB, a);
		drawCenter(FontBigNo, buf, px, bannerCY, title);
	}

	// ── コアリアの顔アイコン（チャット窓風に左上の角へ）──
	if (m_lifeIcoTex)
	{
		const float isz = BannerH * 1.4f;
		const float icx = left + isz * 0.15f;
		const float icy = top  + isz * 0.12f;
		const Math::Color disc(1.0f, 1.0f, 1.0f, a);
		sprite.DrawCircle(static_cast<int>(icx), static_cast<int>(icy), static_cast<int>(isz * 0.56f), &disc, true);
		const Math::Color ic(1.0f, 1.0f, 1.0f, a);
		sprite.DrawTex(m_lifeIcoTex.get(), static_cast<int>(icx), static_cast<int>(icy),
			static_cast<int>(isz), static_cast<int>(isz), nullptr, &ic);
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

	{
		const float ct = static_cast<float>(CardEdgeThickness);
		sprite.DrawRoundedBox(static_cast<int>(px), static_cast<int>(contentCY),
			static_cast<int>(hcw + ct), static_cast<int>(hch + ct), StageSelectConst::SelHintBoxRadius + ct, &cardEdge, 8);
		sprite.DrawRoundedBox(static_cast<int>(px), static_cast<int>(contentCY),
			static_cast<int>(hcw), static_cast<int>(hch), StageSelectConst::SelHintBoxRadius, &cardBody, 8);
	}

	const Math::Color labelCol(LabelR, LabelG, LabelB, ca);
	const Math::Color valueCol(ValueR, ValueG, ValueB, ca);
	const Math::Color msgCol  (ValueR, ValueG, ValueB, ca);

	// アイコン＋数（コインページ用）。コインは3Dモデル(RT)を優先、無ければ従来テクスチャ
	auto drawIconValue = [&](const std::shared_ptr<KdTexture>& tex, int amount)
	{
		char vb[16]; std::snprintf(vb, sizeof(vb), "x%d", amount);
		const int isz = static_cast<int>(ResultConst::PageIconSize * scale);
		KdTexture* useTex = m_coinIcon.GetTexture();
		if (!useTex && tex) { useTex = tex.get(); }
		if (useTex)
		{
			const Math::Color ic(1.0f, 1.0f, 1.0f, ca);
			sprite.DrawTex(useTex, static_cast<int>(px), static_cast<int>(contentCY + ResultConst::PageIconSize * 0.4f),
				isz, isz, nullptr, &ic);
		}
		drawCenter(FontBigNo, vb, px, contentCY - ResultConst::FontBigH * 0.55f, valueCol);
	};

	// 重力コア版：テクスチャの代わりに DrawTriangle で岩(エメラルド)コアを描く
	auto drawCoreValue = [&](int amount)
	{
		char vb[16]; std::snprintf(vb, sizeof(vb), "x%d", amount);
		const int isz = static_cast<int>(ResultConst::PageIconSize * scale);
		CoreIcon::Draw(sprite, static_cast<int>(px),
			static_cast<int>(contentCY + ResultConst::PageIconSize * 0.4f), isz, m_timer);
		drawCenter(FontBigNo, vb, px, contentCY - ResultConst::FontBigH * 0.55f, valueCol);
	};

	// 重力コア(Glow)版：クリアで手に入れたゴールのコアをシアンで描く
	auto drawGlowCoreValue = [&](int amount)
	{
		char vb[16]; std::snprintf(vb, sizeof(vb), "x%d", amount);
		const int isz = static_cast<int>(ResultConst::PageIconSize * scale);
		const Math::Color glow(GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
							   GravityCoreConst::GlowFaceB, 1.0f);
		CoreIcon::Draw(sprite, static_cast<int>(px),
			static_cast<int>(contentCY + ResultConst::PageIconSize * 0.4f), isz, m_timer, glow, true);
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
	else if (m_resStep == 4)
	{
		// ページ4：入手 いわ（エメラルド）
		drawCenter(FontSmallNo, ResultConst::RockLabel, px, contentCY + contentH * 0.5f - LabelInset, labelCol);
		drawCoreValue(m_resRocks);
	}
	else
	{
		// ページ5：入手 じゅうりょくコア（ゴール＝クリアで必ず1個）
		drawCenter(FontSmallNo, ResultConst::CoreLabel, px, contentCY + contentH * 0.5f - LabelInset, labelCol);
		drawGlowCoreValue(1);
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
	// kind: 0=コイン 1=岩(エメラルド/カラフル岩) 2=重力コア
	auto spawn = [&](int kind, int amount)
	{
		if (amount <= 0) { return; }
		const int count = std::min(amount, ResultConst::TallyMaxPerKind);
		const int base  = amount / count;
		const int rem   = amount % count;
		for (int i = 0; i < count; ++i)
		{
			TallyFlyer f;
			f.isRock = (kind == 1);
			f.isCore = (kind == 2);
			f.start  = startBase + Math::Vector3(rnd() * ResultConst::TallySpread, rnd() * ResultConst::TallySpread * 0.5f, 0.0f);
			f.delay  = delay;
			f.t      = 0.0f;
			f.add    = base + (i < rem ? 1 : 0);
			f.done   = false;
			m_flyers.push_back(f);
			delay += ResultConst::TallyStagger;
		}
	};
	const int emeraldGems = std::max(0, m_resRocks - m_resCores);   // 岩系合計から重力コアを除いた分
	spawn(0, m_resCoins);     // コイン
	spawn(1, emeraldGems);    // エメラルド／カラフル岩
	spawn(2, m_resCores);     // 重力コア（専用アイコン）

	m_tallyActive = !m_flyers.empty();

	// 取得アイテムをUIへ送り出す音
	if (m_tallyActive) { SoundManager::Instance().PlaySE(SeId::UiSend, SoundConst::SeVolume); }
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
			// UIが吸収する音（カウント加算の瞬間）
			SoundManager::Instance().PlaySE(SeId::UiAbsorb, SoundConst::SeVolume);
			if (f.isRock || f.isCore)   // 重力コアも岩系合計に加算
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
//----------------------------------------------------------
// 角丸フレーム（金縁＋背景）を1回で描く共通ヘルパー
//----------------------------------------------------------
void StageSelectScene::DrawRoundedFrame(float cx, float cy, float halfW, float halfH, float radius,
	const Math::Color& body, const Math::Color& edge)
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const float t = static_cast<float>(ResultConst::EdgeThickness);
	sprite.DrawRoundedBox(static_cast<int>(cx), static_cast<int>(cy),
		static_cast<int>(halfW + t), static_cast<int>(halfH + t), radius + t, &edge, 8);
	sprite.DrawRoundedBox(static_cast<int>(cx), static_cast<int>(cy),
		static_cast<int>(halfW), static_cast<int>(halfH), radius, &body, 8);
}

//----------------------------------------------------------
// 選択ステージの詳細：画面下部の情報バー（名前/クリア/最高コイン/ベストタイム）。
// GO/BACK のヒントはバーの上に別で出す。
//----------------------------------------------------------
void StageSelectScene::DrawSelectPanel()
{
	using namespace StageSelectConst;
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);
	const float a  = std::clamp(m_selZoom, 0.0f, 1.0f);

	// 中央揃え＋右下シャドウのテキスト
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
		const Math::Color shc(0.0f, 0.0f, 0.0f, col.w * TextFxConst::ShadowAlphaMul);
		sprite.DrawFont(fs, Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY), &shc, 0);
		sprite.DrawFont(fs, pos, &col, 0);
	};

	const int sid = m_nodes[m_current].stageId;
	const auto& rec = StageManager::Instance().GetRecord(sid);

	// ── 上部バー（スコア詳細）。上から少し下げてフェードイン ──
	const float barW   = sw * SelBarWFrac;
	const float barH   = SelBarH;
	const float slide  = (1.0f - a) * (barH + SelBarMargin);   // 上からスライドイン
	const float barCY  = sh * 0.5f - SelBarMargin - barH * 0.5f + slide;

	const float bhw = barW * 0.5f;
	const float bhh = barH * 0.5f;
	const int   ibcy = static_cast<int>(barCY);
	const int   ibhw = static_cast<int>(bhw);
	const int   ibhh = static_cast<int>(bhh);

	// 金縁
	const Math::Color edge(ResultConst::EdgeR, ResultConst::EdgeG, ResultConst::EdgeB, ResultConst::EdgeA * a);
	const float et = static_cast<float>(ResultConst::EdgeThickness);
	sprite.DrawRoundedBox(0, ibcy, ibhw + static_cast<int>(et), ibhh + static_cast<int>(et),
		SelHintBoxRadius + et, &edge, ThumbCornerSegs);

	// 背景：単色ベース → ステージ画像を角丸で切り抜いて敷く → 暗幕（文字可読性）
	const Math::Color body(ResultConst::PanelR, ResultConst::PanelG, ResultConst::PanelB, SelBarA * a);
	sprite.DrawRoundedBox(0, ibcy, ibhw, ibhh, SelHintBoxRadius, &body, ThumbCornerSegs);

	const auto& barThumb = (sid >= 0 && sid < static_cast<int>(m_stageThumbs.size())) ? m_stageThumbs[sid] : nullptr;
	if (barThumb)
	{
		const Math::Color tint(SelBarImgBright, SelBarImgBright, SelBarImgBright, SelBarImgAlpha * a);
		sprite.DrawRoundedTex(barThumb.get(), 0, ibcy, ibhw, ibhh, SelHintBoxRadius, &tint, ThumbCornerSegs);
		const Math::Color scrim(0.0f, 0.0f, 0.0f, SelBarScrimAlpha * a);
		sprite.DrawRoundedBox(0, ibcy, ibhw, ibhh, SelHintBoxRadius, &scrim, ThumbCornerSegs);
	}

	// ── 4列：STAGE / STATUS / BEST COINS / BEST TIME ──
	const float innerL = -barW * 0.5f + SelBarPad;
	const float innerR =  barW * 0.5f - SelBarPad;
	const float colW   = (innerR - innerL) / 4.0f;
	const float labelY = barCY + barH * 0.22f;   // ラベル（上）
	const float valueY = barCY - barH * 0.18f;   // 値（下）

	const Math::Color label(ResultConst::LabelR, ResultConst::LabelG, ResultConst::LabelB, a);
	const Math::Color value(ResultConst::ValueR, ResultConst::ValueG, ResultConst::ValueB, a);

	auto colCX = [&](int i) { return innerL + colW * (static_cast<float>(i) + 0.5f); };

	char buf[96];

	// 列0：STAGE / 名前
	{
		const char* name = (sid >= 0 && sid < StageNameCount) ? StageNames[sid] : StageNameFallback;
		drawCenter(ResultConst::FontSmallNo, "ステージ", colCX(0), labelY, label);
		drawCenter(ResultConst::FontSmallNo, name,    colCX(0), valueY, value);
	}
	// 列1：STATUS / クリア状況（未解放なら「ロック」表示）
	{
		const bool unlocked = IsUnlocked(sid);
		const char* mark;
		Math::Color mc;
		if (!unlocked)
		{
			mark = SelLockedMark;
			mc   = Math::Color(0.85f, 0.45f, 0.45f, a);   // 赤系＝ロック
		}
		else if (rec.cleared)
		{
			mark = SelClearedMark;
			mc   = Math::Color(ResultConst::EdgeR, ResultConst::EdgeG, ResultConst::EdgeB, a);
		}
		else
		{
			mark = SelNotClearedMark;
			mc   = Math::Color(0.7f, 0.7f, 0.75f, a);
		}
		drawCenter(ResultConst::FontSmallNo, "じょうたい", colCX(1), labelY, label);
		drawCenter(ResultConst::FontSmallNo, mark,     colCX(1), valueY, mc);
	}
	// 列2：BEST COINS / xN
	{
		std::snprintf(buf, sizeof(buf), "x%d", rec.bestCoins);
		drawCenter(ResultConst::FontSmallNo, SelBestCoinLabel, colCX(2), labelY, label);
		drawCenter(ResultConst::FontMidNo,   buf,              colCX(2), valueY, value);
	}
	// 列3：BEST TIME / mm:ss.cc
	{
		if (rec.bestTime > 0.0f)
		{
			const int totalCs = static_cast<int>(rec.bestTime * 100.0f);
			const int cs  = totalCs % 100;
			const int sec = (totalCs / 100) % 60;
			const int min = totalCs / 6000;
			std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d", min, sec, cs);
		}
		else { std::snprintf(buf, sizeof(buf), "%s", SelNoTime); }
		drawCenter(ResultConst::FontSmallNo, SelBestTimeLabel, colCX(3), labelY, label);
		drawCenter(ResultConst::FontMidNo,   buf,              colCX(3), valueY, value);
	}

	// ── GO/BACK ボタン（画面下の左右。BACK=左下 / GO=右下。A/Dで選択、Enterで決定）──
	{
		const float boxHalfH = SelHintBoxH * 0.5f;
		const float btnCY    = -sh * 0.5f + SelBarMargin + boxHalfH;   // 下端
		const float pulse    = 0.5f + 0.5f * std::sinf(m_timer * 6.0f);

		const float goHalfW   = m_btnGo.HalfWidth(SelHintBoxPadX);
		const float backHalfW = m_btnBack.HalfWidth(SelHintBoxPadX);

		const float backCX = -sw * 0.5f + SelBarMargin + backHalfW;   // 左下
		const float goCX   =  sw * 0.5f - SelBarMargin - goHalfW;     // 右下

		// 未解放ステージは GO を暗くして押せないことを示す
		const bool  unlocked = IsUnlocked(sid);
		const float goA      = unlocked ? a : a * SelLockedGoAlpha;
		m_btnGo.Draw(  goCX,   btnCY, boxHalfH, SelHintBoxPadX, SelHintBoxRadius, unlocked && m_selChoice == 0, goA, pulse);
		m_btnBack.Draw(backCX, btnCY, boxHalfH, SelHintBoxPadX, SelHintBoxRadius, m_selChoice == 1, a, pulse);

		// ロック時はヒント文を GO の上に表示
		if (!unlocked)
		{
			const Math::Color hintCol(0.95f, 0.6f, 0.6f, a);
			drawCenter(ResultConst::FontSmallNo, SelLockedHint, goCX, btnCY + boxHalfH + SelLockedHintGap, hintCol);
		}
	}
}

void StageSelectScene::DrawTotalsHud()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	// アイコン＋数字（ポップ拡大つき）
	auto drawRow = [&](const std::shared_ptr<KdTexture>& tex, const Math::Vector3& pos, int total, float pop, bool core = false)
	{
		const float k   = (pop > 0.0f) ? (pop / ResultConst::HudPopTime) : 0.0f;
		const float scl = 1.0f + ResultConst::HudPopScale * k;
		const int   isz = static_cast<int>(ResultConst::HudIconSize * scl);
		const Math::Color ic(1.0f, 1.0f, 1.0f, 1.0f);
		if (core) { CoreIcon::Draw(sprite, static_cast<int>(pos.x), static_cast<int>(pos.y), isz, m_timer); }
		else
		{
			// コインは3Dモデル(RT)を優先、無ければ従来テクスチャ
			KdTexture* useTex = m_coinIcon.GetTexture();
			if (!useTex && tex) { useTex = tex.get(); }
			if (useTex) { sprite.DrawTex(useTex, static_cast<int>(pos.x), static_cast<int>(pos.y), isz, isz, nullptr, &ic); }
		}

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
	drawRow(nullptr,   HudRockPos(), StageManager::Instance().GetTotalRocks(), m_rockHudPop, true);   // いわ（かけら）

	// ── ゴールの重力コア（Glow）：取り戻した数＝クリア済みステージ数（シアンで描画）──
	{
		int cleared = 0;
		for (int i = 0; i < StageSelectConst::StageNameCount; ++i)
		{
			if (StageManager::Instance().GetRecord(i).cleared) { ++cleared; }
		}
		Math::Vector3 cpos = HudRockPos();
		cpos.y -= ResultConst::HudRowGap;   // いわの下
		const Math::Color glow(GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
							   GravityCoreConst::GlowFaceB, 1.0f);
		CoreIcon::Draw(sprite, static_cast<int>(cpos.x), static_cast<int>(cpos.y),
			ResultConst::HudIconSize, m_timer, glow, true);   // Glowスタイルで再現

		char nb[24]; std::snprintf(nb, sizeof(nb), "x%d", cleared);
		auto fs = KdFontManager::Instance().CreateFontTexture(ResultConst::HudFontNo, nb, false);
		if (fs)
		{
			float h = 0.0f;
			for (const auto& d : fs->GetTexList()) { if (d && d->FontTex) { h = std::max(h, static_cast<float>(d->FontTex->GetInfo().Height)); } }
			const float tx = cpos.x + ResultConst::HudIconSize * 0.5f + ResultConst::HudTextGap;
			const float ty = cpos.y - h * 0.5f;
			const Math::Color sh(0.0f, 0.0f, 0.0f, TextFxConst::ShadowAlphaMul);
			sprite.DrawFont(fs, Math::Vector2(tx + TextFxConst::ShadowOffX, ty - TextFxConst::ShadowOffY), &sh, 0);
			const Math::Color tc(1.0f, 1.0f, 1.0f, 1.0f);
			sprite.DrawFont(fs, Math::Vector2(tx, ty), &tc, 0);
		}
	}

	// タリー飛行アイコン（プレイヤー→HUDへ弧を描いてホーミング）
	if (m_tallyActive)
	{
		for (const auto& f : m_flyers)
		{
			if (f.done || f.delay > 0.0f) { continue; }
			// 行き先：コア/岩は岩HUD、コインはコインHUD
			const Math::Vector3 target = (f.isRock || f.isCore) ? HudRockPos() : HudCoinPos();
			const float te = f.t * f.t * (3.0f - 2.0f * f.t);   // smoothstep
			const Math::Vector3 ctrl = (f.start + target) * 0.5f + Math::Vector3(0.0f, ResultConst::TallyArcUp, 0.0f);
			const float u = 1.0f - te;
			const Math::Vector3 pos = f.start * (u * u) + ctrl * (2.0f * u * te) + target * (te * te);
			if (f.isCore)
			{
				// 重力コア＝青いクリスタル（Glowスタイル）
				const Math::Color glow(GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
					GravityCoreConst::GlowFaceB, 1.0f);
				CoreIcon::Draw(sprite, static_cast<int>(pos.x), static_cast<int>(pos.y),
					ResultConst::TallyFlySize, m_timer, glow, true);
			}
			else if (f.isRock)
			{
				// エメラルド／カラフル岩＝虹色の岩
				CoreIcon::Draw(sprite, static_cast<int>(pos.x), static_cast<int>(pos.y),
					ResultConst::TallyFlySize, m_timer);
			}
			else
			{
				// コインは3Dモデル(RT)を優先、無ければ従来テクスチャ、それも無ければ金の円
				KdTexture* useTex = m_coinIcon.GetTexture();
				if (!useTex && m_coinTex) { useTex = m_coinTex.get(); }
				if (useTex)
				{
					const Math::Color ic(1.0f, 1.0f, 1.0f, 1.0f);
					sprite.DrawTex(useTex, static_cast<int>(pos.x), static_cast<int>(pos.y),
						ResultConst::TallyFlySize, ResultConst::TallyFlySize, nullptr, &ic);
				}
				else
				{
					const Math::Color gold(1.0f, 0.85f, 0.25f, 1.0f);
					sprite.DrawCircle(static_cast<int>(pos.x), static_cast<int>(pos.y),
						ResultConst::TallyFlySize / 2, &gold, true);
				}
			}
		}
	}
}
