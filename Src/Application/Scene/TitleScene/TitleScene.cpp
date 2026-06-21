#include "TitleScene.h"
#include "../SceneManager.h"
#include "../../GameObject/BackGround/BackGround.h"
#include "../../GameObject/BackGround/StarField.h"
#include "../../GameObject/Light/PointLightObject.h"
#include "../../GameObject/Effect/EffectBase.h"
#include "../../Const/PlayerConst.h"
#include "../../Const/CubunConst.h"
#include "../../Const/FontConst.h"
#include "../../Util/TextFx.h"

namespace
{
	// ── ロゴ / プロンプト ──
	constexpr const char* kLogoPath       = "Asset/Texture/TitleLogo/Corelia.png";
	constexpr float        kLogoWidthRatio = 0.5f;
	constexpr float        kLogoYRatio     = 0.22f;
	constexpr float        kBobAmpRatio    = 0.01f;
	constexpr float        kBobSpeed       = 1.6f;
	constexpr float        kBlinkSpeed     = 3.2f;
	constexpr float        kPromptYRatio   = 0.36f;
	// 登場演出のタイミング
	constexpr float        kIntroDur       = 0.7f;   // ロゴが出そろうまで
	constexpr float        kFlashDur       = 0.35f;  // 白フラッシュ時間
	constexpr float        kBloomDur       = 1.4f;   // 登場時の追加 bloom が消えるまで
	constexpr float        kPromptIn       = 0.9f;   // PRESS ENTER が出始める時刻
	// 常時 bloom（一定間隔で強弱を付けて脈動させる）
	constexpr float        kBloomBase      = 0.45f;  // 常に焚くベース強度
	constexpr float        kBloomPulseAmp  = 0.45f;  // 脈動の振幅
	constexpr float        kBloomPulseSpeed= 2.2f;   // 脈動の速さ
	// Enter 演出（むっちゃ光って小さくしてから遷移）
	constexpr float        kStartDur       = 0.5f;   // 演出時間
	constexpr float        kStartBloom     = 4.0f;   // Enter 時の最大 bloom
	constexpr float        kStartShrink    = 0.85f;  // どれだけ縮むか(0.85=15%まで)
	constexpr int          kFontNo         = 0;
	constexpr int          kFontHeight     = 44;
	constexpr const char*  kFontName       = FontConst::GameFontName;
	constexpr const char*  kPromptText     = "PRESS ENTER";

	// ── カメラ（-Z 側から +Z を見る＝+X が画面右で直感的）──
	const     Math::Vector3 kCamEye        = { 0.0f, 8.0f, -25.0f };
	const     Math::Vector3 kCamTarget     = { 0.0f, 2.0f,   0.0f };
	constexpr float         kCamSwayX      = 1.5f;
	constexpr float         kCamSwayY      = 0.8f;
	constexpr float         kCamSwaySpeed  = 0.18f;
	constexpr float         kCamFov        = 55.0f;

	// ── 漂うキャラ（流れ星のように右上→左下へフワ〜と流れ、くるくる回る）──
	constexpr float kCubunScale   = 0.55f;
	constexpr float kCoreliaScale = 0.6f;
	constexpr float kCharSpreadX  = 42.0f;   // 横の広がり（画面外で長く出入り）
	constexpr float kCharSpreadY  = 25.0f;   // 縦の広がり（画面外で長く出入り）
	constexpr float kCharDrift    = 0.03f;   // 流れる速さ(ループ/秒。距離が伸びた分ゆっくり)
	constexpr float kCubunZ       = -4.0f;   // 奥行き（カメラ視線上＝target付近〜手前）
	constexpr float kCoreliaZ     = 2.0f;

	// ── 流れ星（重力コア彗星）──
	constexpr const char* kCometCoreTex = "Asset/Effect/Particle04_clear_hard.png";
	constexpr const char* kCometGlowTex = "Asset/Effect/Particle03.png";
	constexpr int   kCometCount    = 5;
	constexpr int   kTrailLen      = 16;     // 尾の履歴数
	constexpr float kCometSpeedMin = 22.0f;  // 速い
	constexpr float kCometSpeedMax = 36.0f;
	constexpr float kCometHeadMin  = 0.6f;
	constexpr float kCometHeadMax  = 1.2f;
	// 出現範囲（カメラ前方=+Z 側の遠景。画面外左右から流れて反対へ抜ける）
	constexpr float kCometXRange   = 46.0f;  // 画面外まで（出入りをさらに長く）
	constexpr float kCometYRange   = 28.0f;
	constexpr float kCometZMin     = -8.0f;
	constexpr float kCometZMax     = 15.0f;

	float TsRand01() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
	float TsRandRange(float a, float b) { return a + TsRand01() * (b - a); }
}

//----------------------------------------------------------
void TitleScene::RespawnComet(Comet& c)
{
	// 常に右上の画面外から左下へ流れる一定方向のストリーム（違和感のない流星群）
	c.pos = {
		TsRandRange(kCometXRange * 0.5f, kCometXRange),    // 右側
		TsRandRange(kCometYRange * 0.4f, kCometYRange),    // 上側
		TsRandRange(kCometZMin, kCometZMax) };

	Math::Vector3 dir = { -1.0f, -0.65f, TsRandRange(-0.12f, 0.12f) };   // 左下へ
	dir.Normalize();
	c.vel = dir * TsRandRange(kCometSpeedMin, kCometSpeedMax);

	c.headSize = TsRandRange(kCometHeadMin, kCometHeadMax);
	// 重力コアらしい色（シアン〜白、たまに暖色）
	if (TsRand01() < 0.75f) { c.color = { 0.4f, 0.8f, 1.0f, 1.0f }; }
	else                    { c.color = { 1.0f, 0.85f, 0.5f, 1.0f }; }

	c.history.clear();
	c.history.assign(kTrailLen, c.pos);
}

//----------------------------------------------------------
void TitleScene::Init()
{
	// カメラ
	m_titleCam = std::make_shared<KdCamera>();
	m_titleCam->SetProjectionMatrix(kCamFov);
	m_freeCam  = std::make_shared<EditorCamera>();
	m_freeCam->SetPos({ 0.0f, 2.0f, 16.0f });
	m_camera   = m_titleCam;

	// 漂うキャラ
	m_cubun.SetModelData(CubunConst::ModelPath);
	m_corelia.SetModelData(PlayerConst::ModelPath);
	m_coreliaAnim.Init(&m_corelia);
	// 落下アニメ。"Fall" が無い gltf（Mixamo由来は "Armature|mixamo.com|Layer0" 名）にも対応
	if (!m_coreliaAnim.ChangeAnimation("Fall", true, 0))
	{
		m_coreliaAnim.ChangeAnimation("Armature|mixamo.com|Layer0", true, 0);
	}
	// 剣・傘・背中の剣を非表示
	m_corelia.SetNodeVisible("HandledSword",  false);
	m_corelia.SetNodeVisible("BackSword",     false);
	m_corelia.SetNodeVisible("OpenedParasol", false);
	m_corelia.SetNodeVisible("ClosedParasol", false);

	// ロゴ＋フォント
	m_logoTex = std::make_shared<KdTexture>();
	m_logoTex->Load(kLogoPath);
	KdFontManager::Instance().AddFont(kFontNo, kFontName, kFontHeight);

	// 彗星テクスチャ
	m_cometCoreTex = std::make_shared<KdTexture>();
	m_cometGlowTex = std::make_shared<KdTexture>();
	if (m_cometCoreTex->Load(kCometCoreTex)) { m_cometCorePoly.SetMaterial(m_cometCoreTex); m_cometCorePoly.SetScale(1.0f); }
	if (m_cometGlowTex->Load(kCometGlowTex)) { m_cometGlowPoly.SetMaterial(m_cometGlowTex); m_cometGlowPoly.SetScale(1.0f); }

	// 流れ星を初期生成（時間差で散らす）
	m_comets.resize(kCometCount);
	for (auto& c : m_comets)
	{
		RespawnComet(c);
		// 初期位置をランダムに進めて散らす
		c.pos += c.vel * TsRandRange(0.0f, 1.5f);
		c.history.assign(kTrailLen, c.pos);
	}

	// 宇宙背景
	AddObject(std::make_shared<BackGround>());
	AddObject(std::make_shared<StarField>());

	// キャラを照らす点光源
	{
		auto light = std::make_shared<PointLightObject>();
		light->SetPos({ 4.0f, 5.0f, 10.0f });
		light->SetColor({ 1.0f, 0.95f, 0.85f });
		light->SetRadius(60.0f);
		AddObject(light);
	}
}

//----------------------------------------------------------
void TitleScene::Event()
{
	const float dt = KdFPSController::GetDt();
	m_timer += dt;

	// ライト
	{
		auto& ambient = KdShaderManager::Instance().WorkAmbientController();
		Math::Vector3 dir(0.3f, -1.0f, 0.5f);
		dir.Normalize();
		ambient.SetDirLight(dir, Math::Vector3(2.2f, 2.2f, 2.2f));
		ambient.SetAmbientLight(Math::Vector4(0.5f, 0.52f, 0.6f, 1.0f));
	}

	// コアリアの落下アニメ更新
	m_coreliaAnim.Update(m_corelia, 1.0f);

	// 流れ星更新
	for (auto& c : m_comets)
	{
		c.pos += c.vel * dt;
		// 履歴更新（先頭が最新）
		c.history.insert(c.history.begin(), c.pos);
		if (static_cast<int>(c.history.size()) > kTrailLen) { c.history.pop_back(); }
		// 左下へ抜けたら右上から再生成（一定方向のストリーム）
		if (c.pos.x < -kCometXRange - 4.0f || c.pos.y < -kCometYRange - 4.0f) { RespawnComet(c); }
	}

	// F2 フリーカメラ切替
	const bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
	if (f2 && !m_f2Prev)
	{
		m_freeCamOn = !m_freeCamOn;
		m_camera = m_freeCamOn ? std::static_pointer_cast<KdCamera>(m_freeCam) : m_titleCam;
	}
	m_f2Prev = f2;

	if (m_freeCamOn)
	{
		m_freeCam->Update();
	}
	else
	{
		Math::Vector3 eye = kCamEye;
		eye.x += std::sinf(m_timer * kCamSwaySpeed) * kCamSwayX;
		eye.y += std::cosf(m_timer * kCamSwaySpeed * 0.7f) * kCamSwayY;

		// 左手系(LH)のカメラ「ワールド行列」を直接構築（SetCameraMatrix はワールドを受け取る）。
		// SimpleMath の CreateLookAt は右手系で視線が反転するため使わない。
		Math::Vector3 f = kCamTarget - eye; f.Normalize();          // 前(+Z=視線)
		Math::Vector3 r = Math::Vector3::Up.Cross(f); r.Normalize(); // 右(LH)
		Math::Vector3 u = f.Cross(r);                               // 上
		Math::Matrix world;
		world._11 = r.x; world._12 = r.y; world._13 = r.z; world._14 = 0.0f;
		world._21 = u.x; world._22 = u.y; world._23 = u.z; world._24 = 0.0f;
		world._31 = f.x; world._32 = f.y; world._33 = f.z; world._34 = 0.0f;
		world._41 = eye.x; world._42 = eye.y; world._43 = eye.z; world._44 = 1.0f;
		m_titleCam->SetCameraMatrix(world);
	}

	// カメラ値ログ（F2 で良い位置に合わせ、Output に出る値を貼り付けてもらう）
	m_logTimer += dt;
	if (m_logTimer >= 0.5f)
	{
		m_logTimer = 0.0f;
		const Math::Matrix camW = m_camera->GetCameraMatrix();
		const Math::Vector3 pos = camW.Translation();
		const Math::Vector3 fwd = camW.Backward();   // カメラの視線方向(+Z)
		const Math::Vector3 tgt = pos + fwd * 10.0f; // 視線先(参考)
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"[TitleCam] eye=( %.2f, %.2f, %.2f )  target=( %.2f, %.2f, %.2f )  fwd=( %.2f, %.2f, %.2f )\n",
			pos.x, pos.y, pos.z, tgt.x, tgt.y, tgt.z, fwd.x, fwd.y, fwd.z);
		OutputDebugStringA(buf);
	}

	// Enter で「むっちゃ光って小さくなる」演出を開始 → 終わったらゲームへ
	// （シーン切替直後の押しっぱなし/連打は無視）
	if (!m_starting && (GetAsyncKeyState(VK_RETURN) & 0x8000)
		&& !SceneManager::Instance().IsInputLocked())
	{
		m_starting   = true;
		m_startTimer = 0.0f;
	}
	if (m_starting)
	{
		m_startTimer += dt;
		if (m_startTimer >= kStartDur)
		{
			SceneManager::Instance().SetNextScene(SceneManager::SceneType::Story);
		}
	}
}

//----------------------------------------------------------
float TitleScene::StartProgress() const
{
	if (!m_starting) { return 0.0f; }
	return std::min(m_startTimer / kStartDur, 1.0f);
}

//----------------------------------------------------------
void TitleScene::DrawLitExtra()
{
	auto& shader = KdShaderManager::Instance().m_StandardShader;

	// 右上(+X,+Y)から左下(-X,-Y)へフワ〜と流れる位置（ループ）。両端は画面外なので継ぎ目は見えない。
	auto driftPos = [&](float phase, float zBase) -> Math::Vector3
	{
		float p = std::fmodf(m_timer * kCharDrift + phase, 1.0f);
		Math::Vector3 pos(
			kCharSpreadX  - p * (kCharSpreadX * 2.0f),
			kCharSpreadY  - p * (kCharSpreadY * 2.0f),
			zBase);
		// ゆらぎ
		pos.x += std::sinf(m_timer * 0.5f + phase * 6.0f) * 1.5f;
		pos.y += std::cosf(m_timer * 0.4f + phase * 6.0f) * 1.0f;
		pos.z += std::sinf(m_timer * 0.3f + phase * 3.0f) * 2.0f;
		return pos;
	};

	// Cubun：流れ星のように流れつつ、無重力でくるくる回転
	if (m_cubun.IsEnable())
	{
		const Math::Matrix w =
			Math::Matrix::CreateScale(kCubunScale) *
			Math::Matrix::CreateFromYawPitchRoll(m_timer * 0.7f, m_timer * 0.5f, m_timer * 0.9f) *
			Math::Matrix::CreateTranslation(driftPos(0.0f, kCubunZ));
		shader.DrawModel(m_cubun, w);
	}

	// コアリア：Fall アニメで投げ出され、ゆっくり回転しながら流れる
	if (m_corelia.IsEnable())
	{
		const Math::Matrix w =
			Math::Matrix::CreateScale(kCoreliaScale) *
			Math::Matrix::CreateFromYawPitchRoll(m_timer * 0.4f, m_timer * 0.8f, m_timer * 0.3f) *
			Math::Matrix::CreateTranslation(driftPos(0.5f, kCoreliaZ));
		shader.DrawModel(m_corelia, w);
	}
}

//----------------------------------------------------------
void TitleScene::DrawEffectExtra()
{
	auto& sm = KdShaderManager::Instance();
	sm.ChangeBlendState(KdBlendState::Add);
	sm.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);
	sm.m_StandardShader.SetDissolve(0.0f);

	for (const auto& c : m_comets)
	{
		// 尾：履歴を奥(古い)→手前(新しい)に向けて、太く明るくしながらビーズ描画
		const int n = static_cast<int>(c.history.size());
		for (int i = n - 1; i >= 1; --i)
		{
			const float f = 1.0f - static_cast<float>(i) / static_cast<float>(n);  // 0(尾)→1(頭付近)
			const float a = f * f * 0.6f;
			const float sz = c.headSize * (0.25f + 0.75f * f);
			const Math::Color   col{ c.color.x, c.color.y, c.color.z, a };
			const Math::Vector3 em { c.color.x * a, c.color.y * a, c.color.z * a };
			EffectBase::DrawBillboard(m_cometGlowPoly, c.history[i], sz, 0.0f, col, em);
		}

		// 頭：グロー＋明るいコア
		{
			const Math::Color   gcol{ c.color.x, c.color.y, c.color.z, 0.7f };
			const Math::Vector3 gem { c.color.x, c.color.y, c.color.z };
			EffectBase::DrawBillboard(m_cometGlowPoly, c.pos, c.headSize * 1.6f, 0.0f, gcol, gem);

			const Math::Color   ccol{ 1.0f, 1.0f, 1.0f, 1.0f };
			const Math::Vector3 cem { 1.0f, 1.0f, 1.0f };
			EffectBase::DrawBillboard(m_cometCorePoly, c.pos, c.headSize * 0.9f, m_timer * 2.0f, ccol, cem);
		}
	}

	sm.UndoDepthStencilState();
	sm.UndoBlendState();
}

//----------------------------------------------------------
// ロゴ bloom：ロゴを Bright バッファへ描き、エンジンのブラー(LightBloom/Blur)で滲ませる。
// 常に焚き、一定間隔で脈動。Enter 時はむっちゃ光る。
void TitleScene::DrawBrightExtra()
{
	if (!m_logoTex) { return; }

	const float tw = static_cast<float>(m_logoTex->GetInfo().Width);
	const float th = static_cast<float>(m_logoTex->GetInfo().Height);
	if (tw <= 0.0f || th <= 0.0f) { return; }

	// ── bloom 強度 ──
	// ① 常時ベース＋一定間隔の脈動
	float glow = kBloomBase + kBloomPulseAmp * (0.5f + 0.5f * std::sinf(m_timer * kBloomPulseSpeed));
	// ② 登場直後の追加発光（kBloomDur で消える）
	glow += std::max(0.0f, 1.0f - m_timer / kBloomDur);
	// ③ Enter 演出中はむっちゃ光る
	const float startP = StartProgress();
	if (startP > 0.0f) { glow += kStartBloom * startP * startP; }

	if (glow <= 0.001f) { return; }

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// 登場のオーバーシュート × Enter の縮小
	const float p     = std::min(m_timer / kIntroDur, 1.0f);
	const float ease  = 1.0f - (1.0f - p) * (1.0f - p);
	const float scale = (1.0f + (1.0f - ease) * 0.35f) * (1.0f - startP * kStartShrink);
	const int w = static_cast<int>(sw * kLogoWidthRatio * scale);
	const int h = static_cast<int>(w * (th / tw));
	const float bob = std::sinf(m_timer * kBobSpeed) * (sh * kBobAmpRatio);
	const int y = static_cast<int>(sh * kLogoYRatio + bob);

	// Bright RT に加算で重ね描き。glow>1 の分はパスを増やして明るさを盛る（→より強いブルーム）。
	auto& sm = KdShaderManager::Instance();
	auto& sprite = sm.m_spriteShader;
	sm.ChangeBlendState(KdBlendState::Add);
	sprite.Begin();
	const int passes = static_cast<int>(std::ceil(glow));
	for (int i = 0; i < passes; ++i)
	{
		const float a = std::min(glow - static_cast<float>(i), 1.0f);
		const Math::Color col(1.0f, 1.0f, 1.0f, a);
		sprite.DrawTex(m_logoTex.get(), 0, y, w, h, nullptr, &col);
	}
	sprite.End();
	sm.UndoBlendState();
}

//----------------------------------------------------------
void TitleScene::DrawSpriteExtra()
{
	auto& sprite = KdShaderManager::Instance().m_spriteShader;

	const auto& bb = KdDirect3D::Instance().GetBackBuffer();
	const int sw = static_cast<int>(bb->GetInfo().Width);
	const int sh = static_cast<int>(bb->GetInfo().Height);

	// CORELIA ロゴ（バッと拡大フェードイン。bloom 発光は DrawBrightExtra でエンジンのブラーを使う）
	if (m_logoTex)
	{
		const float tw = static_cast<float>(m_logoTex->GetInfo().Width);
		const float th = static_cast<float>(m_logoTex->GetInfo().Height);
		if (tw > 0.0f && th > 0.0f)
		{
			const float startP = StartProgress();
			const float p     = std::min(m_timer / kIntroDur, 1.0f);
			const float ease  = 1.0f - (1.0f - p) * (1.0f - p);   // easeOut
			// 1.35→1.0 のオーバーシュート × Enter で縮小
			const float scale = (1.0f + (1.0f - ease) * 0.35f) * (1.0f - startP * kStartShrink);
			const float aIn   = std::min(m_timer / 0.18f, 1.0f);

			const int w = static_cast<int>(sw * kLogoWidthRatio * scale);
			const int h = static_cast<int>(w * (th / tw));
			const float bob = std::sinf(m_timer * kBobSpeed) * (sh * kBobAmpRatio);
			const int y = static_cast<int>(sh * kLogoYRatio + bob);

			const Math::Color col(1.0f, 1.0f, 1.0f, aIn);
			sprite.DrawTex(m_logoTex.get(), 0, y, w, h, nullptr, &col);
		}
	}

	// 白フラッシュ（起動直後だけ「バッ」と光らせる。全面を覆って素早く消える）
	if (m_timer < kFlashDur)
	{
		const float fa = (1.0f - m_timer / kFlashDur);
		const Math::Color flash(1.0f, 1.0f, 1.0f, fa * 0.9f);
		sprite.DrawBox(0, 0, sw, sh, &flash, true);
	}

	// Enter 演出：画面全体を白で「バッ」と光らせる（終盤で最大）
	const float startP = StartProgress();
	if (startP > 0.0f)
	{
		const Math::Color flash(1.0f, 1.0f, 1.0f, startP * startP);
		sprite.DrawBox(0, 0, sw, sh, &flash, true);
	}

	// PRESS ENTER（演出後に点滅で登場）
	if (m_timer > kPromptIn)
	{
		const float promptFade = std::min((m_timer - kPromptIn) / 0.4f, 1.0f);
		const float blink = 0.3f + 0.7f * (0.5f + 0.5f * std::sinf(m_timer * kBlinkSpeed));
		const Math::Color col(1.0f, 1.0f, 1.0f, blink * promptFade);

		auto measure = KdFontManager::Instance().CreateFontTexture(kFontNo, kPromptText, false);
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
		const Math::Vector2 pos(-textW * 0.5f, -sh * kPromptYRatio - textH * 0.5f);
		TextFx::DrawShadowed(sprite, pos, col, kPromptText);
	}
}
