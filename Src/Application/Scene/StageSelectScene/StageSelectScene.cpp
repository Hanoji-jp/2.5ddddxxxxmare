#include "StageSelectScene.h"
#include "../SceneManager.h"
#include "../../GameObject/BackGround/BackGround.h"
#include "../../GameObject/BackGround/StarField.h"
#include "../../GameObject/Light/PointLightObject.h"
#include "../../GameObject/Effect/EffectBase.h"
#include "../../Const/StageSelectConst.h"

using namespace StageSelectConst;

//----------------------------------------------------------
void StageSelectScene::Init()
{
	// ── カメラ（横スクロール追従。Event で毎フレーム行列を組む）──
	auto cam = std::make_shared<KdCamera>();
	cam->SetProjectionMatrix(CamFov);
	m_camera = cam;

	// ── 床（静的 MovingFloor。Range=0 で動かさない）──
	m_spFloor = std::make_shared<MovingFloor>();
	m_spFloor->SetCenter({ HubOriginX, FloorCenterY, 0.0f });
	m_spFloor->SetSize({ FloorHalfX, FloorHalfY, FloorHalfZ });
	m_spFloor->SetRange(0.0f);
	m_spFloor->Init();
	AddObject(m_spFloor);

	// ── プレイヤー（フル物理を再利用）──
	// 「吹き飛ばされて投げ出される」：上空から落下させ、不時着するまで操作不能。
	m_spPlayer = std::make_shared<Player>();
	m_spPlayer->SetPos({ HubOriginX + IntroStartLocalX, FloorTopY + IntroStartHeight, 0.0f });
	m_spPlayer->SetHubGravityDown();   // 惑星がないので手動重力Downで落下・歩行
	m_spPlayer->SetControlEnabled(false);                       // 着地まで操作不能
	m_spPlayer->SetVelocity({ IntroThrowVX, IntroThrowVY, 0.0f }); // 吹き飛ばし初速
	{
		std::vector<std::weak_ptr<MovingFloor>> floors = { m_spFloor };
		m_spPlayer->SetMovingFloorObjects(floors);
	}
	AddObject(m_spPlayer);

	// ── 宇宙背景 ──
	AddObject(std::make_shared<BackGround>());
	AddObject(std::make_shared<StarField>());

	// ── プレイヤー＆床を照らす点光源 ──
	{
		auto light = std::make_shared<PointLightObject>();
		light->SetPos({ HubOriginX, FloorTopY + 10.0f, -6.0f });
		light->SetColor({ 1.0f, 0.95f, 0.85f });
		light->SetRadius(80.0f);
		AddObject(light);
	}

	// ── ポータル（ステージ入口）──
	m_portalPos = { HubOriginX + PortalLocalX, FloorTopY + PortalHeight, 0.0f };
	m_portalTex = std::make_shared<KdTexture>();
	if (m_portalTex->Load(PortalTexPath))
	{
		m_portalPoly.SetMaterial(m_portalTex);
		m_portalPoly.SetScale(1.0f);
	}

	// ── フォント ──
	KdFontManager::Instance().AddFont(FontNo, FontName, FontHeight);
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

	// ── 投げ出され演出（落下→不時着→通常カメラ復帰）──
	if (m_introActive)
	{
		m_introElapsed += dt;

		// 絵本の黒から明ける
		m_introFade -= IntroFadeSpeed * dt;
		if (m_introFade < 0.0f) { m_introFade = 0.0f; }

		if (!m_landed)
		{
			// 落下中：くるくる回す（タンブル）
			m_introSpin += IntroSpinSpeed * dt;

			// 不時着判定
			if (m_spPlayer && m_spPlayer->IsGround())
			{
				m_landed = true;
				m_shake  = IntroShakeStr;   // 着地の衝撃で揺らす
			}
		}
		else
		{
			// 着地後：回転を 0 へ戻す（起き上がる）
			m_introSpin += (0.0f - m_introSpin) * std::min(IntroSpinSettle * dt, 1.0f);

			// カメラを通常へ戻し、終わったら操作可能に
			m_settle += IntroSettleSpeed * dt;
			if (m_settle >= 1.0f)
			{
				m_settle      = 1.0f;
				m_introActive = false;
				m_introSpin   = 0.0f;
				if (m_spPlayer) { m_spPlayer->SetControlEnabled(true); }
			}
		}

		// プレイヤーへタンブル角を反映
		if (m_spPlayer) { m_spPlayer->SetCutsceneSpin(m_introSpin); }

		// 揺れ減衰
		if (m_shake > 0.0f) { m_shake -= IntroShakeDecay * dt; if (m_shake < 0.0f) { m_shake = 0.0f; } }
	}

	// カメラ：落下旋回カメラ(b=0) ⇄ 通常横スクロール(b=1) をブレンド
	if (m_spPlayer)
	{
		const Math::Vector3 pp = m_spPlayer->GetPos();

		// 通常カメラ（X だけ追う・床基準）
		const Math::Vector3 normalEye    = { pp.x, FloorTopY + CamOffsetY, CamOffsetZ };
		const Math::Vector3 normalTarget = { pp.x, FloorTopY + CamTargetY, 0.0f };

		// 落下追従カメラ：プレイヤーの周りを旋回（ぐりぐり）しながら追う
		const float az = DirectX::XMConvertToRadians(IntroOrbitStartDeg + IntroOrbitSpeedDeg * m_introElapsed);
		const Math::Vector3 fallEye =
		{
			pp.x + std::sinf(az) * IntroOrbitRadius,
			pp.y + IntroFallCamOffsetY,
			pp.z + std::cosf(az) * IntroOrbitRadius,
		};
		const Math::Vector3 fallTarget = { pp.x, pp.y + IntroFallTargetY, 0.0f };

		// ブレンド係数：演出中で未着地=0、着地後は settle、演出後=1
		float b = 1.0f;
		if (m_introActive)
		{
			const float s = m_landed ? m_settle : 0.0f;
			b = s * s * (3.0f - 2.0f * s);   // smoothstep
		}

		Math::Vector3 eye    = Math::Vector3::Lerp(fallEye,    normalEye,    b);
		Math::Vector3 target = Math::Vector3::Lerp(fallTarget, normalTarget, b);

		// 不時着の揺れ
		if (m_shake > 0.0f)
		{
			auto rnd = []() { return (static_cast<float>(std::rand()) / RAND_MAX) * 2.0f - 1.0f; };
			eye.x += rnd() * m_shake;
			eye.y += rnd() * m_shake;
		}

		Math::Vector3 f = target - eye; f.Normalize();          // 前(+Z=視線)
		Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize();
		Math::Vector3 u = f.Cross(r);
		Math::Matrix world;
		world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
		world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
		world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
		world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
		m_camera->SetCameraMatrix(world);
	}

	// ポータル接触 → 入場フェード開始（演出が終わって操作可能になってから）
	if (!m_introActive && !m_entering && m_spPlayer)
	{
		const Math::Vector3 d = m_spPlayer->GetPos() - m_portalPos;
		// 2.5D なので XY 距離で判定（Z は共通）
		const float dist = std::sqrtf(d.x * d.x + d.y * d.y);
		if (dist < PortalTriggerRange) { m_entering = true; }
	}

	// 入場フェード（白へ）→ 完了でゲームへ
	if (m_entering)
	{
		m_fadeAlpha += FadeOutSpeed * dt;
		if (m_fadeAlpha >= 1.0f)
		{
			m_fadeAlpha = 1.0f;
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Game);
		}
	}
}

//----------------------------------------------------------
void StageSelectScene::DrawEffectExtra()
{
	if (!m_portalTex) { return; }

	auto& sm = KdShaderManager::Instance();
	sm.ChangeBlendState(KdBlendState::Add);
	sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	sm.m_StandardShader.SetDissolve(0.0f);

	// 明滅＋回転するグロー
	const float pulse = 1.0f + PortalPulseAmp * std::sinf(m_timer * PortalPulseSpeed);
	const float spin  = m_timer * PortalSpinSpeed;

	// 外側の柔らかいグロー
	{
		const Math::Color   col{ PortalColR, PortalColG, PortalColB, 0.5f };
		const Math::Vector3 em { PortalColR, PortalColG, PortalColB };
		EffectBase::DrawBillboard(m_portalPoly, m_portalPos, PortalSize * 1.6f * pulse, -spin, col, em);
	}
	// 内側の明るいコア
	{
		const Math::Color   col{ 1.0f, 1.0f, 1.0f, 0.9f };
		const Math::Vector3 em { PortalColR * 1.5f, PortalColG * 1.5f, PortalColB * 1.5f };
		EffectBase::DrawBillboard(m_portalPoly, m_portalPos, PortalSize * pulse, spin, col, em);
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

	// 中央上に見出し（投げ出され演出が終わってから表示）
	if (!m_introActive)
	{
		const Math::Color col(1.0f, 1.0f, 1.0f, 0.9f);
		auto measure = KdFontManager::Instance().CreateFontTexture(FontNo, TitleText, false);
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
		const Math::Vector2 pos(-textW * 0.5f, sh * TitleYRatio - textH * 0.5f);
		sprite.DrawFont(pos, &col, "%s", TitleText);
	}

	// ポータルに近づいたらヒントを点滅表示（演出後のみ）
	if (!m_introActive && !m_entering && m_spPlayer)
	{
		const Math::Vector3 d = m_spPlayer->GetPos() - m_portalPos;
		const float dist = std::sqrtf(d.x * d.x + d.y * d.y);
		if (dist < PortalHintRange)
		{
			const float blink = 0.4f + 0.6f * (0.5f + 0.5f * std::sinf(m_timer * 4.0f));
			const Math::Color col(1.0f, 1.0f, 1.0f, blink);
			auto measure = KdFontManager::Instance().CreateFontTexture(FontNo, HintText, false);
			float textW = 0.0f, textH = 0.0f;
			if (measure)
			{
				for (const auto& dd : measure->GetTexList())
				{
					if (!dd || !dd->FontTex) { continue; }
					textW += static_cast<float>(dd->FontTex->GetInfo().Width);
					textH  = std::max(textH, static_cast<float>(dd->FontTex->GetInfo().Height));
				}
			}
			const Math::Vector2 pos(-textW * 0.5f, -sh * HintYRatio - textH * 0.5f);
			sprite.DrawFont(pos, &col, "%s", HintText);
		}
	}

	// 投げ出され演出の黒フェードイン（絵本の黒から明ける）。最前面。
	if (m_introFade > 0.0f)
	{
		const Math::Color black(0.0f, 0.0f, 0.0f, m_introFade);
		sprite.DrawBox(0, 0, sw, sh, &black, true);
	}

	// 入場フェード（白）。GameScene の白フェードインへ繋ぐ。
	if (m_fadeAlpha > 0.0f)
	{
		const Math::Color flash(1.0f, 1.0f, 1.0f, m_fadeAlpha);
		sprite.DrawBox(0, 0, sw, sh, &flash, true);
	}
}
