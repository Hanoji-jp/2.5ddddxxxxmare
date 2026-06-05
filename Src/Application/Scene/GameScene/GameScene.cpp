#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Const/LightConst.h"
#include"../../Const/JuiceConst.h"
#include"../../Manager/ModelManager.h"
#include"../../../Framework/Utility/KdDebug/KdDebugGUI.h"
#include"../../../Framework/Math/KdEasing.h"
#include <fstream>
#include <sstream>

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
	// 惑星のワールド行列を毎フレーム更新
	PlanetGravityManager::Instance().PostUpdate();

	// ── テレポート出現スケールポップ ─────────────────────────
	if (m_spPlayer && m_teleportPopTimer > 0.0f)
	{
		constexpr float kDt  = 1.0f / 60.0f;
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

			// HP UIはゲーム中も常時表示するのでコールバックは維持
			KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
		}
	}
	m_f2Prev = f2Now;

	// カメラ更新
	if (m_editorMode)
	{
		if (m_pEditorCam) { m_pEditorCam->Update(); }
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
				KdDebugGUI::Instance().AddLog("[ZOOM] TriggerPlanetZoom fired!\n");
			}
			// フラグを読み終えたのでここでリセット
			m_spPlayer->ResetPlanetChangedFlag();
			m_pCamera->Update(m_spPlayer->GetPos(), m_spPlayer->GetUpDir());
		}
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

	// ── ゲームモード時のみ実行 ──────────────────────────
	if (!m_editorMode)
	{
		// ── ワープホール判定 ─────────────────────────────
		if (m_spPlayer && !m_spPlayer->IsExpired())
		{
			const float dt = 1.0f / 60.0f;

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

		// ── 落下死チェック ──────────────────────────────
		// 一旦無効化
		/*
		if (m_spPlayer && !m_spPlayer->IsExpired())
		{
			if (m_spPlayer->GetPos().y < CheckpointConst::DeathY)
			{
				Respawn();
			}
		}
		*/

		// ── チェックポイント更新 ─────────────────────────
		for (auto& cp : m_checkpoints)
		{
			if (cp->IsActivated())
			{
				// このチェックポイントを有効化、他を無効化
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
				if (m_spHpUI) { m_spHpUI->TriggerShake(); }
				if (m_pCamera) { m_pCamera->TriggerShake(JuiceConst::ShakeHitStr); }
			}
			m_prevPlayerHp = curHp;
		}

		// ── FootDust（足元煙パーティクル）──
		{
			constexpr float kDt = 1.0f / 60.0f;
			m_dustTimer -= kDt;

			const Math::Vector3 vel   = m_spPlayer->GetVelocity();
			const float         speed = Math::Vector3(vel.x, vel.y, 0.0f).Length();

			if (speed >= JuiceConst::DustSpeedMin && m_dustTimer <= 0.0f)
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

				// ダッシュ時は1フレームに複数個スポーン
				const int spawnCount = isDash ? 5 : 2;
				for (int i = 0; i < spawnCount; ++i)
				{
					auto dust = std::make_shared<FootDust>();
					dust->Spawn(m_spPlayer->GetPos(), backDir, upDir, dustData);
					AddObject(dust);
				}
			}
		}

		// ── アイテム更新・取得判定 ──────────────────────
		m_itemManager.Update(m_spPlayer->GetPickupHitBox());
		m_itemManager.Refresh();
	}
}

void GameScene::DrawSpriteExtra()
{
	const auto& bb    = KdDirect3D::Instance().GetBackBuffer();
	const int screenW = static_cast<int>(bb->GetInfo().Width);
	const int screenH = static_cast<int>(bb->GetInfo().Height);

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
		constexpr float kDt = 1.0f / 60.0f;
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

	// ── 引力コンパスUI ──
	if (m_spPlayer && m_pCamera)
	{
		const GravityInfluenceResult gravResult =
			PlanetGravityManager::Instance().ComputeGravityInfluence(m_spPlayer->GetPos());

		if (gravResult.hasInfluence)
		{
			// 引力方向をスクリーン2D方向に投影（Z成分は無視）
			const Math::Vector3& gDir = gravResult.totalGravityDir;

			// プレイヤーのスクリーン座標を取得
			Math::Vector3 playerScreenPos;
			m_pCamera->ConvertWorldToScreenDetail(m_spPlayer->GetPos(), playerScreenPos);

			{
				constexpr float kCompassRadius = 28.0f;  // プレイヤーUIからの距離
				constexpr int   kArrowW        = 8;
				constexpr int   kArrowH        = 14;

				// ワールドY軸がスクリーン上で反転するため Y を反転
				const float sx = gDir.x;
				const float sy = -gDir.y;
				const float len = std::sqrtf(sx * sx + sy * sy);
				if (len > 0.001f)
				{
					const float nx = sx / len;
					const float ny = sy / len;

					const int cx = static_cast<int>(playerScreenPos.x + nx * kCompassRadius);
					const int cy = static_cast<int>(playerScreenPos.y + ny * kCompassRadius);

					// 引力方向を示す小さな矩形インジケーター
					const Math::Color compassCol(0.3f, 0.9f, 1.0f, 0.85f);
					KdShaderManager::Instance().m_spriteShader.DrawBox(
						cx, cy, kArrowW, kArrowH, &compassCol, true);
				}
			}
		}
	}
}

void GameScene::DrawDebugExtra()
{
	// 手動重力ゾーンは常時表示
	ManualGravityZoneManager::Instance().DrawDebugShapes();

	if (m_editorMode)
	{
		m_roomEditor.DrawDebugLines();
		m_enemyEditor.DrawDebugSpheres();
		m_checkpointEditor.DrawDebugSpheres();
		m_warpHoleEditor.DrawDebug();
		m_movingFloorEditor.DrawDebug();
		PlanetGravityManager::Instance().DrawDebugShapes();
	}
}

void GameScene::DrawUnLitExtra()
{
	ManualGravityZoneManager::Instance().DrawUnLit();
}

void GameScene::DrawLitExtra()
{
	PlanetGravityManager::Instance().DrawLit();
	m_itemManager.DrawLit();
}

void GameScene::DrawGui()
{
	// HP UI は常時表示（エディターモード問わず）
	if (m_spHpUI) { m_spHpUI->DrawGui(); }

	// プレイヤーコリジョンデバッグ
	if (m_spPlayer) { m_spPlayer->DrawCollisionDebugGui(); }

	// エディターモード時のみエディターGUIを表示
	m_roomEditor.DrawGui();
	m_enemyEditor.DrawGui();
	m_checkpointEditor.DrawGui();
	m_warpHoleEditor.DrawGui();
	m_movingFloorEditor.DrawGui();
	CameraSettings::Instance().DrawGui();
	PlanetGravityManager::Instance().DrawGui();
	ManualGravityZoneManager::Instance().DrawGui();

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
		if (ImGui::Button("Save"))
		{
			SaveSpawn();
		}
	}
	ImGui::End();
}

void GameScene::Respawn()
{
	if (!m_spPlayer) { return; }
	m_spPlayer->SetPos(m_respawnPos);
	// HPを全回復（Player側にリセット関数があれば呼ぶ）
	m_spPlayer->Init();
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
			sp->SetFaceDir(data.cubunFaceDir);
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

void GameScene::Init()
{
	// カメラ（BaseSceneのm_cameraに所有権を渡し、観察用ポインタだけ保持）
	auto upCamera  = std::make_unique<SideScrollCamera>();
	m_pCamera      = upCamera.get();
	m_camera       = std::move(upCamera);

	// プレイヤー
	m_spPlayer = std::make_shared<Player>();
	LoadSpawn();
	m_spPlayer->SetPos(m_spawnPos);
	AddObject(m_spPlayer);

	// リスポーン座標の初期値をスポーン座標と揃える
	m_respawnPos = m_spawnPos;

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

	// 手動重力ゾーン読み込み
	ManualGravityZoneManager::Instance().Load();
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

	// HP UI を常時表示するためゲーム開始時からコールバックをセット
	KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });

	// 移動床エディター → 移動床生成
	RebuildMovingFloors();
	m_movingFloorEditor.ClearDirty();

	}

void GameScene::SaveSpawn()
{
	std::ofstream ofs(SpawnConst::SavePath);
	if (!ofs) { return; }
	ofs << m_spawnPos.x << "," << m_spawnPos.y << "," << m_spawnPos.z << "\n";
}

void GameScene::LoadSpawn()
{
	std::ifstream ifs(SpawnConst::SavePath);
	if (!ifs) { return; }

	std::string line;
	if (!std::getline(ifs, line)) { return; }

	std::istringstream ss(line);
	std::string token;
	float vals[3] = { SpawnConst::DefaultX, SpawnConst::DefaultY, SpawnConst::DefaultZ };
	int i = 0;
	while (std::getline(ss, token, ',') && i < 3)
	{
		vals[i++] = std::stof(token);
	}
	m_spawnPos = { vals[0], vals[1], vals[2] };
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
	std::ofstream ofs(LightConst::SavePath);
	if (!ofs) { return; }

	// 方向(xyz), 太陽色(rgb), 環境光(rgba)
	ofs << m_sunDir.x   << "," << m_sunDir.y   << "," << m_sunDir.z   << ","
		<< m_sunColor.x << "," << m_sunColor.y << "," << m_sunColor.z << ","
		<< m_ambientColor.x << "," << m_ambientColor.y << ","
		<< m_ambientColor.z << "," << m_ambientColor.w << "\n";
}

void GameScene::LoadSunLight()
{
	std::ifstream ifs(LightConst::SavePath);
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
