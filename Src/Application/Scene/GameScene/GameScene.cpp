#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Util/DebugFlags.h"
#include"../../Const/LightConst.h"
#include"../../Const/JuiceConst.h"
#include"../../Const/HealConst.h"
#include"../../Const/CoreliaConst.h"
#include"../../Const/GravityArrowConst.h"
#include"../../Const/EditorPickConst.h"
#include"../../Const/IntroConst.h"
#include"../../Const/ClearConst.h"
#include"../../Const/FontConst.h"
#include"../../Const/StageSelectConst.h"
#include"../../Const/ItemConst.h"
#include"../../Const/ItemMagnetConst.h"
#include"../../Camera/CameraSettings.h"
#include"../../Manager/ModelManager.h"
#include"../../Manager/StageManager.h"
#include"../../Manager/SoundManager.h"
#include"../../Manager/CursorManager.h"
#include"../../Const/SoundConst.h"
#include"../../Util/TextFx.h"
#include"../../Util/CoreIcon.h"
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
	// デバッグ表示フラグをグローバルへ反映（オブジェクト側のDrawDebugもこれで一括ON/OFF）。
	// 早期returnより前に必ず実行する。撮影モードでは一切表示しない。
	DebugFlags::g_debugDrawVisible = m_debugZonesVisible && !m_photoMode;

	// 操作説明（A/D/Spaceを全部押したら消える）。早期returnより前で毎フレーム更新。
	UpdateControlHint();

	// ショーケース中・クリア演出中は操作不要なので自前カーソルを非表示にする。
	// ただしポーズメニュー中はメニューをクリック操作するためカーソルを出す。
	CursorManager::Instance().SetSuppressed(
		!m_menuOpen && (m_clearActive || m_showcaseState != ShowcaseState::Off));

	// ── BGM：飛来イントロ中・ショーケース中は流さない。着地して通常プレイになったら流す ──
	//   ※ショーケースは下のブロックで早期returnするため、BGM管理は必ずその前で行う。
	//     PlayBGM は同曲なら何もしないので毎フレーム呼んでOK（全ステージ共通）。
	{
		if (m_clearActive)
		{
			// クリア時はステージ曲を止めてクリアSEを聴かせる
			SoundManager::Instance().StopBGM();
		}
		else if (m_introCutscene || m_introLanding)
		{
			// 「飛んでくる」演出中は無音（StageFlyInのSEを聴かせ、着地でステージBGM開始）
			SoundManager::Instance().StopBGM();
		}
		else if (m_showcaseState != ShowcaseState::Off)
		{
			// ステージ紹介カメラ中は専用BGM（1曲）
			SoundManager::Instance().PlayBGM(SoundConst::BgmShowcase, SoundConst::BgmVolume);
		}
		else
		{
			const int stageId0 = StageManager::Instance().GetStageIndex() - 1;
			SoundManager::Instance().PlayBGM(SoundConst::BgmForStage(stageId0), SoundConst::BgmVolume);
		}
	}

	// ── ステージ見せカメラ：フライスルー → 黒帯が中央へ閉じて暗転 → 開いてゲームへ ──
	{
		const float dt = KdFPSController::GetDt();
		if (m_showcaseState == ShowcaseState::Playing)
		{
			// 黒帯をシネマ位置までスライドイン
			m_barCover = std::min(m_barCover + ShowcaseCamConst::BarInSpeed * dt,
				ShowcaseCamConst::LetterboxFrac);
			KdEffekseerManager::GetInstance().Update();
			UpdateShowcaseWorld();   // 世界（敵/動く床/コア等）は動かす（プレイヤー除く）
			UpdateShowcaseCam();   // カメラ移動。終了/スキップで Closing へ
			return;
		}
		if (m_showcaseState == ShowcaseState::Closing)
		{
			// 最後のカメラを保持したまま、黒帯を中央へ閉じて暗転
			KdEffekseerManager::GetInstance().Update();
			UpdateShowcaseWorld();
			if (m_camera) { m_camera->SetCameraMatrix(m_showcaseLastWorld); }
			m_barCover += ShowcaseCamConst::CloseSpeed * dt;
			if (m_barCover >= 0.5f)
			{
				m_barCover = 0.5f;                       // 完全な黒
				m_showcaseState = ShowcaseState::Opening;
				if (m_introPending) { StartIntroCutscene(); }  // 暗転下でカメラをカット
			}
			return;
		}
		if (m_showcaseState == ShowcaseState::Opening)
		{
			// 黒帯を開いてゲーム画面へ（ここはゲーム進行を止めない）
			m_barCover -= ShowcaseCamConst::OpenSpeed * dt;
			if (m_barCover <= 0.0f)
			{
				m_barCover = 0.0f;
				m_showcaseState = ShowcaseState::Off;
				if (m_spPlayer) { m_spPlayer->SetDamageEnabled(true); }   // 通常のダメージへ戻す
			}
		}
	}

	// ── ポーズメニュー（TAB で開閉。ESC はアプリ終了に使われるため使わない）──
	{
		const bool tab = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
		// 設定ウィンドウが開いている間は TAB を設定側に渡す（ポーズは閉じない）
		if (tab && !m_menuEscPrev && !m_settingsMenu.IsOpen())
		{
			m_menuOpen = !m_menuOpen; m_menuIndex = 0;
			// 再生中のSEを一時停止/再開（開く時はSE停止→開閉SEを鳴らす順に）
			if (m_menuOpen) { SoundManager::Instance().PauseAllSE(); }
			else            { SoundManager::Instance().ResumeAllSE(); }
			// 開閉SE
			SoundManager::Instance().PlaySE(m_menuOpen ? SeId::PauseOpen : SeId::PauseClose,
				SoundConst::SeVolume);
			// ポーズ中はBGMをこもらせつつ音量も下げる（解除で元に戻す）
			SoundManager::Instance().SetBgmMuffle(m_menuOpen);
			SoundManager::Instance().SetBgmVolumeScale(
				m_menuOpen ? SoundConst::PauseBgmVolumeScale : 1.0f);
		}
		m_menuEscPrev = tab;
	}

	// ── デバッグ可視化トグル（1キー）：ManualZone以外の判定表示をON/OFF ──
	{
		const bool k1 = (GetAsyncKeyState('1') & 0x8000) != 0;
		if (k1 && !m_debugKeyPrev) { m_debugZonesVisible = !m_debugZonesVisible; }
		m_debugKeyPrev = k1;
	}

	// ── デバッグ：Rキーでセーブデータをリセット（初回起動フラグ・合計・ステージ記録）──
	{
		const bool kR = (GetAsyncKeyState('R') & 0x8000) != 0;
		if (kR && !m_resetKeyPrev)
		{
			StageManager::Instance().ResetSaveData();
			KdDebugGUI::Instance().AddLog("[SAVE] セーブデータをリセットしました\n");
		}
		m_resetKeyPrev = kR;
	}

	// ── デバッグ：F10キーでステージクリアを即発火（クリア演出→リザルトへ）──
	{
		const bool kClear = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
		if (kClear && !m_debugClearPrev && !m_clearActive && m_spPlayer)
		{
			StartStageClear(m_spPlayer->GetPos());
			KdDebugGUI::Instance().AddLog("[DEBUG] ステージクリア（F10）\n");
		}
		m_debugClearPrev = kClear;
	}

	// ── エディタ画面トグル（F3）：ゲームをImGuiウィンドウ表示 ⇔ 通常フルスクリーン ──
	{
		const bool k3 = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
		if (k3 && !m_editorKeyPrev) { m_editorScreen = !m_editorScreen; }
		m_editorKeyPrev = k3;
	}
	// エディタ画面ON時のみImGui表示（OFFで全ImGui非表示＝通常フルスクリーン）。Eventで毎フレーム反映
	KdDebugGUI::Instance().SetGameViewport(m_editorScreen);

	// プレイタイム計測（通常プレイ中のみ。ポーズ/会話/演出/死亡中は止める）
	if (!IsUpdatePaused() && !m_introCutscene && !m_introLanding && !m_deathActive)
	{
		m_playTime += KdFPSController::GetDt();
	}

	// 重力矢印の流れ（重力方向へスクロール）
	m_gravArrowScroll += KdFPSController::GetDt();
	if (m_gameOverActive) { UpdateGameOver(); return; }
	if (m_menuOpen) { UpdatePauseMenu(); return; }

	// ── コアリア会話：会話中は世界を止めてカメラを寄せる ──
	UpdateCorelia();
	if (m_convoActive) { return; }

	// ── 導入カットシーン（Stage1：落下きりもみ→ズサーバタン着地）──
	if (m_introCutscene || m_introLanding) { UpdateIntroCutscene(); }

	// ── 着地後「〜にやってきた！」バナーの時間進行 ──
	if (m_arrivalTimer >= 0.0f)
	{
		m_arrivalTimer += KdFPSController::GetDt();
		if (m_arrivalTimer > IntroConst::ArrivalShowTime) { m_arrivalTimer = -1.0f; }
	}

	// ── ステージクリア演出（ゴールコア取得→周回カメラ→暗転→StageSelectへ）──
	if (m_clearActive)
	{
		m_clearTimer += KdFPSController::GetDt();

		// クリア中はオブジェクト更新が止まるので、プレイヤーのアニメ/姿勢だけ手動で駆動
		// （物理は動かさない＝その場で GetGravityCore ポーズ再生＋drawWorld更新）。
		if (m_spPlayer)
		{
			m_spPlayer->UpdateClearPose(KdFPSController::GetDt());
			// 取得したコアを GravityCoreBorn ボーンへ追従＝手に持っているように見せる
			if (m_spHeldCore)
			{
				m_spHeldCore->Update();   // グロー/星エフェクトを進める（クリア中は通常更新が止まるため）
				const Math::Matrix bw = m_spPlayer->GetBoneWorld("GravityCoreBorn");
				m_spHeldCore->SetPos(Math::Vector3{ bw._41, bw._42, bw._43 });
			}
		}

		// クリア中はオブジェクト更新(PostUpdate)が止まるので、エフェクトを手動で進める
		KdEffekseerManager::GetInstance().Update();
		m_itemManager.UpdatePickupEffects();   // キメの星バーストを進める

		// スター取得風カメラ：旋回→（決め）少し引く→ズーム→少し保持→もう一度引く（全部連続）
		if (m_camera && m_spPlayer)
		{
			constexpr float kPi = 3.14159265f;
			const float tt = m_clearTimer;

			// 回転：オービットを easeOutBack でスイープ＝「旋回してきてドリフト」（この仕様は維持）。
			const float yp = std::clamp(tt / ClearConst::CamOrbitTime, 0.0f, 1.0f);
			const float c1 = ClearConst::CamOrbitOvershoot;
			const float c3 = c1 + 1.0f;
			const float pm = yp - 1.0f;
			const float yawT = 1.0f + c3 * pm * pm * pm + c1 * pm * pm;   // easeOutBack
			const float yawDeg = ClearConst::CamStartYawDeg + ClearConst::CamTotalOrbitDeg * yawT;


			// フレーミング進行 ez：旋回中に最終フレーミングまで寄せ切る（smootherstep で 0→1）
			const float op = std::clamp(tt / ClearConst::CamOrbitTime, 0.0f, 1.0f);
			const float ez = op * op * op * (op * (op * 6.0f - 15.0f) + 10.0f);   // smootherstep
			const float pitchDeg = std::lerp(ClearConst::CamStartPitchDeg, ClearConst::CamEndPitchDeg, ez);
			const float focusUp  = std::lerp(ClearConst::CamStartFocusUp,  ClearConst::CamEndFocusUp,  ez);

			// キメズームの着地で小フラッシュ＋コア中心に星バースト（1回だけ）
			if (!m_clearDecideFlashed && tt >= ClearConst::CamZoomInEnd)
			{
				TriggerFlash(ClearConst::DecideFlash);
				m_clearDecideFlashed = true;

				if (m_spHeldCore)
				{
					const Math::Color burstCol(
						GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
						GravityCoreConst::GlowFaceB, 1.0f);
					m_itemManager.SpawnBurstAt(m_spHeldCore->GetPos(), burstCol, PickupBurst::Style::Ring);
				}
			}

			// 距離：旋回で寄せる → 旋回ドリフト直後にイージングで一気にキメズーム → 1秒止め → 引き
			float distV;
			if (tt <= ClearConst::CamOrbitTime)
			{
				distV = std::lerp(ClearConst::CamStartDist, ClearConst::CamEndDist, ez); // 旋回で寄ってくる
			}
			else if (tt <= ClearConst::CamZoomInEnd)
			{
				// キメズーム：旋回直後に smootherstep で一気に寄る
				const float zp = std::clamp(
					(tt - ClearConst::CamOrbitTime) / (ClearConst::CamZoomInEnd - ClearConst::CamOrbitTime), 0.0f, 1.0f);
				const float ze = zp * zp * zp * (zp * (zp * 6.0f - 15.0f) + 10.0f);
				distV = std::lerp(ClearConst::CamEndDist, ClearConst::CamZoomInDist, ze);
			}
			else if (tt <= ClearConst::CamStopEnd)
			{
				distV = ClearConst::CamZoomInDist;   // 1秒止め（キメのあと完全静止）
			}
			else
			{
				// 引き：カット直前まで止まらず引き続ける（マリギャラ風）。全区間を easeOut で連続移動。
				const float pp = std::clamp(
					(tt - ClearConst::CamStopEnd) / (ClearConst::CamPullEnd - ClearConst::CamStopEnd), 0.0f, 1.0f);
				const float u = 1.0f - pp;
				const float e = 1.0f - u * u;   // easeOut（止めから出て、最後まで動き続ける）
				distV = std::lerp(ClearConst::CamZoomInDist, ClearConst::FadePullbackDist, e);
			}

			// 傾き(roll, 度)：ドリフト〜止めの間は軽く傾け → 引きで水平へ
			float roll = 0.0f;
			if (tt <= ClearConst::CamStopEnd)
			{
				roll = ClearConst::DecideTiltSnapDeg;
			}
			else
			{
				const float bp = std::clamp(
					(tt - ClearConst::CamStopEnd) / (ClearConst::CamPullEnd - ClearConst::CamStopEnd), 0.0f, 1.0f);
				roll = ClearConst::DecideTiltSnapDeg * (1.0f - bp * bp * (3.0f - 2.0f * bp));
			}

			m_spPlayer->SetCutsceneFaceZ(tt >= ClearConst::CamOrbitTime);

			const float pitch = DirectX::XMConvertToRadians(pitchDeg);
			const float yaw   = DirectX::XMConvertToRadians(yawDeg);
			const float cp    = std::cosf(pitch);

			// キメに向かってカメラを上げる：focusUp を kime ズーム中に加算（その後保持）
			float kimeRaise = 0.0f;
			if (tt > ClearConst::CamOrbitTime)
			{
				const float kp = std::clamp(
					(tt - ClearConst::CamOrbitTime) / (ClearConst::CamZoomInEnd - ClearConst::CamOrbitTime), 0.0f, 1.0f);
				kimeRaise = ClearConst::CamKimeRaise * (kp * kp * (3.0f - 2.0f * kp));   // smoothstep
			}
			const Math::Vector3 focus = m_spPlayer->GetPos()
				+ Math::Vector3(0.0f, focusUp + kimeRaise, ClearConst::CamFocusZAdd);
			Math::Vector3 eye = focus + Math::Vector3(
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
					// 死亡回数が上限に達した → ゲームオーバー画面を出す（暗転したまま）
					m_gameOverActive  = true;
					m_gameOverIndex   = 0;
					m_gameOverTimer   = 0.0f;
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
			SoundManager::Instance().PlaySE(SeId::Death, SoundConst::SeVolume);
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

	// F4で撮影モードのトグル（自由移動カメラ＋ワイヤー一切非表示）。エディタモード中は無効。
	const bool f4Now = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
	if (f4Now && !m_f4Prev && !m_editorMode)
	{
		m_photoMode = !m_photoMode;
		if (m_photoMode)
		{
			// 自由カメラへ切替（SideScrollCamera は破棄するので観察ポインタを先に null に）
			m_pCamera        = nullptr;
			auto upPhotoCam  = std::make_unique<EditorCamera>();
			m_pEditorCam     = upPhotoCam.get();
			if (m_spPlayer)
			{
				Math::Vector3 p = m_spPlayer->GetPos();
				p.z -= 15.0f; p.y += 3.0f;
				m_pEditorCam->SetPos(p);
			}
			m_camera = std::move(upPhotoCam);
			if (m_spPlayer)
			{
				m_spPlayer->SetControlEnabled(false);   // 撮影中はプレイヤー固定
				m_spPlayer->SetDamageEnabled(false);    // 撮影中は被弾しない
			}
		}
		else
		{
			// ゲームカメラへ戻す
			auto upGameCam = std::make_unique<SideScrollCamera>();
			m_pCamera      = upGameCam.get();
			m_camera       = std::move(upGameCam);
			m_pCamera->SetRooms(m_rooms);
			m_pEditorCam   = nullptr;
			if (m_spPlayer)
			{
				m_spPlayer->SetControlEnabled(true);
				m_spPlayer->SetDamageEnabled(true);
			}
		}
	}
	m_f4Prev = f4Now;

	// カメラ更新
	if (m_photoMode)
	{
		if (m_pEditorCam) { m_pEditorCam->Update(); }
		if (m_spPlayer) { m_spPlayer->SetControlEnabled(false); }   // 撮影中は毎フレーム固定
	}
	else if (m_editorMode)
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
						// ワンウェイは通過後に収縮して消える
						if (auto wh = m_currentWarpHole.lock()) { if (wh->GetData().OneWay) { wh->Consume(); } }
						m_currentWarpHole.reset();
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
					// ワンウェイは通過後に収縮して消える
					if (auto wh = m_currentWarpHole.lock()) { if (wh->GetData().OneWay) { wh->Consume(); } }
					m_currentWarpHole.reset();
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
								m_currentWarpHole        = wh;   // 完了時にワンウェイなら収縮消滅させる
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
				const Math::Vector3 cpPos = cp->GetPos();

				// 「新しいチェックポイントを初めて踏んだ瞬間」だけ取得演出を出す
				const bool isNew = !m_cpFxInit || (cpPos - m_cpFxPos).LengthSquared() > 0.01f;

				// このチェックポイントを踏んだ → 以降はここに復活
				m_checkpointReached = true;
				m_respawnPos = cpPos;
				for (auto& other : m_checkpoints)
				{
					if (other != cp) { other->Deactivate(); }
				}

				if (isNew)
				{
					m_cpFxInit = true;
					m_cpFxPos  = cpPos;

					const Math::Vector3 up = m_spPlayer ? m_spPlayer->GetUpDir() : Math::Vector3{ 0.0f, 1.0f, 0.0f };
					const Math::Vector3 fxPos = cpPos + up * 1.0f;

					// カラフルな星バースト（虹色で複数回）
					const Math::Color cols[5] = {
						{ 1.0f, 0.35f, 0.35f, 1.0f },   // 赤
						{ 1.0f, 0.85f, 0.30f, 1.0f },   // 黄
						{ 0.35f, 1.0f, 0.45f, 1.0f },   // 緑
						{ 0.35f, 0.65f, 1.0f, 1.0f },   // 青
						{ 0.85f, 0.45f, 1.0f, 1.0f },   // 紫
					};
					for (const auto& c : cols)
					{
						m_itemManager.SpawnBurstAt(fxPos, c, PickupBurst::Style::Full);
					}

					// プレイヤーを白く発光
					if (m_spPlayer) { m_spPlayer->TriggerPickupGlow(Math::Color{ 1.0f, 1.0f, 1.0f, 1.0f }); }

					// 一瞬止める（ヒットストップ）
					TriggerHitStop(SparkleConst::PickupBurstLife);

					// 取得SE
					SoundManager::Instance().PlaySE(SeId::Checkpoint, SoundConst::SeVolume);
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
				SpawnDamageBurst();   // 体全体から赤boxパーティクルが弾ける
				SoundManager::Instance().PlaySE(SeId::Damage, SoundConst::SeVolume);
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
		int  gemsPicked  = 0;

		// カーソル磁石：通常プレイ中のみ、カーソル位置のワールドレイを計算して渡す。
		ItemManager::CursorMagnet cm;
		const bool cursorPlay = !m_editorMode && !m_menuOpen && !m_gameOverActive && !m_convoActive
			&& !m_clearActive && m_showcaseState == ShowcaseState::Off
			&& !m_introCutscene && !m_introLanding
			&& CursorManager::Instance().IsActive();
		if (cursorPlay)
		{
			const auto& bb = KdDirect3D::Instance().GetBackBuffer();
			const float bw = static_cast<float>(bb->GetInfo().Width);
			const float bh = static_cast<float>(bb->GetInfo().Height);
			const float ndcX = CursorManager::Instance().PosX() / (bw * 0.5f);
			const float ndcY = CursorManager::Instance().PosY() / (bh * 0.5f);
			const auto& cam = KdShaderManager::Instance().GetCameraCB();
			const Math::Matrix invVP = (cam.mView * cam.mProj).Invert();
			Math::Vector4 n4 = Math::Vector4::Transform(Math::Vector4(ndcX, ndcY, 0.0f, 1.0f), invVP);
			Math::Vector4 f4 = Math::Vector4::Transform(Math::Vector4(ndcX, ndcY, 1.0f, 1.0f), invVP);
			if (std::abs(n4.w) > 1e-6f && std::abs(f4.w) > 1e-6f)
			{
				n4 /= n4.w; f4 /= f4.w;
				const Math::Vector3 o(n4.x, n4.y, n4.z);
				const Math::Vector3 fp(f4.x, f4.y, f4.z);
				Math::Vector3 dir = fp - o;
				if (dir.LengthSquared() > 1e-6f) { dir.Normalize(); }
				cm.valid     = true;
				cm.rayOrigin = o;
				cm.rayDir    = dir;
				cm.clicked   = CursorManager::Instance().Clicked();

				// 左クリック：所持rockを1消費して、カメラの位置からクリック方向へ撃ち出す
				if (cm.clicked && m_coreTotal > 0)
				{
					const Math::Vector3 start = o + dir * ItemMagnetConst::FlingStartAhead;
					m_itemManager.ShootRock(start, dir, ItemMagnetConst::FlingSpeed);
					--m_coreTotal;
					m_corePopTimer = UIConst::CorePopTime;   // カウンターをポップ
					SoundManager::Instance().PlaySE(SeId::Pickup, SoundConst::SeVolume);
				}
			}
		}

		const int gotCoins = m_itemManager.Update(m_spPlayer->GetPickupHitBox(), parasolPickedUp, rocksPicked, gemsPicked, cm);
		if (gotCoins > 0)
		{
			m_coinTotal   += gotCoins;
			m_coinPopTimer = UIConst::CoinPopTime;   // 取得でカウンターをポップ
			SoundManager::Instance().PlaySE(SeId::Coin, SoundConst::SeVolume);
		}
		if (rocksPicked > 0)
		{
			// 緑石(エメラルド)＝回復：HP加算＋プレイヤー緑発光（Heal内）。＋マークをプレイヤーとHP UIに出す
			m_spPlayer->Heal(rocksPicked);
			SpawnHealPlus(rocksPicked);
			// 取得数を右上カウンター（いわ/エメラルド）に反映＋取得ポップ
			m_coreTotal   += rocksPicked;
			m_corePopTimer = UIConst::CorePopTime;
			SoundManager::Instance().PlaySE(SeId::Pickup, SoundConst::SeVolume);
		}
		if (gemsPicked > 0)
		{
			// カラフル岩(スターピース)＝収集のみ：右上カウンターに加算＋ポップ（回復なし）
			m_coreTotal   += gemsPicked;
			m_corePopTimer = UIConst::CorePopTime;
			SoundManager::Instance().PlaySE(SeId::Pickup, SoundConst::SeVolume);
		}
		if (parasolPickedUp)
		{
			SoundManager::Instance().PlaySE(SeId::ParasolGet, SoundConst::SeVolume);
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
					// Expire しない：取得後はプレイヤーのボーンに追従させて「持つ」演出に使う
					m_spHeldCore = gc;
					m_spPlayer->SetClearHold(true);

					// 取得演出：プレイヤーを光らせる → クリアシーケンス開始
					m_spPlayer->TriggerPickupGlow(Math::Color{
						GravityCoreConst::GlowFaceR, GravityCoreConst::GlowFaceG,
						GravityCoreConst::GlowFaceB, 1.0f });
					StartStageClear(corePos);
				}
			}

			// ── Rock型 重力コア取得（カウント＋HUD表示）──
			for (auto& gc : m_gravityCores)
			{
				if (!gc || gc->IsExpired() || gc->IsGlow()) { continue; }
				if (gc->Intersects(coreHit, nullptr))
				{
					gc->Expire();                 // 取得＝消す（リスポーンで復活）
					m_coreTotal++;
					m_corePopTimer = UIConst::CorePopTime;
					m_spPlayer->TriggerPickupGlow(Math::Color{
						GravityCoreConst::FaceColorR, GravityCoreConst::FaceColorG,
						GravityCoreConst::FaceColorB, 1.0f });
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
					SoundManager::Instance().PlaySE(SeId::Stomp, SoundConst::SeVolume);

					// プレイヤーは演出を待たず即座に跳ね返す
					const Math::Vector3 up = sp->GetPhysicsUpDir();
					Math::Vector3 v = pvel;
					v -= up * v.Dot(up);                    // up 方向成分を除去
					v += up * CubunConst::StompBounceVel;   // 跳ね
					m_spPlayer->SetVelocity(v);
				}
				else if (!sp->IsSquashing() && sp->IsSpikeHit(ppos))
				{
					const int beforeHp = m_spPlayer->GetHp();
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
					// 実際にダメージが入ったときだけカメラを揺らす（無敵中は無視）
					if (m_spPlayer->GetHp() < beforeHp && m_pCamera)
					{
						m_pCamera->TriggerShake(JuiceConst::ShakeHitStr);
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
					const int beforeHp = m_spPlayer->GetHp();
					m_spPlayer->TakeDamageFrom(SpikeBoxConst::SpikeDamage, sb->GetCenter());
					// 実際にダメージが入ったときだけカメラを揺らす（無敵中は無視）
					if (m_spPlayer->GetHp() < beforeHp && m_pCamera)
					{
						m_pCamera->TriggerShake(JuiceConst::ShakeHitStr);
					}
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

	// ── ポーズ中の背景ぼかし：HUDをシーンRT（SRV有）へ描き、後でまとめてぼかす ──
	// バックバッファはSRV不可のため、ぼかし対象（背景＋UI）をシーンRTに集約する。
	auto& pp = KdShaderManager::Instance().m_postProcessShader;
	auto  pauseSceneRT = pp.GetSceneRT();   // 非const共有のためコピー
	const bool pauseBlur = m_menuOpen && !m_editorMode && m_pauseBlurInit
		&& pauseSceneRT && pauseSceneRT->WorkRTView();
	if (pauseBlur)
	{
		// 以降のHUD描画先をシーンRTへ（背景3Dの上にHUDが乗る）
		D3D11_VIEWPORT vp{};
		vp.Width    = static_cast<float>(pauseSceneRT->GetWidth());
		vp.Height   = static_cast<float>(pauseSceneRT->GetHeight());
		vp.MaxDepth = 1.0f;
		m_pauseRTChanger.ChangeRenderTarget(pauseSceneRT, nullptr, &vp);
	}

	// ── コイン（収集物）カウンター：右上にアイコン＋数字（取得時にポップ）──
	// ※ ポーズ中も表示（ぼかし対象に含める）
	// アイコンは 3Dコイン(coin.gltf)をRTへ描いたテクスチャを優先。未生成なら従来の2D画像。
	KdTexture* coinIconTex = m_coinIcon.GetTexture();
	if (!coinIconTex && m_coinIconTex) { coinIconTex = m_coinIconTex.get(); }

	if (!m_editorMode && !m_clearActive && m_showcaseState == ShowcaseState::Off && coinIconTex)
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
		sprite.DrawTex(coinIconTex, iconCX, iconCY, sz, sz, nullptr, nullptr);

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
		TextFx::DrawShadowed(sprite, textPos, textCol, buf);
	}

	// ── 重力コア(Rock)カウンター：コインの下にアイコン(DrawTriangleで岩)＋数字 ──
	if (!m_editorMode && !m_clearActive && m_showcaseState == ShowcaseState::Off)
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;

		m_coreIconSpin += KdFPSController::GetDt() * UIConst::CoreSpinSpeed;
		if (m_corePopTimer > 0.0f) { m_corePopTimer -= KdFPSController::GetDt(); }
		const float popK  = (m_corePopTimer > 0.0f) ? (m_corePopTimer / UIConst::CorePopTime) : 0.0f;
		const float scale = 1.0f + UIConst::CorePopScale * popK;

		const int baseSz = UIConst::CoreIconSize;
		const int sz     = static_cast<int>(baseSz * scale);
		// 右上：コインカウンターの真下
		const int iconCX = static_cast<int>(screenW * 0.5f - UIConst::CoinMargin - UIConst::CoinIconSize * 0.5f);
		const int iconCY = static_cast<int>(screenH * 0.5f - UIConst::CoinMargin - UIConst::CoinIconSize
						 - UIConst::CoreGapBelowCoin - baseSz * 0.5f);

		// 岩コアを DrawTriangle で2D投影描画（テクスチャ無し）
		DrawCoreIcon(iconCX, iconCY, sz, m_coreIconSpin);

		// 「× N」を左に
		char buf[32] = {};
		sprintf_s(buf, "× %d", m_coreTotal);
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
		const float textRight = iconCX - baseSz * 0.5f - UIConst::CoinTextGap;
		const Math::Vector2 textPos(textRight - textW, iconCY - textH * 0.5f);
		const Math::Color textCol(1.0f, 1.0f, 1.0f, 1.0f);
		TextFx::DrawShadowed(sprite, textPos, textCol, buf);
	}

	// ── HP：宝石ハート。左上に表示・被ダメで揺れる（ポーズ中も表示）──
	if (!m_editorMode && !m_clearActive && m_showcaseState == ShowcaseState::Off && m_spPlayer)
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
	if (!m_editorMode && !m_menuOpen && !m_clearActive && m_showcaseState == ShowcaseState::Off && !m_healPluses.empty())
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

	// ── 重力コンパス（左下）：重力の「下」方向＋切替可否（ポーズ中も表示）──
	if (!m_editorMode && !m_clearActive && m_showcaseState == ShowcaseState::Off && m_spPlayer)
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
	// ── 低HP時の黒ビネット（赤フラッシュは廃止）──
	DrawLowHpVignette();

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

	// ── ステージクリア：キメの瞬間に「ステージクリアー！」をポップ表示（ワールド座標アンカー）──
	if (m_clearActive && m_clearTimer >= ClearConst::BannerStartT && m_spPlayer)
	{
		// プレイヤー頭上のワールド位置 → スクリーンへ投影。
		// 重力反転でも破綻しないよう、ワールド上方向(0,1,0)でアンカー
		// （クリアの決めポーズは常に直立で正面を向くため、頭上に出る）。
		const Math::Vector3 anchor = m_spPlayer->GetPos()
			+ Math::Vector3(0.0f, ClearConst::BannerWorldUp, 0.0f);
		const Math::Matrix vp = KdShaderManager::Instance().GetCameraCB().mView
			* KdShaderManager::Instance().GetCameraCB().mProj;
		const Math::Vector4 clip = Math::Vector4::Transform(
			Math::Vector4(anchor.x, anchor.y, anchor.z, 1.0f), vp);

		if (clip.w > 0.001f)   // カメラ後方なら描かない
		{
			const float sx = (clip.x / clip.w) * screenW * 0.5f;   // 中心原点・+Xが右
			const float sy = (clip.y / clip.w) * screenH * 0.5f;   // +Yが上

			auto& sprite = KdShaderManager::Instance().m_spriteShader;
			auto fs = KdFontManager::Instance().CreateFontTexture(
				ClearConst::BannerFontNo, ClearConst::BannerText, false);
			if (fs)
			{
				// 出現アニメ：フェードイン＋下からせり上がり（easeOutBackで軽くオーバーシュート）
				const float tp = std::clamp(
					(m_clearTimer - ClearConst::BannerStartT) / ClearConst::BannerPopTime, 0.0f, 1.0f);
				const float c1 = 1.70158f;                       // easeOutBack 係数
				const float u  = tp - 1.0f;
				const float eb = 1.0f + (c1 + 1.0f) * u * u * u + c1 * u * u;   // easeOutBack
				const float rise = ClearConst::BannerRisePx * (1.0f - eb);      // 下→定位置
				const float alpha = tp;

				// テキスト寸法を測ってアンカー位置に中央寄せ
				float tw = 0.0f, th = 0.0f;
				for (const auto& d : fs->GetTexList())
				{
					if (d && d->FontTex)
					{
						tw += static_cast<float>(d->FontTex->GetInfo().Width);
						th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
					}
				}
				const Math::Vector2 pos(sx - tw * 0.5f, sy - th * 0.5f + rise);

				// 影 → 縁取り(8方向) → 本体
				const Math::Color shadow(0.0f, 0.0f, 0.0f, ClearConst::BannerShadowA * alpha);
				sprite.DrawFont(fs, Math::Vector2(
					pos.x + ClearConst::BannerShadowOff, pos.y - ClearConst::BannerShadowOff), &shadow, 0);
				const Math::Color outline(ClearConst::BannerOutR, ClearConst::BannerOutG, ClearConst::BannerOutB, alpha);
				const int t = ClearConst::BannerOutlinePx;
				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						if (dx == 0 && dy == 0) { continue; }
						sprite.DrawFont(fs, Math::Vector2(pos.x + dx * t, pos.y + dy * t), &outline, 0);
					}
				}
				const Math::Color main(ClearConst::BannerR, ClearConst::BannerG, ClearConst::BannerB, alpha);
				sprite.DrawFont(fs, pos, &main, 0);
			}
		}
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
				// 円が閉じていく：マスク四角のサイズを 開→0 へ（等速＝開始直後から閉じて見える）
				const float ec = a;                                      // linear
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

	// ── ポーズ中：シーンRT（背景＋HUD）をぼかしてバックバッファへ全画面合成 ──
	if (pauseBlur)
	{
		auto& sprite = KdShaderManager::Instance().m_spriteShader;
		m_pauseRTChanger.UndoRenderTarget();   // 描画先をバックバッファへ戻す
		sprite.End();                          // スプライトバッチを一旦閉じる（ステート確定）
		pp.GenerateBlurTexture(pauseSceneRT, m_pauseBlurRT.m_RTTexture,
			m_pauseBlurRT.m_viewPort, PauseMenuConst::BlurRadius);
		sprite.Begin();                        // 外側のEnd()と釣り合うよう再開
		const Math::Color white(1.0f, 1.0f, 1.0f, 1.0f);
		sprite.DrawTex(m_pauseBlurRT.m_RTTexture.get(), 0, 0, screenW, screenH, nullptr, &white);
		// この後はポーズメニューのみ鮮明に描く（HUD/会話などは出さない）
		DrawPauseMenu();
		return;
	}

	// ── 近くのコアリアに「Ｗ 話す」吹き出し（会話前のみ）──
	DrawTalkPrompt();

	// ── コアリア会話UI（ポーズメニューより下、ゲームUIより上）──
	DrawCorelia();

	// ── 着地後「〜にやってきた！」バナー ──
	DrawArrivalBanner();

	// ── ステージ1の操作説明（全部押すまで表示）──
	DrawControlHint();

	// ── 見せカメラのシネマ黒帯（上下）──
	DrawShowcaseBars();

	// ── ポーズメニュー（最前面）──
	if (m_menuOpen) { DrawPauseMenu(); }

	// ── ゲームオーバー（さらに最前面）──
	if (m_gameOverActive) { DrawGameOver(); }
}

//----------------------------------------------------------
// ポーズメニュー：操作
//----------------------------------------------------------
void GameScene::UpdatePauseMenu()
{
	m_menuBlinkTimer += KdFPSController::GetDt();

	// 設定ウィンドウが開いている間はそちらに入力を渡す（ポーズ操作は止める）
	if (m_settingsMenu.IsOpen())
	{
		m_settingsMenu.Update();
		// 設定を閉じた瞬間に「押しっぱなしのEnter/W/S」がポーズ側で誤発火しないよう、
		// エッジ用の prev を現在の押下状態に合わせておく（貫通防止）。
		const bool navHeld =
			((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0) ||
			((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
		const bool decideHeld =
			((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
		m_menuNavPrev     = navHeld;
		m_menuConfirmPrev = decideHeld;
		return;
	}

	// 上下で選択（W/S または ↑↓）
	const bool up   = ((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0);
	const bool down = ((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
	const bool nav  = up || down;
	if (nav && !m_menuNavPrev)
	{
		if (up) { m_menuIndex = (m_menuIndex + PauseMenuConst::Count - 1) % PauseMenuConst::Count; }
		else    { m_menuIndex = (m_menuIndex + 1) % PauseMenuConst::Count; }
		SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume);
	}
	m_menuNavPrev = nav;

	// シーン遷移の黒フェードアウト中は入力を無視して暗転を進める
	if (m_pauseExitFade >= 0.0f)
	{
		m_pauseExitFade += PauseMenuConst::ExitFadeSpeed * KdFPSController::GetDt();
		if (m_pauseExitFade >= 1.0f)
		{
			if (m_pauseExitTarget == 2)
			{
				// やりなおす：同じステージを最初から再読込
				SceneManager::Instance().RestartScene();
			}
			else
			{
				SceneManager::Instance().SetNextScene(
					m_pauseExitTarget == 1 ? SceneManager::SceneType::Title
										   : SceneManager::SceneType::StageSelect);
			}
		}
		return;
	}

	// マウス：ホバーで選択／クリックで決定
	bool mouseConfirm = false;
	{
		auto& cur = CursorManager::Instance();
		if (cur.IsActive())
		{
			using namespace PauseMenuConst;
			const float hw       = PanelFullW * 0.5f; (void)hw;
			const float panelH   = BannerH + ContentPadTop + Count * ItemRowH + ContentPadBottom;
			const float hh       = panelH * 0.5f;
			const float bannerCY = hh - BannerH * 0.5f;
			const float firstItemY = bannerCY - BannerH * 0.5f - ContentPadTop - ItemRowH * 0.5f;
			const float barHalfW = (PanelFullW - SidePad * 2.0f) * 0.5f;
			for (int i = 0; i < Count; ++i)
			{
				const float y = firstItemY - i * ItemRowH;
				if (!cur.HitRect(0.0f, y, barHalfW, ItemRowH * 0.5f)) { continue; }
				if (m_menuIndex != i) { m_menuIndex = i; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
				if (cur.Clicked()) { mouseConfirm = true; }
				break;
			}
		}
	}

	// 決定（Enter / Space / マウスクリック）
	const bool confirm = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	if ((confirm && !m_menuConfirmPrev) || mouseConfirm)
	{
		SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
		switch (m_menuIndex)
		{
		case PauseMenuConst::Resume:
			m_menuOpen = false;
			SoundManager::Instance().ResumeAllSE();                // 一時停止したSEを再開
			SoundManager::Instance().SetBgmMuffle(false);          // こもり解除
			SoundManager::Instance().SetBgmVolumeScale(1.0f);      // 音量を元に戻す
			break;
		case PauseMenuConst::Settings:
			m_settingsMenu.Open();   // 設定ウィンドウを開く（ポーズの上に重ねる）
			break;
		case PauseMenuConst::Retry:
			m_pauseExitFade = 0.0f; m_pauseExitTarget = 2;   // 暗転してから同ステージ再読込
			break;
		case PauseMenuConst::StageSelect:
			m_pauseExitFade = 0.0f; m_pauseExitTarget = 0;   // 暗転してから遷移
			break;
		case PauseMenuConst::Title:
			m_pauseExitFade = 0.0f; m_pauseExitTarget = 1;
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

	// 中央寄せ＋右下シャドウのテキスト（fontNoスロット・cx中心・y中心）。
	// ※ DrawFont の原点は「文字列の左端・下端」で右・上へ伸びる仕様。
	//   計測と描画を同じフォントスプライト(fontNoスロット)で行い、ズレを無くす。
	//   フレームワークの DrawFont(format) はスロット0固定なので使わない。
	auto drawCentered = [&](int fontNo, const char* text, float cx, float y, const Math::Color& col)
	{
		auto fs = KdFontManager::Instance().CreateFontTexture(fontNo, text, false);
		if (!fs) { return; }
		float textW = 0.0f, textH = 0.0f;
		for (const auto& d : fs->GetTexList())
		{
			if (!d || !d->FontTex) { continue; }
			textW += static_cast<float>(d->FontTex->GetInfo().Width);
			textH  = std::max(textH, static_cast<float>(d->FontTex->GetInfo().Height));
		}
		const Math::Vector2 pos(cx - textW * 0.5f, y - textH * 0.5f);
		// 右下ドロップシャドウ → 本体（同じフォントスプライトで描く）
		const Math::Color shc(TextFxConst::ShadowR, TextFxConst::ShadowG, TextFxConst::ShadowB,
			col.w * TextFxConst::ShadowAlphaMul);
		sprite.DrawFont(fs, Math::Vector2(pos.x + TextFxConst::ShadowOffX, pos.y - TextFxConst::ShadowOffY), &shc, 0);
		sprite.DrawFont(fs, pos, &col, 0);
	};

	// ── レイアウト（StageSelect風：パネルは画面中央。中心原点・+Yが上）──
	using namespace PauseMenuConst;
	const float hw     = PanelFullW * 0.5f;
	const float panelH = BannerH + ContentPadTop + Count * ItemRowH + ContentPadBottom;
	const float hh     = panelH * 0.5f;
	const float panelCY = 0.0f;
	const float top     = panelCY + hh;

	// ── パネル（影 → 金縁 → 紺の本体、すべて角丸）──
	{
		const int ct = PanelEdgeThickness;
		const Math::Color shadow(0.0f, 0.0f, 0.0f, PanelShadowA);
		sprite.DrawRoundedBox(6, static_cast<int>(panelCY) - 6, static_cast<int>(hw) + ct, static_cast<int>(hh) + ct,
			PanelRadius + ct, &shadow, PanelCornerSegs);
		const Math::Color edge(PanelEdgeR, PanelEdgeG, PanelEdgeB, PanelEdgeA);
		sprite.DrawRoundedBox(0, static_cast<int>(panelCY), static_cast<int>(hw) + ct, static_cast<int>(hh) + ct,
			PanelRadius + ct, &edge, PanelCornerSegs);
		const Math::Color body(PanelBodyR, PanelBodyG, PanelBodyB, PanelBodyA);
		sprite.DrawRoundedBox(0, static_cast<int>(panelCY), static_cast<int>(hw), static_cast<int>(hh),
			PanelRadius, &body, PanelCornerSegs);
	}

	// ── 上部バナー（金帯）＋タイトル（暗い文字）──
	const float bannerCY = top - BannerH * 0.5f;
	{
		const Math::Color banner(PanelEdgeR, PanelEdgeG, PanelEdgeB, 1.0f);
		sprite.DrawRoundedBox(0, static_cast<int>(bannerCY), static_cast<int>(hw), static_cast<int>(BannerH * 0.5f),
			PanelRadius, &banner, PanelCornerSegs);
		// タイトルは暗い文字をくっきり（金地なので影は付けない）
		auto tfs = KdFontManager::Instance().CreateFontTexture(TitleFontNo, TitleText, false);
		if (tfs)
		{
			float ttw = 0.0f, tth = 0.0f;
			for (const auto& d : tfs->GetTexList())
			{
				if (!d || !d->FontTex) { continue; }
				ttw += static_cast<float>(d->FontTex->GetInfo().Width);
				tth  = std::max(tth, static_cast<float>(d->FontTex->GetInfo().Height));
			}
			const Math::Color titleCol(BannerTextR, BannerTextG, BannerTextB, 1.0f);
			sprite.DrawFont(tfs, Math::Vector2(-ttw * 0.5f, bannerCY - tth * 0.5f), &titleCol, 0);
		}
	}

	// ── 項目（バナー直下から下へ等間隔）──
	const float firstItemY = bannerCY - BannerH * 0.5f - ContentPadTop - ItemRowH * 0.5f;
	const float barHalfW   = (PanelFullW - SidePad * 2.0f) * 0.5f;
	const char* items[Count] = { ItemResume, ItemSettings, ItemRetry, ItemStageSelect, ItemTitle };
	const float blink = 0.5f + 0.5f * std::sinf(m_menuBlinkTimer * BlinkSpeed);
	for (int i = 0; i < Count; ++i)
	{
		const bool  sel = (i == m_menuIndex);
		const float y   = firstItemY - i * ItemRowH;

		if (sel)
		{
			// 選択ハイライトバー（金・点滅、角丸）
			const Math::Color bar(PanelEdgeR, PanelEdgeG, PanelEdgeB, HighlightA + HighlightBlinkA * blink);
			sprite.DrawRoundedBox(0, static_cast<int>(y), static_cast<int>(barHalfW), static_cast<int>(HighlightH * 0.5f),
				PanelRadius, &bar, PanelCornerSegs);
			// コアリアの選択アイコン（バー左側）
			if (m_lifeIconTex)
			{
				const int ax = static_cast<int>(-barHalfW + AccentIconSize * 0.5f + AccentGap);
				const Math::Color ic(1.0f, 1.0f, 1.0f, 0.7f + 0.3f * blink);
				sprite.DrawTex(m_lifeIconTex.get(), ax, static_cast<int>(y), AccentIconSize, AccentIconSize, nullptr, &ic);
			}
		}

		const Math::Color col = sel
			? Math::Color(1.0f, 0.97f, 0.7f, 1.0f)   // 選択中は明るく
			: Math::Color(0.75f, 0.78f, 0.85f, 0.85f);
		drawCentered(FontNo, items[i], 0.0f, y, col);
	}

	// ── シーン遷移の黒フェードアウト（メニューの上から暗転）──
	if (m_pauseExitFade >= 0.0f)
	{
		const float a = std::clamp(m_pauseExitFade, 0.0f, 1.0f);
		const Math::Color black(0.0f, 0.0f, 0.0f, a);
		sprite.DrawBox(0, 0, sw, sh, &black, true);
	}

	// 設定ウィンドウ（開いていればポーズの上に重ねて描画）
	m_settingsMenu.Draw();
}

//----------------------------------------------------------
// ゲームオーバー：操作（W/S で選択、Enter/Space で決定）
//----------------------------------------------------------
void GameScene::UpdateGameOver()
{
	m_gameOverTimer += KdFPSController::GetDt();

	const bool up   = ((GetAsyncKeyState('W') & 0x8000) != 0) || ((GetAsyncKeyState(VK_UP)   & 0x8000) != 0);
	const bool down = ((GetAsyncKeyState('S') & 0x8000) != 0) || ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0);
	const bool nav  = up || down;
	if (nav && !m_gameOverNavPrev)
	{
		if (up) { m_gameOverIndex = (m_gameOverIndex + GameOverConst::Count - 1) % GameOverConst::Count; }
		else    { m_gameOverIndex = (m_gameOverIndex + 1) % GameOverConst::Count; }
		SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume);
	}
	m_gameOverNavPrev = nav;

	// マウス：ホバーで選択／クリックで決定
	bool mouseConfirm = false;
	{
		auto& cur = CursorManager::Instance();
		if (cur.IsActive())
		{
			using namespace GameOverConst;
			for (int i = 0; i < Count; ++i)
			{
				const float y = ItemStartY - i * ItemGapPx;
				if (!cur.HitRect(0.0f, y, 220.0f, ItemGapPx * 0.45f)) { continue; }
				if (m_gameOverIndex != i) { m_gameOverIndex = i; SoundManager::Instance().PlaySE(SeId::MenuMove, SoundConst::SeVolume); }
				if (cur.Clicked()) { mouseConfirm = true; }
				break;
			}
		}
	}

	const bool confirm = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	if ((confirm && !m_gameOverConfPrev) || mouseConfirm)
	{
		SoundManager::Instance().PlaySE(SeId::MenuDecide, SoundConst::SeVolume);
		switch (m_gameOverIndex)
		{
		case GameOverConst::Retry:
			// 同じステージを最初からやり直し（同一シーンの強制再読込）
			SceneManager::Instance().RestartScene();
			break;
		case GameOverConst::StageSelect:
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::StageSelect);
			break;
		case GameOverConst::Title:
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Title);
			break;
		}
	}
	m_gameOverConfPrev = confirm;
}

//----------------------------------------------------------
// ゲームオーバー：描画（赤バナーのパネル。ポーズと同じレイアウト）
//----------------------------------------------------------
void GameScene::DrawGameOver()
{
	using namespace GameOverConst;
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	const float appear = std::clamp(m_gameOverTimer / FadeInTime, 0.0f, 1.0f);
	const float a      = appear;

	// 中央寄せ＋右下シャドウのテキスト（計測も描画も同じフォントスプライト）
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

	// ── 背景：画面全体を濃く暗転（わずかに赤を含む）──
	{
		const Math::Color dim(0.05f, 0.0f, 0.01f, DimAlpha * appear);
		sprite.DrawBox(0, 0, sw, sh, &dim, true);
	}

	const float pulse = 0.5f + 0.5f * std::sinf(m_gameOverTimer * TitlePulse);
	const float titleY = sh * TitleYRatio;

	// ── タイトル背後の赤グロー（加算で柔らかく脈動）──
	{
		auto& sm = KdShaderManager::Instance();
		sm.ChangeBlendState(KdBlendState::Add);
		constexpr int LAYERS = 10;
		for (int li = LAYERS - 1; li >= 0; --li)
		{
			const float u   = static_cast<float>(li) / (LAYERS - 1);
			const int   rad = static_cast<int>(60.0f + u * 220.0f);
			const float ga  = TitleGlowMax * (0.18f) * (1.0f - u) * (0.6f + 0.4f * pulse) * appear;
			const Math::Color gc(TitleColR, TitleColG * 0.4f, TitleColB * 0.4f, ga);
			sprite.DrawCircle(0, static_cast<int>(titleY), rad, &gc, true);
		}
		sm.UndoBlendState();
	}

	// ── タイトル「ゲームオーバー」：大きく中央上、赤く脈動 ──
	{
		const float br = 0.80f + 0.20f * pulse;
		const Math::Color titleCol(TitleColR * br, TitleColG, TitleColB, appear);
		drawCentered(TitleFontNo, TitleText, 0.0f, titleY, titleCol);
	}

	// ── タイトル下の区切り線（赤・出現で横に伸びる）──
	{
		const int dw = static_cast<int>(DividerHalfW * appear);
		const Math::Color div(PanelEdgeR, PanelEdgeG, PanelEdgeB, 0.7f * appear);
		sprite.DrawBox(0, static_cast<int>(titleY - DividerGap), dw, static_cast<int>(DividerH), &div, true);
	}

	// ── 項目（中央・縦並び。選択は明るく＋下線、点滅）──
	const char* items[Count] = { ItemRetry, ItemStageSelect, ItemTitle };
	const float blink = 0.5f + 0.5f * std::sinf(m_gameOverTimer * BlinkSpeed);
	for (int i = 0; i < Count; ++i)
	{
		const bool  sel = (i == m_gameOverIndex);
		const float y   = ItemStartY - i * ItemGapPx;

		const Math::Color col = sel
			? Math::Color(1.0f, 0.95f, 0.92f, a)
			: Math::Color(0.78f, 0.70f, 0.70f, 0.8f * a);
		drawCentered(FontNo, items[i], 0.0f, y, col);

		// 選択中：下線（赤・点滅）
		if (sel)
		{
			const float ua = (0.55f + 0.45f * blink) * a;
			const Math::Color uc(HighlightR, HighlightG, HighlightB, ua);
			sprite.DrawBox(0, static_cast<int>(y - 26.0f), static_cast<int>(ItemUnderlineW), 2, &uc, true);
		}
	}
}

//----------------------------------------------------------
// 低HP時の黒ビネット（画面端が暗くなる＋うっすら鼓動）。赤フラッシュの代わり。
//----------------------------------------------------------
void GameScene::DrawLowHpVignette()
{
	if (!m_vignetteTex || !m_spPlayer) { return; }
	if (m_editorMode || m_clearActive || m_menuOpen || m_gameOverActive
		|| m_showcaseState != ShowcaseState::Off || m_introCutscene || m_introLanding) { return; }

	const int hp = m_spPlayer->GetHp();
	const float ratio = (PlayerConst::MaxHp > 0) ? static_cast<float>(hp) / PlayerConst::MaxHp : 0.0f;
	if (ratio >= UIConst::LowHpThreshold) { return; }   // HPが十分なら出さない

	// しきい値で0、HP0付近で1
	float k = 1.0f - (ratio / UIConst::LowHpThreshold);
	k = std::clamp(k, 0.0f, 1.0f);
	// 鼓動の脈動
	const float pulse = 1.0f + UIConst::LowHpPulseAmp * std::sinf(m_playTime * UIConst::LowHpPulseSpeed);
	float alpha = UIConst::LowHpVignetteMaxA * k * pulse;
	alpha = std::clamp(alpha, 0.0f, 1.0f);

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int w = static_cast<int>(bb->GetInfo().Width);
	const int h = static_cast<int>(bb->GetInfo().Height);
	const Math::Color col(1.0f, 1.0f, 1.0f, alpha);
	sprite.DrawTex(m_vignetteTex.get(), 0, 0, w, h, nullptr, &col);
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
	// 撮影モードはワイヤーフレームを一切描かない
	if (m_photoMode) { return; }

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
		m_showcaseEditor.DrawDebug();
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
	// 導入の落下トレイル（カメラを向く連結リボン＝彗星の尾）
	if (m_introCutscene && m_introTrail.size() >= 2)
	{
		DrawIntroTrail();
	}

	// アイテム周りの星きらめき（半透明・発光のためエフェクトパスで描画）
	m_itemManager.DrawEffect();

	// 重力ゾーンの重力方向矢印（ワールド固定で壁に貼り付く）
	DrawGravityArrows();

	// デバッグワイヤー（ゾーン枠／エディタ配置）。シーンRTに描くのでF3画面でも見える
	DrawDebugWires();

	// エディタ：選択中オブジェクト＋軸ギズモ（シーンRTに描いてGameウィンドウに表示）
	if (m_editorMode) { DrawSelectionMarker(); }

	// UI用コイン3D（別RTへ。HUD表示時のみ）。3Dパス内で行い、状態を戻す。
	if (!m_editorMode && !m_clearActive && m_showcaseState == ShowcaseState::Off)
	{
		m_coinIcon.Render(KdFPSController::GetDt(), m_camera);
		KdShaderManager::Instance().m_StandardShader.BeginUnLit();   // 後続(見せカメラ)のためエフェクトパス復帰
	}

	// 見せカメラのプレビュー（別RTへ2パス目）。3Dパス内で行い、状態を戻す。
	DrawShowcasePreview();
}

//----------------------------------------------------------
// 落下トレイル：位置履歴をカメラを向く連結リボン（彗星の尾）として描く。
// 頭ほど太く明るく、尾ほど細く透明に。外周を透明にして柔らかい発光に。
//----------------------------------------------------------
void GameScene::DrawIntroTrail()
{
	// 先端は必ずプレイヤーの現在の描画位置に合わせる（フレーム間のズレで先端に隙間が出るのを防ぐ）
	std::vector<Math::Vector3> hist;
	hist.reserve(m_introTrail.size() + 1);
	if (m_spPlayer)
	{
		// 先端はbodyボーンの軸方向（体の中心側）へ寄せて、尾が本体から出るようにする
		const Math::Matrix hbm = m_spPlayer->GetBoneWorld("Body");
		Math::Vector3 hUp = hbm.Up();
		if (hUp.LengthSquared() > 1e-8f) { hUp.Normalize(); }
		const Math::Vector3 head = hbm.Translation() + hUp * IntroConst::TrailFootGlowUp;
		hist.push_back(head);
		// 先頭の履歴がほぼ同じ位置なら重複を避ける
		if (m_introTrail.empty() || (m_introTrail[0] - head).LengthSquared() > 1e-4f)
		{
			for (const auto& p : m_introTrail) { hist.push_back(p); }
		}
		else
		{
			for (size_t k = 1; k < m_introTrail.size(); ++k) { hist.push_back(m_introTrail[k]); }
		}
	}
	else
	{
		hist = m_introTrail;
	}

	const int hn = static_cast<int>(hist.size());
	if (hn < 2) { return; }

	// 履歴点が落下速度ぶん飛び飛びなので、Catmull-Romで間を補間して密な連続線にする
	std::vector<Math::Vector3> pts;
	{
		const int sub = (IntroConst::TrailSubdiv > 0) ? IntroConst::TrailSubdiv : 1;
		auto at = [&](int i) -> const Math::Vector3&
		{
			return hist[std::clamp(i, 0, hn - 1)];
		};
		pts.reserve(static_cast<size_t>(hn - 1) * sub + 1);
		for (int i = 0; i < hn - 1; ++i)
		{
			const Math::Vector3& p0 = at(i - 1);
			const Math::Vector3& p1 = at(i);
			const Math::Vector3& p2 = at(i + 1);
			const Math::Vector3& p3 = at(i + 2);
			for (int s = 0; s < sub; ++s)
			{
				const float t  = static_cast<float>(s) / static_cast<float>(sub);
				const float t2 = t * t;
				const float t3 = t2 * t;
				// Catmull-Rom
				const Math::Vector3 p =
					(p1 * 2.0f
					+ (p2 - p0) * t
					+ (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
					+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
				pts.push_back(p);
			}
		}
		pts.push_back(hist[hn - 1]);
	}

	const int n = static_cast<int>(pts.size());
	if (n < 2) { return; }

	// 色 → 頂点カラー(uint, ABGR) パッカー
	auto pack = [](float r, float g, float b, float a) -> unsigned int
	{
		const auto cv = [](float v) { return static_cast<unsigned int>(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
		return (cv(a) << 24) | (cv(b) << 16) | (cv(g) << 8) | cv(r);
	};

	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	// 各点のカメラ向き横方向(side)・幅・長さフェードを求める。
	// 視線と尾が平行になって side が潰れる箇所は直前の向きを引き継いで隙間を防ぐ。
	std::vector<Math::Vector3> side(n);
	std::vector<float>         hw(n);
	std::vector<float>         lf(n);   // 1(頭)→0(尾)
	Math::Vector3 prevSide(1.0f, 0.0f, 0.0f);
	for (int i = 0; i < n; ++i)
	{
		Math::Vector3 tan;
		if (i == 0)            { tan = pts[0] - pts[1]; }
		else if (i == n - 1)   { tan = pts[n - 2] - pts[n - 1]; }
		else                   { tan = pts[i - 1] - pts[i + 1]; }
		if (tan.LengthSquared() < 1e-8f) { tan = Math::Vector3(0.0f, 1.0f, 0.0f); }
		tan.Normalize();

		Math::Vector3 view = camPos - pts[i];
		if (view.LengthSquared() < 1e-8f) { view = Math::Vector3(0.0f, 0.0f, -1.0f); }
		view.Normalize();

		Math::Vector3 s = tan.Cross(view);
		if (s.LengthSquared() < 1e-6f) { s = prevSide; }   // 潰れる箇所は前の向きを維持
		else
		{
			s.Normalize();
			if (s.Dot(prevSide) < 0.0f) { s = -s; }        // 反転して捻れるのを防ぐ
		}
		prevSide = s;
		side[i]  = s;

		lf[i] = 1.0f - static_cast<float>(i) / static_cast<float>(n - 1);
		hw[i] = IntroConst::TrailWidth * (IntroConst::TrailTailMul + (1.0f - IntroConst::TrailTailMul) * lf[i]);
	}

	std::vector<KdPolygon::Vertex> verts;
	verts.reserve(static_cast<size_t>(n - 1) * 18);
	auto addQuad = [&](const Math::Vector3& a, unsigned int ca, const Math::Vector3& b, unsigned int cb,
	                   const Math::Vector3& c, unsigned int cc, const Math::Vector3& d, unsigned int cd)
	{
		KdPolygon::Vertex v0{}, v1{}, v2{}, v3{}, v4{}, v5{};
		v0.pos = a; v0.color = ca;  v1.pos = b; v1.color = cb;  v2.pos = c; v2.color = cc;
		verts.push_back(v0); verts.push_back(v1); verts.push_back(v2);
		v3.pos = c; v3.color = cc;  v4.pos = b; v4.color = cb;  v5.pos = d; v5.color = cd;
		verts.push_back(v3); verts.push_back(v4); verts.push_back(v5);
	};

	for (int i = 0; i < n - 1; ++i)
	{
		const int j = i + 1;
		const float aI = IntroConst::TrailAlpha * lf[i] * lf[i];
		const float aJ = IntroConst::TrailAlpha * lf[j] * lf[j];
		const Math::Vector3 cI = pts[i];
		const Math::Vector3 cJ = pts[j];

		const unsigned int edge = pack(0, 0, 0, 0.0f);   // 外周＝透明
		const unsigned int gI = pack(IntroConst::TrailColR, IntroConst::TrailColG, IntroConst::TrailColB, aI);
		const unsigned int gJ = pack(IntroConst::TrailColR, IntroConst::TrailColG, IntroConst::TrailColB, aJ);

		// 外周(透明)→中心(発光)→外周(透明) のグラデ帯
		addQuad(cI + side[i] * hw[i], edge, cI, gI, cJ + side[j] * hw[j], edge, cJ, gJ);
		addQuad(cI, gI, cI - side[i] * hw[i], edge, cJ, gJ, cJ - side[j] * hw[j], edge);

		// 中心の明るいコア（細め）
		const float ci = hw[i] * IntroConst::TrailCoreWidthMul;
		const float cj = hw[j] * IntroConst::TrailCoreWidthMul;
		const unsigned int kI = pack(IntroConst::TrailCoreR, IntroConst::TrailCoreG, IntroConst::TrailCoreB, aI);
		const unsigned int kJ = pack(IntroConst::TrailCoreR, IntroConst::TrailCoreG, IntroConst::TrailCoreB, aJ);
		addQuad(cI + side[i] * ci, kI, cI - side[i] * ci, kI, cJ + side[j] * cj, kJ, cJ - side[j] * cj, kJ);
	}

	auto& sm = KdShaderManager::Instance();
	sm.ChangeBlendState(KdBlendState::Add);
	sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	sm.m_StandardShader.SetDissolve(0.0f);
	sm.m_StandardShader.DrawVertices(verts, Math::Matrix::Identity,
		Math::Color(1, 1, 1, 1),
		KdDepthStencilState::ZWriteDisable,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// プレイヤーの体に大きめのglowを重ねて、トレイルとの繋ぎ目を隠す（体ボーン基準でぴったり）
	if (m_footGlowTex && m_spPlayer)
	{
		// bodyボーンの軸方向（ボーンの先＝体の中心側）へオフセットして置く
		const Math::Matrix bm = m_spPlayer->GetBoneWorld("Body");
		Math::Vector3 boneUp = bm.Up();
		if (boneUp.LengthSquared() > 1e-8f) { boneUp.Normalize(); }
		const Math::Vector3 foot = bm.Translation() + boneUp * IntroConst::TrailFootGlowUp;
		const Math::Color   col{ IntroConst::TrailColR, IntroConst::TrailColG, IntroConst::TrailColB,
								 IntroConst::TrailFootGlowAlpha };
		const Math::Vector3 em { IntroConst::TrailColR, IntroConst::TrailColG, IntroConst::TrailColB };
		EffectBase::DrawBillboard(m_footGlowPoly, foot, IntroConst::TrailFootGlowSize, 0.0f, col, em);
	}

	sm.UndoDepthStencilState();
	sm.UndoBlendState();
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
	if (ImGui::Begin(U8("エディタ")))
	{
		ImGui::TextColored({ 0.6f, 1.0f, 0.8f, 1.0f }, "Editor Controls");
		ImGui::Separator();

		// エディタ画面（ゲームをImGuiウィンドウ表示）⇔ 通常フルスクリーン
		ImGui::Checkbox(U8("エディタ画面 (F3)"), &m_editorScreen);
		// デバッグ可視化（デッドゾーン/コアリア/各ゾーン枠）1キーと連動
		ImGui::Checkbox(U8("デバッグ表示 (1)"), &m_debugZonesVisible);
		// 他オブジェクトのUpdateを止める（プレイヤー操作は常にロック）
		ImGui::Checkbox(U8("ワールド停止（他の更新を止める）"), &m_editorFreeze);

		// ── オブジェクト操作（マウス選択＋ドラッグ移動／生成／コピー）──
		if (m_editorMode)
		{
			ImGui::Separator();
			ImGui::TextColored({ 1.0f, 0.9f, 0.4f, 1.0f }, "Object Editing");
			ImGui::TextWrapped(U8("オブジェクトを左クリックで選択。X/Y/Z軸ハンドルをドラッグでその軸に移動。Ctrl+Zで元に戻す / Ctrl+Yでやり直し。"));

			// グリッドスナップ（カクカク移動）
			ImGui::Checkbox(U8("グリッドスナップ"), &m_snapEnabled);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::DragFloat(U8("グリッド幅"), &m_snapSize, 0.1f, 0.1f, 100.0f, "%.2f");

			// 選択中の表示＋コピー
			if (m_selEntry >= 0 && m_selEntry < static_cast<int>(m_editEntries.size()))
			{
				const EditEntry& sel = m_editEntries[m_selEntry];
				ImGui::Text(U8("選択中: %s"), sel.label.c_str());
				ImGui::Text(U8("位置: %.1f, %.1f, %.1f"), sel.pos.x, sel.pos.y, sel.pos.z);
				if (ImGui::Button(U8("選択をコピー")))
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
				ImGui::TextDisabled(U8("選択中: (なし)"));
			}
			ImGui::Text(U8("元に戻す: %d  やり直し: %d"),
				static_cast<int>(m_undoStack.size()), static_cast<int>(m_redoStack.size()));
			if (ImGui::Button(U8("元に戻す (Ctrl+Z)"))) { EditUndo(); } ImGui::SameLine();
			if (ImGui::Button(U8("やり直し (Ctrl+Y)"))) { EditRedo(); }

			// 現在位置（カメラ前方）に新規生成
			ImGui::Text(U8("視点に生成:"));
			if (ImGui::Button(U8("惑星")))      { SpawnAtCursor(EditKind::Planet); }      ImGui::SameLine();
			if (ImGui::Button(U8("風ボックス")))     { SpawnAtCursor(EditKind::WindBox); }     ImGui::SameLine();
			if (ImGui::Button(U8("スパイクボックス")))    { SpawnAtCursor(EditKind::SpikeBox); }
			if (ImGui::Button(U8("重力コア"))) { SpawnAtCursor(EditKind::GravityCore); } ImGui::SameLine();
			if (ImGui::Button(U8("移動床"))) { SpawnAtCursor(EditKind::MovingFloor); }
			if (ImGui::Button(U8("手動ゾーン")))  { SpawnAtCursor(EditKind::ManualZone); }  ImGui::SameLine();
			if (ImGui::Button(U8("デッドゾーン")))    { SpawnAtCursor(EditKind::DeadZone); }    ImGui::SameLine();
			if (ImGui::Button(U8("コアリア")))     { SpawnAtCursor(EditKind::Corelia); }
		}

		ImGui::Separator();
		if (ImGui::Button(U8("全部保存")))
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
			m_showcaseEditor.Save();       // 見せカメラ経路
			m_warpHoleEditor.Save();
			m_enemyEditor.Save();
			m_itemManager.Save();          // コイン
			m_itemManager.SaveParasols();  // パラソル
			m_itemManager.SaveRockGems();  // カラフル岩
			SaveSpawn();                   // スポーン＋導入開始位置
			SaveSunLight();                // 太陽光
			CameraSettings::Instance().Save();
			KdDebugGUI::Instance().AddLog("[Editor] Saved ALL (planets/zones/items/spawn/etc.)\n");
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("全部再読込")))
		{
			ManualGravityZoneManager::Instance().Load();
			DeadZoneManager::Instance().Load();
			CoreliaManager::Instance().Load();
			KdDebugGUI::Instance().AddLog("[Editor] Reloaded zones / dead zones / corelia.\n");
		}

		ImGui::Separator();
		ImGui::Text(U8("ゾーン: %d / デッドゾーン: %d / コアリア: %d"),
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
	m_showcaseEditor.DrawGui();
	DrawShowcasePreviewWindow();
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
	if (ImGui::Begin(U8("太陽光")))
	{
		// 平行光の方向
		if (ImGui::DragFloat3(U8("向き"), &m_sunDir.x, 0.01f, -1.0f, 1.0f))
		{
			ApplySunLight();
		}

		// 平行光の色
		if (ImGui::ColorEdit3(U8("太陽の色"), &m_sunColor.x))
		{
			ApplySunLight();
		}

		// 環境光の色と強度（アルファが全体の明るさ）
		if (ImGui::ColorEdit4(U8("環境光の色"), &m_ambientColor.x))
		{
			ApplySunLight();
		}
		ImGui::SameLine();
		ImGui::TextDisabled(U8("(A=強さ)"));

		ImGui::Separator();

		// 保存・読み込み
		if (ImGui::Button(U8("太陽光を保存")))
		{
			SaveSunLight();
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("太陽光を読込")))
		{
			LoadSunLight();
			ApplySunLight();
		}
	}
	ImGui::End();

	// ポイントライトエディター
	if (ImGui::Begin(U8("ポイントライト")))
	{
		if (ImGui::Button(U8("ライト追加")))
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
	if (ImGui::Begin(U8("スポーン設定")))
	{
		// 初期リスポーン（着地）位置
		ImGui::TextColored({ 0.7f, 1.0f, 0.8f, 1.0f }, "Respawn / Landing");
		float pos[3] = { m_spawnPos.x, m_spawnPos.y, m_spawnPos.z };
		if (ImGui::DragFloat3(U8("スポーン位置"), pos, 0.1f))
		{
			m_spawnPos = { pos[0], pos[1], pos[2] };
		}
		if (ImGui::Button(U8("適用（リスポーン）")))
		{
			if (m_spPlayer) { m_spPlayer->SetPos(m_spawnPos); }
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("スポーン=プレイヤー位置")) && m_spPlayer)
		{
			m_spawnPos = m_spPlayer->GetPos();
		}

		ImGui::Separator();

		// 導入カットシーンの「飛んでくる」開始位置
		ImGui::TextColored({ 1.0f, 0.9f, 0.4f, 1.0f }, "Intro Start (fly-in)");
		float ipos[3] = { m_introStartPos.x, m_introStartPos.y, m_introStartPos.z };
		if (ImGui::DragFloat3(U8("イントロ開始位置"), ipos, 0.1f))
		{
			m_introStartPos = { ipos[0], ipos[1], ipos[2] };
		}
		// エディタカメラの位置を開始位置に採用（見ている位置から飛ばす）
		if (ImGui::Button(U8("イントロ=カメラ位置")) && m_pEditorCam)
		{
			m_introStartPos = m_pEditorCam->GetPos();
		}
		ImGui::SameLine();
		// その場でカットシーンを再生して確認
		if (ImGui::Button(U8("イントロ確認")))
		{
			StartIntroCutscene();
		}

		ImGui::Separator();
		if (ImGui::Button(U8("保存")))
		{
			SaveSpawn();
		}
	}
	ImGui::End();

	// ─── StarBurst Viewer（手動テスト）───────────────────────────
	if (ImGui::Begin(U8("スターバースト確認")))
	{
		if (ImGui::Button(U8("プレイヤー位置でバースト")))
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
	if (ImGui::Begin(U8("Effekseer確認")))
	{
		ImGui::InputText(U8("エフェクトファイル"), m_efkViewerPath, sizeof(m_efkViewerPath));
		ImGui::DragFloat(U8("スケール"),       &m_efkViewerScale, 0.1f, 0.1f, 100.0f);
		ImGui::DragFloat(U8("速さ"),       &m_efkViewerSpeed, 0.05f, 0.01f, 10.0f);
		ImGui::Checkbox(U8("ループ"),         &m_efkViewerLoop);

		if (ImGui::Button(U8("再生")))
		{
			const Math::Vector3 playPos = m_spPlayer ? m_spPlayer->GetPos() : Math::Vector3::Zero;
			KdEffekseerManager::GetInstance().Play(
				m_efkViewerPath, playPos, m_efkViewerScale, m_efkViewerSpeed, m_efkViewerLoop);
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("全停止")))
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
			default:                                g = { 0.0f, -1.0f, 0.0f }; break;  // None=自動：下向き固定（プレイヤーの重力角に追尾しない）
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
	// 見せカメラの制御点（eye / look）。マウスでドラッグ移動できる（idx = phase*1000 + 点index）
	{
		const auto& phases = m_showcaseEditor.GetPhases();
		for (int pi = 0; pi < static_cast<int>(phases.size()); ++pi)
		{
			for (int ei = 0; ei < static_cast<int>(phases[pi].eye.size()); ++ei)
			{
				const int id = pi * 1000 + ei;
				pushEntry(phases[pi].eye[ei], EditKind::ShowcaseEye, id,
					"Cam" + std::to_string(pi + 1) + " Eye" + std::to_string(ei),
					[this, pi, ei](const Math::Vector3& p) { m_showcaseEditor.SetEyePoint(pi, ei, p); },
					[]() {},
					[this, pi, ei]() { m_showcaseEditor.SelectPoint(pi, ei, false); });
			}
			for (int li = 0; li < static_cast<int>(phases[pi].look.size()); ++li)
			{
				const int id = pi * 1000 + li;
				pushEntry(phases[pi].look[li], EditKind::ShowcaseLook, id,
					"Cam" + std::to_string(pi + 1) + " Look" + std::to_string(li),
					[this, pi, li](const Math::Vector3& p) { m_showcaseEditor.SetLookPoint(pi, li, p); },
					[]() {},
					[this, pi, li]() { m_showcaseEditor.SelectPoint(pi, li, true); });
			}
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
	case EditKind::ShowcaseEye:  { m_showcaseEditor.SetEyePoint(_idx / 1000, _idx % 1000, _pos); }  break;
	case EditKind::ShowcaseLook: { m_showcaseEditor.SetLookPoint(_idx / 1000, _idx % 1000, _pos); } break;
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

		// W で次ページ → 最後のページなら閉じる
		if (wEdge)
		{
			if (m_convoPage + 1 < static_cast<int>(m_convoPages.size()))
			{
				++m_convoPage;
				m_convoText = m_convoPages[m_convoPage];
				SoundManager::Instance().PlaySE(SeId::CoreliaTalk, SoundConst::SeVolume);
			}
			else
			{
				m_convoActive = false;
				m_convoZoom   = 0.0f;
				if (m_pCamera) { m_pCamera->ClearOffsetZOverride(); }
				SoundManager::Instance().PlaySE(SeId::MenuCancel, SoundConst::SeVolume);
			}
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
				// ヒントを PageSep でページ分割（1ページ目から表示。W で次ページ）
				const std::string hint = CoreliaManager::Instance().GetHint(
					CoreliaManager::Instance().GetNpcHintId(idx));
				m_convoPages.clear();
				{
					const std::string sep = CoreliaConst::PageSep;
					size_t start = 0;
					for (;;)
					{
						const size_t pos = hint.find(sep, start);
						if (pos == std::string::npos) { m_convoPages.push_back(hint.substr(start)); break; }
						m_convoPages.push_back(hint.substr(start, pos - start));
						start = pos + sep.size();
					}
				}
				if (m_convoPages.empty()) { m_convoPages.push_back(hint); }
				m_convoPage   = 0;
				m_convoText   = m_convoPages[0];
				m_convoActive = true;
				SoundManager::Instance().PlaySE(SeId::CoreliaTalk, SoundConst::SeVolume);
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

	// シャドウ＋アウトライン付きフォント描画
	//  ① 右下へドロップシャドウ → ② 黒を8方向にずらして縁取り → ③ 本体を上に
	const Math::Color outlineCol(CoreliaConst::OutR, CoreliaConst::OutG, CoreliaConst::OutB, CoreliaConst::OutA);
	const Math::Color shadowCol (CoreliaConst::ShadowR, CoreliaConst::ShadowG, CoreliaConst::ShadowB, CoreliaConst::ShadowA);
	auto drawOutlined = [&](std::shared_ptr<KdFontSprite>& fs, const Math::Vector2& pos, const Math::Color& mainCol)
	{
		if (!fs) { return; }
		// ① ドロップシャドウ（右下＝+X, -Y）
		sprite.DrawFont(fs,
			Math::Vector2(pos.x + CoreliaConst::ShadowOffX, pos.y - CoreliaConst::ShadowOffY),
			&shadowCol, 0);
		// ② 縁取り（8方向）
		const int t = CoreliaConst::OutlinePx;
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if (dx == 0 && dy == 0) { continue; }
				sprite.DrawFont(fs, Math::Vector2(pos.x + dx * t, pos.y + dy * t), &outlineCol, 0);
			}
		}
		// ③ 本体
		sprite.DrawFont(fs, pos, &mainCol, 0);
	};

	const int   bw = static_cast<int>(CoreliaConst::BoxWidth);
	const int   bh = static_cast<int>(CoreliaConst::BoxHeight);
	const int   cx = 0;   // 画面中央(横)
	const int   cy = static_cast<int>(-screenH * 0.5f + CoreliaConst::BoxMargin + bh * 0.5f);

	// StageSelect風：影 → 薄い金縁 → 紺の本体（すべて角丸）
	const int hbw = bw / 2;
	const int hbh = bh / 2;
	const int bct = CoreliaConst::BoxEdgeThickness;
	const Math::Color shadow(0.0f, 0.0f, 0.0f, CoreliaConst::BoxShadowA);
	sprite.DrawRoundedBox(cx + 6, cy - 6, hbw + bct, hbh + bct,
		CoreliaConst::BoxRadius + bct, &shadow, CoreliaConst::BoxCornerSegs);
	const Math::Color frame(CoreliaConst::EdgeR, CoreliaConst::EdgeG, CoreliaConst::EdgeB, CoreliaConst::BoxEdgeA);
	sprite.DrawRoundedBox(cx, cy, hbw + bct, hbh + bct,
		CoreliaConst::BoxRadius + bct, &frame, CoreliaConst::BoxCornerSegs);
	const Math::Color body(CoreliaConst::BoxR, CoreliaConst::BoxG, CoreliaConst::BoxB, CoreliaConst::BoxA);
	sprite.DrawRoundedBox(cx, cy, hbw, hbh,
		CoreliaConst::BoxRadius, &body, CoreliaConst::BoxCornerSegs);
	const Math::Color edge(CoreliaConst::EdgeR, CoreliaConst::EdgeG, CoreliaConst::EdgeB, CoreliaConst::EdgeA);

	// 話者名（枠の左上）
	{
		const Math::Vector2 namePos(
			static_cast<float>(cx) - bw * 0.5f + CoreliaConst::TextPadX,
			static_cast<float>(cy) + bh * 0.5f + CoreliaConst::NameOffsetY);
		auto fs = KdFontManager::Instance().CreateFontTexture(CoreliaConst::FontNo, CoreliaConst::SpeakerName, false);
		drawOutlined(fs, namePos, edge);
	}

	// 本文：実ピクセル幅で折り返し（はみ出し防止）。'|' か改行で手動改行も可。
	{
		const std::string& text = m_convoText;

		// 文字列の描画ピクセル幅を測る
		auto measureWidth = [](const std::string& s) -> float
		{
			if (s.empty()) { return 0.0f; }
			auto m = KdFontManager::Instance().CreateFontTexture(CoreliaConst::FontNo, s, false);
			float w = 0.0f;
			if (m)
			{
				for (const auto& d : m->GetTexList())
				{
					if (d && d->FontTex) { w += static_cast<float>(d->FontTex->GetInfo().Width); }
				}
			}
			return w;
		};

		const float innerW = CoreliaConst::BoxWidth - CoreliaConst::TextPadX * 2.0f;

		std::vector<std::string> lines;
		std::string cur;
		for (size_t i = 0; i < text.size(); )
		{
			const unsigned char b = static_cast<unsigned char>(text[i]);
			// 手動改行（'|' か '\n'）
			if (b == '|' || b == '\n') { lines.push_back(cur); cur.clear(); i += 1; continue; }

			const bool lead = (b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC);
			const int  n    = (lead && i + 1 < text.size()) ? 2 : 1;
			const std::string ch = text.substr(i, n);

			// 1文字足して幅オーバーなら、その前で折り返す
			if (!cur.empty() && measureWidth(cur + ch) > innerW)
			{
				lines.push_back(cur);
				cur.clear();
			}
			cur += ch;
			i += n;
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
				drawOutlined(fs, Math::Vector2(tx, ty), textCol);
			}
			ty -= CoreliaConst::LineHeight;
		}
	}

	// 送り（次へ）マーク：LifeIcon.png を右下で上下に揺らして表示
	if (m_lifeIconTex)
	{
		static float s_markTime = 0.0f;
		s_markTime += KdFPSController::GetDt();
		const float bob = std::sinf(s_markTime * CoreliaConst::NextIconBobSpeed) * CoreliaConst::NextIconBobAmp;
		const int mx = static_cast<int>(cx + bw * 0.5f - CoreliaConst::NextIconMarginX);
		const int my = static_cast<int>(cy - bh * 0.5f + CoreliaConst::NextIconMarginY + bob);
		const Math::Color ic(CoreliaConst::EdgeR, CoreliaConst::EdgeG, CoreliaConst::EdgeB, 1.0f);
		sprite.DrawTex(m_lifeIconTex.get(), mx, my,
			CoreliaConst::NextIconSize, CoreliaConst::NextIconSize, nullptr, &ic);
	}
}

void GameScene::SpawnDamageBurst()
{
	if (!m_spPlayer) { return; }
	const auto boxData = ModelManager::Instance().GetModel(PlayerConst::DustPath);   // Box.gltf
	auto burst = std::make_shared<DamageBurst>();
	burst->Spawn(m_spPlayer->GetPos(), m_spPlayer->GetUpDir(), boxData);
	AddObject(burst);
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
	SoundManager::Instance().PlaySE(SeId::Clear, SoundConst::SeVolume);
	m_clearDecideFlashed = false;
	m_clearYaw    = ClearConst::CamStartYawDeg;   // 振り子バネの初期角
	m_clearYawVel = 0.0f;
	(void)corePos;

	// リザルト用データを StageManager へ渡す（StageSelect 帰還時に表示）
	const int clearedStageId = StageManager::Instance().GetStageIndex() - 1;   // 0始まり
	StageManager::Instance().SetResult(
		clearedStageId, m_coinTotal, m_coreTotal, m_deathCount, m_playTime);   // rocks枠に重力コア数を載せる
	// ステージ別の記録（クリア済み・最高コイン・ベストタイム）を更新・保存
	StageManager::Instance().RecordClear(clearedStageId, m_coinTotal, m_playTime);

	if (m_spPlayer) { m_spPlayer->SetControlEnabled(false); }   // 操作ロック
	TriggerFlash(ClearConst::FlashStrength);                    // 取得の白フラッシュ

	// クリア演出は FOV を下げて望遠ぎみ（圧縮効果）にして劇的に見せる
	if (m_camera) { m_camera->SetProjectionMatrix(ClearConst::CamFov); }
}

//----------------------------------------------------------
// 導入カットシーン（Stage1限定）：開始位置→スポーンへスクリプト移動で着地
//----------------------------------------------------------
//----------------------------------------------------------
// ステージ見せカメラ：フェーズ順にスプライン経路を再生（eye/look 別スプライン）。
// 各フェーズは所要時間とイージング(ease in/out)を持つ。Enter/Spaceでスキップ。
//----------------------------------------------------------
// 経過秒 t（全フェーズ通し）から見せカメラのワールド行列を求める。終端を越えたら false。
bool GameScene::EvalShowcaseCam(float _t, Math::Matrix& _outWorld) const
{
	const auto& phases = m_showcaseEditor.GetPhases();
	float acc = 0.0f;
	for (const auto& ph : phases)
	{
		if (ph.eye.size() < 2) { continue; }   // 再生できないフェーズは飛ばす
		const float moveDur   = std::max(ph.duration / std::max(ph.speed, 0.01f), ShowcaseCamConst::MinDuration);
		const float hold      = std::max(ph.hold, 0.0f);
		const float phaseTotal = moveDur + hold;
		if (_t < acc + phaseTotal)
		{
			const float local = _t - acc;
			// 移動区間は 0→1、Hold区間は終端(1)で静止
			const float t = (local < moveDur) ? std::clamp(local / moveDur, 0.0f, 1.0f) : 1.0f;
			// 時間 t → 経路パラメータ u（イージングで加減速＝止め）
			float u = t;
			if (ph.easeIn && ph.easeOut) { u = t * t * (3.0f - 2.0f * t); }
			else if (ph.easeIn)          { u = t * t; }
			else if (ph.easeOut)         { u = 1.0f - (1.0f - t) * (1.0f - t); }

			const Math::Vector3 eye  = ShowcaseCamEditor::EvalSpline(ph.eye, u);
			Math::Vector3       look = (!ph.look.empty())
				? ShowcaseCamEditor::EvalSpline(ph.look, u)
				: eye + Math::Vector3(0.0f, 0.0f, 1.0f);

			Math::Vector3 f = look - eye;
			if (f.LengthSquared() < 1e-8f) { f = Math::Vector3(0.0f, 0.0f, 1.0f); }
			f.Normalize();
			Math::Vector3 r = Math::Vector3::Up.Cross(f);
			if (r.LengthSquared() < 1e-8f) { r = Math::Vector3(1.0f, 0.0f, 0.0f); }
			r.Normalize();
			const Math::Vector3 up = f.Cross(r);

			Math::Matrix world;
			world._11 = r.x;  world._12 = r.y;  world._13 = r.z;  world._14 = 0.0f;
			world._21 = up.x; world._22 = up.y; world._23 = up.z; world._24 = 0.0f;
			world._31 = f.x;  world._32 = f.y;  world._33 = f.z;  world._34 = 0.0f;
			world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
			_outWorld = world;
			return true;
		}
		acc += phaseTotal;
	}
	return false;   // 終端を越えた
}

void GameScene::UpdateShowcaseCam()
{
	// スキップ（Enter / Space）
	const bool skipKey = ((GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
		|| ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0);
	const bool doSkip = skipKey && !m_showcaseSkipPrev;
	m_showcaseSkipPrev = skipKey;

	m_showcaseTimer += KdFPSController::GetDt();

	Math::Matrix world;
	if (doSkip || !EvalShowcaseCam(m_showcaseTimer, world))
	{
		// スキップ or 全フェーズ終了 → 黒帯を閉じる遷移(Closing)へ。カメラは最後の画を保持
		m_showcaseState = ShowcaseState::Closing;
		return;
	}
	m_showcaseLastWorld = world;
	if (m_camera) { m_camera->SetCameraMatrix(world); }
}

//----------------------------------------------------------
// 上下の黒帯（シネマ）。m_barCover（画面高比, 0.5で中央到達）ぶん上下から覆う。
//----------------------------------------------------------
void GameScene::DrawShowcaseBars()
{
	if (m_barCover <= 0.0001f) { return; }

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);

	const float barH = m_barCover * sh;                 // 各帯の高さ(px)
	const Math::Color black(0.0f, 0.0f, 0.0f, 1.0f);
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	// 中心原点・+Yが上。上帯は画面上端から下へ、下帯は下端から上へ。
	// 端（左右・上下の外側）は丸め誤差で透けるので、内側のレターボックス位置は
	// 保ったまま外側へ少しオーバースキャンして覆う。
	constexpr float kOver = 4.0f;
	const int hw    = static_cast<int>(sw * 0.5f + kOver);
	const int hh    = static_cast<int>((barH + kOver) * 0.5f);          // 外側へ kOver 拡張
	const int topCy = static_cast<int>(sh * 0.5f - barH * 0.5f + kOver * 0.5f); // 内側は維持
	sprite.DrawBox(0,  topCy, hw, hh, &black, true);    // 上
	sprite.DrawBox(0, -topCy, hw, hh, &black, true);    // 下
}

//----------------------------------------------------------
// 着地後「’ステージ名’にやってきた！」バナー（マリギャラ風・フェードイン/アウト）
//----------------------------------------------------------
//----------------------------------------------------------
// 重力コア(Rock)を HUD に DrawTriangle で2D描画（テクスチャ無し・自転）。
// HPの宝石描画と同じ「頂点を生成→投影→三角形塗り」の方式の簡易版。
//----------------------------------------------------------
void GameScene::DrawCoreIcon(int cx, int cy, int size, float spin)
{
	// Rock(緑エメラルド)アイコンは共通ヘルパーへ委譲（ステージセレクトと同一の見た目）
	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	CoreIcon::Draw(sprite, cx, cy, size, spin);
}

void GameScene::DrawArrivalBanner()
{
	if (m_arrivalTimer < 0.0f) { return; }

	// フェード：開始/終了で In/Out
	const float t   = m_arrivalTimer;
	const float fin = std::clamp(t / IntroConst::ArrivalFadeTime, 0.0f, 1.0f);
	const float fout = std::clamp((IntroConst::ArrivalShowTime - t) / IntroConst::ArrivalFadeTime, 0.0f, 1.0f);
	const float alpha = std::min(fin, fout);
	if (alpha <= 0.0f) { return; }

	// ステージ名 ＋「にやってきた！」
	// 名前は「ステージN　〇〇」形式なので、全角スペース以降（〇〇）だけを使う。
	const int sid = StageManager::Instance().GetStageIndex() - 1;
	const char* name = (sid >= 0 && sid < StageSelectConst::StageNameCount)
		? StageSelectConst::StageNames[sid] : StageSelectConst::StageNameFallback;
	const char* disp = name;
	// 実行時は SJIS(CP932) なので全角スペースは 0x81 0x40。それ以降（名前部分）だけ使う。
	if (const char* sp = std::strstr(name, "\x81\x40")) { disp = sp + 2; }
	char buf[160];
	std::snprintf(buf, sizeof(buf), "%sにやってきた！", disp);

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);

	auto measureW = [](std::shared_ptr<KdFontSprite>& f) -> float
	{
		float w = 0.0f;
		if (f) { for (const auto& d : f->GetTexList()) { if (d && d->FontTex) { w += static_cast<float>(d->FontTex->GetInfo().Width); } } }
		return w;
	};

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	auto fs = KdFontManager::Instance().CreateFontTexture(IntroConst::ArrivalFontNo, buf, false);
	if (!fs) { return; }

	// 画面幅に収まるよう一度だけフォントサイズを縮小（長いステージ名対策）
	if (!m_arrivalFitDone)
	{
		const float maxW = sw * 0.92f;
		const float w0   = measureW(fs);
		if (w0 > maxW && w0 > 1.0f)
		{
			int fitH = static_cast<int>(IntroConst::ArrivalFontH * (maxW / w0));
			if (fitH < 16) { fitH = 16; }
			KdFontManager::Instance().AddFont(IntroConst::ArrivalFontNo, FontConst::GameFontName, fitH);
			fs = KdFontManager::Instance().CreateFontTexture(IntroConst::ArrivalFontNo, buf, false);
			if (!fs) { return; }
		}
		m_arrivalFitDone = true;
	}

	float tw = 0.0f, th = 0.0f;
	for (const auto& d : fs->GetTexList())
	{
		if (d && d->FontTex)
		{
			tw += static_cast<float>(d->FontTex->GetInfo().Width);
			th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
		}
	}
	const float cy = sh * IntroConst::ArrivalYRatio;   // 中央より上
	const Math::Vector2 pos(-tw * 0.5f, cy - th * 0.5f);

	// 縁取り(8方向・黒) → 本体(黄)
	const Math::Color black(IntroConst::ArrivalOutR, IntroConst::ArrivalOutG, IntroConst::ArrivalOutB, alpha);
	const int o = IntroConst::ArrivalOutlinePx;
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			if (dx == 0 && dy == 0) { continue; }
			sprite.DrawFont(fs, Math::Vector2(pos.x + dx * o, pos.y + dy * o), &black, 0);
		}
	}
	const Math::Color main(IntroConst::ArrivalR, IntroConst::ArrivalG, IntroConst::ArrivalB, alpha);
	sprite.DrawFont(fs, pos, &main, 0);   // 本体（黄）
}

//----------------------------------------------------------
// ステージ1の操作説明：着地後に表示し、A/D/Space を全部押したら消える。
//----------------------------------------------------------
void GameScene::UpdateControlHint()
{
	if (!m_ctrlHintActive) { return; }

	const float dt = KdFPSController::GetDt();

	// 押した入力を検知（操作可能なゲーム中だけ）
	const bool canInput = !IsUpdatePaused() && !m_introCutscene && !m_introLanding
		&& !m_deathActive && !m_gameOverActive && !m_convoActive;
	if (canInput && !m_ctrlHintDone)
	{
		if (GetAsyncKeyState('A')     & 0x8000) { m_ctrlGotA    = true; }
		if (GetAsyncKeyState('D')     & 0x8000) { m_ctrlGotD    = true; }
		if (GetAsyncKeyState(VK_SPACE)& 0x8000) { m_ctrlGotJump = true; }
		if (m_ctrlGotA && m_ctrlGotD && m_ctrlGotJump) { m_ctrlHintDone = true; }   // 全部押した→消す
	}

	// フェード：通常は1へ。押し切った／死亡・ゲームオーバー・カットシーン・会話中は0へ。
	// （死亡時に出っぱなしになるのを防ぐ。完了でなければ復活後にまたフェードインする）
	const bool fadeOut = m_ctrlHintDone || m_deathActive || m_gameOverActive
		|| m_introCutscene || m_introLanding || m_convoActive;
	const float spd = dt / std::max(IntroConst::ArrivalFadeTime, 0.01f);
	if (fadeOut) { m_ctrlHintAlpha -= spd; }
	else         { m_ctrlHintAlpha += spd; }
	m_ctrlHintAlpha = std::clamp(m_ctrlHintAlpha, 0.0f, 1.0f);
	// 「全部押して完了」したときだけ完全終了。死亡等の一時フェードでは終了させない。
	if (m_ctrlHintDone && m_ctrlHintAlpha <= 0.0f) { m_ctrlHintActive = false; }
}

//----------------------------------------------------------
void GameScene::DrawControlHint()
{
	if (!m_ctrlHintActive || m_ctrlHintAlpha <= 0.0f) { return; }
	// 他演出中は隠す（メニュー・会話・クリア等）
	if (m_menuOpen || m_gameOverActive || m_convoActive || m_clearActive
		|| m_showcaseState != ShowcaseState::Off) { return; }

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sh = static_cast<float>(bb->GetInfo().Height);
	const float alpha = m_ctrlHintAlpha;

	auto hf = KdFontManager::Instance().CreateFontTexture(IntroConst::ControlHintFontNo, IntroConst::ControlHintText, false);
	if (!hf) { return; }

	float hw = 0.0f, hh = 0.0f;
	for (const auto& d : hf->GetTexList())
	{
		if (d && d->FontTex)
		{
			hw += static_cast<float>(d->FontTex->GetInfo().Width);
			hh  = std::max(hh, static_cast<float>(d->FontTex->GetInfo().Height));
		}
	}
	// ステージ名と同じ高さ基準の少し下に出す
	const float cy = sh * IntroConst::ArrivalYRatio;
	const float hy = cy - IntroConst::ControlHintGap;
	const Math::Vector2 hpos(-hw * 0.5f, hy - hh * 0.5f);

	const Math::Color hbk(IntroConst::ArrivalOutR, IntroConst::ArrivalOutG, IntroConst::ArrivalOutB, alpha);
	const int ho = IntroConst::ControlHintOutlinePx;
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			if (dx == 0 && dy == 0) { continue; }
			sprite.DrawFont(hf, Math::Vector2(hpos.x + dx * ho, hpos.y + dy * ho), &hbk, 0);
		}
	}
	const Math::Color hmain(IntroConst::ControlHintR, IntroConst::ControlHintG, IntroConst::ControlHintB, alpha);
	sprite.DrawFont(hf, hpos, &hmain, 0);
}

//----------------------------------------------------------
// 近くのコアリアの頭上に「Ｗ 話す」の吹き出し（StageSelectと同じ吹き出しを流用）
//----------------------------------------------------------
void GameScene::DrawTalkPrompt()
{
	// 会話中・メニュー・演出中・死亡中・ゲームオーバー中は出さない
	if (m_convoActive || m_menuOpen || m_clearActive
		|| m_introCutscene || m_introLanding
		|| m_showcaseState != ShowcaseState::Off
		|| m_deathActive || m_gameOverActive
		|| !m_spPlayer || m_spPlayer->IsDead())
	{
		return;
	}

	const int idx = CoreliaManager::Instance().FindInteractable(m_spPlayer->GetPos());
	if (idx < 0) { return; }
	Math::Vector3 gpos;
	if (!CoreliaManager::Instance().GetNpcGroundPos(idx, gpos)) { return; }
	Math::Vector3 nup(0.0f, 1.0f, 0.0f);
	CoreliaManager::Instance().GetNpcUp(idx, nup);

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const float sw = static_cast<float>(bb->GetInfo().Width);
	const float sh = static_cast<float>(bb->GetInfo().Height);

	const Math::Matrix vp = KdShaderManager::Instance().GetCameraCB().mView
		* KdShaderManager::Instance().GetCameraCB().mProj;
	auto toScreen = [&](const Math::Vector3& wpos, float& ox, float& oy) -> bool
	{
		const Math::Vector4 c = Math::Vector4::Transform(Math::Vector4(wpos.x, wpos.y, wpos.z, 1.0f), vp);
		if (c.w <= 0.001f) { return false; }
		ox = (c.x / c.w) * sw * 0.5f;   // 中心原点・+Xが右
		oy = (c.y / c.w) * sh * 0.5f;   // +Yが上
		return true;
	};

	// 頭・体下部ともに接地点からUp方向(nup)で取る＝重力に関係なく常に頭基準。
	// 横重力なら nup が横向きなので、画面上でも頭の横へ自然に出る。
	float footX, footY, headX, headY;
	if (!toScreen(gpos + nup * CoreliaConst::PromptFootH, footX, footY)) { return; }
	if (!toScreen(gpos + nup * CoreliaConst::PromptHeadH, headX, headY)) { return; }

	auto& sprite = KdShaderManager::Instance().m_spriteShader;
	auto fs = KdFontManager::Instance().CreateFontTexture(CoreliaConst::PromptFontNo, "Ｗで話す", false);
	if (!fs) { return; }

	float tw = 0.0f, th = 0.0f;
	for (const auto& d : fs->GetTexList())
	{
		if (!d || !d->FontTex) { continue; }
		tw += static_cast<float>(d->FontTex->GetInfo().Width);
		th  = std::max(th, static_cast<float>(d->FontTex->GetInfo().Height));
	}
	const int ex = static_cast<int>(tw * 0.5f + CoreliaConst::PromptPadX);
	const int ey = static_cast<int>(th * 0.5f + CoreliaConst::PromptPadY);
	const Math::Color body(0.93f, 0.91f, 0.99f, 1.0f);
	const Math::Color txt(0.28f, 0.22f, 0.38f, 1.0f);

	// 画面上での「頭の向き」＝体下部→頭。これに沿って吹き出しを頭の先へ置く。
	float ux = headX - footX, uy = headY - footY;
	const float ul = std::sqrtf(ux * ux + uy * uy);
	if (ul > 1e-3f) { ux /= ul; uy /= ul; } else { ux = 0.0f; uy = 1.0f; }
	const float bx = headX + ux * (static_cast<float>(ey) + CoreliaConst::PromptGap);
	const float by = headY + uy * (static_cast<float>(ey) + CoreliaConst::PromptGap);
	sprite.DrawRoundedBubbleTo(static_cast<int>(bx), static_cast<int>(by),
		ex, ey, static_cast<float>(ey), headX, headY,
		CoreliaConst::PromptTailHalfW, CoreliaConst::PromptMaxTail, &body, CoreliaConst::PromptCornerSegs);
	sprite.DrawFont(fs, Math::Vector2(bx - tw * 0.5f, by - th * 0.5f), &txt, 0);
}

// 見せカメラ中もプレイヤー以外のオブジェクト（敵/動く床/コア等）は動かす。
// IsUpdatePaused が真でも、ここで手動更新することで世界が止まって見えないようにする。
void GameScene::UpdateShowcaseWorld()
{
	// 更新中に敵が弾などを AddObject すると m_objList が変化してイテレータが壊れるため、
	// 先にスナップショットを取り、それを回す（新規オブジェクトは次フレームから動く）。
	std::vector<std::shared_ptr<KdGameObject>> snapshot(m_objList.begin(), m_objList.end());

	for (auto& obj : snapshot)
	{
		if (!obj || obj.get() == m_spPlayer.get()) { continue; }   // プレイヤーは固定
		obj->Update();
	}
	for (auto& obj : snapshot)
	{
		if (!obj || obj.get() == m_spPlayer.get()) { continue; }
		obj->PostUpdate();
	}
}

// 全フェーズ（再生可能なもの）の合計時間
float GameScene::ShowcaseTotalDuration() const
{
	float total = 0.0f;
	for (const auto& ph : m_showcaseEditor.GetPhases())
	{
		if (ph.eye.size() < 2) { continue; }
		const float moveDur = std::max(ph.duration / std::max(ph.speed, 0.01f), ShowcaseCamConst::MinDuration);
		total += moveDur + std::max(ph.hold, 0.0f);   // 移動＋末端で止まる時間
	}
	return total;
}

//----------------------------------------------------------
// 見せカメラ視点をオフスクリーンRTへ描く（2パス目）。DrawSpriteExtra から呼ぶ。
// 後処理(ブルーム/アウトライン)は省略した簡易プレビュー。
//----------------------------------------------------------
void GameScene::DrawShowcasePreview()
{
	if (!m_camPreviewOn || !m_showcaseEditor.HasPath()) { return; }

	// 初期化（RT＋プレビューカメラの射影）
	if (!m_camPreviewInit)
	{
		constexpr int W = 480, H = 270;   // 16:9
		m_camPreviewRT.CreateRenderTarget(W, H, true);
		m_camPreviewCam.SetProjectionMatrix(60.0f, 2000.0f, 0.01f,
			static_cast<float>(W) / static_cast<float>(H));
		m_camPreviewInit = true;
	}

	// タイムライン再生（再生中のみ進む。終端でループ）。一時停止/スクラブ中は止める
	const float total = ShowcaseTotalDuration();
	if (total <= 0.0f) { return; }
	if (m_camPreviewPlaying)
	{
		m_camPreviewTime += KdFPSController::GetDt();
		if (m_camPreviewTime >= total) { m_camPreviewTime = 0.0f; }
	}
	m_camPreviewTime = std::clamp(m_camPreviewTime, 0.0f, total - 0.0001f);

	Math::Matrix world;
	if (!EvalShowcaseCam(m_camPreviewTime, world)) { return; }
	m_camPreviewCam.SetCameraMatrix(world);

	// RT切替 → クリア → 見せカメラ視点で lit / effect を描く
	auto& sm = KdShaderManager::Instance();
	KdRenderTargetChanger changer;
	changer.ChangeRenderTarget(m_camPreviewRT);
	m_camPreviewRT.ClearTexture(Math::Color(0.02f, 0.03f, 0.06f, 1.0f));
	m_camPreviewCam.SetToShader();

	// 不透明3D描画のステート（深度ON）。終わったら元へ戻す。
	sm.ChangeDepthStencilState(KdDepthStencilState::ZEnable);
	sm.ChangeBlendState(KdBlendState::Alpha);

	auto& ss = sm.m_StandardShader;
	ss.BeginLit();
	for (auto& obj : m_objList) { obj->DrawLit(); }
	PlanetGravityManager::Instance().DrawLit();   // 惑星/Box（マップ地形）
	m_itemManager.DrawLit();                       // コイン等
	ss.EndLit();

	// 重力コアだけは見せたい（コアの描画はエフェクトパス側）。その他のエフェクトは省略。
	ss.BeginUnLit();
	for (auto& core : m_gravityCores) { if (core) { core->DrawEffect(); } }
	ss.EndUnLit();

	sm.UndoBlendState();
	sm.UndoDepthStencilState();
	changer.UndoRenderTarget();

	// メインカメラへ戻し、エフェクトパス(UnLit)状態へ復帰（この後 EndUnLit で閉じる）
	if (m_camera) { m_camera->SetToShader(); }
	ss.BeginUnLit();
}

//----------------------------------------------------------
// 見せカメラのプレビューを ImGui ウィンドウに表示
//----------------------------------------------------------
void GameScene::DrawShowcasePreviewWindow()
{
	if (ImGui::Begin(U8("見せカメラ プレビュー")))
	{
		ImGui::Checkbox(U8("プレビュー有効"), &m_camPreviewOn);
		ImGui::SameLine();
		if (ImGui::Button(U8("メインビューで再生")))   // メインビューで本番再生（テスト）
		{
			m_showcaseState    = ShowcaseState::Playing;
			m_showcaseTimer    = 0.0f;
			m_barCover         = 0.0f;
			m_showcaseSkipPrev = true;
			if (m_spPlayer) { m_spPlayer->SetDamageEnabled(false); }
		}
		ImGui::SameLine();
		if (ImGui::Button(U8("停止")))
		{
			m_showcaseState = ShowcaseState::Off; m_barCover = 0.0f;
			if (m_spPlayer) { m_spPlayer->SetDamageEnabled(true); }
		}

		if (!m_showcaseEditor.HasPath())
		{
			ImGui::TextDisabled(U8("(カメラ経路なし: 視点を2つ以上持つフェーズを追加)"));
			ImGui::End();
			return;
		}

		// プレビュー画像
		if (m_camPreviewOn && m_camPreviewInit
			&& m_camPreviewRT.m_RTTexture && m_camPreviewRT.m_RTTexture->WorkSRView())
		{
			const float aspect = 480.0f / 270.0f;
			float w = ImGui::GetContentRegionAvail().x;
			if (w < 64.0f) { w = 64.0f; }
			ImGui::Image((ImTextureID)(intptr_t)m_camPreviewRT.m_RTTexture->WorkSRView(),
				ImVec2(w, w / aspect));
		}

		// ── 再生バー（動画編集ソフト風）──
		const float total = ShowcaseTotalDuration();

		// 再生 / 一時停止 / 先頭
		if (ImGui::Button(m_camPreviewPlaying ? "Pause" : "Play")) { m_camPreviewPlaying = !m_camPreviewPlaying; }
		ImGui::SameLine();
		if (ImGui::Button(U8("|< 先頭"))) { m_camPreviewTime = 0.0f; }
		ImGui::SameLine();
		ImGui::Text(U8("%.2f / %.2f 秒"), m_camPreviewTime, total);

		// シーク（つまむと一時停止してスクラブ）
		ImGui::SetNextItemWidth(-1.0f);
		float t = m_camPreviewTime;
		if (ImGui::SliderFloat("##showcaseTime", &t, 0.0f, total, "%.2f s"))
		{
			m_camPreviewTime = std::clamp(t, 0.0f, total);
			m_camPreviewPlaying = false;   // スクラブ中は止める
		}

		// 各フェーズの境目を区切りとして表示（どのフェーズか把握用）
		{
			const auto& phases = m_showcaseEditor.GetPhases();
			float acc = 0.0f;
			std::string marks = "phases: ";
			for (int i = 0; i < static_cast<int>(phases.size()); ++i)
			{
				if (phases[i].eye.size() < 2) { continue; }
				acc += std::max(phases[i].duration, ShowcaseCamConst::MinDuration);
				char b[32]; std::snprintf(b, sizeof(b), "%d@%.1fs  ", i + 1, acc);
				marks += b;
			}
			ImGui::TextDisabled("%s", marks.c_str());
		}
	}
	ImGui::End();
}

void GameScene::StartIntroCutscene()
{
	if (!m_spPlayer) { return; }
	m_introCutscene = true;
	m_introTimer    = 0.0f;
	m_introSpin     = 0.0f;

	// ステージへ飛んでくるSE
	SoundManager::Instance().PlaySE(SeId::StageFlyIn, SoundConst::SeVolume);

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

		// 落下トレイル：位置履歴を先頭(最新)へ積む。古い分は末尾から捨てる
		m_introTrail.insert(m_introTrail.begin(), pos);
		if (static_cast<int>(m_introTrail.size()) > IntroConst::TrailLen)
		{
			m_introTrail.pop_back();
		}

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
			// 接地！共通の着地FX（つぶれ・揺れ・白フラッシュ・到着バナー）
			m_introCutscene = false;
			m_introTrail.clear();   // 落下トレイルは着地で消す

			m_spPlayer->SetPos(m_spawnPos);
			m_spPlayer->SetVelocity(Math::Vector3::Zero);
			m_spPlayer->SetCutsceneSpin(0.0f);
			m_spPlayer->SetCutsceneTumble(Math::Vector3::Zero);
			m_spPlayer->TriggerLandingSquash();       // バタン（つぶれ）
			if (m_pCamera) { m_pCamera->TriggerShake(IntroConst::LandShake); }  // 着地の揺れ
			TriggerFlash(IntroConst::LandFlash);

			// 着地の box パーティクル爆発（飛んできた着地は多め＆大きめ。滑り/ピタ両方）
			{
				const auto burstData = ModelManager::Instance().GetModel(PlayerConst::DustPath);
				auto landBurst = std::make_shared<StarBurstEffect>();
				landBurst->Spawn(m_spawnPos, m_spPlayer->GetUpDir(), burstData,
					IntroConst::LandBurstCountMul, IntroConst::LandBurstScaleMul);
				AddObject(landBurst);
			}

			m_arrivalTimer = 0.0f;   // 白フラッシュと同時に「〜にやってきた！」開始
			m_arrivalFitDone = false;// 表示時に画面幅へフィットさせ直す
			m_airTime = 0.0f;

			// ステージ1だけ：操作説明を出す（A/D/Spaceを全部押したら消える）
			if (StageManager::Instance().GetStageIndex() == IntroConst::Stage)
			{
				m_ctrlHintActive = true;
				m_ctrlHintDone   = false;
				m_ctrlHintAlpha  = 0.0f;
				m_ctrlGotA = m_ctrlGotD = m_ctrlGotJump = false;
			}

			// 滑るのは「初回起動の最初のステージ入り」だけ（オープニング）。
			// 一度滑ったらこの起動中は二度と滑らない（Stage1を再訪してもピタっと）。
			static bool s_introSkidUsed = false;
			const bool skidLanding =
				(StageManager::Instance().GetStageIndex() == IntroConst::Stage) && !s_introSkidUsed;
			if (skidLanding)
			{
				s_introSkidUsed = true;   // 初回の滑りを消費（以後はピタっと）
				// ズサー…バタンの着地リアクションへ
				m_introLanding   = true;
				m_introLandTimer = 0.0f;
				Math::Vector3 d = m_spawnPos - m_introStartPos; d.y = 0.0f;  // 横滑り方向＝水平の進行方向
				if (d.LengthSquared() > 1e-6f) { d.Normalize(); }
				m_landSkidDir = d;
			}
			else
			{
				// ピタっと：滑らずその場で操作復帰
				m_introLanding = false;
				m_spPlayer->Revive();                 // 操作ON・速度0・Idle・パラソル復帰
				m_spPlayer->SetPos(m_spawnPos);
				m_spPlayer->SetGravityScale(1.0f);
			}
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
	// 復活演出：ディゾルブを逆再生（溶けた状態から再構成して現れる）
	m_spPlayer->TriggerRespawnDissolve();
	SoundManager::Instance().PlaySE(SeId::Respawn, SoundConst::SeVolume);
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
	m_itemManager.ClearRockGems();
	m_itemManager.ClearThrownRocks();
	m_itemManager.Load();
	m_itemManager.LoadParasols();
	m_itemManager.LoadRockGems();

	// コインが復活するのに合わせて取得カウントも戻す
	m_coinTotal    = 0;
	m_coinPopTimer = 0.0f;
	// 重力コアも復活するのでカウントを戻す
	m_coreTotal    = 0;
	m_corePopTimer = 0.0f;

	// プレイタイムはリセットしない：クリアタイムは「ステージ入場〜クリアまでの
	// 総プレイ時間」。死亡→復活してもタイマーは継続する（死亡中は元々加算が
	// 止まるので、死亡演出ぶんは含まれない）。回数制限オーバーのリトライは
	// GameScene 自体が作り直されるため、その時だけ 0 から始まる。
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
		// 移動床へアタッチ（追従）：indexが有効なら対応する移動床を渡す
		if (data.attachFloor >= 0 && data.attachFloor < static_cast<int>(m_movingFloors.size()))
		{
			sp->SetAttachFloor(m_movingFloors[data.attachFloor]);
		}
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
		// 移動床へアタッチ（追従）：indexが有効なら対応する移動床を渡す
		if (data.attachFloor >= 0 && data.attachFloor < static_cast<int>(m_movingFloors.size()))
		{
			sp->SetAttachFloor(m_movingFloors[data.attachFloor]);
		}
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

	// シングルトンが保持するステージ依存リソース（モデル/テクスチャ）を解放しておく。
	// 次のステージ Load で再構築されるので、StageSelect 滞在中の常駐を減らせる。
	PlanetGravityManager::Instance().ClearPlanets();
	CoreliaManager::Instance().ClearNpcs();
	DeadZoneManager::Instance().ClearZones();
	ManualGravityZoneManager::Instance().ClearZones();
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

	// 導入カットシーン（全ステージ：上空から降下→着地で操作開始）
	// ただし惑星（着地できる地面）が1つも無いステージではスキップ（空中で固定カメラが止まるため）
	m_introPending = !PlanetGravityManager::Instance().GetPlanets().empty();

	// ステージ見せカメラ：経路が作られていれば、まずフライスルーを再生 → 終わったら導入落下へ
	m_showcaseEditor.Load();
	if (m_showcaseEditor.HasPath())
	{
		m_showcaseState = ShowcaseState::Playing;
		m_showcasePhase = 0;
		m_showcaseTimer = 0.0f;
		m_barCover      = 0.0f;
		if (m_spPlayer) { m_spPlayer->SetDamageEnabled(false); }   // 見せカメラ中は被弾しない
	}
	else if (m_introPending)
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

	// クリア演出のキメ文字「ステージクリアー！」用フォント
	KdFontManager::Instance().AddFont(ClearConst::BannerFontNo, FontConst::GameFontName, ClearConst::BannerFontH);
	// 着地後「〜にやってきた！」用フォント
	KdFontManager::Instance().AddFont(IntroConst::ArrivalFontNo, FontConst::GameFontName, IntroConst::ArrivalFontH);

	// コアリア（ヒントNPC）：日本語フォント登録＋配置/ヒント読み込み
	KdFontManager::Instance().AddFont(CoreliaConst::FontNo, CoreliaConst::FontName, CoreliaConst::FontHeight);
	// 「話す」プロンプト用の小さめフォント
	KdFontManager::Instance().AddFont(CoreliaConst::PromptFontNo, CoreliaConst::FontName, CoreliaConst::PromptFontH);
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

	// ポーズメニュー用フォント（項目＋バナータイトル）
	KdFontManager::Instance().AddFont(PauseMenuConst::FontNo, PauseMenuConst::FontName, PauseMenuConst::FontHeight);
	KdFontManager::Instance().AddFont(PauseMenuConst::TitleFontNo, PauseMenuConst::FontName, PauseMenuConst::TitleFontH);

	// ポーズ中の背景ぼかし用RT（バックバッファと同サイズ）
	{
		const auto& bbTex = KdDirect3D::Instance().GetBackBuffer();
		if (bbTex)
		{
			m_pauseBlurRT.CreateRenderTarget(bbTex->GetWidth(), bbTex->GetHeight());
			m_pauseBlurInit = (m_pauseBlurRT.m_RTTexture != nullptr);
		}
	}

	// クリア暗転用アイリスマスク（マリオ風の閉じる円）
	m_irisMaskTex = std::make_shared<KdTexture>();
	m_irisMaskTex->Load(ClearConst::IrisMaskPath);

	// 低HP時の黒ビネット（無ければ描画スキップ）
	m_vignetteTex = std::make_shared<KdTexture>();
	if (!m_vignetteTex->Load(UIConst::LowHpVignetteTex)) { m_vignetteTex = nullptr; }

	// 落下トレイルの繋ぎ目を隠す足元glow
	m_footGlowTex = std::make_shared<KdTexture>();
	if (m_footGlowTex->Load(IntroConst::TrailFootGlowTex))
	{
		m_footGlowPoly.SetMaterial(m_footGlowTex);
		m_footGlowPoly.SetScale(1.0f);
	}

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

	// コイン・パラソル・カラフル岩アイテム読み込み
	m_itemManager.Load();
	m_itemManager.LoadParasols();
	m_itemManager.LoadRockGems();

	// ※BGMはここでは流さない。飛来イントロ／ショーケース中は無音で、
	//   着地して通常プレイに入ったら Event 側で流し始める（全ステージ共通）。
	//   ただし着地時にロードでカクつかないよう、ここで事前ロード（デコード）しておく。
	{
		auto& sound = SoundManager::Instance();
		const int stageId0 = StageManager::Instance().GetStageIndex() - 1;
		sound.Preload(SoundConst::BgmForStage(stageId0));
		sound.Preload(SoundConst::BgmShowcase);   // ショーケースBGMも先読み
		// 登録SE（差し替え後の割り当てを含む）を事前ロード（初回再生のカクつき防止）
		sound.PreloadAllSE();
	}
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
	m_spawnPos = { vals[0], vals[1], vals[2] };

	if (i >= 6)
	{
		// intro 開始位置が保存済み（旧Stage1など）→ それを使う
		m_introStartPos = { vals[3], vals[4], vals[5] };
	}
	else
	{
		// 未設定ステージは「スポーン直上 Height」から降らせる（全ステージで飛んでくる演出）
		m_introStartPos = m_spawnPos + Math::Vector3(0.0f, IntroConst::Height, 0.0f);
	}
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

