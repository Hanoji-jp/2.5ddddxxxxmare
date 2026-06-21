#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Const/LightConst.h"
#include"../../Const/JuiceConst.h"
#include"../../Const/HealConst.h"
#include"../../Const/CoreliaConst.h"
#include"../../Const/GravityArrowConst.h"
#include"../../Const/EditorPickConst.h"
#include"../../Const/IntroConst.h"
#include"../../Const/ClearConst.h"
#include"../../Camera/CameraSettings.h"
#include"../../Manager/ModelManager.h"
#include"../../Manager/StageManager.h"
#include"../../../Framework/Utility/KdDebug/KdDebugGUI.h"
#include"../../../Framework/Math/KdEasing.h"
#include <fstream>
#include <sstream>
#include <cfloat>

//----------------------------------------------------------
// サンプリング済み曲線上を、始点からの距離 dist で位置を取得する。
// 弧長ベースなので等速移動になる。
//----------------------------------------------------------
static Math::Vector3 SampleCurveByDistance(const std::vector<Math::Vector3>& curve,
										   float dist, Math::Vector3& outDir)
{
	const int n = static_cast<int>(curve.size());
	if (n == 0) { outDir = Math::Vector3::Up; return Math::Vector3::Zero; }
	if (n == 1) { outDir = Math::Vector3::Up; return curve[0]; }

	float accum = 0.0f;
	for (int i = 0; i < n - 1; ++i)
	{
		Math::Vector3 seg = curve[i + 1] - curve[i];
		const float segLen = seg.Length();
		if (dist <= accum + segLen || i == n - 2)
		{
			const float local = (segLen > 1e-6f) ? (dist - accum) / segLen : 0.0f;
			const float t = std::clamp(local, 0.0f, 1.0f);
			if (segLen > 1e-6f) { seg.Normalize(); }
			outDir = seg;
			return Math::Vector3::Lerp(curve[i], curve[i + 1], t);
		}
		accum += segLen;
	}
	outDir = Math::Vector3::Up;
	return curve.back();
}

void GameScene::Event()
{
	// ── ポーズメニュー（TAB で開閉。ESC はアプリ終了に使われるため使わない）──
	{
		const bool tab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
		if (tab && !m_menuEscPrev) { m_menuOpen = !m_menuOpen; m_menuIndex = 0; }
		m_menuEscPrev = tab;
	}

	// ── デバッグ可視化トグル（1キー）：ManualZone以外の判定表示をON/OFF ──
	{
		const bool k1 = (GetAsyncKeyState('1') & 0x8000) != 0;
		if (k1 && !m_debugKeyPrev) { m_debugZonesVisible = !m_debugZonesVisible; }
		m_debugKeyPrev = k1;
	}

	// ── エディタ画面トグル（F3）：ゲームをImGuiウィンドウ表示 ⇔ 通常フルスクリーン ──
	{
		const bool k3 = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
		if (k3 && !m_editorKeyPrev) { m_editorScreen = !m_editorScreen; }
		m_editorKeyPrev = k3;
	}
	// エディタ画面ON時のみImGui表示（OFFで全ImGui非表示＝通常フルスクリーン）。Eventで毎フレーム反映
	KdDebugGUI::Instance().SetGameViewport(m_editorScreen);

	// 重力矢印の流れ（重力方向へスクロール）
	m_gravArrowScroll += KdFPSController::GetDt();
	if (m_menuOpen) { UpdatePauseMenu(); return; }

	// ── コアリア会話：会話中は世界を止めてカメラを寄せる ──
	UpdateCorelia();
	if (m_convoActive) { return; }

	// ── 導入カットシーン（Stage1：落下きりもみ→ズサーバタン着地）──
	if (m_introCutscene || m_introLanding) { UpdateIntroCutscene(); }

	// ── ステージクリア演出（ゴールコア取得→周回カメラ→暗転→StageSelectへ）──
	if (m_clearActive)
	{
		m_clearTimer += KdFPSController::GetDt();

		// スター取得風カメラ：旋回→（決め）少し引く→ズーム→少し保持→もう一度引く（全部連続）
		if (m_camera && m_spPlayer)
		{
			constexpr float kPi = 3.14159265f;
			const float tt = m_clearTimer;

			// 回転：オービット区間を easeOutBack（行き過ぎて戻る＝ドリフト/慣性）でスイープ。
			// 目標角を一度オーバーシュートしてから滑り込むので、急ターンにならない。
			const float yp = std::clamp(tt / ClearConst::CamOrbitTime, 0.0f, 1.0f);
			const float c1 = ClearConst::CamOrbitOvershoot;
			const float c3 = c1 + 1.0f;
			const float pm = yp - 1.0f;
			const float yawT = 1.0f + c3 * pm * pm * pm + c1 * pm * pm;   // easeOutBack
			const float yawDeg = ClearConst::CamStartYawDeg + ClearConst::CamTotalOrbitDeg * yawT;

			// フレーミング進行 ez：旋回中は中間まで控えめ → 旋回後に一気にスナップ（決め）。
			// 旋回区間は smootherstep で MidFrac までゆるく寄せ、決め区間は easeOutBack で
			// 1.0 を一度オーバーシュート（＝近くへ突っ込んで急減速＝ヒットストップ風）してから収まる。
			float ez;
			if (tt <= ClearConst::CamOrbitTime)
			{
				const float op = std::clamp(tt / ClearConst::CamOrbitTime, 0.0f, 1.0f);
				const float ss = op * op * op * (op * (op * 6.0f - 15.0f) + 10.0f);   // smootherstep
				ez = ClearConst::CamMidFrac * ss;                                     // 0 -> MidFrac
			}
			else
			{
				const float dp = std::clamp(
					(tt - ClearConst::CamOrbitTime) / (ClearConst::CamDecideEnd - ClearConst::CamOrbitTime), 0.0f, 1.0f);
				const float dpm = dp - 1.0f;
				const float eb  = 1.0f + (ClearConst::CamDecideOvershoot + 1.0f) * dpm * dpm * dpm
					+ ClearConst::CamDecideOvershoot * dpm * dpm;                      // easeOutBack（強オーバーシュート）
				ez = std::lerp(ClearConst::CamMidFrac, 1.0f, eb);                     // MidFrac -> 1（突っ込む）
			}
			const float pitchDeg = std::lerp(ClearConst::CamStartPitchDeg, ClearConst::CamEndPitchDeg, ez);
			const float focusUp  = std::lerp(ClearConst::CamStartFocusUp,  ClearConst::CamEndFocusUp,  ez);

			// 決めのスナップ着地点で小フラッシュを1回（パンチを足す）
			if (!m_clearDecideFlashed && tt >= ClearConst::CamDecideEnd)
			{
				TriggerFlash(ClearConst::DecideFlash);
				m_clearDecideFlashed = true;
			}

			// 距離：寄り（ez連動でスナップ＋突っ込み）→ 保持 → 引き
			float distV;
			if (tt <= ClearConst::CamDecideEnd)
			{
				distV = std::lerp(ClearConst::CamStartDist, ClearConst::CamEndDist, ez); // ezがオーバーシュートで突っ込む
			}
			else if (tt <= ClearConst::CamHoldEnd)
			{
				distV = ClearConst::CamEndDist;   // 決めを保持（ヒットストップ）
			}
			else
			{
				const float pp = std::clamp(
					(tt - ClearConst::CamHoldEnd) / (ClearConst::CamPullEnd - ClearConst::CamHoldEnd), 0.0f, 1.0f);
				const float ep = pp * pp * (3.0f - 2.0f * pp);
				distV = std::lerp(ClearConst::CamEndDist, ClearConst::FadePullbackDist, ep);
			}

			// 傾き(roll, 度)：旋回中に先に左へ寝かせる → 寄せる瞬間に反対(右)へドリフトで振る → 保持 → 引きで水平へ
			float roll = 0.0f;
			if (tt <= ClearConst::CamOrbitTime)
			{
				// 旋回中に DecideTiltDeg(左) まで傾け切る（スナップ前にもう傾いている）
				const float op = std::clamp(tt / ClearConst::CamOrbitTime, 0.0f, 1.0f);
				const float ss = op * op * op * (op * (op * 6.0f - 15.0f) + 10.0f);   // smootherstep
				roll = ClearConst::DecideTiltDeg * ss;
			}
			else if (tt <= ClearConst::CamDecideEnd)
			{
				// 寄せの瞬間に左→右へ easeOutBack（行き過ぎて戻る＝ドリフト）で振り切る
				const float rp = std::clamp(
					(tt - ClearConst::CamOrbitTime) / (ClearConst::CamDecideEnd - ClearConst::CamOrbitTime), 0.0f, 1.0f);
				const float rpm = rp - 1.0f;
				const float eb  = 1.0f + (ClearConst::CamDecideOvershoot + 1.0f) * rpm * rpm * rpm
					+ ClearConst::CamDecideOvershoot * rpm * rpm;     // easeOutBack
				roll = std::lerp(ClearConst::DecideTiltDeg, ClearConst::DecideTiltSnapDeg, eb);
			}
			else if (tt <= ClearConst::CamHoldEnd)
			{
				roll = ClearConst::DecideTiltSnapDeg;   // 決めの間は右傾きを保持
			}
			else
			{
				// 引き：傾きを水平へ戻す
				const float bp = std::clamp(
					(tt - ClearConst::CamHoldEnd) / (ClearConst::CamPullEnd - ClearConst::CamHoldEnd), 0.0f, 1.0f);
				roll = ClearConst::DecideTiltSnapDeg * (1.0f - bp * bp * (3.0f - 2.0f * bp));
			}

			m_spPlayer->SetCutsceneFaceZ(tt >= ClearConst::CamOrbitTime);

			const float pitch = DirectX::XMConvertToRadians(pitchDeg);
			const float yaw   = DirectX::XMConvertToRadians(yawDeg);
			const float cp    = std::cosf(pitch);

			const Math::Vector3 focus = m_spPlayer->GetPos()
				+ Math::Vector3(0.0f, focusUp, 0.0f);
			const Math::Vector3 eye = focus + Math::Vector3(
				std::sinf(yaw) * cp, std::sinf(pitch), std::cosf(yaw) * cp) * distV;

			Math::Vector3 f = focus - eye; f.Normalize();
			Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize();
			Math::Vector3 u = f.Cross(r);

			// 前方軸まわりの傾き（ロール）を適用
			const float rollRad = DirectX::XMConvertToRadians(roll);
			const float cr  = std::cosf(rollRad);
			const float srl = std::sinf(rollRad);
			const Math::Vector3 rRoll = r * cr + u * srl;
			const Math::Vector3 uRoll = u * cr - r * srl;
			r = rRoll;
			u = uRoll;
			Math::Matrix world;
			world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
			world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
			world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
			world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
			m_camera->SetCameraMatrix(world);
		}

		if (m_clearTimer >= ClearConst::CamPullEnd)
		{
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::StageSelect);
		}
		return;   // クリア中はゲーム進行を止める
	}

	// ── 死亡シーケンス（暗転→復活→明転）。ネストの外で必ず毎フレーム実行する ──
	if (m_spPlayer)
	{
		// 落下死：一定時間ずっと接地していない＝場外に落ちた（重力方向に依存しない判定）
		// エディタ中・導入カットシーン中は判定しない（勝手に死なないように）
		if (!m_editorMode && !m_introCutscene && !m_deathActive && !m_spPlayer->IsDead())
		{
			// パラソル滑空中は滞空が長くなるのが正常なので落下死タイマーを止める
			if (m_spPlayer->IsGround() || m_spPlayer->IsParasolOpen()) { m_airTime = 0.0f; }
			else                                                       { m_airTime += KdFPSController::GetDt(); }
			if (m_airTime >= DeathConst::FallTime)
			{
				m_spPlayer->InstantDeath();
				m_airTime = 0.0f;
			}
		}
		else if (m_editorMode)
		{
			// エディタ中は滞空タイマーを溜めない（退出直後に即死しないようリセット）
			m_airTime = 0.0f;
		}

		if (m_deathActive)
		{
			m_deathTimer += KdFPSController::GetDt();

			// 残機が1つ減る瞬間にSEを鳴らす（1回だけ。.wav 未配置なら無音）
			if (!m_lifeLostSePlayed)
			{
				const float holdT = m_deathTimer - DeathConst::FadeOutTime;
				const float p = (holdT - DeathConst::LoseAnimDelay) / DeathConst::LoseAnimTime;
				if (holdT >= 0.0f && p >= DeathConst::SparkStartP)
				{
					KdAudioManager::Instance().Play(DeathConst::LifeLostSePath);
					m_lifeLostSePlayed = true;
				}
			}

			// 暗転しきって残機の減りを見せ終えたタイミングで復活／リトライを実行
			const float kAction = DeathConst::FadeOutTime + DeathConst::HoldTime;
			if (!m_deathRevived && m_deathTimer >= kAction)
			{
				if (m_deathCount >= DeathConst::MaxDeathsPerStage)
				{
					// 死亡回数が上限に達した → 画面が暗転している今のうちにステージを最初からやり直し
					SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
				}
				else
				{
					Respawn();                      // 復活（位置・HP・速度リセット）
					m_prevPlayerHp = m_spPlayer->GetHp();
				}
				m_deathRevived = true;
			}
			if (m_deathTimer >= kAction + DeathConst::FadeInTime)
			{
				m_deathActive = false;
			}
		}
		else if (m_spPlayer->IsDead())
		{
			// 死んだ瞬間：演出開始（死亡カメラの注視点を死んだ場所に固定）
			m_deathActive   = true;
			m_deathTimer    = 0.0f;
			m_deathRevived  = false;
			m_lifeLostSePlayed = false;           // 減るSEの再生フラグをリセット
			m_deathCount++;                       // この死亡をカウント（上限到達でリトライ）
			m_deathCamFocus = m_spPlayer->GetPos() + m_spPlayer->GetUpDir() * DeathConst::CamFocusHeight;
		}
	}

	// 惑星のワールド行列を毎フレーム更新
	PlanetGravityManager::Instance().PostUpdate();

	// ── アイテム（コイン・パラソル）の Update は常時実行 ──────
	// pickup 判定は下の !m_editorMode ブロックで行う
	for (auto& item : m_itemManager.WorkParasols())
	{
		if (!item->IsExpired()) { item->Update(); }
	}

	// ── テレポート出現スケールポップ ─────────────────────────
	if (m_spPlayer && m_teleportPopTimer > 0.0f)
	{
		const float kDt  = KdFPSController::GetDt();
		m_teleportPopTimer  -= kDt;
		const float t        = std::max(0.0f, m_teleportPopTimer / JuiceConst::PopDuration); // 1→0
		const float popScale = 1.0f + (JuiceConst::PopOvershoot - 1.0f) * t;
		m_spPlayer->SetWarpScale(popScale);
		if (m_teleportPopTimer <= 0.0f)
		{
			m_teleportPopTimer = 0.0f;
			m_spPlayer->SetWarpScale(1.0f);
		}
	}

	// F2でエディターモードのトグル（チャタリング防止）
	const bool f2Now = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
	if (f2Now && !m_f2Prev)
	{
		m_editorMode = !m_editorMode;

		if (m_editorMode)
		{
			// エディターカメラに切り替え（SideScrollCamera は破棄されるので観察ポインタを先に null にする）
			m_pCamera        = nullptr;
			auto upEditorCam = std::make_unique<EditorCamera>();
			m_pEditorCam     = upEditorCam.get();

			// プレイヤーの位置からエディターカメラを開始
			if (m_spPlayer)
			{
				Math::Vector3 playerPos = m_spPlayer->GetPos();
				// カメラを少し後ろと上に配置
				playerPos.z -= 15.0f;
				playerPos.y += 3.0f;
				m_pEditorCam->SetPos(playerPos);
			}

			m_camera         = std::move(upEditorCam);

			// エディタ中はプレイヤー操作を絶対ロック（入力で動かさない）
			if (m_spPlayer) { m_spPlayer->SetControlEnabled(false); }
			// 選択・ドラッグ状態をリセット
			m_selEntry   = -1;
			m_moveActive = false;
			m_dragAxis   = GizmoAxis::None;

			KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
		}
		else
		{
			// ゲームカメラに戻す
			auto upGameCam = std::make_unique<SideScrollCamera>();
			m_pCamera      = upGameCam.get();
			m_camera       = std::move(upGameCam);
			m_pCamera->SetRooms(m_rooms);
			m_pEditorCam   = nullptr;

			// プレイヤー操作を復帰
			if (m_spPlayer) { m_spPlayer->SetControlEnabled(true); }
			m_selEntry   = -1;
			m_moveActive = false;
			m_dragAxis   = GizmoAxis::None;

			// HP UIはゲーム中も常時表示するのでコールバックは維持
			KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
		}
	}
	m_f2Prev = f2Now;

	// カメラ更新
	if (m_editorMode)
	{
		if (m_pEditorCam) { m_pEditorCam->Update(); }

		// エディタ中はプレイヤー操作を毎フレーム強制ロック（外部で有効化されても保険）
		if (m_spPlayer) { m_spPlayer->SetControlEnabled(false); }

		// マウスピッキング＋ドラッグ（オブジェクト選択・移動）
		UpdateEditorPick();
	}
	else
	{
		if (m_spPlayer && m_pCamera)
		{
			// 惑星到達時のカメラズームアウト→戻りをトリガー
			// （フラグは前フレームの PostUpdate() でセット済み）
			if (m_spPlayer->IsPlanetChanged())
			{
				m_pCamera->TriggerPlanetZoom();
				//KdDebugGUI::Instance().AddLog("[ZOOM] TriggerPlanetZoom fired!\n");
			}
			// フラグを読み終えたのでここでリセット
			m_spPlayer->ResetPlanetChangedFlag();
			m_pCamera->Update(m_spPlayer->GetPos(), m_spPlayer->GetUpDir());
		}
	}

	// ── 死亡カメラ（マリギャラ風：死んだ場所へ寄りつつゆっくり回り込む）──
	// 暗転前半（復活前）だけ通常カメラを上書きする。復活後は黒画面の裏で通常カメラへ。
	if (m_deathActive && !m_deathRevived && m_camera)
	{
		const float t = std::clamp(m_deathTimer / DeathConst::FadeOutTime, 0.0f, 1.0f);
		const float e = 1.0f - (1.0f - t) * (1.0f - t);   // easeOut（最初に速く寄る）

		const float dist  = DeathConst::CamStartDist + (DeathConst::CamCloseDist - DeathConst::CamStartDist) * e;
		const float yaw   = DirectX::XMConvertToRadians(DeathConst::CamStartYawDeg + DeathConst::CamOrbitDeg * e);
		const float pitch = DirectX::XMConvertToRadians(DeathConst::CamPitchDeg);
		const float cp    = std::cosf(pitch);

		Math::Vector3 focus = m_deathCamFocus;
		Math::Vector3 eye   = focus +
			Math::Vector3(std::sinf(yaw) * cp, std::sinf(pitch), std::cosf(yaw) * cp) * dist;

		// 死亡の瞬間の揺れ（時間で減衰）
		const float shake = DeathConst::CamShake * (1.0f - t);
		if (shake > 0.0f)
		{
			auto rnd = []() { return (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f; };
			eye.x += rnd() * shake;
			eye.y += rnd() * shake;
		}

		Math::Vector3 f = focus - eye; f.Normalize();
		Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize();
		Math::Vector3 u = f.Cross(r);
		Math::Matrix world;
		world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
		world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
		world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
		world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
		m_camera->SetCameraMatrix(world);
	}

	// ── 導入カットシーン：下から見上げて降下を見せ、着地した瞬間から通常カメラへ戻す ──
	if ((m_introCutscene || m_introLanding) && m_camera && m_spPlayer)
	{
		const auto& cs = CameraSettings::Instance();
		const float landDist = std::fabs(cs.OffsetZ);

		const Math::Vector3 focus = m_spPlayer->GetPos();   // プレイヤーを注視

		// 各姿勢の視点：上からの覗き込み／通常カメラ
		const Math::Vector3 eyeAbove = focus + Math::Vector3(0.0f, IntroConst::CamAboveHeight, -IntroConst::CamAboveBack);
		const Math::Vector3 eyeNorm  = focus + Math::Vector3(cs.OffsetX, cs.OffsetY, -landDist);
		const Math::Vector3 lookNorm = focus + Math::Vector3(cs.LookAheadX, cs.LookAtHeight, 0.0f);

		Math::Vector3 eye, look;
		if (m_introCutscene)
		{
			// 落下中：ずっと下から見上げ → 着地しそうになったら上から覗くへ（lerpで寄せて止める）
			const float dur = (IntroConst::Duration > 0.0f) ? IntroConst::Duration : 1.0f;
			const float p   = std::clamp(m_introTimer / dur, 0.0f, 1.0f);
			const float ein = 1.0f - (1.0f - p) * (1.0f - p);   // easeOut
			const float dist = std::lerp(IntroConst::CamStartDist, IntroConst::CamCloseDist, ein);

			// 下から覗く（真下＋少し手前-Z）
			const Math::Vector3 eyeBelow = focus + Math::Vector3(0.0f, -dist, -dist * 0.25f);

			// 着地しそう(UprightStart以降)で 下→上 へsmoothstepでブレンドして止める
			float a = 0.0f;
			if (p >= IntroConst::UprightStart)
			{
				const float ta = (p - IntroConst::UprightStart) / (1.0f - IntroConst::UprightStart);
				a = ta * ta * (3.0f - 2.0f * ta);
			}
			eye  = Math::Vector3::Lerp(eyeBelow, eyeAbove, a);
			look = focus;
		}
		else
		{
			// 地面に着地した！ → 上から覗く姿勢から通常カメラへ戻す
			const float lt = std::clamp(m_introLandTimer / IntroConst::LandRecoverTime, 0.0f, 1.0f);
			const float le = lt * lt * (3.0f - 2.0f * lt);   // smoothstep
			eye  = Math::Vector3::Lerp(eyeAbove, eyeNorm,  le);
			look = Math::Vector3::Lerp(focus,    lookNorm, le);
		}

		Math::Vector3 f = look - eye; f.Normalize();
		Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize();
		Math::Vector3 u = f.Cross(r);
		Math::Matrix world;
		world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
		world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
		world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
		world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
		m_camera->SetCameraMatrix(world);
	}

	// ── エディタ Dirty チェック（モードに関わらず毎フレーム反映）──────
	if (m_enemyEditor.IsDirty())
	{
		RebuildEnemies();
		m_enemyEditor.ClearDirty();
	}
	if (m_checkpointEditor.IsDirty())
	{
		RebuildCheckpoints();
		m_checkpointEditor.ClearDirty();
	}
	if (m_warpHoleEditor.IsDirty())
	{
		const auto& holes = m_warpHoleEditor.GetHoles();
		if (holes.size() != m_warpHoles.size())
		{
			RebuildWarpHoles();
		}
		else
		{
			for (int i = 0; i < static_cast<int>(holes.size()); ++i)
			{
				m_warpHoles[i]->SetData(holes[i]);
			}
		}
		m_warpHoleEditor.ClearDirty();
	}
	if (m_movingFloorEditor.IsDirty())
	{
		RebuildMovingFloors();
		m_movingFloorEditor.ClearDirty();
	}
	if (m_windBoxEditor.IsDirty())
	{
		RebuildWindBoxes();
		m_windBoxEditor.ClearDirty();
	}
	if (m_gravityCoreEditor.IsDirty())
	{
		RebuildGravityCores();
		m_gravityCoreEditor.ClearDirty();
	}
	if (m_spikeBoxEditor.IsDirty())
	{
		RebuildSpikeBoxes();
		m_spikeBoxEditor.ClearDirty();
	}

	// ── 風ボックス：範囲内プレイヤーに風を適用 ──────────
	bool inAnyWind = false;
	if (m_spPlayer && !m_spPlayer->IsExpired())
	{
		for (const auto& wb : m_windBoxes)
		{
			if (!wb || !wb->IsEnabled()) { continue; }
			if (wb->IsInRange(m_spPlayer->GetPos()))
			{
				m_spPlayer->ApplyWind(wb->GetWindDir(), wb->GetPower());
				inAnyWind = true;
			}
		}
	}
	if (m_spPlayer && !m_spPlayer->IsExpired() && !inAnyWind)
	{
		m_spPlayer->ClearWindState();
	}
	if (!m_editorMode)
	{
		// ── ワープホール判定 ─────────────────────────────
		if (m_spPlayer && !m_spPlayer->IsExpired())
		{
			const float dt = KdFPSController::GetDt();

			if (m_warpPhase == WarpPhase::Sucking)
			{
				// ── フェーズ1：入口を中心に螺旋しながら吸い込まれる ──
				m_warpSuckProgress += dt / WarpHoleConst::SuckDuration;
				m_warpSuckProgress  = std::min(m_warpSuckProgress, 1.0f);

				KdEase ease;
				const float easedT = ease.InOutSine(m_warpSuckProgress);

				// 入口への軸を構築
				Math::Vector3 axisY = m_warpEntryPos - m_warpSuckStartPos;
				if (axisY.LengthSquared() < 0.0001f) { axisY = Math::Vector3::Up; }
				axisY.Normalize();

				Math::Vector3 axisX = (std::abs(axisY.y) < 0.9f)
					? Math::Vector3(0.f, 1.f, 0.f)
					: Math::Vector3(1.f, 0.f, 0.f);
				axisX -= axisY * axisX.Dot(axisY);
				axisX.Normalize();
				Math::Vector3 axisZ;
				axisX.Cross(axisY, axisZ);
				axisZ.Normalize();

				const float r     = WarpHoleConst::SuckSpiralRadius * (1.0f - easedT);
				const float angle = m_warpSuckStartAngle + easedT * WarpHoleConst::SuckSpinRevolutions * DirectX::XM_2PI;

				const float alongAxis = (m_warpSuckStartPos - m_warpEntryPos).Dot(axisY);
				const float along     = alongAxis * (1.0f - easedT);

				const Math::Vector3 pos = m_warpEntryPos
					+ axisY * along
					+ axisX * (r * cosf(angle))
					+ axisZ * (r * sinf(angle));

				m_spPlayer->SetPos(pos);
				m_spPlayer->SetVelocity(Math::Vector3::Zero);

				// 螺旋の接線方向（微小進行方向）を向きとして使う → 位置と向きがシームレスに同期
				const float nextAngle = angle + 0.15f;
				const float nextAlong = alongAxis * (1.0f - std::min(easedT + 0.01f, 1.0f));
				const float nextR     = WarpHoleConst::SuckSpiralRadius * (1.0f - std::min(easedT + 0.01f, 1.0f));
				const Math::Vector3 nextPos = m_warpEntryPos
					+ axisY * nextAlong
					+ axisX * (nextR * cosf(nextAngle))
					+ axisZ * (nextR * sinf(nextAngle));
				Math::Vector3 tangent = nextPos - pos;
				if (tangent.LengthSquared() > 0.0001f) { tangent.Normalize(); }
				else { tangent = axisY; }

				m_spPlayer->SetWarpUpOverride(tangent, WarpHoleConst::WarpRotSlerpSpeedSucking);

				// 吸い込み完了 → 型判別
				if (m_warpSuckProgress >= 1.0f)
				{
					if (m_currentWarpTeleport)
						{
							// テレポート型：フェードアウト開始（Entry 奥へ移動しながら暗転）
							m_warpPhase          = WarpPhase::TeleportFadeOut;
							m_teleportFadeAlpha  = 0.0f;
							m_teleportPathDist   = 0.0f;
							m_spPlayer->ClearWarpUpOverride();
						}
					else
					{
						// トンネル型：パス移動へ
						m_warpPhase       = WarpPhase::Traveling;
						m_warpSegment     = 0;
						m_warpSegProgress = 0.0f;

						m_warpCurve = m_warpPath;
						m_warpCurveTotalLen = 0.0f;
						for (int i = 0; i + 1 < static_cast<int>(m_warpCurve.size()); ++i)
						{
							m_warpCurveTotalLen += (m_warpCurve[i + 1] - m_warpCurve[i]).Length();
							}
							m_warpDist     = 0.0f;
							m_warpProgress = 0.0f;

						m_spPlayer->SetWarpStretch(true);
					}
				}
			}
			else if (m_warpPhase == WarpPhase::TeleportFadeOut)
			{
				// フェードアウト：暗転しながら Entry 奥へプレイヤーを進める
				m_teleportFadeAlpha += WarpHoleConst::TeleportFadeSpeed;
				m_teleportFadeAlpha  = std::min(m_teleportFadeAlpha, 1.0f);

				// パス上を進行（フェード進捗に合わせて距離を割り当て）
				if (!m_teleportEntryPath.empty() && m_teleportPathTotalLen > 1e-4f)
				{
					m_teleportPathDist = m_teleportFadeAlpha * m_teleportPathTotalLen;
					float remain = m_teleportPathDist;
					Math::Vector3 pathPos  = m_teleportEntryPath.front();
					Math::Vector3 tangent  = (m_teleportEntryPath.size() >= 2)
						? (m_teleportEntryPath[1] - m_teleportEntryPath[0])
						: Math::Vector3::Up;
					for (int pi = 0; pi + 1 < static_cast<int>(m_teleportEntryPath.size()); ++pi)
					{
						const Math::Vector3 seg =
							m_teleportEntryPath[pi + 1] - m_teleportEntryPath[pi];
						const float segLen = seg.Length();
						if (remain <= segLen)
						{
							const float t = (segLen > 1e-6f) ? (remain / segLen) : 0.0f;
							pathPos = Math::Vector3::Lerp(
								m_teleportEntryPath[pi],
								m_teleportEntryPath[pi + 1], t);
							tangent = seg;
							break;
						}
						remain -= segLen;
						pathPos = m_teleportEntryPath[pi + 1];
						tangent = seg;
					}
					tangent.Normalize();
					m_spPlayer->SetPos(pathPos);
					m_spPlayer->SetVelocity(Math::Vector3::Zero);
					m_spPlayer->SetWarpUpOverride(tangent, WarpHoleConst::WarpRotSlerpSpeed);
				}

				if (m_teleportFadeAlpha >= 1.0f)
				{
					// 完全暗転 → TeleportHold へ（瞬間移動は Hold で）
					m_warpPhase         = WarpPhase::TeleportHold;
					m_teleportHoldTimer = WarpHoleConst::TeleportHoldTime;
					m_spPlayer->ClearWarpUpOverride();
				}
			}
			else if (m_warpPhase == WarpPhase::TeleportHold)
			{
				// 完全暗転中：プレイヤーを Exit 奥に即座に移動
				m_teleportHoldTimer -= dt;
				if (m_teleportHoldTimer <= 0.0f)
				{
					// Exit 奥（パス先頭）にプレイヤーをテレポート
					if (!m_teleportExitPath.empty())
					{
						m_spPlayer->SetPos(m_teleportExitPath.front());
					}
					else
					{
						m_spPlayer->SetPos(m_teleportExitPos);
					}
					m_spPlayer->SetVelocity(Math::Vector3::Zero);

					// Exit パスの全長を計算
					m_teleportPathTotalLen = 0.0f;
					for (int pi = 0; pi + 1 < static_cast<int>(m_teleportExitPath.size()); ++pi)
					{
						m_teleportPathTotalLen +=
							(m_teleportExitPath[pi + 1] - m_teleportExitPath[pi]).Length();
					}
					m_teleportPathDist = 0.0f;

					m_warpPhase = WarpPhase::TeleportFadeIn;
				}
			}
			else if (m_warpPhase == WarpPhase::TeleportFadeIn)
			{
				// フェードイン：明転しながら Exit 奥→口元 へプレイヤーを進める
				m_teleportFadeAlpha -= WarpHoleConst::TeleportFadeSpeed;
				m_teleportFadeAlpha  = std::max(m_teleportFadeAlpha, 0.0f);

				// フェードイン進捗（1→0 をパス進行 0→1 に変換）
				const float fadeInProgress = 1.0f - m_teleportFadeAlpha;
				if (!m_teleportExitPath.empty() && m_teleportPathTotalLen > 1e-4f)
				{
					m_teleportPathDist = fadeInProgress * m_teleportPathTotalLen;
					float remain = m_teleportPathDist;
					Math::Vector3 pathPos = m_teleportExitPath.front();
					Math::Vector3 tangent = (m_teleportExitPath.size() >= 2)
						? (m_teleportExitPath[1] - m_teleportExitPath[0])
						: Math::Vector3::Up;
					for (int pi = 0; pi + 1 < static_cast<int>(m_teleportExitPath.size()); ++pi)
					{
						const Math::Vector3 seg =
							m_teleportExitPath[pi + 1] - m_teleportExitPath[pi];
						const float segLen = seg.Length();
						if (remain <= segLen)
						{
							const float t = (segLen > 1e-6f) ? (remain / segLen) : 0.0f;
							pathPos = Math::Vector3::Lerp(
								m_teleportExitPath[pi],
								m_teleportExitPath[pi + 1], t);
							tangent = seg;
							break;
						}
						remain -= segLen;
						pathPos = m_teleportExitPath[pi + 1];
						tangent = seg;
					}
					tangent.Normalize();
					m_spPlayer->SetPos(pathPos);
					m_spPlayer->SetVelocity(Math::Vector3::Zero);
					m_spPlayer->SetWarpUpOverride(tangent, WarpHoleConst::WarpRotSlerpSpeed);
				}

				if (m_teleportFadeAlpha <= 0.0f)
					{
						// 明転完了：ExitDir へ射出してワープ終了
						m_teleportFadeAlpha = 0.0f;
						m_warpPhase = WarpPhase::None;
						m_warpCooldown = WarpHoleConst::WarpCooldownTime;
						m_spPlayer->ClearWarpUpOverride();
						Math::Vector3 dir = m_teleportExitDir;
						dir.Normalize();
						m_spPlayer->SetVelocity(dir * WarpHoleConst::LaunchSpeed);
						UpdateCameraZFromExitDir(dir);
						// スケールポップ開始
						m_teleportPopTimer = JuiceConst::PopDuration;
					}
			}
			else if (m_warpPhase == WarpPhase::Traveling)
			{
				// ── フェーズ2：InOutExpo イージングによるパス移動 ──
				// m_warpProgress を 0→1 へ一定速で進め、
				// InOutExpo で「ゆっくり→爆速→ゆっくり」のベジェ感に変換。
				m_warpProgress += dt / WarpHoleConst::WarpTravelDuration;
				m_warpProgress  = std::min(m_warpProgress, 1.0f);

				// InOutExpo イージング
				KdEase ease;
				const float easedT = ease.InOutExpo(m_warpProgress);

				// イージング後の値をパス上の弧長距離に変換
				m_warpDist = easedT * m_warpCurveTotalLen;

				if (m_warpProgress >= 1.0f)
				{
					// ── ワープ完了 ──
					m_warpPhase = WarpPhase::None;
					m_warpCooldown = WarpHoleConst::WarpCooldownTime;
					m_spPlayer->ClearWarpUpOverride();
					m_spPlayer->SetPos(m_warpCurve.empty() ? m_warpPath.back() : m_warpCurve.back());
					Math::Vector3 dir = m_warpExitDir;
					dir.Normalize();
					m_spPlayer->SetVelocity(dir * WarpHoleConst::LaunchSpeed);
					UpdateCameraZFromExitDir(dir);
				}
				else
				{
					Math::Vector3 travelDir;
					const Math::Vector3 pos = SampleCurveByDistance(m_warpCurve, m_warpDist, travelDir);
					m_spPlayer->SetPos(pos);
					m_spPlayer->SetVelocity(Math::Vector3::Zero);

					if (travelDir.LengthSquared() > 0.0001f)
					{
						m_spPlayer->SetWarpUpOverride(travelDir, WarpHoleConst::WarpRotSlerpSpeed);
					}
				}
			}
				else
					{
						// ── クールダウンを消費 ──
						if (m_warpCooldown > 0.0f)
						{
							m_warpCooldown -= dt;
							if (m_warpCooldown < 0.0f) { m_warpCooldown = 0.0f; }
						}

						// ── 通常：吸い込み範囲チェック（クールダウン中は無視）──
						if (m_warpCooldown <= 0.0f)
						{
						const Math::Vector3 playerPos = m_spPlayer->GetPos();
						for (auto& wh : m_warpHoles)
					{
						if (!wh->GetData().Enabled) { continue; }

						const WarpHoleData& d = wh->GetData();
						const float rSq = WarpHoleConst::SuckPullRadius * WarpHoleConst::SuckPullRadius;

						// Entry 側チェック（常に有効）
						bool triggered  = false;
						bool isReverse  = false;
						Math::Vector3 entryPos = d.EntryPos;
						Math::Vector3 exitPos  = d.ExitPos;
						Math::Vector3 exitDir  = d.ExitDir;

						if ((d.EntryPos - playerPos).LengthSquared() <= rSq)
						{
							triggered  = true;
							isReverse  = false;
						}
						// Exit 側チェック（双方向のみ）
						else if (!d.OneWay && (d.ExitPos - playerPos).LengthSquared() <= rSq)
						{
							triggered  = true;
							isReverse  = true;
							// Entry/Exit を入れ替えて逆走
							entryPos = d.ExitPos;
							exitPos  = d.EntryPos;
							// 射出方向は Entry 口元の外向き（逆向き）
							exitDir  = -d.GetEntryMouthDir();
						}

						if (triggered)
							{
								m_warpPhase              = WarpPhase::Sucking;
								m_currentWarpTeleport    = d.Teleport;
								m_teleportExitPos        = exitPos;
								m_teleportExitDir        = exitDir;
								m_warpExitDir            = exitDir;
								m_warpEntryPos           = entryPos;
								m_warpSuckStartPos       = playerPos;
								m_warpSuckProgress       = 0.0f;
								m_warpSuckStartAngle     = atan2f(
									playerPos.z - entryPos.z,
									playerPos.x - entryPos.x);

								// 逆走時はパスを反転
								m_warpPath = wh->BuildTunnelCenterPath();
								if (isReverse) { std::reverse(m_warpPath.begin(), m_warpPath.end()); }

								// テレポート型用：入退場パスを事前生成
									if (d.Teleport)
									{
										// isReverse は上位スコープで決定済み
										// 正走：Entry口元→奥 / Exit奥→口元
										// 逆走：Exit口元→奥 / Entry奥→口元
										if (!isReverse)
										{
											m_teleportEntryPath = wh->BuildTeleportEntryPath();
											m_teleportExitPath  = wh->BuildTeleportExitPath();
										}
										else
										{
											m_teleportEntryPath = wh->BuildTeleportExitPathReverse();
											m_teleportExitPath  = wh->BuildTeleportEntryPathReverse();
										}
										m_teleportPathDist     = 0.0f;
										m_teleportPathTotalLen = 0.0f;
										for (int pi = 0; pi + 1 < static_cast<int>(m_teleportEntryPath.size()); ++pi)
										{
											m_teleportPathTotalLen +=
												(m_teleportEntryPath[pi + 1] - m_teleportEntryPath[pi]).Length();
										}
									}
														break;
													}
											}
										}
									}  // if (m_warpCooldown <= 0.0f)
								}  // else (warpPhase == None)

		// ── デッドゾーン死チェック（死亡シーケンスへ）──────
		// ※ Y座標(DeathY)による落下死は廃止。死亡はデッドゾーンのみ。
		// 導入カットシーン中は判定しない（投げ出され落下中に即死しないように）
		if (!m_introCutscene &&
			m_spPlayer && !m_spPlayer->IsExpired() && !m_spPlayer->IsDead() && !m_deathActive)
		{
			if (DeadZoneManager::Instance().IsInDeadZone(m_spPlayer->GetPos()))
			{
				m_spPlayer->InstantDeath();   // HP0 → この後の死亡シーケンスが起動
			}
		}

		// ── 奈落落下の保険：着地できず落ちすぎたら静かにスポーンへ戻す（死亡なし）──
		if (m_spPlayer && !m_spPlayer->IsExpired() &&
			m_spPlayer->GetPos().y < m_spawnPos.y - SpawnConst::FallResetBelow)
		{
			m_spPlayer->SetPos(m_spawnPos);
			m_spPlayer->SetVelocity(Math::Vector3::Zero);
			m_spPlayer->SetGravityScale(1.0f);
			m_airTime = 0.0f;
			// 導入カットシーン中なら終了して操作可能に戻す
			if (m_introCutscene)
			{
				m_introCutscene = false;
				m_spPlayer->SetControlEnabled(true);
				m_spPlayer->SetCutsceneSpin(0.0f);
				m_spPlayer->SetCutsceneTumble(Math::Vector3::Zero);
				m_spPlayer->SetIntroPose(false);
			}
		}

		// ── チェックポイント更新 ─────────────────────────
		for (auto& cp : m_checkpoints)
		{
			if (cp->IsActivated())
			{
				// このチェックポイントを踏んだ → 以降はここに復活
				m_checkpointReached = true;
				m_respawnPos = cp->GetPos();
				for (auto& other : m_checkpoints)
				{
					if (other != cp) { other->Deactivate(); }
				}
			}
		}

		// ── 敵撃破：フラッシュ＆カメラシェイク ──────────────
		for (auto& e : m_enemies)
		{
			if (e && e->IsDead() && !e->IsExpired())
			{
				TriggerFlash(JuiceConst::FlashHitAlpha);
				if (m_pCamera) { m_pCamera->TriggerShake(JuiceConst::ShakeDeathStr); }
			}
		}

		// ── プレイヤー被ダメ：HP シェイク＆カメラシェイク ────
		if (m_spPlayer)
		{
			const int curHp = m_spPlayer->GetHp();
			if (m_prevPlayerHp > 0 && curHp < m_prevPlayerHp)
			{
				m_hpShakeTimer = UIConst::HpShakeTime;   // 星HPを揺らす
				if (m_pCamera) { m_pCamera->TriggerShake(JuiceConst::ShakeHitStr); }
			}
			m_prevPlayerHp = curHp;
		}

		// ── FootDust（足元煙パーティクル）──
		{
			const float kDt = KdFPSController::GetDt();
			m_dustTimer -= kDt;

			const bool  isGround = m_spPlayer->IsGround();
			const Math::Vector3 vel   = m_spPlayer->GetVelocity();
			const float         speed = Math::Vector3(vel.x, vel.y, 0.0f).Length();

			// 地上かつ一定速度以上のときだけ FootDust を生成
			if (isGround && speed >= JuiceConst::DustSpeedMin && m_dustTimer <= 0.0f)
			{
				const bool isDash = m_spPlayer->IsDashing();
				m_dustTimer = isDash ? JuiceConst::DustIntervalDash : JuiceConst::DustIntervalWalk;

				// 進行方向の逆＝後方ベクトル
				Math::Vector3 backDir = -vel;
				backDir.z = 0.0f;
				if (backDir.LengthSquared() > 1e-6f) { backDir.Normalize(); }
				else                                  { backDir = Math::Vector3{ 1.0f, 0.0f, 0.0f }; }

				const Math::Vector3 upDir = m_spPlayer->GetUpDir();
				const auto dustData = ModelManager::Instance().GetModel(PlayerConst::DustPath);

				const int spawnCount = isDash ? 5 : 2;
				for (int i = 0; i < spawnCount; ++i)
				{
					auto dust = std::make_shared<FootDust>();
					dust->Spawn(m_spPlayer->GetPos(), backDir, upDir, dustData);
					AddObject(dust);
				}
			}

			// ── 着地エッジ検出 → StarBurstEffect ──────────
			const bool justLanded = isGround && !m_prevPlayerGround;
			if (justLanded)
			{
				const auto dustData = ModelManager::Instance().GetModel(PlayerConst::DustPath);
				auto burst = std::make_shared<StarBurstEffect>();
				burst->Spawn(m_spPlayer->GetPos(), m_spPlayer->GetUpDir(), dustData);
				AddObject(burst);
			}
			m_prevPlayerGround = isGround;
		}

		// ── アイテム更新・取得判定 ──────────────────────
		bool parasolPickedUp = false;
		int  rocksPicked = 0;
		const int gotCoins = m_itemManager.Update(m_spPlayer->GetPickupHitBox(), parasolPickedUp, rocksPicked);
		if (gotCoins > 0)
		{
			m_coinTotal   += gotCoins;
			m_coinPopTimer = UIConst::CoinPopTime;   // 取得でカウンターをポップ
		}
		if (rocksPicked > 0)
		{
			// 緑石＝回復：HP加算＋プレイヤー緑発光（Heal内）。＋マークをプレイヤーとHP UIに出す
			m_spPlayer->Heal(rocksPicked);
			SpawnHealPlus(rocksPicked);
		}
		if (parasolPickedUp)
		{
			m_spPlayer->GiveParasol();
			// ユニーク取得時のみ：メインバーストが終わるまで停止（その後に余韻が流れる）
			TriggerHitStop(SparkleConst::PickupBurstLife);
			// プレイヤー本体を加算ブルームで光らせる（アイテム色）
			m_spPlayer->TriggerPickupGlow(Math::Color{
				SparkleConst::ParasolColorR, SparkleConst::ParasolColorG,
				SparkleConst::ParasolColorB, 1.0f });
		}
		m_itemManager.Refresh();

		// ── Glow コア取得 → 即ステージクリア（マリギャラ風）──
		{
			const KdCollider::SphereInfo coreHit = m_spPlayer->GetPickupHitBox().GetSphereInfo();
			for (auto& gc : m_gravityCores)
			{
				if (!gc || gc->IsExpired() || !gc->IsGlow()) { continue; }
				if (gc->Intersects(coreHit, nullptr))
				{
					const Math::Vector3 corePos = gc->GetPos();
					gc->Expire();

					// 取得演出：プレイヤーを光らせる → クリアシーケンス開始
					m_spPlayer->TriggerPickupGlow(Math::Color{
						GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
						GravityCoreConst::GlowFaceB, 1.0f });
					StartStageClear(corePos);
				}
			}
		}

		// ── Cubun 踏みつけ・棘ダメージ判定 ────────────────
		// 頭側(+m_upDir)＝安全面：上から踏むと 1 回で撃破＋プレイヤー跳ね返り
		// 足側(-m_upDir)＝棘面　：触れるとプレイヤー被ダメージ
		if (m_spPlayer && !m_spPlayer->IsExpired() && !m_spPlayer->IsDead())
		{
			const Math::Vector3 ppos = m_spPlayer->GetPos();
			const Math::Vector3 pvel = m_spPlayer->GetVelocity();
			for (auto& sp : m_cubuns)
			{
				if (!sp || sp->IsDead()) { continue; }

				if (sp->CheckStomp(ppos, pvel) && !sp->IsSquashing())
				{
					// ぺちゃんこ演出開始（即死ではなく 0.35 秒後に消える）
					sp->StartStomp();

					// プレイヤーは演出を待たず即座に跳ね返す
					const Math::Vector3 up = sp->GetPhysicsUpDir();
					Math::Vector3 v = pvel;
					v -= up * v.Dot(up);                    // up 方向成分を除去
					v += up * CubunConst::StompBounceVel;   // 跳ね
					m_spPlayer->SetVelocity(v);
				}
				else if (!sp->IsSquashing() && sp->IsSpikeHit(ppos))
				{
					if (sp->IsSpikeFacingUp())
					{
						// 重力反転：棘がワールド上を向く → 棘ダメージ（ノックバック付き）
						m_spPlayer->TakeDamageFrom(CubunConst::SpikeDamage, sp->GetPos());
					}
					else
					{
						// 通常重力：叩きつけ(COL_Attack)はスラム中＝空中のみダメージ（座っている Cubun の横では当たらない）
						if (!sp->IsGround()) { m_spPlayer->TakeDamageFrom(CubunConst::CrushDamage, sp->GetPos()); }
					}
				}
			}
		}

		// ── SpikeBox 棘ダメージ判定 ──────────────────────
		if (m_spPlayer && !m_spPlayer->IsExpired() && !m_spPlayer->IsDead())
		{
			const Math::Vector3 ppos = m_spPlayer->GetPos()
				+ m_spPlayer->GetUpDir() * SpikeBoxConst::HitRadius;
			for (auto& sb : m_spikeBoxes)
			{
				if (!sb || !sb->IsEnabled()) { continue; }
				if (sb->IsSpikeHit(ppos, SpikeBoxConst::HitRadius))
				{
					m_spPlayer->TakeDamageFrom(SpikeBoxConst::SpikeDamage, sb->GetCenter());
				}
			}
		}

		// ── Cubun 撃破バーストエフェクト ────────────────
		{
			const auto dustData = ModelManager::Instance().GetModel(PlayerConst::DustPath);
			for (auto& sp : m_cubuns)
			{
				if (sp->m_requestBurstEffect)
				{
					sp->m_requestBurstEffect = false;
					auto burst = std::make_shared<StarBurstEffect>();
					burst->Spawn(sp->GetPos(), sp->GetUpDir(), dustData);
					AddObject(burst);
				}
				// 撃破時の岩石ドロップ（6〜10個ばら撒く）
				if (sp->m_requestRockDrop)
				{
					sp->m_requestRockDrop = false;
					m_itemManager.SpawnRockBurst(sp->GetPos(), sp->GetUpDir());
				}
			}
		}

		// ── StarBurst 手動テスト（ImGui Viewer から） ──
		if (m_starBurstTestRequest)
		{
			m_starBurstTestRequest = false;
			const auto dustData = ModelManager::Instance().GetModel(PlayerConst::DustPath);
			const Math::Vector3 pos = m_spPlayer ? m_spPlayer->GetPos() : Math::Vector3::Zero;
			const Math::Vector3 up  = m_spPlayer ? m_spPlayer->GetUpDir() : Math::Vector3::Up;
			auto burst = std::make_shared<StarBurstEffect>();
			burst->Spawn(pos, up, dustData);
			AddObject(burst);
		}
	}
}

void GameScene::DrawSpriteExtra()
{
	const auto& bb    = KdDirect3D::Instance().GetBackBuffer();
	const int screenW = static_cast<int>(bb->GetInfo().Width);
	const int screenH = static_cast<int>(bb->GetInfo().Height);

	// ── コイン（収集物）カウンター：右上にアイコン＋数字（取得時にポップ）──
	// ※ 死亡暗転・ポーズ暗幕より先に描いて、それらが上に乗るようにする
	if (!m_editorMode && !m_menuOpen && m_coinIconTex)
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;

		// 取得ポップ（取得直後に一瞬拡大）
		if (m_coinPopTimer > 0.0f) { m_coinPopTimer -= KdFPSController::GetDt(); }
		const float popK  = (m_coinPopTimer > 0.0f) ? (m_coinPopTimer / UIConst::CoinPopTime) : 0.0f;
		const float scale = 1.0f + UIConst::CoinPopScale * popK;

		const int   baseSz = UIConst::CoinIconSize;
		const int   sz      = static_cast<int>(baseSz * scale);
		// 右上にアイコン中心を配置（中心原点・+Xが右／+Yが上）
		const int   iconCX = static_cast<int>(screenW * 0.5f - UIConst::CoinMargin - baseSz * 0.5f);
		const int   iconCY = static_cast<int>(screenH * 0.5f - UIConst::CoinMargin - baseSz * 0.5f);
		sprite.DrawTex(m_coinIconTex.get(), iconCX, iconCY, sz, sz, nullptr, nullptr);

		// アイコンの左に「× N」を右詰めで描画
		char buf[32] = {};
		sprintf_s(buf, "× %d", m_coinTotal);
		auto measure = KdFontManager::Instance().CreateFontTexture(PauseMenuConst::FontNo, buf, false);
		float textW = 0.0f, textH = 0.0f;
		if (measure)
		{
			for (const auto& d : measure->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				textW += static_cast<float>(d->FontTex->GetInfo().Width);
				textH  = (std::max)(textH, static_cast<float>(d->FontTex->GetInfo().Height));
			}
		}
		const float textRight = iconCX - baseSz * 0.5f - UIConst::CoinTextGap; // アイコン左端から少し左
		const Math::Vector2 textPos(textRight - textW, iconCY - textH * 0.5f);
		const Math::Color textCol(1.0f, 1.0f, 1.0f, 1.0f);
		sprite.DrawFont(textPos, &textCol, "%s", buf);
	}

	// ── HP：宝石ハート。左上に表示・被ダメで揺れる ──
	if (!m_editorMode && !m_menuOpen && m_spPlayer)
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;

		const int hp  = std::clamp(m_spPlayer->GetHp(), 0, PlayerConst::MaxHp);
		const float r = (PlayerConst::MaxHp > 0) ? static_cast<float>(hp) / PlayerConst::MaxHp : 0.0f;

		// HP連動カラー：緑(満タン)→赤(残り少)。緑を早めに落としてオレンジ帯を狭め赤寄りに
		const float hpR = std::clamp(2.0f * (1.0f - r),  0.0f, 1.0f);
		const float hpG = std::clamp(2.5f * r - 0.8f,    0.0f, 1.0f);
		const float hpB = 0.06f;

		// 被ダメ揺れ
		if (m_hpShakeTimer > 0.0f) { m_hpShakeTimer -= KdFPSController::GetDt(); }
		int ox = 0, oy = 0;
		if (m_hpShakeTimer > 0.0f)
		{
			const float k = m_hpShakeTimer / UIConst::HpShakeTime;
			const float s = UIConst::HpShakeAmp * k;
			ox = static_cast<int>(sinf(m_hpShakeTimer * 60.0f) * s);
			oy = static_cast<int>(cosf(m_hpShakeTimer * 78.0f) * s);
		}

		const int S  = UIConst::HpStarSize;
		const int cx = static_cast<int>(-screenW * 0.5f + UIConst::HpMarginX + S * 0.5f) + ox;
		const int cy = static_cast<int>( screenH * 0.5f - UIConst::HpMarginY - S * 0.5f) + oy;

		// ── 重力コア(Glow型)を本物と同じ頂点ロジックで生成→固定回転で投影して2D再現 ──
		const float R   = S * 0.5f;

		// 本物 DrawGlowEffect と同じ波アニメ時間を進める
		m_hpCoreAnim += KdFPSController::GetDt();

		// 死亡(HP0)時はハート本体を一切描かない（パーティクルだけ後段で残す）
		if (hp > 0)
		{
		// ラフな結晶カット風：リング(緯度)は粗く、ハート輪郭(経度)は細かく分けて形を出す
		const int lats = GravityCoreConst::LatitudeSegs;           // 7
		const int lons = GravityCoreConst::LongitudeSegs * 2 + 4;  // 20
		constexpr float kPi = 3.14159265359f, kTau = 6.28318530718f;

		// ハートは正面寄りに見せる（わずかに3/4で立体感）
		const float cY = cosf(0.30f),  sY = sinf(0.30f);
		const float cX = cosf(-0.14f), sX = sinf(-0.14f);

		// HPが減るほどコアが「へこむ」。被ダメ箇所の半径を内側へ縮めて陥没を作る。
		// （面は消さず、形を保ったまま凹ませる＝ダメージを受けた塊が潰れる感じ）
		const float missing = std::clamp(1.0f - r, 0.0f, 1.0f);

		// へこみ量[0,1]：低周波の塊(lump)ごとにダメージが閾値を超えたら陥没。
		// th の整数倍波を使い、継ぎ目(lo=0とlo=lons)で連続させる。頂点・面色で共用。
		auto dentAt = [&](float la, float lo) -> float
		{
			const float th = kTau * lo / lons;
			const float lump = 0.5f + 0.5f * sinf(2.0f * th + la * 0.9f) * cosf(la * 1.3f);
			return std::clamp((missing - lump * 0.9f) * 2.0f, 0.0f, 1.0f);
		};

		// 頂点グリッド (lats+1)×(lons+1) を生成。各リングを「ハート輪郭の縮小版」にして
		// 前後(z)へ膨らませた立体ハート。波変形→へこみ→回転→投影。
		struct PV { float x, y, z; };  // x:右+ y:上+ z:奥行(手前+)
		std::vector<PV> proj(static_cast<size_t>((lats + 1) * (lons + 1)));
		auto vidx = [&](int la, int lo) { return la * (lons + 1) + lo; };
		constexpr float kHeartK = 1.0f / 17.0f;   // ハート曲線の正規化係数
		// 結晶の凸凹：頂点ごとの固定ジッター（継ぎ目で割れないよう lo は周回）
		auto vJit = [&](int la, int lo) -> float
		{
			lo = ((lo % lons) + lons) % lons;
			unsigned int h = static_cast<unsigned int>(la * 92837111) ^ static_cast<unsigned int>((lo + 1) * 689287499);
			h ^= h >> 13; h *= 0x85EBCA6Bu; h ^= h >> 16;
			return static_cast<float>(h & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;   // [-1,1]
		};
		// 全体が一様に脈打つ「鼓動」（per-vertex のさざ波はローブが歪んで気持ち悪いので廃止）
		const float beat = 1.0f + sinf(m_hpCoreAnim * 2.2f) * 0.04f;
		for (int la = 0; la <= lats; ++la)
		{
			const float phi = kPi * la / lats;
			const float ring = sinf(phi);          // 赤道(=ハート輪郭)で1、極で0
			const float zb   = cosf(phi) * R * 0.55f;  // 前後の膨らみ
			for (int lo = 0; lo <= lons; ++lo)
			{
				const float th = kTau * lo / lons;
				const float dent = dentAt(static_cast<float>(la), static_cast<float>(lo));
				// 結晶ジッターは極で抑える(ring倍)＝トポロジ破綻防止
				const float jit = 1.0f + vJit(la, lo) * 0.20f * ring;
				const float f = beat * (1.0f - dent * 0.55f) * jit;

				// 2Dハート曲線（th で一周）
				const float s1 = sinf(th);
				const float hx = (16.0f * s1 * s1 * s1) * kHeartK;
				const float hy = (13.0f * cosf(th) - 5.0f * cosf(2.0f * th)
								- 2.0f * cosf(3.0f * th) - cosf(4.0f * th)) * kHeartK + 0.18f;

				const float px = hx * R * ring * f;
				const float py = hy * R * ring * f;
				const float pz = zb * f;
				const float x1 = px * cY + pz * sY;
				const float z1 = -px * sY + pz * cY;
				const float y2 = py * cX - z1 * sX;
				const float z2 = py * sX + z1 * cX;
				proj[vidx(la, lo)] = { x1, y2, z2 };
			}
		}

		// 面のフラット色：HP連動色(緑→黄→赤)を基準に、緯度＆面ごとの明暗だけ乗算（色相は保つ）
		auto faceColor = [&](int la, int lo, float mul) -> Math::Color
		{
			const float t = static_cast<float>(la) / (lats - 1);
			const float bright = 1.0f - t * 0.5f;
			const float ph = kTau * lo / lons + la * 0.8f;
			const float var = 0.78f + 0.22f * sinf(ph);   // 面ごとの明暗バリエーション
			const float m = mul * bright * var;
			return Math::Color(std::min(hpR * m, 1.0f), std::min(hpG * m, 1.0f), std::min(hpB * m + 0.02f, 1.0f), GravityCoreConst::GlowFaceAlpha);
		};

		// シルエットの最下点（底ハイライト位置用）
		float sMinY = 1e9f;
		for (const PV& p : proj) { sMinY = std::min(sMinY, p.y); }

		// 1) ハート形のブルーム（背景の丸いグロー画像はやめ、ハート自体を光らせる）。
		//    ハートのシルエットを拡大しながら低アルファで重ねて発光のにじみを作る。
		{
			constexpr int NB = 28;
			auto heartPt = [&](float th, float scale) -> Math::Vector2
			{
				const float s1 = sinf(th);
				const float hx = (16.0f * s1 * s1 * s1) * kHeartK;
				const float hy = (13.0f * cosf(th) - 5.0f * cosf(2.0f * th)
								- 2.0f * cosf(3.0f * th) - cosf(4.0f * th)) * kHeartK + 0.18f;
				const float px = hx * R * scale, py = hy * R * scale;   // z=0(赤道)
				const float x1 = px * cY;
				const float z1 = -px * sY;
				const float y2 = py * cX - z1 * sX;
				return { x1, y2 };
			};
			auto bloomLayer = [&](float scale, float a)
			{
				const Math::Color bc(std::min(hpR + 0.15f, 1.0f), std::min(hpG + 0.15f, 1.0f), hpB + 0.10f, a);
				Math::Vector2 prev = heartPt(0.0f, scale);
				for (int k = 1; k <= NB; ++k)
				{
					const Math::Vector2 cur = heartPt(kTau * k / NB, scale);
					sprite.DrawTriangle(cx, cy,
						static_cast<int>(cx + prev.x), static_cast<int>(cy + prev.y),
						static_cast<int>(cx + cur.x),  static_cast<int>(cy + cur.y), &bc, true);
					prev = cur;
				}
			};
			// 多層をガウス減衰で重ねて滑らかなにじみ（ぼかし発光）にする。
			// 加算合成で光が重なって本物のブルームらしく明るくにじむ。
			KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);
			const float gi = 0.05f + 0.08f * r;   // HPが多いほど強い（かなり控えめ）
			constexpr int LAYERS = 22;
			for (int li = LAYERS - 1; li >= 0; --li)   // 外(薄)→内(濃)の順で重ねる
			{
				const float u = static_cast<float>(li) / (LAYERS - 1);   // 0(内)..1(外)
				const float scale = 1.0f + u * 1.05f;                    // 1.0 .. 2.05
				const float a = gi * expf(-3.0f * u * u) * 0.07f;        // ガウス状の減衰
				bloomLayer(scale, a);
			}
			KdShaderManager::Instance().UndoBlendState();
		}

		// 三角形を実ポリゴン(DrawTriangle)で塗る
		auto fillTri = [&](const PV& a, const PV& b, const PV& c, const Math::Color& col)
		{
			sprite.DrawTriangle(
				static_cast<int>(cx + a.x), static_cast<int>(cy + a.y),
				static_cast<int>(cx + b.x), static_cast<int>(cy + b.y),
				static_cast<int>(cx + c.x), static_cast<int>(cy + c.y),
				&col, true);
		};

		// 面の法線でフラットシェーディング＋鏡面ハイライト（宝石のギラつき）
		struct Shade { float d, s; };   // d=拡散, s=鏡面グリント
		auto triShade = [&](const PV& a, const PV& b, const PV& c) -> Shade
		{
			const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
			const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
			float nx = uy * vz - uz * vy;
			float ny = uz * vx - ux * vz;
			float nz = ux * vy - uy * vx;
			const float len = sqrtf(nx * nx + ny * ny + nz * nz);
			if (len < 1e-5f) { return { 0.7f, 0.0f }; }
			nx /= len; ny /= len; nz /= len;
			if (nz < 0.0f) { nx = -nx; ny = -ny; nz = -nz; }   // 法線を手前向きに
			// 左上手前からのライト
			const float d = std::clamp(nx * (-0.40f) + ny * 0.50f + nz * 0.77f, 0.0f, 1.0f);
			const float spec = powf(d, 14.0f);                  // 鋭いグリント＝宝石の輝き
			return { 0.30f + 0.70f * d, spec };
		};

		// 2) 面リストを奥行きソート。shade(拡散・鏡面)を事前計算
		struct Tri { int a, b, c, la, lo; float depth; Shade sh; };
		std::vector<Tri> tris; tris.reserve(static_cast<size_t>(lats) * lons * 2);
		for (int la = 0; la < lats; ++la)
		{
			for (int lo = 0; lo < lons; ++lo)
			{
				const int i00 = vidx(la, lo),     i01 = vidx(la, lo + 1);
				const int i10 = vidx(la + 1, lo), i11 = vidx(la + 1, lo + 1);
				tris.push_back({ i00, i01, i10, la, lo, (proj[i00].z + proj[i01].z + proj[i10].z) / 3.0f, triShade(proj[i00], proj[i01], proj[i10]) });
				tris.push_back({ i01, i11, i10, la, lo, (proj[i01].z + proj[i11].z + proj[i10].z) / 3.0f, triShade(proj[i01], proj[i11], proj[i10]) });
			}
		}
		std::sort(tris.begin(), tris.end(), [](const Tri& p, const Tri& q) { return p.depth < q.depth; });

		// 面を塗る：HP色×拡散×へこみ ＋ 白い鏡面グリントを加算（宝石のキラッ）
		for (const Tri& t : tris)
		{
			const float cellDent = dentAt(t.la + 0.5f, t.lo + 0.5f);
			Math::Color col = faceColor(t.la, t.lo, t.sh.d * (1.0f - cellDent * 0.55f));
			const float g = t.sh.s * 0.95f * (1.0f - cellDent);   // 凹み面は光らない
			col.x = std::min(col.x + g, 1.0f);
			col.y = std::min(col.y + g, 1.0f);
			col.z = std::min(col.z + g, 1.0f);
			fillTri(proj[t.a], proj[t.b], proj[t.c], col);
		}

		// 3) ワイヤー（緯線＋経線）：暗めを常時メイン。手前側のみ描く。
		{
			auto wireCol = [&](float la) -> Math::Color
			{
				const float t = la / lats;
				const float b = (1.0f - t * 0.5f) * 0.55f;   // 控えめのエッジ
				// HP色に白をほんの少しだけ混ぜる（明るすぎないように）
				return Math::Color(
					std::min(hpR * b + 0.10f, 1.0f),
					std::min(hpG * b + 0.10f, 1.0f),
					std::min(hpB * b + 0.10f, 1.0f),
					GravityCoreConst::GlowWireAlpha * 0.6f);
			};
			auto edge = [&](int ia, int ib, float la)
			{
				const float zmid = (proj[ia].z + proj[ib].z) * 0.5f;
				if (zmid < 0.0f) return;                                       // 裏面はスキップ
				const float fade = std::clamp(zmid / (R * 0.2f), 0.0f, 1.0f);  // リム境界だけ柔らかく
				Math::Color c = wireCol(la);
				c.w *= fade;
				sprite.DrawLine(
					static_cast<int>(cx + proj[ia].x), static_cast<int>(cy + proj[ia].y),
					static_cast<int>(cx + proj[ib].x), static_cast<int>(cy + proj[ib].y), &c);
			};
			for (int rl = 0; rl <= lats; ++rl)            // 緯線
				for (int lo = 0; lo < lons; ++lo) edge(vidx(rl, lo), vidx(rl, lo + 1), static_cast<float>(rl));
			for (int lo = 0; lo <= lons; ++lo)            // 経線
				for (int la = 0; la < lats; ++la) edge(vidx(la, lo), vidx(la + 1, lo), la + 0.5f);
		}

		// 4) 底の強いハイライト（白い発光点。HPで弱まる）
		if (m_hpStarTex)
		{
			const Math::Color hot(0.85f, 0.95f, 1.0f, 0.9f * (0.35f + 0.65f * r));
			const int hs = static_cast<int>(S * 0.5f);
			sprite.DrawTex(m_hpStarTex.get(), cx, cy + static_cast<int>(sMinY * 0.75f), hs, hs, nullptr, &hot);
		}

		// 4.5) 宝石のきらめき（4方向の星。ゆっくり明滅）
		{
			const float tw = 0.5f + 0.5f * sinf(m_hpCoreAnim * 3.0f);   // 明滅
			const float sx = cx - R * 0.30f;
			const float sy = cy + R * 0.42f;
			const float L  = R * (0.18f + 0.16f * tw);
			const float l  = L * 0.32f;
			Math::Color sc(1.0f, 1.0f, 1.0f, 0.45f + 0.45f * tw);
			sprite.DrawLine(static_cast<int>(sx - L), static_cast<int>(sy), static_cast<int>(sx + L), static_cast<int>(sy), &sc);
			sprite.DrawLine(static_cast<int>(sx), static_cast<int>(sy - L), static_cast<int>(sx), static_cast<int>(sy + L), &sc);
			sprite.DrawLine(static_cast<int>(sx - l), static_cast<int>(sy - l), static_cast<int>(sx + l), static_cast<int>(sy + l), &sc);
			sprite.DrawLine(static_cast<int>(sx - l), static_cast<int>(sy + l), static_cast<int>(sx + l), static_cast<int>(sy - l), &sc);
		}

		}   // ← if (hp > 0)：ハート本体の描画ここまで（死亡時は描かない）

		// ── パーティクル：被ダメで弾け、死亡(HP0)時はぶち壊れた大量破片 ──
		constexpr float kTau = 6.28318530718f;
		static unsigned int s_pSeed = 0x1234567u;
		auto frand = [&]() -> float { s_pSeed = s_pSeed * 1664525u + 1013904223u; return static_cast<float>((s_pSeed >> 8) & 0xFFFFu) / 65535.0f; };
		if (m_hpPrevHp >= 0 && hp < m_hpPrevHp)
		{
			const bool dead = (hp <= 0);                 // 死亡＝大破（大量・大きめ・赤い破片）
			const int  cnt  = dead ? 48 : 16;
			for (int i = 0; i < cnt; ++i)
			{
				const float ang = kTau * frand();
				const float spd = R * (dead ? (4.0f + 6.0f * frand()) : (2.0f + 3.0f * frand()));
				const float startR = R * (dead ? 0.25f : 0.55f);
				HpParticle p;
				p.pos = { cosf(ang) * startR, sinf(ang) * startR };
				p.vel = { cosf(ang) * spd, sinf(ang) * spd };
				p.maxLife = dead ? (0.6f + 0.7f * frand()) : (0.35f + 0.35f * frand());
				p.life    = p.maxLife;
				p.size    = R * (dead ? (0.16f + 0.24f * frand()) : (0.12f + 0.12f * frand()));
				p.rot     = ang;
				p.col = dead
					? Math::Color(1.0f, 0.18f + 0.15f * frand(), 0.12f, 1.0f)   // 赤い破片
					: Math::Color(std::min(hpR + 0.25f, 1.0f), std::min(hpG + 0.25f, 1.0f), hpB + 0.15f, 1.0f);
				m_hpParticles.push_back(p);
			}
		}
		m_hpPrevHp = hp;

		// パーティクル更新＆DrawTriangleで描画
		{
			const float dt = KdFPSController::GetDt();
			for (HpParticle& p : m_hpParticles)
			{
				p.life -= dt;
				p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt;
				p.vel.x *= 0.90f; p.vel.y *= 0.90f;
				p.vel.y -= 360.0f * dt;     // 重力で破片が落ちる（+Yが上）
				p.rot += dt * 6.0f;
			}
			m_hpParticles.erase(
				std::remove_if(m_hpParticles.begin(), m_hpParticles.end(),
					[](const HpParticle& p) { return p.life <= 0.0f; }),
				m_hpParticles.end());

			for (const HpParticle& p : m_hpParticles)
			{
				const float tl = std::clamp(p.life / p.maxLife, 0.0f, 1.0f);
				const float sz = p.size * (0.4f + 0.6f * tl);
				Math::Color c = p.col; c.w = tl;
				const float bx = cx + p.pos.x, by = cy + p.pos.y;
				const float a0 = p.rot, a1 = p.rot + kTau / 3.0f, a2 = p.rot + 2.0f * kTau / 3.0f;
				sprite.DrawTriangle(
					static_cast<int>(bx + cosf(a0) * sz), static_cast<int>(by + sinf(a0) * sz),
					static_cast<int>(bx + cosf(a1) * sz), static_cast<int>(by + sinf(a1) * sz),
					static_cast<int>(bx + cosf(a2) * sz), static_cast<int>(by + sinf(a2) * sz),
					&c, true);
			}
		}
	}

	// ── 回復「＋」マーク：上昇しながらフェード（プレイヤー位置・HP UI位置） ──
	if (!m_editorMode && !m_menuOpen && !m_healPluses.empty())
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;
		const float dt = KdFPSController::GetDt();
		for (HealPlus& hp : m_healPluses)
		{
			hp.life -= dt;
			hp.pos.y += HealConst::PlusRiseSpeed * dt;   // 上昇(+Yが上)
		}
		m_healPluses.erase(
			std::remove_if(m_healPluses.begin(), m_healPluses.end(),
				[](const HealPlus& h) { return h.life <= 0.0f; }),
			m_healPluses.end());

		for (const HealPlus& h : m_healPluses)
		{
			// 薄め（フェードなし。寿命が尽きたら消えるだけ）
			const Math::Color col(HealConst::PlusColorR, HealConst::PlusColorG, HealConst::PlusColorB, HealConst::PlusAlpha);
			const int x = static_cast<int>(h.pos.x);
			const int y = static_cast<int>(h.pos.y);
			const int L = static_cast<int>(h.size);
			const int T = static_cast<int>(HealConst::PlusThickness);
			sprite.DrawBox(x, y, L, T, &col, true);   // 横棒
			sprite.DrawBox(x, y, T, L, &col, true);   // 縦棒
		}
	}

	// ── 重力コンパス（左下）：重力の「下」方向＋切替可否 ──
	if (!m_editorMode && !m_menuOpen && m_spPlayer)
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;

		// 重力の下方向をビュー空間へ → 画面2D方向に投影
		const Math::Vector3 down = -m_spPlayer->GetUpDir();
		const Math::Matrix  view = KdShaderManager::Instance().GetCameraCB().mView;
		Math::Vector3 dv = Math::Vector3::TransformNormal(down, view);
		Math::Vector2 dir(dv.x, dv.y);
		if (dir.Length() > 1e-4f) { dir.Normalize(); } else { dir = { 0.0f, -1.0f }; }

		const float R  = UIConst::GravCompassRadius;
		const int   cx = static_cast<int>(-screenW * 0.5f + UIConst::GravCompassMarginX + R);
		const int   cy = static_cast<int>(-screenH * 0.5f + UIConst::GravCompassMarginY + R);

		// ダイヤルの目盛り（薄い点）
		const Math::Color dotCol(0.7f, 0.75f, 0.85f, 0.30f);
		for (int k = 0; k < UIConst::GravCompassDots; ++k)
		{
			const float a = 6.2831853f * k / UIConst::GravCompassDots;
			const int dx = cx + static_cast<int>(cosf(a) * R);
			const int dy = cy + static_cast<int>(sinf(a) * R);
			sprite.DrawBox(dx, dy, UIConst::GravCompassDotSize, UIConst::GravCompassDotSize, &dotCol, true);
		}

		// 中心ドット：重力切替が使える場所ならシアン、使えなければ灰
		const bool canSwitch = ManualGravityZoneManager::Instance().CanUseManualGravity(m_spPlayer->GetPos());
		const Math::Color centerCol = canSwitch
			? Math::Color(0.4f, 0.9f, 1.0f, 0.95f)
			: Math::Color(0.5f, 0.5f, 0.55f, 0.8f);
		sprite.DrawBox(cx, cy, UIConst::GravCompassCenter, UIConst::GravCompassCenter, &centerCol, true);

		// 方向マーカー（星）：重力の下方向に配置
		if (m_lifeStarTex)
		{
			const int mx = cx + static_cast<int>(dir.x * R);
			const int my = cy + static_cast<int>(dir.y * R);
			const int ms = UIConst::GravCompassMark;
			const Math::Color markCol = canSwitch
				? Math::Color(1.0f, 0.95f, 0.5f, 1.0f)
				: Math::Color(1.0f, 1.0f, 1.0f, 0.9f);
			sprite.DrawTex(m_lifeStarTex.get(), mx, my, ms, ms, nullptr, &markCol);
		}
	}

	// ── テレポート黒フェードオーバーレイ ──
	if (m_teleportFadeAlpha > 0.0f)
	{
		const Math::Color blackOverlay(0.0f, 0.0f, 0.0f, m_teleportFadeAlpha);
		KdShaderManager::Instance().m_spriteShader.DrawBox(
			screenW / 2, screenH / 2,
			screenW, screenH,
			&blackOverlay, true);
	}

	// ── 画面白フラッシュ ──
	if (m_flashAlpha > 0.0f)
	{
		const float kDt = KdFPSController::GetDt();
		const Math::Color whiteOverlay(1.0f, 1.0f, 1.0f, m_flashAlpha);
		KdShaderManager::Instance().m_spriteShader.DrawBox(
			screenW / 2, screenH / 2,
			screenW, screenH,
			&whiteOverlay, true);
		m_flashAlpha -= JuiceConst::FlashDecay * kDt;
		if (m_flashAlpha < 0.0f) { m_flashAlpha = 0.0f; }
	}
	// ── 被ダメ赤フラッシュ ──
	KdShaderManager::Instance().m_postProcessShader.DrawDamageFlash();

	// ── シーン開始フェードイン（白→透明。タイトルの白フラッシュから繋ぐ）──
	if (m_introFadeAlpha > 0.0f)
	{
		const float kDt = KdFPSController::GetDt();
		const Math::Color introOverlay(1.0f, 1.0f, 1.0f, m_introFadeAlpha);
		KdShaderManager::Instance().m_spriteShader.DrawBox(
			screenW / 2, screenH / 2,
			screenW, screenH,
			&introOverlay, true);
		m_introFadeAlpha -= JuiceConst::IntroFadeSpeed * kDt;
		if (m_introFadeAlpha < 0.0f) { m_introFadeAlpha = 0.0f; }
	}

	// ── ステージクリア：カメラの引きと並行してアイリス暗転（マリオ風の閉じる円）→ StageSelect へ ──
	if (m_clearActive)
	{
		float a = 0.0f;
		if (m_clearTimer > ClearConst::FadeBeginT)
		{
			a = std::clamp((m_clearTimer - ClearConst::FadeBeginT)
				/ (ClearConst::FadeEndT - ClearConst::FadeBeginT), 0.0f, 1.0f);
		}
		if (a > 0.0f)
		{
			auto& sprite = KdShaderManager::Instance().m_spriteShader;
			const Math::Color black(0.0f, 0.0f, 0.0f, 1.0f);

			if (m_irisMaskTex)
			{
				// 円が閉じていく：マスク四角のサイズを 開→0 へ（easeIn で最後にキュッと閉じる）
				const float ec = a * a;                                  // easeInQuad
				const float openSz = ClearConst::IrisOpenScale * static_cast<float>(screenW);
				const float s  = std::lerp(openSz, ClearConst::IrisCloseSize, ec);
				const int   si = static_cast<int>(s);

				// 中央にアイリスマスク（中心が透明な穴・外周が黒）。線形補間で穴の縁を滑らかに
				sprite.Begin(true);
				sprite.DrawTex(m_irisMaskTex.get(), 0, 0, si, si, nullptr, &black);

				// マスク四角の外側を黒帯で埋める（縮小しても画面端が透けないように）。
				// 黒帯はマスク四角の縁に seam ぶん食い込ませて、丸め誤差の隙間（白線）を消す。
				const float halfW = screenW * 0.5f;
				const float halfH = screenH * 0.5f;
				constexpr float seam = 2.0f;                       // 重ね幅(px)
				const float he    = std::max(0.0f, s * 0.5f - seam); // 黒帯の内側境界
				const int   over  = screenW;                       // 画面外まで余裕を持って覆う
				if (he < halfH)
				{
					const int cy = static_cast<int>((he + halfH) * 0.5f);
					const int hy = static_cast<int>((halfH - he) * 0.5f) + 1;
					sprite.DrawBox(0,  cy, screenW + over, hy, &black, true); // 上帯
					sprite.DrawBox(0, -cy, screenW + over, hy, &black, true); // 下帯
				}
				if (he < halfW)
				{
					const int cx = static_cast<int>((he + halfW) * 0.5f);
					const int hx = static_cast<int>((halfW - he) * 0.5f) + 1;
					const int hy = static_cast<int>(he) + 1;
					sprite.DrawBox( cx, 0, hx, hy, &black, true);            // 右帯
					sprite.DrawBox(-cx, 0, hx, hy, &black, true);            // 左帯
				}
				sprite.End();
			}
			else
			{
				// フォールバック：従来のベタ黒フェード
				const Math::Color fade(0.0f, 0.0f, 0.0f, a);
				sprite.DrawBox(0, 0, screenW, screenH, &fade, true);
			}
		}
	}

	// ── 死亡演出オーバーレイ（赤→黒に暗転 → 黒画面で残機が1つ減る → 明転）──
	if (m_deathActive)
	{
		const float kFadeOut = DeathConst::FadeOutTime;
		const float kHold    = DeathConst::HoldTime;
		const float kFadeIn  = DeathConst::FadeInTime;

		// フェーズ判定と暗幕の不透明度
		float overlayA;          // 暗幕アルファ（0→1→1→0）
		float holdT = -1.0f;     // 黒画面ホールド中の経過秒（それ以外は -1）
		if (m_deathTimer < kFadeOut)
		{
			overlayA = m_deathTimer / kFadeOut;                       // 赤み→黒で濃くなる
		}
		else if (m_deathTimer < kFadeOut + kHold)
		{
			overlayA = 1.0f;                                          // 完全な黒（ホールド）
			holdT    = m_deathTimer - kFadeOut;
		}
		else
		{
			overlayA = 1.0f - (m_deathTimer - kFadeOut - kHold) / kFadeIn; // 明転
			overlayA = std::clamp(overlayA, 0.0f, 1.0f);
		}

		// 暗幕（フェードアウト中は赤み、それ以降は黒）
		const float tint = (m_deathTimer < kFadeOut) ? (1.0f - overlayA) : 0.0f;
		Math::Color col(DeathConst::TintR * tint, DeathConst::TintG * tint, DeathConst::TintB * tint, overlayA);
		auto& sprite = KdShaderManager::Instance().m_spriteShader;
		sprite.DrawBox(0, 0, screenW, screenH, &col, true);

		// ── マリギャラ風：黒くなりきってからアイコンを出し、明転前に先に消す ──
		// アイコンは「黒画面ホールド中」だけ表示。出入りは暗幕とは独立にフェードさせる。
		if (m_lifeIconTex && holdT >= 0.0f)
		{
			// アイコン群の表示アルファ：ホールド開始後に出現 → 明転が始まる前に消える
			float panelA;
			if      (holdT < DeathConst::IconAppearTime)            { panelA = holdT / DeathConst::IconAppearTime; }
			else if (holdT > kHold - DeathConst::IconVanishTime)    { panelA = std::clamp((kHold - holdT) / DeathConst::IconVanishTime, 0.0f, 1.0f); }
			else                                                    { panelA = 1.0f; }

			if (panelA > 0.01f)
			{
				const int remain = (std::max)(0, DeathConst::MaxDeathsPerStage - m_deathCount); // 減った後の残機
				const int n0     = remain + 1;                                                  // 減る前（今表示する総数）

				const int   sz = UIConst::LifeIconSize;
				const float sp = UIConst::LifeIconSpacing;
				const int   y  = static_cast<int>(UIConst::LifeIconCenterY);

				// 減るアニメ進行（0→1）：溜め→ポップ→縮小消滅
				const float animP = std::clamp((holdT - DeathConst::LoseAnimDelay) / DeathConst::LoseAnimTime, 0.0f, 1.0f);
				float losePop = 1.0f;
				if (animP < 0.25f) { losePop = 1.0f + (DeathConst::LosePopScale - 1.0f) * (animP / 0.25f); }
				else               { losePop = DeathConst::LosePopScale * (1.0f - (animP - 0.25f) / 0.75f); }
				const float loseFactor = 1.0f - animP; // 減るアイコンの不透明度係数

				// 減る瞬間のUI揺れ（減り始めに最大→減衰）
				int oxi = 0, oyi = 0;
				if (animP > 0.0f && animP < 1.0f)
				{
					const float s = DeathConst::UiShakeAmp * (1.0f - animP);
					oxi = static_cast<int>(sinf(m_deathTimer * DeathConst::UiShakeFreq) * s);
					oyi = static_cast<int>(cosf(m_deathTimer * DeathConst::UiShakeFreq * 1.3f) * s);
				}

				// n0 個ぶんの位置で中央揃え（残るアイコンは位置固定＝ガタつかない）
				for (int i = 0; i < n0; ++i)
				{
					const int x = static_cast<int>((static_cast<float>(i) - (n0 - 1) * 0.5f) * sp) + oxi;
					if (i < remain)
					{
						const Math::Color c(1.0f, 1.0f, 1.0f, panelA);                 // 残るアイコン
						sprite.DrawTex(m_lifeIconTex.get(), x, y + oyi, sz, sz, nullptr, &c);
					}
					else
					{
						const float a = panelA * loseFactor;                           // 減るアイコン
						if (a > 0.01f && losePop > 0.01f)
						{
							const int w = static_cast<int>(sz * losePop);
							const Math::Color c(1.0f, 1.0f, 1.0f, a);
							sprite.DrawTex(m_lifeIconTex.get(), x, y + oyi, w, w, nullptr, &c);
						}
					}
				}

				// ── 星屑バースト：減るアイコンの位置から放射状に散る ──
				if (m_lifeStarTex && animP >= DeathConst::SparkStartP && animP < 1.0f)
				{
					const float bt = std::clamp((animP - DeathConst::SparkStartP) / (1.0f - DeathConst::SparkStartP), 0.0f, 1.0f);
					const int   loseX = static_cast<int>((static_cast<float>(remain) - (n0 - 1) * 0.5f) * sp) + oxi;
					const float a  = (1.0f - bt) * panelA;                         // 広がりながら消える
					const int   ss = static_cast<int>(DeathConst::SparkSize * (1.0f - 0.4f * bt));
					const Math::Color sc(1.0f, 1.0f, 1.0f, a);
					for (int k = 0; k < DeathConst::SparkCount; ++k)
					{
						const float ang = (6.2831853f * k) / DeathConst::SparkCount + animP * 1.5f;
						const float jit = 0.7f + 0.3f * ((k * 37) % 100) / 100.0f; // 粒ごとの距離ばらつき
						const float r   = DeathConst::SparkMaxR * bt * jit;
						const int   px  = loseX + static_cast<int>(cosf(ang) * r);
						const int   py  = y + oyi + static_cast<int>(sinf(ang) * r);
						sprite.DrawTex(m_lifeStarTex.get(), px, py, ss, ss, nullptr, &sc);
					}
				}
			}
		}
	}

	// ── コアリア会話UI（ポーズメニューより下、ゲームUIより上）──
	DrawCorelia();

	// ── ポーズメニュー（最前面）──
	if (m_menuOpen) { DrawPauseMenu(); }
}

//----------------------------------------------------------
// ポーズメニュー：操作
//----------------------------------------------------------
void GameScene::UpdatePauseMenu()
{
	m_menuBlinkTimer += KdFPSController::GetDt();

	// 上下で選択（W/S または ↑↓）
	const bool up   = ((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0);
	const bool down = ((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
	const bool nav  = up || down;
	if (nav && !m_menuNavPrev)
	{
		if (up) { m_menuIndex = (m_menuIndex + PauseMenuConst::Count - 1) % PauseMenuConst::Count; }
		else    { m_menuIndex = (m_menuIndex + 1) % PauseMenuConst::Count; }
	}
	m_menuNavPrev = nav;

	// 決定（Enter / Space）
	const bool confirm = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	if (confirm && !m_menuConfirmPrev)
	{
		switch (m_menuIndex)
		{
		case PauseMenuConst::Resume:
			m_menuOpen = false;
			break;
		case PauseMenuConst::StageSelect:
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::StageSelect);
			break;
		case PauseMenuConst::Title:
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Title);
			break;
		}
	}
	m_menuConfirmPrev = confirm;
}

//----------------------------------------------------------
// ポーズメニュー：描画
//----------------------------------------------------------
void GameScene::DrawPauseMenu()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// 背景を暗転
	{
		const Math::Color dim(0.0f, 0.0f, 0.0f, PauseMenuConst::DimAlpha);
		sprite.DrawBox(0, 0, sw, sh, &dim, true);
	}

	auto drawCentered = [&](const char* text, float y, const Math::Color& col)
	{
		auto measure = KdFontManager::Instance().CreateFontTexture(PauseMenuConst::FontNo, text, false);
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
		const Math::Vector2 pos(-textW * 0.5f, y - textH * 0.5f);
		sprite.DrawFont(pos, &col, "%s", text);
	};

	// ── レイアウト計算（中心原点・+Yが上）──
	const float titleY    = sh * PauseMenuConst::TitleYRatio;
	const float lastItemY = sh * PauseMenuConst::ItemTopYRatio - (PauseMenuConst::Count - 1) * PauseMenuConst::ItemSpacing;
	const float panelTop  = titleY + PauseMenuConst::PanelPadTop;
	const float panelBot  = lastItemY - PauseMenuConst::PanelPadBottom;
	const float panelCY   = (panelTop + panelBot) * 0.5f;
	const float panelH    = panelTop - panelBot;

	// ── パネル（影 → 本体 → 上端アクセント）──
	{
		const Math::Color shadow(0.0f, 0.0f, 0.0f, 0.5f);
		sprite.DrawBox(0, static_cast<int>(panelCY), static_cast<int>(PauseMenuConst::PanelWidth) + 10, static_cast<int>(panelH) + 10, &shadow, true);
		const Math::Color panel(0.06f, 0.07f, 0.12f, 0.92f);
		sprite.DrawBox(0, static_cast<int>(panelCY), static_cast<int>(PauseMenuConst::PanelWidth), static_cast<int>(panelH), &panel, true);
		const Math::Color edge(1.0f, 0.85f, 0.35f, 0.85f);
		sprite.DrawBox(0, static_cast<int>(panelTop) - 3, static_cast<int>(PauseMenuConst::PanelWidth), 3, &edge, true); // 上端の金ライン
	}

	// 見出し＋下線
	drawCentered(PauseMenuConst::TitleText, titleY, Math::Color(1.0f, 1.0f, 1.0f, 0.95f));
	{
		const Math::Color underline(1.0f, 0.85f, 0.35f, 0.7f);
		sprite.DrawBox(0, static_cast<int>(titleY) - 28, 200, 2, &underline, true);
	}

	// 項目
	const char* items[PauseMenuConst::Count] = { "RESUME", "STAGE SELECT", "TITLE" };
	const float blink = 0.5f + 0.5f * std::sinf(m_menuBlinkTimer * PauseMenuConst::BlinkSpeed);
	for (int i = 0; i < PauseMenuConst::Count; ++i)
	{
		const bool  sel = (i == m_menuIndex);
		const float y   = sh * PauseMenuConst::ItemTopYRatio - i * PauseMenuConst::ItemSpacing;

		if (sel)
		{
			// 選択ハイライトバー（金・点滅）
			const Math::Color bar(1.0f, 0.85f, 0.35f, 0.18f + 0.12f * blink);
			sprite.DrawBox(0, static_cast<int>(y), static_cast<int>(PauseMenuConst::HighlightW), static_cast<int>(PauseMenuConst::HighlightH), &bar, true);
			// コアリアの選択アイコン（バー左側）
			if (m_lifeIconTex)
			{
				const int ax = static_cast<int>(-PauseMenuConst::HighlightW * 0.5f + PauseMenuConst::AccentIconSize * 0.5f + PauseMenuConst::AccentGap);
				const Math::Color ic(1.0f, 1.0f, 1.0f, 0.7f + 0.3f * blink);
				sprite.DrawTex(m_lifeIconTex.get(), ax, static_cast<int>(y), PauseMenuConst::AccentIconSize, PauseMenuConst::AccentIconSize, nullptr, &ic);
			}
		}

		const Math::Color col = sel
			? Math::Color(1.0f, 0.97f, 0.7f, 1.0f)   // 選択中は明るく
			: Math::Color(0.75f, 0.78f, 0.85f, 0.85f);
		drawCentered(items[i], y, col);
	}
}

void GameScene::DrawDebugExtra()
{
	// デバッグワイヤーは DrawEffectExtra（シーンRT）側で描く。
	// ここ（DrawDebugパス）はバックバッファ行きで、F3エディタ画面では隠れて見えないため。
}

// デバッグワイヤー類の実体。シーンRTに描くパス(DrawEffectExtra)から呼ぶことで
// F3エディタ画面の Game ウィンドウ内でも見えるようにする。
void GameScene::DrawDebugWires()
{
	// ワイヤーフレーム類は 1キーON時のみ（通常はManualZoneのボックスだけ表示）
	if (m_debugZonesVisible)
	{
		ManualGravityZoneManager::Instance().DrawDebugShapes();
		DeadZoneManager::Instance().DrawDebugShapes();
		CoreliaManager::Instance().DrawDebugShapes();
	}

	if (m_editorMode)
	{
		m_roomEditor.DrawDebugLines();
		m_enemyEditor.DrawDebugSpheres();
		m_checkpointEditor.DrawDebugSpheres();
		m_warpHoleEditor.DrawDebug();
		m_movingFloorEditor.DrawDebug();
		m_windBoxEditor.DrawDebug();
		m_gravityCoreEditor.DrawDebug();
		m_spikeBoxEditor.DrawDebug();
		PlanetGravityManager::Instance().DrawDebugShapes();
	}
}

void GameScene::DrawUnLitExtra()
{
	// 通常は ManualGravity の箱だけ。NormalGravity の箱は 1キーON時のみ。
	// ManualGravity の箱は重力方向で色替え（上=赤/下=青）
	const bool manualUp = (m_spPlayer && m_spPlayer->GetManualGravityDir() == Character::ManualGravityDir::Up);
	ManualGravityZoneManager::Instance().DrawUnLit(m_debugZonesVisible, manualUp);
	// デッドゾーンの赤ボックスは 1キーON時のみ表示（通常は見えない死亡ゾーン）
	if (m_debugZonesVisible) { DeadZoneManager::Instance().DrawUnLit(); }
}

void GameScene::DrawLitExtra()
{
	PlanetGravityManager::Instance().DrawLit();
	m_itemManager.DrawLit();
	CoreliaManager::Instance().DrawLit();   // ヒントNPC（コアリア）
}

void GameScene::DrawOutlineExtra()
{
	// 惑星（地形）の細めアウトライン。objList 外なのでここで描く。
	PlanetGravityManager::Instance().DrawOutline();
	// アイテム（コイン・パラソル）のアウトライン
	m_itemManager.DrawOutline();
}

void GameScene::DrawEffectExtra()
{
	// アイテム周りの星きらめき（半透明・発光のためエフェクトパスで描画）
	m_itemManager.DrawEffect();

	// 重力ゾーンの重力方向矢印（ワールド固定で壁に貼り付く）
	DrawGravityArrows();

	// デバッグワイヤー（ゾーン枠／エディタ配置）。シーンRTに描くのでF3画面でも見える
	DrawDebugWires();

	// エディタ：選択中オブジェクト＋軸ギズモ（シーンRTに描いてGameウィンドウに表示）
	if (m_editorMode) { DrawSelectionMarker(); }
}

void GameScene::UpdateDuringHitStop()
{
	// 世界は止めたまま、取得バースト（吸い込み→放射ビーム）だけ動かす
	m_itemManager.UpdatePickupEffects();

	// ※プレイヤー発光はヒットストップ中は進めない。
	//   止まっている間に消費すると「取得時ポーズのまま」発光が終わり、
	//   解除後のモーションに追従しなくなるため。解除後に Player::Update が進める。
}

void GameScene::DrawGui()
{
	// ── エディタ操作パネル ──（既定で中央ドックへ。別ウィンドウ化で見失わないように）
	ImGui::SetNextWindowDockID(ImGui::GetID("##MainDockSpace"), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Editor"))
	{
		ImGui::TextColored({ 0.6f, 1.0f, 0.8f, 1.0f }, "Editor Controls");
		ImGui::Separator();

		// エディタ画面（ゲームをImGuiウィンドウ表示）⇔ 通常フルスクリーン
		ImGui::Checkbox("Editor Screen (F3)", &m_editorScreen);
		// デバッグ可視化（デッドゾーン/コアリア/各ゾーン枠）1キーと連動
		ImGui::Checkbox("Show Debug Zones (1)", &m_debugZonesVisible);
		// 他オブジェクトのUpdateを止める（プレイヤー操作は常にロック）
		ImGui::Checkbox("Freeze World (stop other Update)", &m_editorFreeze);

		// ── オブジェクト操作（マウス選択＋ドラッグ移動／生成／コピー）──
		if (m_editorMode)
		{
			ImGui::Separator();
			ImGui::TextColored({ 1.0f, 0.9f, 0.4f, 1.0f }, "Object Editing");
			ImGui::TextWrapped("Left-click an object to select. Drag the X/Y/Z axis handle to move along that axis. Ctrl+Z undo / Ctrl+Y redo.");

			// グリッドスナップ（カクカク移動）
			ImGui::Checkbox("Grid Snap", &m_snapEnabled);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::DragFloat("Grid Size", &m_snapSize, 0.1f, 0.1f, 100.0f, "%.2f");

			// 選択中の表示＋コピー
			if (m_selEntry >= 0 && m_selEntry < static_cast<int>(m_editEntries.size()))
			{
				const EditEntry& sel = m_editEntries[m_selEntry];
				ImGui::Text("Selected: %s", sel.label.c_str());
				ImGui::Text("Pos: %.1f, %.1f, %.1f", sel.pos.x, sel.pos.y, sel.pos.z);
				if (ImGui::Button("Copy Selected"))
				{
					if (sel.dup)
					{
						const EditKind k = sel.kind;
						std::function<void()> dupFn = sel.dup;
						dupFn();
						EditCommand cmd;
						cmd.undo = [this, k]()   { RemoveLastOfKind(k); };
						cmd.redo = [dupFn]()     { dupFn(); };
						m_undoStack.push_back(std::move(cmd));
						m_redoStack.clear();
					}
				}
			}
			else
			{
				ImGui::TextDisabled("Selected: (none)");
			}
			ImGui::Text("Undo: %d  Redo: %d",
				static_cast<int>(m_undoStack.size()), static_cast<int>(m_redoStack.size()));
			if (ImGui::Button("Undo (Ctrl+Z)")) { EditUndo(); } ImGui::SameLine();
			if (ImGui::Button("Redo (Ctrl+Y)")) { EditRedo(); }

			// 現在位置（カメラ前方）に新規生成
			ImGui::Text("Create at view:");
			if (ImGui::Button("Planet"))      { SpawnAtCursor(EditKind::Planet); }      ImGui::SameLine();
			if (ImGui::Button("WindBox"))     { SpawnAtCursor(EditKind::WindBox); }     ImGui::SameLine();
			if (ImGui::Button("SpikeBox"))    { SpawnAtCursor(EditKind::SpikeBox); }
			if (ImGui::Button("GravityCore")) { SpawnAtCursor(EditKind::GravityCore); } ImGui::SameLine();
			if (ImGui::Button("MovingFloor")) { SpawnAtCursor(EditKind::MovingFloor); }
			if (ImGui::Button("ManualZone"))  { SpawnAtCursor(EditKind::ManualZone); }  ImGui::SameLine();
			if (ImGui::Button("DeadZone"))    { SpawnAtCursor(EditKind::DeadZone); }    ImGui::SameLine();
			if (ImGui::Button("Corelia"))     { SpawnAtCursor(EditKind::Corelia); }
		}

		ImGui::Separator();
		if (ImGui::Button("Save All"))
		{
			// 全エディタ/マネージャのデータを一括保存
			PlanetGravityManager::Instance().Save();
			ManualGravityZoneManager::Instance().Save();
			DeadZoneManager::Instance().Save();
			CoreliaManager::Instance().Save();
			m_windBoxEditor.Save();
			m_spikeBoxEditor.Save();
			m_gravityCoreEditor.Save();
			m_movingFloorEditor.Save();
			m_roomEditor.Save();
			m_checkpointEditor.Save();
			m_warpHoleEditor.Save();
			m_enemyEditor.Save();
			m_itemManager.Save();          // コイン
			m_itemManager.SaveParasols();  // パラソル
			SaveSpawn();                   // スポーン＋導入開始位置
			SaveSunLight();                // 太陽光
			CameraSettings::Instance().Save();
			KdDebugGUI::Instance().AddLog("[Editor] Saved ALL (planets/zones/items/spawn/etc.)\n");
		}
		ImGui::SameLine();
		if (ImGui::Button("Reload All"))
		{
			ManualGravityZoneManager::Instance().Load();
			DeadZoneManager::Instance().Load();
			CoreliaManager::Instance().Load();
			KdDebugGUI::Instance().AddLog("[Editor] Reloaded zones / dead zones / corelia.\n");
		}

		ImGui::Separator();
		ImGui::Text("Zones: %d / DeadZones: %d / Corelia: %d",
			static_cast<int>(ManualGravityZoneManager::Instance().GetZoneCount()),
			static_cast<int>(DeadZoneManager::Instance().GetZoneCount()),
			CoreliaManager::Instance().GetNpcCount());
	}
	ImGui::End();

	// HP は星アイコン（スプライト）で表示するため、ImGui版HPは描画しない
	// if (m_spHpUI) { m_spHpUI->DrawGui(); }

	// プレイヤーコリジョンデバッグ
	if (m_spPlayer) { m_spPlayer->DrawCollisionDebugGui(); }

	// エディターモード時のみエディターGUIを表示
	m_roomEditor.DrawGui();
	m_enemyEditor.DrawGui();
	m_checkpointEditor.DrawGui();
	m_warpHoleEditor.DrawGui();
	m_movingFloorEditor.DrawGui();
	m_windBoxEditor.DrawGui();
	m_gravityCoreEditor.DrawGui();
	m_spikeBoxEditor.DrawGui();
	CameraSettings::Instance().DrawGui();
	PlanetGravityManager::Instance().DrawGui();
	ManualGravityZoneManager::Instance().DrawGui();
	DeadZoneManager::Instance().DrawGui();
	CoreliaManager::Instance().DrawGui();

	// アイテムエディター（常時表示）
	m_itemManager.DrawGui();
	if (ImGui::Begin("Sun Light"))
	{
		// 平行光の方向
		if (ImGui::DragFloat3("Direction", &m_sunDir.x, 0.01f, -1.0f, 1.0f))
		{
			ApplySunLight();
		}

		// 平行光の色
		if (ImGui::ColorEdit3("Sun Color", &m_sunColor.x))
		{
			ApplySunLight();
		}

		// 環境光の色と強度（アルファが全体の明るさ）
		if (ImGui::ColorEdit4("Ambient Color", &m_ambientColor.x))
		{
			ApplySunLight();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(A=Intensity)");

		ImGui::Separator();

		// 保存・読み込み
		if (ImGui::Button("Save Sun Light"))
		{
			SaveSunLight();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load Sun Light"))
		{
			LoadSunLight();
			ApplySunLight();
		}
	}
	ImGui::End();

	// ポイントライトエディター
	if (ImGui::Begin("Point Lights"))
	{
		if (ImGui::Button("Add Light"))
		{
			auto spLight = std::make_shared<PointLightObject>();
			m_pointLights.push_back(spLight);
			AddObject(spLight);
		}

		for (int i = static_cast<int>(m_pointLights.size()) - 1; i >= 0; --i)
		{
			m_pointLights[i]->DrawGui();
			ImGui::SameLine();
			const std::string btnLabel = "Remove##" + std::to_string(i);
			if (ImGui::Button(btnLabel.c_str()))
			{
				m_pointLights[i]->Expire();
				m_pointLights.erase(m_pointLights.begin() + i);
			}
		}
	}
	ImGui::End();

	if (m_roomEditor.IsDirty())
	{
		m_rooms = m_roomEditor.GetRooms();
		if (m_pCamera) { m_pCamera->SetRooms(m_rooms); }
		m_roomEditor.ClearDirty();
	}

	// スポーン位置エディター
	if (ImGui::Begin("Spawn Settings"))
	{
		// 初期リスポーン（着地）位置
		ImGui::TextColored({ 0.7f, 1.0f, 0.8f, 1.0f }, "Respawn / Landing");
		float pos[3] = { m_spawnPos.x, m_spawnPos.y, m_spawnPos.z };
		if (ImGui::DragFloat3("Spawn Position", pos, 0.1f))
		{
			m_spawnPos = { pos[0], pos[1], pos[2] };
		}
		if (ImGui::Button("Apply (Respawn)"))
		{
			if (m_spPlayer) { m_spPlayer->SetPos(m_spawnPos); }
		}
		ImGui::SameLine();
		if (ImGui::Button("Spawn = Player") && m_spPlayer)
		{
			m_spawnPos = m_spPlayer->GetPos();
		}

		ImGui::Separator();

		// 導入カットシーンの「飛んでくる」開始位置
		ImGui::TextColored({ 1.0f, 0.9f, 0.4f, 1.0f }, "Intro Start (fly-in)");
		float ipos[3] = { m_introStartPos.x, m_introStartPos.y, m_introStartPos.z };
		if (ImGui::DragFloat3("Intro Start Position", ipos, 0.1f))
		{
			m_introStartPos = { ipos[0], ipos[1], ipos[2] };
		}
		// エディタカメラの位置を開始位置に採用（見ている位置から飛ばす）
		if (ImGui::Button("Intro = Camera") && m_pEditorCam)
		{
			m_introStartPos = m_pEditorCam->GetPos();
		}
		ImGui::SameLine();
		// その場でカットシーンを再生して確認
		if (ImGui::Button("Test Intro"))
		{
			StartIntroCutscene();
		}

		ImGui::Separator();
		if (ImGui::Button("Save"))
		{
			SaveSpawn();
		}
	}
	ImGui::End();

	// ─── StarBurst Viewer（手動テスト）───────────────────────────
	if (ImGui::Begin("StarBurst Viewer"))
	{
		if (ImGui::Button("Play Burst at Player"))
		{
			m_starBurstTestRequest = true;
		}
	}
	ImGui::End();

	// ─── 
	// 
	// 
	// 
	// 
	// 
	// Viewer ─────────────────────────────────────────
	if (ImGui::Begin("Effekseer Viewer"))
	{
		ImGui::InputText("Effect File", m_efkViewerPath, sizeof(m_efkViewerPath));
		ImGui::DragFloat("Scale",       &m_efkViewerScale, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat("Speed",       &m_efkViewerSpeed, 0.05f, 0.01f, 10.0f);
		ImGui::Checkbox("Loop",         &m_efkViewerLoop);

		if (ImGui::Button("Play"))
		{
			const Math::Vector3 playPos = m_spPlayer ? m_spPlayer->GetPos() : Math::Vector3::Zero;
			KdEffekseerManager::GetInstance().Play(
				m_efkViewerPath, playPos, m_efkViewerScale, m_efkViewerSpeed, m_efkViewerLoop);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop All"))
		{
			KdEffekseerManager::GetInstance().StopAllEffect();
		}
	}
	ImGui::End();
}

//----------------------------------------------------------
// 重力ゾーンの重力方向を壁に2D矢印で表示（マリギャラ風）
//----------------------------------------------------------
void GameScene::DrawGravityArrows()
{
	if (m_editorMode || m_menuOpen) { return; }

	// 色を uint へ（KdPolygon::Vertex 用：a<<24|b<<16|g<<8|r）
	auto toUint = [](float r, float g, float b, float a) -> unsigned int
	{
		const unsigned int R = static_cast<unsigned int>(std::min(r, 1.0f) * 255.0f);
		const unsigned int G = static_cast<unsigned int>(std::min(g, 1.0f) * 255.0f);
		const unsigned int B = static_cast<unsigned int>(std::min(b, 1.0f) * 255.0f);
		const unsigned int A = static_cast<unsigned int>(std::min(a, 1.0f) * 255.0f);
		return (A << 24) | (B << 16) | (G << 8) | R;
	};
	std::vector<KdPolygon::Vertex> verts;
	int drawn = 0;

	auto& zoneMgr = ManualGravityZoneManager::Instance();
	const int zc = static_cast<int>(zoneMgr.GetZoneCount());
	for (int zi = 0; zi < zc && drawn < GravityArrowConst::MaxArrows; ++zi)
	{
		ManualGravityZone* z = zoneMgr.GetZone(zi);
		if (!z || !z->bEnabled) { continue; }
		if (z->Type != ZoneType::ManualGravity) { continue; }   // ManualGravityゾーンのみ

		// 重力ワールド方向：手動重力の離散値を使い、切替時に滑らかでなく「ぱっと」切り替わる
		Math::Vector3 g{ 0.0f, -1.0f, 0.0f };
		bool isUp = false;
		if (m_spPlayer)
		{
			switch (m_spPlayer->GetManualGravityDir())
			{
			case Character::ManualGravityDir::Down: g = { 0.0f, -1.0f, 0.0f }; break;
			case Character::ManualGravityDir::Up:   g = { 0.0f,  1.0f, 0.0f }; isUp = true; break;
			default:                                g = -m_spPlayer->GetUpDir(); break;  // None=自動
			}
		}
		// 上向き重力＝赤、それ以外＝青
		const float cr = isUp ? GravityArrowConst::UpColorR : GravityArrowConst::ColorR;
		const float cg = isUp ? GravityArrowConst::UpColorG : GravityArrowConst::ColorG;
		const float cb = isUp ? GravityArrowConst::UpColorB : GravityArrowConst::ColorB;
		Math::Vector2 d2(g.x, g.y);
		if (d2.LengthSquared() < 1e-5f) { continue; }   // 重力がZ方向＝壁に三角を描けない
		d2.Normalize();
		const Math::Vector3 dir(d2.x, d2.y, 0.0f);
		const Math::Vector3 perp(-d2.y, d2.x, 0.0f);

		// ゾーンの背景Boxのカメラ側の面に合わせる（少しだけ手前へ出してZファイト回避）
		const float z0   = zoneMgr.GetWallFrontZ(zi) - GravityArrowConst::FrontOffsetZ;
		const float minX = z->Center.x - z->HalfExtents.x;
		const float maxX = z->Center.x + z->HalfExtents.x;
		const float minY = z->Center.y - z->HalfExtents.y;
		const float maxY = z->Center.y + z->HalfExtents.y;
		const float sp   = GravityArrowConst::Spacing;

		auto addTri = [&](const Math::Vector3& c, float scale, float alpha)
		{
			const unsigned int col = toUint(cr, cg, cb, alpha);
			const float hl = GravityArrowConst::TriHalfLen  * scale;
			const float hw = GravityArrowConst::TriHalfWide * scale;
			const Math::Vector3 tip = c + dir * hl;                     // 重力方向の頂点
			const Math::Vector3 bL  = c - dir * hl + perp * hw;
			const Math::Vector3 bR  = c - dir * hl - perp * hw;
			KdPolygon::Vertex v0{}, v1{}, v2{};
			v0.pos = tip; v0.color = col;
			v1.pos = bL;  v1.color = col;
			v2.pos = bR;  v2.color = col;
			verts.push_back(v0); verts.push_back(v1); verts.push_back(v2);
		};

		// (perp=u, dir=v) のローカル格子で生成。v方向に流す＋千鳥（三角格子）
		float uMin = 1e9f, uMax = -1e9f, vMin = 1e9f, vMax = -1e9f;
		const float cxs[4] = { minX, maxX, minX, maxX };
		const float cys[4] = { minY, minY, maxY, maxY };
		for (int k = 0; k < 4; ++k)
		{
			const float u = perp.x * cxs[k] + perp.y * cys[k];
			const float v = dir.x  * cxs[k] + dir.y  * cys[k];
			uMin = std::min(uMin, u); uMax = std::max(uMax, u);
			vMin = std::min(vMin, v); vMax = std::max(vMax, v);
		}

		const float vRow   = sp * 0.866f;   // 三角格子の行間（√3/2）
		const float period = vRow * 2.0f;   // 千鳥が一致する周期（2行ぶん）。これで巻き戻しが継ぎ目なし
		float phase = fmodf(m_gravArrowScroll * GravityArrowConst::ScrollSpeed, period);
		if (phase < 0.0f) { phase += period; }

		const float fadeM = sp;   // 端でのフェード幅
		// 絶対格子インデックス基準で行を回す（位相に依存しないので巻き戻しても並びが変わらない）
		const int iMin = static_cast<int>(floorf((vMin - phase - vRow) / vRow)) - 1;
		const int iMax = static_cast<int>(ceilf ((vMax - phase + vRow) / vRow)) + 1;
		for (int iv = iMin; iv <= iMax && drawn < GravityArrowConst::MaxArrows; ++iv)
		{
			const float vv = iv * vRow + phase;
			const float ushift = (iv & 1) ? sp * 0.5f : 0.0f;   // 絶対インデックスで千鳥
			for (float u = uMin - sp; u <= uMax + sp && drawn < GravityArrowConst::MaxArrows; u += sp)
			{
				const float uu = u + ushift;
				// ゾーン端でアルファをフェード（出入りを滑らかにしてループの繋ぎを隠す）
				const float fv = std::clamp(std::min(vv - vMin, vMax - vv) / fadeM, 0.0f, 1.0f);
				const float fu = std::clamp(std::min(uu - uMin, uMax - uu) / fadeM, 0.0f, 1.0f);
				const float fade = fv * fu;
				if (fade <= 0.001f) { continue; }

				const Math::Vector3 p = perp * uu + dir * vv;   // XY平面上の点
				const Math::Vector3 c(p.x, p.y, z0);
				addTri(c, GravityArrowConst::OuterScale, GravityArrowConst::OuterAlpha * fade);   // 大きく薄い
				addTri(c, GravityArrowConst::InnerScale, GravityArrowConst::InnerAlpha * fade);   // 中心の明るいコア
				++drawn;
			}
		}
	}

	if (!verts.empty())
	{
		// 加算合成でやわらかく光る半透明の三角形に
		KdShaderManager::Instance().ChangeBlendState(KdBlendState::Add);
		KdShaderManager::Instance().m_StandardShader.DrawVertices(
			verts, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
			KdDepthStencilState::ZWriteDisable,   // 手前の物に隠れる（貫通しない）
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		KdShaderManager::Instance().UndoBlendState();
	}
}

//==========================================================
// エディタ：オブジェクト選択・操作（自前マウスピッキング＋ドラッグ）
//==========================================================

// PlanetData はコピー禁止（モデルの unique_ptr 保持）なので、POD フィールドだけを複製して
// 新しいモデルを生成する。位置オフセットは呼び出し側で行う。
static PlanetData ClonePlanetFields(const PlanetData& _s)
{
	PlanetData d;
	d.Position             = _s.Position;
	d.SurfaceRadius        = _s.SurfaceRadius;
	d.GroundRadius         = _s.GroundRadius;
	d.GravityRadius        = _s.GravityRadius;
	d.Priority             = _s.Priority;
	d.bNormalGravity       = _s.bNormalGravity;
	d.GravityStrength      = _s.GravityStrength;
	d.Shape                = _s.Shape;
	d.BoxHalfExtents       = _s.BoxHalfExtents;
	d.BoxFaceGravityTop    = _s.BoxFaceGravityTop;
	d.BoxFaceGravityBottom = _s.BoxFaceGravityBottom;
	d.BoxFaceGravityLeft   = _s.BoxFaceGravityLeft;
	d.BoxFaceGravityRight  = _s.BoxFaceGravityRight;
	d.InitModel();   // モデル＆コリジョン生成（内部で UpdateWorld も呼ばれる）
	return d;
}

// 軸方向の単位ベクトルを返す（0=X, 1=Y, 2=Z）
static Math::Vector3 AxisDirOf(int _a)
{
	if (_a == 0) { return Math::Vector3(1.0f, 0.0f, 0.0f); }
	if (_a == 1) { return Math::Vector3(0.0f, 1.0f, 0.0f); }
	return Math::Vector3(0.0f, 0.0f, 1.0f);
}

// レイ(_ro,_rd)に対し、点_Aを通り方向_u(単位)の直線上で最も近い点のパラメータ s を返す
// （2直線の最近接点。軸ドラッグの移動量算出に使う）
static float AxisClosestParam(const Math::Vector3& _ro, const Math::Vector3& _rd,
							  const Math::Vector3& _A, const Math::Vector3& _u)
{
	const Math::Vector3 w0 = _A - _ro;
	const float b = _u.Dot(_rd);   // a=1(_u単位), c=1(_rd単位)
	const float d = _u.Dot(w0);
	const float e = _rd.Dot(w0);
	float denom = 1.0f - b * b;
	if (fabsf(denom) < 1e-6f) { denom = (denom < 0.0f) ? -1e-6f : 1e-6f; }
	return (b * e - d) / denom;
}

// ゲーム画像内の正規化座標(_u,_v)からワールド空間のピッキングレイを作る
bool GameScene::ScreenRayFromGameUV(float _u, float _v,
	Math::Vector3& _outOrigin, Math::Vector3& _outDir) const
{
	if (_u < 0.0f || _u > 1.0f || _v < 0.0f || _v > 1.0f) { return false; }

	const Math::Matrix view = KdShaderManager::Instance().GetCameraCB().mView;
	const Math::Matrix proj = KdShaderManager::Instance().GetCameraCB().mProj;
	const Math::Matrix invVP = (view * proj).Invert();

	const float ndcX = _u * 2.0f - 1.0f;
	const float ndcY = 1.0f - _v * 2.0f;   // スクリーンYは下向き、NDCは上向き
	const Math::Vector3 pNear = Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 0.0f), invVP);
	const Math::Vector3 pFar  = Math::Vector3::Transform(Math::Vector3(ndcX, ndcY, 1.0f), invVP);

	_outOrigin = pNear;
	_outDir    = pFar - pNear;
	if (_outDir.LengthSquared() < 1e-8f) { return false; }
	_outDir.Normalize();
	return true;
}

// 全オブジェクトを位置の取得／設定／複製を抽象化した編集エントリへ集約（毎フレーム）
void GameScene::BuildEditEntries()
{
	m_editEntries.clear();
	const float copyOff = EditorPickConst::CopyOffset;

	auto pushEntry = [&](const Math::Vector3& _p, EditKind _k, int _idx, const std::string& _label,
		std::function<void(const Math::Vector3&)> _sp,
		std::function<void()> _dp, std::function<void()> _sel)
	{
		EditEntry e;
		e.pos = _p; e.kind = _k; e.idx = _idx; e.label = _label;
		e.setPos = std::move(_sp); e.dup = std::move(_dp); e.select = std::move(_sel);
		m_editEntries.push_back(std::move(e));
	};

	// 惑星（Planet）
	{
		auto& v = PlanetGravityManager::Instance().WorkPlanets();
		for (int i = 0; i < static_cast<int>(v.size()); ++i)
		{
			pushEntry(v[i].Position, EditKind::Planet, i, "Planet " + std::to_string(i),
				[i](const Math::Vector3& p) { auto& vv = PlanetGravityManager::Instance().WorkPlanets(); if (i < (int)vv.size()) { vv[i].Position = p; PlanetGravityManager::Instance().MarkWorldDirty(); } },
				[i, copyOff]() { auto& vv = PlanetGravityManager::Instance().WorkPlanets(); if (i < (int)vv.size()) { PlanetData d = ClonePlanetFields(vv[i]); d.Position.x += copyOff; d.UpdateWorld(); vv.push_back(std::move(d)); PlanetGravityManager::Instance().MarkWorldDirty(); } },
				[i]() { PlanetGravityManager::Instance().SetSelected(i); });
		}
	}
	// 風ボックス（WindBox）
	{
		auto& v = m_windBoxEditor.WorkBoxes();
		for (int i = 0; i < static_cast<int>(v.size()); ++i)
		{
			pushEntry(v[i].center, EditKind::WindBox, i, "WindBox " + std::to_string(i),
				[this, i](const Math::Vector3& p) { auto& vv = m_windBoxEditor.WorkBoxes(); if (i < (int)vv.size()) { vv[i].center = p; m_windBoxEditor.MarkDirty(); } },
				[this, i, copyOff]() { auto& vv = m_windBoxEditor.WorkBoxes(); if (i < (int)vv.size()) { auto d = vv[i]; d.center.x += copyOff; vv.push_back(d); m_windBoxEditor.MarkDirty(); } },
				[this, i]() { m_windBoxEditor.SetSelected(i); });
		}
	}
	// 棘ボックス（SpikeBox）
	{
		auto& v = m_spikeBoxEditor.WorkBoxes();
		for (int i = 0; i < static_cast<int>(v.size()); ++i)
		{
			pushEntry(v[i].center, EditKind::SpikeBox, i, "SpikeBox " + std::to_string(i),
				[this, i](const Math::Vector3& p) { auto& vv = m_spikeBoxEditor.WorkBoxes(); if (i < (int)vv.size()) { vv[i].center = p; m_spikeBoxEditor.MarkDirty(); } },
				[this, i, copyOff]() { auto& vv = m_spikeBoxEditor.WorkBoxes(); if (i < (int)vv.size()) { auto d = vv[i]; d.center.x += copyOff; vv.push_back(d); m_spikeBoxEditor.MarkDirty(); } },
				[this, i]() { m_spikeBoxEditor.SetSelected(i); });
		}
	}
	// 重力コア（GravityCore）
	{
		auto& v = m_gravityCoreEditor.WorkCores();
		for (int i = 0; i < static_cast<int>(v.size()); ++i)
		{
			pushEntry(v[i].pos, EditKind::GravityCore, i, "GravityCore " + std::to_string(i),
				[this, i](const Math::Vector3& p) { auto& vv = m_gravityCoreEditor.WorkCores(); if (i < (int)vv.size()) { vv[i].pos = p; m_gravityCoreEditor.MarkDirty(); } },
				[this, i, copyOff]() { auto& vv = m_gravityCoreEditor.WorkCores(); if (i < (int)vv.size()) { auto d = vv[i]; d.pos.x += copyOff; vv.push_back(d); m_gravityCoreEditor.MarkDirty(); } },
				[this, i]() { m_gravityCoreEditor.SetSelected(i); });
		}
	}
	// 移動床（MovingFloor）
	{
		auto& v = m_movingFloorEditor.WorkFloors();
		for (int i = 0; i < static_cast<int>(v.size()); ++i)
		{
			pushEntry(v[i].center, EditKind::MovingFloor, i, "MovingFloor " + std::to_string(i),
				[this, i](const Math::Vector3& p) { auto& vv = m_movingFloorEditor.WorkFloors(); if (i < (int)vv.size()) { vv[i].center = p; m_movingFloorEditor.MarkDirty(); } },
				[this, i, copyOff]() { auto& vv = m_movingFloorEditor.WorkFloors(); if (i < (int)vv.size()) { auto d = vv[i]; d.center.x += copyOff; vv.push_back(d); m_movingFloorEditor.MarkDirty(); } },
				[this, i]() { m_movingFloorEditor.SetSelected(i); });
		}
	}
	// 手動重力ゾーン（ManualZone）
	{
		auto& mgr = ManualGravityZoneManager::Instance();
		const int n = static_cast<int>(mgr.GetZoneCount());
		for (int i = 0; i < n; ++i)
		{
			ManualGravityZone* z = mgr.GetZone(i);
			if (!z) { continue; }
			pushEntry(z->Center, EditKind::ManualZone, i, "ManualZone " + std::to_string(i),
				[i](const Math::Vector3& p) { auto* zz = ManualGravityZoneManager::Instance().GetZone(i); if (zz) { zz->Center = p; } },
				[i, copyOff]() { auto* zz = ManualGravityZoneManager::Instance().GetZone(i); if (zz) { ManualGravityZone c = *zz; c.Center.x += copyOff; ManualGravityZoneManager::Instance().AddZone(c); } },
				[i]() { ManualGravityZoneManager::Instance().SetSelected(i); });
		}
	}
	// デッドゾーン（DeadZone）
	{
		auto& mgr = DeadZoneManager::Instance();
		const int n = static_cast<int>(mgr.GetZoneCount());
		for (int i = 0; i < n; ++i)
		{
			DeadZone* z = mgr.GetZone(i);
			if (!z) { continue; }
			pushEntry(z->Center, EditKind::DeadZone, i, "DeadZone " + std::to_string(i),
				[i](const Math::Vector3& p) { auto* zz = DeadZoneManager::Instance().GetZone(i); if (zz) { zz->Center = p; } },
				[i, copyOff]() { auto* zz = DeadZoneManager::Instance().GetZone(i); if (zz) { DeadZone c = *zz; c.Center.x += copyOff; DeadZoneManager::Instance().AddZone(c); } },
				[i]() { DeadZoneManager::Instance().SetSelected(i); });
		}
	}
	// コアリア（Corelia NPC）
	{
		auto& mgr = CoreliaManager::Instance();
		const int n = mgr.GetNpcCount();
		for (int i = 0; i < n; ++i)
		{
			CoreliaNpc* npc = mgr.GetNpc(i);
			if (!npc) { continue; }
			pushEntry(npc->Pos, EditKind::Corelia, i, "Corelia " + std::to_string(i),
				[i](const Math::Vector3& p) { auto* nn = CoreliaManager::Instance().GetNpc(i); if (nn) { nn->Pos = p; } },
				[i, copyOff]() { auto* nn = CoreliaManager::Instance().GetNpc(i); if (nn) { CoreliaNpc c = *nn; c.Pos.x += copyOff; CoreliaManager::Instance().AddNpc(c); } },
				[i]() { CoreliaManager::Instance().SetSelected(i); });
		}
	}
}

// マウスでオブジェクトを選択し、ドラッグでカメラ平面に沿って移動する
// 選択中オブジェクトの軸ハンドルのうち、レイが掴んだ軸を返す（無ければ None）
GameScene::GizmoAxis GameScene::PickGizmoAxis(const Math::Vector3& _ro, const Math::Vector3& _rd,
	const Math::Vector3& _selPos) const
{
	const float L = EditorPickConst::GizmoLength;
	const float R = EditorPickConst::GizmoPickRadius;
	int   bestAxis = -1;
	float bestDist = R;
	for (int a = 0; a < 3; ++a)
	{
		const Math::Vector3 u = AxisDirOf(a);
		float s = AxisClosestParam(_ro, _rd, _selPos, u);
		s = std::clamp(s, 0.0f, L);                 // ハンドルは [selPos, selPos+u*L]
		const Math::Vector3 pAxis = _selPos + u * s;
		float tRay = (pAxis - _ro).Dot(_rd);
		if (tRay < 0.0f) { tRay = 0.0f; }
		const Math::Vector3 pRay = _ro + _rd * tRay;
		const float dist = (pAxis - pRay).Length();
		if (dist < bestDist) { bestDist = dist; bestAxis = a; }
	}
	if (bestAxis == 0) { return GizmoAxis::X; }
	if (bestAxis == 1) { return GizmoAxis::Y; }
	if (bestAxis == 2) { return GizmoAxis::Z; }
	return GizmoAxis::None;
}

void GameScene::UpdateEditorPick()
{
	BuildEditEntries();

	// ── アンドゥ／リドゥ（Ctrl+Z / Ctrl+Y）──
	{
		const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool zKey = (GetAsyncKeyState('Z') & 0x8000) != 0;
		const bool yKey = (GetAsyncKeyState('Y') & 0x8000) != 0;
		const bool zNow = ctrl && zKey;
		const bool yNow = ctrl && yKey;
		if (zNow && !m_ctrlZPrev) { EditUndo(); }
		if (yNow && !m_ctrlYPrev) { EditRedo(); }
		m_ctrlZPrev = zNow;
		m_ctrlYPrev = yNow;
	}

	float u = 0.0f, v = 0.0f;
	KdDebugGUI::Instance().GetGameUV(u, v);
	const bool hovered = KdDebugGUI::Instance().IsGameHovered();
	const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	Math::Vector3 ro, rd;
	const bool rayOk = hovered && ScreenRayFromGameUV(u, v, ro, rd);

	const bool selValid = (m_selEntry >= 0 && m_selEntry < static_cast<int>(m_editEntries.size()));

	// クリック開始：まず選択中オブジェクトの軸ハンドルを判定 → 軸ドラッグ開始。
	// 軸でなければオブジェクトを選択（ドラッグはしない）。何も無ければ選択解除。
	if (lmb && !m_lmbPrev)
	{
		m_dragAxis   = GizmoAxis::None;
		m_moveActive = false;

		if (rayOk)
		{
			// 1) 選択中なら軸ハンドルを優先判定
			GizmoAxis ax = GizmoAxis::None;
			if (selValid)
			{
				ax = PickGizmoAxis(ro, rd, m_editEntries[m_selEntry].pos);
			}

			if (ax != GizmoAxis::None)
			{
				m_dragAxis     = ax;
				m_dragStartPos = m_editEntries[m_selEntry].pos;
				m_movePreStart = m_dragStartPos;
				const Math::Vector3 udir = AxisDirOf(ax == GizmoAxis::X ? 0 : (ax == GizmoAxis::Y ? 1 : 2));
				m_dragStartS   = AxisClosestParam(ro, rd, m_dragStartPos, udir);
				m_moveActive   = true;
			}
			else
			{
				// 2) オブジェクトをピック（選択のみ）
				int best = -1; float bestT = FLT_MAX;
				const float R = EditorPickConst::PickRadius;
				for (int i = 0; i < static_cast<int>(m_editEntries.size()); ++i)
				{
					const Math::Vector3 oc = ro - m_editEntries[i].pos;
					const float b = oc.Dot(rd);
					const float c = oc.LengthSquared() - R * R;
					const float disc = b * b - c;
					if (disc < 0.0f) { continue; }
					const float sq = sqrtf(disc);
					float t = -b - sq;
					if (t < 0.0f) { t = -b + sq; }
					if (t < 0.0f) { continue; }
					if (t < bestT) { bestT = t; best = i; }
				}
				m_selEntry = best;   // 何も当たらなければ -1（選択解除）
				if (best >= 0 && m_editEntries[best].select) { m_editEntries[best].select(); }
			}
		}
	}

	// 軸ドラッグ中：選んだ軸方向にだけ移動
	if (m_dragAxis != GizmoAxis::None && lmb && rayOk && selValid)
	{
		const Math::Vector3 udir = AxisDirOf(m_dragAxis == GizmoAxis::X ? 0 : (m_dragAxis == GizmoAxis::Y ? 1 : 2));
		const float s = AxisClosestParam(ro, rd, m_dragStartPos, udir);
		Math::Vector3 np = m_dragStartPos + udir * (s - m_dragStartS);

		// グリッドスナップ（カクカク移動）：動かした軸の座標だけグリッドに吸着
		if (m_snapEnabled && m_snapSize > 1e-4f)
		{
			auto snap = [g = m_snapSize](float val) { return std::roundf(val / g) * g; };
			if      (m_dragAxis == GizmoAxis::X) { np.x = snap(np.x); }
			else if (m_dragAxis == GizmoAxis::Y) { np.y = snap(np.y); }
			else                                 { np.z = snap(np.z); }
		}

		if (m_editEntries[m_selEntry].setPos) { m_editEntries[m_selEntry].setPos(np); }
	}

	// ボタンを離したら：移動が発生していればアンドゥに記録
	if (!lmb)
	{
		if (m_moveActive && selValid)
		{
			const Math::Vector3 nowPos = m_editEntries[m_selEntry].pos;
			if ((nowPos - m_movePreStart).LengthSquared() > EditorPickConst::MoveEpsilon)
			{
				PushMoveCommand(m_editEntries[m_selEntry].kind, m_editEntries[m_selEntry].idx,
					m_movePreStart, nowPos);
			}
		}
		m_moveActive = false;
		m_dragAxis   = GizmoAxis::None;
	}
	m_lmbPrev = lmb;
}

// 既定オブジェクトを指定位置に生成
void GameScene::CreateDefaultAt(EditKind _kind, const Math::Vector3& _pos)
{
	switch (_kind)
	{
	case EditKind::Planet:      { PlanetData d;      d.Position = _pos; d.InitModel(); PlanetGravityManager::Instance().WorkPlanets().push_back(std::move(d)); PlanetGravityManager::Instance().MarkWorldDirty(); } break;
	case EditKind::WindBox:     { WindBoxData d;     d.center   = _pos; m_windBoxEditor.WorkBoxes().push_back(d);     m_windBoxEditor.MarkDirty(); } break;
	case EditKind::SpikeBox:    { SpikeBoxData d;    d.center   = _pos; m_spikeBoxEditor.WorkBoxes().push_back(d);    m_spikeBoxEditor.MarkDirty(); } break;
	case EditKind::GravityCore: { GravityCoreData d; d.pos      = _pos; m_gravityCoreEditor.WorkCores().push_back(d); m_gravityCoreEditor.MarkDirty(); } break;
	case EditKind::MovingFloor: { MovingFloorData d; d.center   = _pos; m_movingFloorEditor.WorkFloors().push_back(d); m_movingFloorEditor.MarkDirty(); } break;
	case EditKind::ManualZone:  { ManualGravityZone z; z.Center = _pos; ManualGravityZoneManager::Instance().AddZone(z); } break;
	case EditKind::DeadZone:    { DeadZone z;          z.Center = _pos; DeadZoneManager::Instance().AddZone(z); } break;
	case EditKind::Corelia:     { CoreliaNpc n;        n.Pos    = _pos; CoreliaManager::Instance().AddNpc(n); } break;
	}
}

// 種別コンテナの末尾要素を削除（生成のアンドゥ）
void GameScene::RemoveLastOfKind(EditKind _kind)
{
	switch (_kind)
	{
	case EditKind::Planet:      { auto& v = PlanetGravityManager::Instance().WorkPlanets(); if (!v.empty()) { v.pop_back(); PlanetGravityManager::Instance().MarkWorldDirty(); } } break;
	case EditKind::WindBox:     { auto& v = m_windBoxEditor.WorkBoxes();     if (!v.empty()) { v.pop_back(); m_windBoxEditor.MarkDirty(); } } break;
	case EditKind::SpikeBox:    { auto& v = m_spikeBoxEditor.WorkBoxes();    if (!v.empty()) { v.pop_back(); m_spikeBoxEditor.MarkDirty(); } } break;
	case EditKind::GravityCore: { auto& v = m_gravityCoreEditor.WorkCores(); if (!v.empty()) { v.pop_back(); m_gravityCoreEditor.MarkDirty(); } } break;
	case EditKind::MovingFloor: { auto& v = m_movingFloorEditor.WorkFloors();if (!v.empty()) { v.pop_back(); m_movingFloorEditor.MarkDirty(); } } break;
	case EditKind::ManualZone:  { ManualGravityZoneManager::Instance().RemoveLastZone(); } break;
	case EditKind::DeadZone:    { DeadZoneManager::Instance().RemoveLastZone(); } break;
	case EditKind::Corelia:     { CoreliaManager::Instance().RemoveLastNpc(); } break;
	}
}

// 種別＋index で位置を設定（アンドゥ／リドゥ用）
void GameScene::SetPosByKindIndex(EditKind _kind, int _idx, const Math::Vector3& _pos)
{
	switch (_kind)
	{
	case EditKind::Planet:      { auto& v = PlanetGravityManager::Instance().WorkPlanets(); if (_idx >= 0 && _idx < (int)v.size()) { v[_idx].Position = _pos; PlanetGravityManager::Instance().MarkWorldDirty(); } } break;
	case EditKind::WindBox:     { auto& v = m_windBoxEditor.WorkBoxes();     if (_idx >= 0 && _idx < (int)v.size()) { v[_idx].center = _pos; m_windBoxEditor.MarkDirty(); } } break;
	case EditKind::SpikeBox:    { auto& v = m_spikeBoxEditor.WorkBoxes();    if (_idx >= 0 && _idx < (int)v.size()) { v[_idx].center = _pos; m_spikeBoxEditor.MarkDirty(); } } break;
	case EditKind::GravityCore: { auto& v = m_gravityCoreEditor.WorkCores(); if (_idx >= 0 && _idx < (int)v.size()) { v[_idx].pos    = _pos; m_gravityCoreEditor.MarkDirty(); } } break;
	case EditKind::MovingFloor: { auto& v = m_movingFloorEditor.WorkFloors();if (_idx >= 0 && _idx < (int)v.size()) { v[_idx].center = _pos; m_movingFloorEditor.MarkDirty(); } } break;
	case EditKind::ManualZone:  { auto* z = ManualGravityZoneManager::Instance().GetZone(_idx); if (z) { z->Center = _pos; } } break;
	case EditKind::DeadZone:    { auto* z = DeadZoneManager::Instance().GetZone(_idx);          if (z) { z->Center = _pos; } } break;
	case EditKind::Corelia:     { auto* n = CoreliaManager::Instance().GetNpc(_idx);            if (n) { n->Pos    = _pos; } } break;
	}
}

// 移動コマンドをアンドゥスタックへ積む（リドゥスタックはクリア）
void GameScene::PushMoveCommand(EditKind _kind, int _idx,
	const Math::Vector3& _oldPos, const Math::Vector3& _newPos)
{
	EditCommand c;
	c.undo = [this, _kind, _idx, _oldPos]() { SetPosByKindIndex(_kind, _idx, _oldPos); };
	c.redo = [this, _kind, _idx, _newPos]() { SetPosByKindIndex(_kind, _idx, _newPos); };
	m_undoStack.push_back(std::move(c));
	m_redoStack.clear();
}

void GameScene::EditUndo()
{
	if (m_undoStack.empty()) { return; }
	EditCommand c = std::move(m_undoStack.back());
	m_undoStack.pop_back();
	if (c.undo) { c.undo(); }
	m_redoStack.push_back(std::move(c));
}

void GameScene::EditRedo()
{
	if (m_redoStack.empty()) { return; }
	EditCommand c = std::move(m_redoStack.back());
	m_redoStack.pop_back();
	if (c.redo) { c.redo(); }
	m_undoStack.push_back(std::move(c));
}

// エディタカメラ前方の一定距離に新規オブジェクトを生成（アンドゥ対応）
void GameScene::SpawnAtCursor(EditKind _kind)
{
	if (!m_pEditorCam) { return; }
	const Math::Matrix view    = KdShaderManager::Instance().GetCameraCB().mView;
	const Math::Matrix invView = view.Invert();
	Math::Vector3 fwd(invView._31, invView._32, invView._33);
	if (fwd.LengthSquared() > 1e-8f) { fwd.Normalize(); }
	const Math::Vector3 p = m_pEditorCam->GetPos() + fwd * EditorPickConst::SpawnDist;

	CreateDefaultAt(_kind, p);

	// アンドゥ：末尾を削除／リドゥ：同じ位置に再生成
	EditCommand c;
	c.undo = [this, _kind]()         { RemoveLastOfKind(_kind); };
	c.redo = [this, _kind, p]()      { CreateDefaultAt(_kind, p); };
	m_undoStack.push_back(std::move(c));
	m_redoStack.clear();
}

// 選択中オブジェクトに軸ギズモ（X赤/Y緑/Z青）と選択ボックスを描画（常に手前）
void GameScene::DrawSelectionMarker()
{
	if (m_selEntry < 0 || m_selEntry >= static_cast<int>(m_editEntries.size())) { return; }

	const Math::Vector3 c = m_editEntries[m_selEntry].pos;
	std::vector<KdPolygon::Vertex> verts;

	auto addLine = [&](const Math::Vector3& a, const Math::Vector3& b, unsigned int col)
	{
		KdPolygon::Vertex v0{}, v1{};
		v0.pos = a; v0.color = col;
		v1.pos = b; v1.color = col;
		verts.push_back(v0); verts.push_back(v1);
	};
	auto addBox = [&](const Math::Vector3& ctr, float h, unsigned int col)
	{
		const Math::Vector3 cn[8] = {
			{ ctr.x - h, ctr.y - h, ctr.z - h }, { ctr.x + h, ctr.y - h, ctr.z - h },
			{ ctr.x + h, ctr.y + h, ctr.z - h }, { ctr.x - h, ctr.y + h, ctr.z - h },
			{ ctr.x - h, ctr.y - h, ctr.z + h }, { ctr.x + h, ctr.y - h, ctr.z + h },
			{ ctr.x + h, ctr.y + h, ctr.z + h }, { ctr.x - h, ctr.y + h, ctr.z + h },
		};
		const int eg[12][2] = { {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7} };
		for (int e = 0; e < 12; ++e) { addLine(cn[eg[e][0]], cn[eg[e][1]], col); }
	};

	// 選択ボックス（黄）
	addBox(c, EditorPickConst::MarkerHalf, 0xFF00FFFF);

	// 軸ギズモ（X=赤, Y=緑, Z=青。掴み白）
	const float L = EditorPickConst::GizmoLength;
	const float tip = EditorPickConst::GizmoTipHalf;
	const unsigned int axisCol[3] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000 }; // R, G, B
	const GizmoAxis cur = m_dragAxis;
	for (int a = 0; a < 3; ++a)
	{
		const Math::Vector3 u = AxisDirOf(a);
		const Math::Vector3 end = c + u * L;
		const bool active =
			(a == 0 && cur == GizmoAxis::X) ||
			(a == 1 && cur == GizmoAxis::Y) ||
			(a == 2 && cur == GizmoAxis::Z);
		const unsigned int col = active ? 0xFFFFFFFF : axisCol[a];
		addLine(c, end, col);
		addBox(end, tip, col);   // 先端ハンドル
	}

	KdShaderManager::Instance().m_StandardShader.DrawVertices(
		verts, Math::Matrix::Identity, Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZDisable,   // 常に手前に表示
		D3D_PRIMITIVE_TOPOLOGY_LINELIST);
}

//----------------------------------------------------------
// コアリア：インタラクト判定＆会話中のカメラズーム
//----------------------------------------------------------
void GameScene::UpdateCorelia()
{
	const float dt = KdFPSController::GetDt();
	if (m_spPlayer) { CoreliaManager::Instance().SetPlayerPos(m_spPlayer->GetPos()); }
	CoreliaManager::Instance().Update(dt);

	// Wキーの押下エッジ
	const bool wDown = (GetAsyncKeyState('W') & 0x8000) != 0;
	const bool wEdge = wDown && !m_interactPrev;
	m_interactPrev = wDown;

	if (m_convoActive)
	{
		// ズームイン補間
		m_convoZoom += (1.0f - m_convoZoom) * CoreliaConst::ConvoZoomLerp;

		// カメラをプレイヤーとコアリアの間へ寄せ、Z基準を縮めてズームイン
		if (m_pCamera && m_spPlayer)
		{
			const Math::Vector3 focus = Math::Vector3::Lerp(
				m_spPlayer->GetPos(), m_convoNpcPos, CoreliaConst::ConvoFocusToNpc);
			const float baseZ = CameraSettings::Instance().OffsetZ;
			const float zoomZ = baseZ * (1.0f - (1.0f - CoreliaConst::ConvoZoomZMul) * m_convoZoom);
			m_pCamera->SetOffsetZOverride(zoomZ);
			m_pCamera->Update(focus, m_spPlayer->GetUpDir());
		}

		// 会話中はプレイヤーも顔(頭)をコアリアへ向ける
		if (m_spPlayer) { m_spPlayer->LookAtHead(m_convoNpcPos); }

		// W でクローズ
		if (wEdge)
		{
			m_convoActive = false;
			m_convoZoom   = 0.0f;
			if (m_pCamera) { m_pCamera->ClearOffsetZOverride(); }
		}
		return;
	}

	// 非会話中：近くのコアリアに W で話しかける
	if (wEdge && m_spPlayer)
	{
		const int idx = CoreliaManager::Instance().FindInteractable(m_spPlayer->GetPos());
		if (idx >= 0)
		{
			Math::Vector3 npos;
			if (CoreliaManager::Instance().GetNpcPos(idx, npos))
			{
				m_convoNpcPos = npos;
				m_convoText   = CoreliaManager::Instance().GetHint(
					CoreliaManager::Instance().GetNpcHintId(idx));
				m_convoActive = true;
			}
		}
	}
}

//----------------------------------------------------------
// コアリア：会話ウィンドウ描画（SJIS文字を日本語フォントで折り返し表示）
//----------------------------------------------------------
void GameScene::DrawCorelia()
{
	if (!m_convoActive) { return; }

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float screenW = static_cast<float>(bb->GetInfo().Width);
	const float screenH = static_cast<float>(bb->GetInfo().Height);

	const int   bw = static_cast<int>(CoreliaConst::BoxWidth);
	const int   bh = static_cast<int>(CoreliaConst::BoxHeight);
	const int   cx = 0;   // 画面中央(横)
	const int   cy = static_cast<int>(-screenH * 0.5f + CoreliaConst::BoxMargin + bh * 0.5f);

	// 影 → 本体 → 枠
	const Math::Color shadow(0.0f, 0.0f, 0.0f, 0.5f);
	sprite.DrawBox(cx + 6, cy - 6, bw, bh, &shadow, true);
	const Math::Color body(CoreliaConst::BoxR, CoreliaConst::BoxG, CoreliaConst::BoxB, CoreliaConst::BoxA);
	sprite.DrawBox(cx, cy, bw, bh, &body, true);
	const Math::Color edge(CoreliaConst::EdgeR, CoreliaConst::EdgeG, CoreliaConst::EdgeB, CoreliaConst::EdgeA);
	sprite.DrawBox(cx, cy + bh / 2, bw, 3, &edge, true);   // 上辺
	sprite.DrawBox(cx, cy - bh / 2, bw, 3, &edge, true);   // 下辺

	// 話者名（枠の左上）
	{
		const Math::Vector2 namePos(
			static_cast<float>(cx) - bw * 0.5f + CoreliaConst::TextPadX,
			static_cast<float>(cy) + bh * 0.5f + CoreliaConst::NameOffsetY);
		auto fs = KdFontManager::Instance().CreateFontTexture(CoreliaConst::FontNo, CoreliaConst::SpeakerName, false);
		sprite.DrawFont(fs, namePos, &edge, 0);
	}

	// 本文：SJIS を考慮して WrapChars 文字ごとに折り返し
	{
		const std::string& text = m_convoText;
		std::vector<std::string> lines;
		std::string cur;
		int cells = 0;
		for (size_t i = 0; i < text.size(); )
		{
			const unsigned char b = static_cast<unsigned char>(text[i]);
			const bool lead = (b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC);
			const int n = (lead && i + 1 < text.size()) ? 2 : 1;
			cur.append(text, i, n);
			i += n;
			++cells;
			if (cells >= CoreliaConst::WrapChars) { lines.push_back(cur); cur.clear(); cells = 0; }
		}
		if (!cur.empty()) { lines.push_back(cur); }

		const Math::Color textCol(1.0f, 1.0f, 1.0f, 1.0f);
		float ty = static_cast<float>(cy) + bh * 0.5f - CoreliaConst::TextPadY;
		const float tx = static_cast<float>(cx) - bw * 0.5f + CoreliaConst::TextPadX;
		for (const auto& ln : lines)
		{
			if (!ln.empty())
			{
				auto fs = KdFontManager::Instance().CreateFontTexture(CoreliaConst::FontNo, ln, false);
				sprite.DrawFont(fs, Math::Vector2(tx, ty), &textCol, 0);
			}
			ty -= CoreliaConst::LineHeight;
		}
	}

	// クローズ案内（右下）
	{
		const Math::Vector2 p(
			static_cast<float>(cx) + bw * 0.5f - 120.0f,
			static_cast<float>(cy) - bh * 0.5f + 24.0f);
		const Math::Color c(0.7f, 0.85f, 1.0f, 0.9f);
		sprite.DrawFont(p, &c, "%s", CoreliaConst::ClosePrompt);
	}
}

void GameScene::SpawnHealPlus(int count)
{
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float screenW = static_cast<float>(bb->GetInfo().Width);
	const float screenH = static_cast<float>(bb->GetInfo().Height);

	static unsigned int s_hpSeed = 0x9E3779B9u;
	auto frand = [&]() -> float { s_hpSeed = s_hpSeed * 1664525u + 1013904223u; return static_cast<float>((s_hpSeed >> 8) & 0xFFFFu) / 65535.0f; };

	// 指定位置から、小さな「＋」を縦に少しずつずらして複数生成（上昇していく）
	auto spawnStream = [&](float baseX, float baseY, float sizeMul)
	{
		const int cnt = HealConst::PlusCount + (count - 1);   // 取得数が多いほど少し増える
		for (int k = 0; k < cnt; ++k)
		{
			HealPlus hp;
			hp.pos     = { baseX + (frand() * 2.0f - 1.0f) * HealConst::PlusJitterX,
						   baseY + k * HealConst::PlusSpacing };          // 上に積み上げて昇る
			hp.maxLife = HealConst::PlusLife + k * HealConst::PlusLifeStagger;
			hp.life    = hp.maxLife;
			hp.size    = HealConst::PlusSize * sizeMul * (0.8f + 0.4f * frand());
			m_healPluses.push_back(hp);
		}
	};

	// 1) プレイヤー頭上：ワールド座標→スクリーン(中心原点・+Yが上)
	if (m_spPlayer)
	{
		const Math::Matrix vp = KdShaderManager::Instance().GetCameraCB().mView
							  * KdShaderManager::Instance().GetCameraCB().mProj;
		const Math::Vector3 head = m_spPlayer->GetPos() + m_spPlayer->GetUpDir() * HealConst::HeadOffsetUp;
		const Math::Vector4 cp = Math::Vector4::Transform(
			Math::Vector4(head.x, head.y, head.z, 1.0f), vp);
		if (cp.w > 0.0001f)
		{
			spawnStream(cp.x / cp.w * screenW * 0.5f, cp.y / cp.w * screenH * 0.5f, 1.0f);
		}
	}

	// 2) HP UI（左上のハート）位置にも＋ストリーム
	{
		const float S = static_cast<float>(UIConst::HpStarSize);
		spawnStream(-screenW * 0.5f + UIConst::HpMarginX + S * 0.5f,
					 screenH * 0.5f - UIConst::HpMarginY - S * 0.5f, 0.85f);
	}
}

//----------------------------------------------------------
// ステージクリア開始（ゴールコア取得＝即クリア）
//----------------------------------------------------------
void GameScene::StartStageClear(const Math::Vector3& corePos)
{
	if (m_clearActive) { return; }
	m_clearActive = true;
	m_clearTimer  = 0.0f;
	m_clearDecideFlashed = false;
	(void)corePos;

	if (m_spPlayer) { m_spPlayer->SetControlEnabled(false); }   // 操作ロック
	TriggerFlash(ClearConst::FlashStrength);                    // 取得の白フラッシュ
}

//----------------------------------------------------------
// 導入カットシーン（Stage1限定）：開始位置→スポーンへスクリプト移動で着地
//----------------------------------------------------------
void GameScene::StartIntroCutscene()
{
	if (!m_spPlayer) { return; }
	m_introCutscene = true;
	m_introTimer    = 0.0f;
	m_introSpin     = 0.0f;

	// 開始位置へ配置。物理を止めて（重力0・速度0）、位置を時間で直接動かす（横に吹っ飛ばない・必ず着地）
	m_spPlayer->SetPos(m_introStartPos);
	m_spPlayer->SetControlEnabled(false);
	m_spPlayer->SetVelocity(Math::Vector3::Zero);
	m_spPlayer->SetGravityScale(0.0f);
	m_spPlayer->SetIntroPose(true);          // パラソル等を隠す（落下中の見た目）
	m_spPlayer->SetCutsceneTumble(Math::Vector3::Zero);
}

void GameScene::UpdateIntroCutscene()
{
	if (!m_spPlayer) { m_introCutscene = false; m_introLanding = false; return; }

	const float dt = KdFPSController::GetDt();

	// ── 降下フェーズ：開始位置→スポーンへ ──
	if (m_introCutscene)
	{
		m_introTimer += dt;
		const float dur = (IntroConst::Duration > 0.0f) ? IntroConst::Duration : 1.0f;
		const float p   = std::clamp(m_introTimer / dur, 0.0f, 1.0f);

		// 横は等速、縦はsmoothstep（最後やわらかく接地）
		const float ph = p;
		const float pv = p * p * (3.0f - 2.0f * p);
		Math::Vector3 pos;
		pos.x = std::lerp(m_introStartPos.x, m_spawnPos.x, ph);
		pos.z = std::lerp(m_introStartPos.z, m_spawnPos.z, ph);
		pos.y = std::lerp(m_introStartPos.y, m_spawnPos.y, pv);
		m_spPlayer->SetPos(pos);
		m_spPlayer->SetVelocity(Math::Vector3::Zero);

		// 頭から落下→着地直前に頭を上へ起こす（XY平面の落下なのでZロールで姿勢を作る）
		Math::Vector3 fall2D = m_spawnPos - m_introStartPos; fall2D.z = 0.0f;
		if (fall2D.LengthSquared() > 1e-6f) { fall2D.Normalize(); }
		else { fall2D = { 0.0f, -1.0f, 0.0f }; }
		const float headRoll = std::atan2f(-fall2D.x, fall2D.y);   // 頭(+Y)を落下方向へ向ける角度
		float ru = (p < IntroConst::UprightStart)
			? 0.0f : (p - IntroConst::UprightStart) / (1.0f - IntroConst::UprightStart);
		ru = std::clamp(ru, 0.0f, 1.0f);
		const float rue  = ru * ru * (3.0f - 2.0f * ru);            // smoothstep
		const float roll = std::lerp(headRoll, 0.0f, rue);          // 頭から → 頭上へ
		m_spPlayer->SetCutsceneTumble(Math::Vector3(0.0f, 0.0f, roll));

		if (p >= 1.0f)
		{
			// 接地！ → ズサー…バタンの着地リアクションへ
			m_introCutscene  = false;
			m_introLanding   = true;
			m_introLandTimer = 0.0f;

			// 横滑り方向＝水平の進行方向
			Math::Vector3 d = m_spawnPos - m_introStartPos; d.y = 0.0f;
			if (d.LengthSquared() > 1e-6f) { d.Normalize(); }
			m_landSkidDir = d;

			m_spPlayer->SetPos(m_spawnPos);
			m_spPlayer->SetVelocity(Math::Vector3::Zero);
			m_spPlayer->SetCutsceneSpin(0.0f);
			m_spPlayer->SetCutsceneTumble(Math::Vector3::Zero);
			// パラソルは引きずり(スキッド)中も隠したまま。操作復帰時に戻す
			m_spPlayer->TriggerLandingSquash();       // バタン（つぶれ）
			if (m_pCamera) { m_pCamera->TriggerShake(IntroConst::LandShake); }  // 着地の揺れ
			TriggerFlash(IntroConst::LandFlash);
			m_airTime = 0.0f;
		}
		return;
	}

	// ── 着地リアクション：ズサーっと前へ滑って止まる ──
	if (m_introLanding)
	{
		m_introLandTimer += dt;
		const float lt = std::clamp(m_introLandTimer / IntroConst::LandRecoverTime, 0.0f, 1.0f);
		const float eo = 1.0f - (1.0f - lt) * (1.0f - lt);   // easeOut（最初速く滑って減速）
		Math::Vector3 pos = m_spawnPos + m_landSkidDir * (IntroConst::SkidDist * eo);
		pos.y = m_spawnPos.y;
		m_spPlayer->SetPos(pos);
		m_spPlayer->SetVelocity(Math::Vector3::Zero);

		if (lt >= 1.0f)
		{
			// 復帰：死→復活と同じ状態に通す（最初のスポーンでも惑星に正しく捕まるように）
			m_introLanding = false;
			const Math::Vector3 landedPos = m_spPlayer->GetPos();
			m_spPlayer->Revive();              // 状態を完全初期化（操作ON・速度0・Idle・パラソル復帰）
			m_spPlayer->SetPos(landedPos);     // 着地位置は維持
			m_spPlayer->SetGravityScale(1.0f);
			m_airTime = 0.0f;
		}
	}
}

void GameScene::Respawn()
{
	if (!m_spPlayer) { return; }
	// モデルを作り直さず状態だけ初期化（Init だと復活後に表示・入力が壊れることがある）
	m_spPlayer->Revive();
	// チェックポイント未取得なら初期スポーン、取得済みならそのチェックポイントへ
	const Math::Vector3 respawn = m_checkpointReached ? m_respawnPos : m_spawnPos;
	// リスポーン位置へ移動し、死亡時の落下速度を必ず消す（残っていると復活直後に吹っ飛ぶ）
	m_spPlayer->SetPos(respawn);
	m_spPlayer->SetVelocity(Math::Vector3::Zero);
	// 落下死タイマーをリセット（空中で死んだ場合に復活直後また落下死扱いになるのを防ぐ）
	m_airTime = 0.0f;

	// ── ワールドを初期状態へ戻す（敵・ギミック・アイテムを全部リセット）──
	// 倒した敵は復活、移動床などは初期位置、取得したコイン・落ちている岩は元通り。
	RebuildEnemies();
	RebuildMovingFloors();
	RebuildWindBoxes();
	RebuildGravityCores();
	RebuildSpikeBoxes();

	// アイテム：いったん全消し（Load は追記なので必ずクリアしてから）→ 初期配置を読み直す
	m_itemManager.ClearCoins();
	m_itemManager.ClearRocks();
	m_itemManager.ClearParasols();
	m_itemManager.Load();
	m_itemManager.LoadParasols();

	// コインが復活するのに合わせて取得カウントも戻す
	m_coinTotal    = 0;
	m_coinPopTimer = 0.0f;
}

void GameScene::RebuildEnemies()
{
	// 既存の敵をシーンから除去
	for (auto& e : m_enemies) { e->Expire(); }
	m_enemies.clear();
	for (auto& c : m_cubuns)  { c->Expire(); }
	m_cubuns.clear();

	// EnemyPlacementEditor のデータから敵を生成
	for (const auto& data : m_enemyEditor.GetPlacements())
	{
		if (data.type == EnemyType::Cubun)
		{
			auto sp = std::make_shared<Cubun>();
			sp->SetPos(data.position);
			sp->SetInitGravDir(data.initGravDir);
			sp->Init();
			if (m_spPlayer) { sp->SetTarget(m_spPlayer); }
			m_cubuns.push_back(sp);
			AddObject(sp);
		}
		else
		{
			auto sp = std::make_shared<EnemyRanged>();
			sp->SetPos(data.position);
			sp->Init();
			if (m_spPlayer) { sp->SetTarget(m_spPlayer); }
			m_enemies.push_back(sp);
			AddObject(sp);
		}
	}

	// プレイヤーに Cubun 障害物リストを登録（めり込み防止押し出し用）
	if (m_spPlayer)
	{
		std::vector<std::weak_ptr<KdGameObject>> obstList;
		obstList.reserve(m_cubuns.size());
		for (auto& sp : m_cubuns) { obstList.push_back(sp); }
		m_spPlayer->SetEnemyObstacles(obstList);
	}
}

void GameScene::RebuildCheckpoints()
{
	// 既存チェックポイントを除去
	for (auto& cp : m_checkpoints) { cp->Expire(); }
	m_checkpoints.clear();

	for (const auto& pos : m_checkpointEditor.GetPositions())
	{
		auto cp = std::make_shared<Checkpoint>();
		cp->SetPos(pos);
		cp->SetPlayer(m_spPlayer);
		m_checkpoints.push_back(cp);
		AddObject(cp);
	}
}

void GameScene::RebuildWarpHoles()
{
	for (auto& wh : m_warpHoles) { wh->Expire(); }
	m_warpHoles.clear();

	for (const auto& data : m_warpHoleEditor.GetHoles())
	{
		auto sp = std::make_shared<WarpHole>(data);
		sp->Init();
		m_warpHoles.push_back(sp);
		AddObject(sp);
	}
}

void GameScene::SpawnGoalWarpHole(const Math::Vector3& pos)
{
	// コア位置を入口に、ゴール用の WarpHole を動的生成して開く（Enabled=true）
	WarpHoleData d;
	d.EntryPos = pos;
	d.ExitPos  = pos + Math::Vector3{ 0.0f, GravityCoreConst::GoalWarpExitUp, 0.0f };
	d.Enabled  = true;

	auto sp = std::make_shared<WarpHole>(d);
	sp->Init();
	m_warpHoles.push_back(sp);
	AddObject(sp);
}

void GameScene::RebuildMovingFloors()
{
	for (auto& mf : m_movingFloors) { mf->Expire(); }
	m_movingFloors.clear();

	for (const auto& data : m_movingFloorEditor.GetFloors())
	{
		auto sp = std::make_shared<MovingFloor>();
		sp->SetData(data);
		sp->Init();
		m_movingFloors.push_back(sp);
		AddObject(sp);
	}

	// プレイヤー・敵に移動床リストを渡す
	std::vector<std::weak_ptr<MovingFloor>> wpList;
	wpList.reserve(m_movingFloors.size());
	for (auto& sp : m_movingFloors) { wpList.push_back(sp); }

	if (m_spPlayer) { m_spPlayer->SetMovingFloorObjects(wpList); }
	for (auto& sp : m_enemies)  { sp->SetMovingFloorObjects(wpList); }
	for (auto& sp : m_cubuns)   { sp->SetMovingFloorObjects(wpList); }
}

void GameScene::RebuildWindBoxes()
{
	for (auto& wb : m_windBoxes) { wb->Expire(); }
	m_windBoxes.clear();

	for (const auto& data : m_windBoxEditor.GetBoxes())
	{
		auto sp = std::make_shared<WindBox>();
		sp->Init(data);
		m_windBoxes.push_back(sp);
		AddObject(sp);
	}

	// プレイヤー・敵に WindBox コライダーリストを登録
	std::vector<std::weak_ptr<KdGameObject>> wpList;
	wpList.reserve(m_windBoxes.size());
	for (const auto& sp : m_windBoxes) { wpList.push_back(sp); }

	if (m_spPlayer) { m_spPlayer->SetWindBoxObjects(wpList); }
	for (auto& sp : m_enemies) { sp->SetWindBoxObjects(wpList); }
	for (auto& sp : m_cubuns)  { sp->SetWindBoxObjects(wpList); }
}

void GameScene::RebuildSpikeBoxes()
{
	for (auto& sb : m_spikeBoxes) { sb->Expire(); }
	m_spikeBoxes.clear();

	for (const auto& data : m_spikeBoxEditor.GetBoxes())
	{
		auto sp = std::make_shared<SpikeBox>();
		sp->Init(data);
		m_spikeBoxes.push_back(sp);
		AddObject(sp);
	}

	// プレイヤー・敵に SpikeBox コライダーリストを登録（押し出し用）
	std::vector<std::weak_ptr<KdGameObject>> wpList;
	wpList.reserve(m_spikeBoxes.size());
	for (const auto& sp : m_spikeBoxes) { wpList.push_back(sp); }

	if (m_spPlayer) { m_spPlayer->SetSpikeBoxObjects(wpList); }
	for (auto& sp : m_enemies) { sp->SetSpikeBoxObjects(wpList); }
	for (auto& sp : m_cubuns)  { sp->SetSpikeBoxObjects(wpList); }
}

void GameScene::RebuildGravityCores()
{
	for (auto& gc : m_gravityCores) { gc->Expire(); }
	m_gravityCores.clear();

	for (const auto& data : m_gravityCoreEditor.GetCores())
	{
		if (!data.enabled) { continue; }
		auto sp = std::make_shared<GravityCore>();
		sp->Init(data.pos, data.radius, data.type);
		m_gravityCores.push_back(sp);
		AddObject(sp);
	}
}

GameScene::~GameScene()
{
	// 破棄後にKdDebugGUIが死んだthisのDrawGui()を呼ぶのを防ぐ（StageSelect等への遷移時のクラッシュ対策）
	KdDebugGUI::Instance().ClearGuiCallback();
	KdDebugGUI::Instance().SetGameViewport(false);
}

void GameScene::Init()
{
	// カメラ（BaseSceneのm_cameraに所有権を渡し、観察用ポインタだけ保持）
	auto spCamera  = std::make_shared<SideScrollCamera>();
	m_pCamera      = spCamera.get();
	m_camera       = spCamera;

	// プレイヤー
	m_spPlayer = std::make_shared<Player>();
	LoadSpawn();
	m_spPlayer->SetPos(m_spawnPos);
	AddObject(m_spPlayer);

	// リスポーン座標の初期値をスポーン座標と揃える
	m_respawnPos = m_spawnPos;

	// 惑星をこのステージのマップへ先に読み込む（導入カットシーンの着地判定に必要）
	PlanetGravityManager::Instance().Load();
	PlanetGravityManager::Instance().MarkWorldDirty();

	// 導入カットシーン（Stage1だけ：上空から降下→着地で操作開始）
	// ただし惑星（着地できる地面）が1つも無いステージではスキップ（空中で固定カメラが止まるため）
	if (StageManager::Instance().GetStageIndex() == IntroConst::Stage &&
		!PlanetGravityManager::Instance().GetPlanets().empty())
	{
		StartIntroCutscene();
	}

	// スカイボックス背景
	auto spBG = std::make_shared<BackGround>();
	AddObject(spBG);

	// 星空（スカイボックス内にランダム配置・ブルームで発光）
	auto spStarField = std::make_shared<StarField>();
	AddObject(spStarField);

	// デフォルトのポイントライトを1個配置
	{
		auto spLight = std::make_shared<PointLightObject>();
		spLight->SetPos({ 0.0f, 10.0f, 0.0f });
		m_pointLights.push_back(spLight);
		AddObject(spLight);
	}

	// HP UI
	m_spHpUI = std::make_shared<HpUI>();
	m_spHpUI->SetPlayer(m_spPlayer);
	AddObject(m_spHpUI);


	// ルームエディター初期化・読込
	m_roomEditor.Load();
	if (m_roomEditor.GetRooms().empty())
	{
		// CSVがない場合のデフォルトルーム
		m_rooms.clear();
		m_rooms.push_back({ -10.0f, 10.0f, 0.0f, 10.0f,  9.5f, 3.0f });
		m_rooms.push_back({  10.0f, 30.0f, 0.0f, 10.0f, 29.5f, 3.0f });
		m_rooms.push_back({  20.0f, 20.0f, 5.0f,  5.0f, FLT_MAX, 0.0f });
		m_roomEditor.SetRooms(m_rooms);
	}
	m_roomEditor.ClearDirty();

	// （惑星は導入カットシーン判定のため上で読込済み）

	// 手動重力ゾーン読み込み
	ManualGravityZoneManager::Instance().Load();
	DeadZoneManager::Instance().Load();

	// コアリア（ヒントNPC）：日本語フォント登録＋配置/ヒント読み込み
	KdFontManager::Instance().AddFont(CoreliaConst::FontNo, CoreliaConst::FontName, CoreliaConst::FontHeight);
	CoreliaManager::Instance().Load();
	m_rooms = m_roomEditor.GetRooms();
	m_pCamera->SetRooms(m_rooms);

	// 敵配置エディター（Loadはコンストラクタ内で実行済み）→敵を生成
	if (m_enemyEditor.IsDirty())
	{
		RebuildEnemies();
		m_enemyEditor.ClearDirty();
	}

	// チェックポイントエディター → チェックポイント生成
	if (m_checkpointEditor.IsDirty())
	{
		RebuildCheckpoints();
		m_checkpointEditor.ClearDirty();
	}

	// ワープホールエディター → ワープホール生成
	m_warpHoleEditor.Load();
	RebuildWarpHoles();
	m_warpHoleEditor.ClearDirty();

	// カメラ設定読込
	CameraSettings::Instance().Load();

	// ライト設定
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();

	// 保存済みの太陽光設定を読み込んで反映（無ければ既定値のまま）
	LoadSunLight();
	ApplySunLight();

	// 影（シャドウマップ）の描画範囲を広げる
	ambient.SetDirLightShadowArea(LightConst::ShadowAreaSize, LightConst::ShadowAreaHeight);

	// ポーズメニュー用フォント
	KdFontManager::Instance().AddFont(PauseMenuConst::FontNo, PauseMenuConst::FontName, PauseMenuConst::FontHeight);

	// クリア暗転用アイリスマスク（マリオ風の閉じる円）
	m_irisMaskTex = std::make_shared<KdTexture>();
	m_irisMaskTex->Load(ClearConst::IrisMaskPath);

	// コアリア残機アイコン（仮画像）を読み込み
	m_lifeIconTex = std::make_shared<KdTexture>();
	m_lifeIconTex->Load(UIConst::LifeIconPath);
	// 減る瞬間に散る星屑
	m_lifeStarTex = std::make_shared<KdTexture>();
	m_lifeStarTex->Load(UIConst::LifeStarPath);
	// コイン（収集物）カウンターのアイコン（仮）
	m_coinIconTex = std::make_shared<KdTexture>();
	m_coinIconTex->Load(UIConst::CoinIconPath);
	// HP の星（仮）
	m_hpStarTex = std::make_shared<KdTexture>();
	m_hpStarTex->Load(UIConst::HpStarPath);

	// HP UI を常時表示するためゲーム開始時からコールバックをセット
	KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });

	// 移動床エディター → 移動床生成
	RebuildMovingFloors();
	m_movingFloorEditor.ClearDirty();

	// 風ボックスエディター → 風ボックス生成
	RebuildWindBoxes();
	m_windBoxEditor.ClearDirty();

	// 重力コアエディター → 重力コア生成
	RebuildGravityCores();
	m_gravityCoreEditor.ClearDirty();

	// 棘ボックスエディター → 棘ボックス生成
	RebuildSpikeBoxes();
	m_spikeBoxEditor.ClearDirty();

	// コイン・パラソルアイテム読み込み
	m_itemManager.Load();
	m_itemManager.LoadParasols();

	}

void GameScene::SaveSpawn()
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("spawn_settings.csv"));
	if (!ofs) { return; }
	// spawn(xyz) + 導入の飛んでくる開始位置(xyz)
	ofs << m_spawnPos.x      << "," << m_spawnPos.y      << "," << m_spawnPos.z      << ","
		<< m_introStartPos.x << "," << m_introStartPos.y << "," << m_introStartPos.z << "\n";
}

void GameScene::LoadSpawn()
{
	std::ifstream ifs(StageManager::Instance().ResolvePath("spawn_settings.csv"));
	if (!ifs) { return; }

	std::string line;
	if (!std::getline(ifs, line)) { return; }

	std::istringstream ss(line);
	std::string token;
	// 0-2: spawn, 3-5: intro start。旧フォーマット(3個)なら intro はデフォルト維持
	float vals[6] = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ,
					  m_introStartPos.x, m_introStartPos.y, m_introStartPos.z };
	int i = 0;
	while (std::getline(ss, token, ',') && i < 6)
	{
		vals[i++] = std::stof(token);
	}
	m_spawnPos      = { vals[0], vals[1], vals[2] };
	m_introStartPos = { vals[3], vals[4], vals[5] };
}

void GameScene::ApplySunLight()
{
	auto& ambient = KdShaderManager::Instance().WorkAmbientController();

	Math::Vector3 dir = m_sunDir;
	dir.Normalize();
	ambient.SetDirLight(dir, m_sunColor);
	ambient.SetAmbientLight(m_ambientColor);
}

void GameScene::SaveSunLight()
{
	std::ofstream ofs(StageManager::Instance().ResolvePath("sun_light.csv"));
	if (!ofs) { return; }

	// 方向(xyz), 太陽色(rgb), 環境光(rgba)
	ofs << m_sunDir.x   << "," << m_sunDir.y   << "," << m_sunDir.z   << ","
		<< m_sunColor.x << "," << m_sunColor.y << "," << m_sunColor.z << ","
		<< m_ambientColor.x << "," << m_ambientColor.y << ","
		<< m_ambientColor.z << "," << m_ambientColor.w << "\n";
}

void GameScene::LoadSunLight()
{
	std::ifstream ifs(StageManager::Instance().ResolvePath("sun_light.csv"));
	if (!ifs) { return; }

	std::string line;
	if (!std::getline(ifs, line)) { return; }

	std::istringstream ss(line);
	std::string token;
	float vals[10];
	int i = 0;
	while (std::getline(ss, token, ',') && i < 10)
	{
		vals[i++] = std::stof(token);
	}
	if (i < 10) { return; } // データが不足していれば既定値のまま

	m_sunDir       = { vals[0], vals[1], vals[2] };
	m_sunColor     = { vals[3], vals[4], vals[5] };
	m_ambientColor = { vals[6], vals[7], vals[8], vals[9] };
}

//----------------------------------------------------------
// WarpHole通過完了時にカメラのZ基準を ExitDir から更新する
// ExitDir の Z 成分が正 → プレイヤーがZ+方向へ飛び出た → カメラを前にずらす
// ExitDir の Z 成分が負 → Z- 方向 → カメラを後ろ
// Z 成分がほぼ 0    → 元の OffsetZ を維持（オーバーライドなし）
//----------------------------------------------------------
void GameScene::UpdateCameraZFromExitDir(const Math::Vector3& exitDir)
{
	if (!m_pCamera) { return; }

	const auto& cs = CameraSettings::Instance();
	constexpr float kZThreshold = 0.3f;      // Z方向とみなす閾値
	constexpr float kZShiftAmount = 8.0f;    // 基準Zのシフト量

	if (std::fabsf(exitDir.z) >= kZThreshold)
	{
		// ExitDirのZ符号に応じてカメラZをシフト
		const float newZ = cs.OffsetZ - exitDir.z * kZShiftAmount;
		m_pCamera->SetOffsetZOverride(newZ);
	}
	else
	{
		// Z方向への移動でなければオーバーライドを解除して標準に戻す
		m_pCamera->ClearOffsetZOverride();
	}
}

