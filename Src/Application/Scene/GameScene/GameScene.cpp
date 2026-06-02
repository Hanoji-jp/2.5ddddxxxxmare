#include "GameScene.h"
#include"../SceneManager.h"
#include"../../Const/LightConst.h"
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
	// 惑星のワールド行列を毎フレーム更新（エディターで位置変更した場合にも追従）
	PlanetGravityManager::Instance().PostUpdate();
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
			m_pCamera->Update(m_spPlayer->GetPos(), m_spPlayer->GetUpDir());
		}
	}

	// ── エディタ Dirty チェック（モードに関わらず毎フレーム反映）──────
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

				// 吸い込み完了 → パス移動へ
				if (m_warpSuckProgress >= 1.0f)
				{
					m_warpPhase       = WarpPhase::Traveling;
					m_warpSegment     = 0;
					m_warpSegProgress = 0.0f;

					// 移動曲線はトンネルのビジュアル中心線（既にスプラインで
					// 滑らかに曲げ済み）をそのまま使う。こうすると見えている
					// トンネルとプレイヤーの飛行軌道が完全に一致する。
					m_warpCurve = m_warpPath;
					m_warpCurveTotalLen = 0.0f;
					for (int i = 0; i + 1 < static_cast<int>(m_warpCurve.size()); ++i)
					{
						m_warpCurveTotalLen += (m_warpCurve[i + 1] - m_warpCurve[i]).Length();
					}
					m_warpDist = 0.0f;

					m_spPlayer->SetWarpStretch(true);   // Traveling 中だけストレッチON
				}
			}
			else if (m_warpPhase == WarpPhase::Traveling)
			{
				// ── フェーズ2：パス移動（弧長ベースの可変速移動）──
				// マリオギャラクシーのランチスター風：
				//   発射直後にドンッと最高速 → 緩やかに巡航速度へ落ち着く。
				//   速度は「曲線全長に対する進行割合」で決まるため、
				//   ウェイポイント境界で不連続にならず止まって見えない。
				const float progress = (m_warpCurveTotalLen > 1e-4f)
					? (m_warpDist / m_warpCurveTotalLen)
					: 1.0f;

				// 発射直後(0)で 1.0 → BlendDist で 0.0 になる減衰係数（OutExpo 的）
				float launchT = 1.0f;
				if (WarpHoleConst::WarpLaunchBlendDist > 1e-4f)
				{
					launchT = 1.0f - std::clamp(progress / WarpHoleConst::WarpLaunchBlendDist, 0.0f, 1.0f);
				}
				// OutExpo：序盤の落ち方を急に、終盤を緩やかに
				const float launchEase = launchT * launchT;

				const float speedMul = WarpHoleConst::WarpCruiseSpeedMul
					+ (WarpHoleConst::WarpLaunchSpeedMul - WarpHoleConst::WarpCruiseSpeedMul) * launchEase;

				m_warpDist += WarpHoleConst::WarpMoveSpeed * speedMul * dt;

				if (m_warpDist >= m_warpCurveTotalLen)
				{
					// ── ワープ完了 ──
					m_warpPhase = WarpPhase::None;
					m_spPlayer->ClearWarpUpOverride();
					m_spPlayer->SetPos(m_warpCurve.empty() ? m_warpPath.back() : m_warpCurve.back());
					Math::Vector3 dir = m_warpExitDir;
					dir.Normalize();
					m_spPlayer->SetVelocity(dir * WarpHoleConst::LaunchSpeed);
				}
				else
				{
					Math::Vector3 travelDir;
					const Math::Vector3 pos = SampleCurveByDistance(m_warpCurve, m_warpDist, travelDir);
					m_spPlayer->SetPos(pos);
					m_spPlayer->SetVelocity(Math::Vector3::Zero);

					// 進行方向（曲線の接線）に Slerp で頭を向ける
					if (travelDir.LengthSquared() > 0.0001f)
					{
						m_spPlayer->SetWarpUpOverride(travelDir, WarpHoleConst::WarpRotSlerpSpeed);
					}
				}
			}
			else
			{
				// ── 通常：吸い込み範囲チェック ──
				const Math::Vector3 playerPos = m_spPlayer->GetPos();
				for (auto& wh : m_warpHoles)
				{
					if (!wh->GetData().Enabled) { continue; }
					const Math::Vector3 toEntry = wh->GetData().EntryPos - playerPos;
					if (toEntry.LengthSquared() <= WarpHoleConst::SuckPullRadius * WarpHoleConst::SuckPullRadius)
					{
						m_warpPhase          = WarpPhase::Sucking;
							// プレイヤーをトンネルのビジュアル中心線に沿って動かす。
							// こうすると「見えているトンネルの中」を必ず通る。
							m_warpPath           = wh->BuildTunnelCenterPath();
							m_warpExitDir        = wh->GetData().ExitDir;
							m_warpEntryPos       = wh->GetData().EntryPos;
							m_warpSuckStartPos   = playerPos;
							m_warpSuckProgress   = 0.0f;
							m_warpSuckStartAngle = atan2f(
								playerPos.z - wh->GetData().EntryPos.z,
								playerPos.x - wh->GetData().EntryPos.x);
						break;
					}
				}
			}
		}

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
	CameraSettings::Instance().DrawGui();
	PlanetGravityManager::Instance().DrawGui();
	ManualGravityZoneManager::Instance().DrawGui();

	// 太陽光（ディレクショナルライト）エディター
	if (ImGui::Begin("Sun Light"))
	{
		auto& ambient = KdShaderManager::Instance().WorkAmbientController();

		static float s_dirLightDir[3]   = { LightConst::DirLightDir.x,   LightConst::DirLightDir.y,   LightConst::DirLightDir.z };
		static float s_dirLightColor[3] = { LightConst::DirLightColor.x, LightConst::DirLightColor.y, LightConst::DirLightColor.z };
		static float s_ambientColor[4]  = { LightConst::AmbientColor.x,  LightConst::AmbientColor.y,  LightConst::AmbientColor.z, LightConst::AmbientColor.w };

		// 平行光の方向
		if (ImGui::DragFloat3("Direction", s_dirLightDir, 0.01f, -1.0f, 1.0f))
		{
			Math::Vector3 dir = { s_dirLightDir[0], s_dirLightDir[1], s_dirLightDir[2] };
			dir.Normalize();
			ambient.SetDirLight(dir, { s_dirLightColor[0], s_dirLightColor[1], s_dirLightColor[2] });
		}

		// 平行光の色
		if (ImGui::ColorEdit3("Sun Color", s_dirLightColor))
		{
			Math::Vector3 dir = { s_dirLightDir[0], s_dirLightDir[1], s_dirLightDir[2] };
			ambient.SetDirLight(dir, { s_dirLightColor[0], s_dirLightColor[1], s_dirLightColor[2] });
		}

		// 環境光の色と強度（アルファが全体の明るさ）
		if (ImGui::ColorEdit4("Ambient Color", s_ambientColor))
		{
			ambient.SetAmbientLight({ s_ambientColor[0], s_ambientColor[1], s_ambientColor[2], s_ambientColor[3] });
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(A=Intensity)");
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
	for (auto& e : m_enemies)
	{
		e->Expire();
	}
	m_enemies.clear();

	// EnemyPlacementEditor のデータから敵を生成
	for (const auto& data : m_enemyEditor.GetPlacements())
	{
		std::shared_ptr<Enemy> spEnemy;

		if (data.type == EnemyType::Melee)
		{
			auto sp = std::make_shared<EnemyMelee>();
			sp->SetPos(data.position);
			sp->Init();
			spEnemy = sp;
		}
		else
		{
			auto sp = std::make_shared<EnemyRanged>();
			sp->SetPos(data.position);
			sp->Init();
			spEnemy = sp;
		}

		if (m_spPlayer) { spEnemy->SetTarget(m_spPlayer); }

		m_enemies.push_back(spEnemy);
		AddObject(spEnemy);
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
		m_warpHoles.push_back(sp);
		AddObject(sp);
	}
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
	ambient.SetAmbientLight(LightConst::AmbientColor);
	ambient.SetDirLight(LightConst::DirLightDir, LightConst::DirLightColor);

	// 影（シャドウマップ）の描画範囲を広げる
	ambient.SetDirLightShadowArea(LightConst::ShadowAreaSize, LightConst::ShadowAreaHeight);

	// HP UI を常時表示するためゲーム開始時からコールバックをセット
	KdDebugGUI::Instance().SetGuiCallback([this] { DrawGui(); });
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
