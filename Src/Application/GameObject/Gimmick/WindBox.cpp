#include "../../../Pch.h"
#include "WindBox.h"
#include "../../Const/OutlineConst.h"
#include "MovingFloor.h"

static float WBRand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
static float WBRandSym() { return (WBRand01() - 0.5f) * 2.0f; }

//----------------------------------------------------------
void WindBox::Init(const WindBoxData& data)
{
	m_center     = data.center;
	m_baseCenter = data.center;   // 追従オフセットの基準
	m_halfSize = data.size * 0.5f;
	m_windDir  = data.windDir;
	m_power    = data.power;
	m_length   = data.length;
	m_enabled  = data.enabled;

	if (m_windDir.LengthSquared() > 0.0001f) { m_windDir.Normalize(); }

	// 風向きに垂直な右・上基底を構築
	m_windRight = (std::abs(m_windDir.y) < 0.9f)
		? Math::Vector3{ 0.0f, 1.0f, 0.0f }.Cross(m_windDir)
		: Math::Vector3{ 1.0f, 0.0f, 0.0f }.Cross(m_windDir);
	if (m_windRight.LengthSquared() < 1e-4f) m_windRight = { 1.0f, 0.0f, 0.0f };
	m_windRight.Normalize();
	m_windUp = m_windDir.Cross(m_windRight);
	m_windUp.Normalize();

	// ボックスの風方向への奥行き（往復距離）
	m_windDepth = std::abs(m_halfSize.Dot(m_windDir)) * 2.0f;
	if (m_windDepth < 0.1f) m_windDepth = m_halfSize.x * 2.0f;

	SetPos(m_center);
	m_mWorld = Math::Matrix::CreateTranslation(m_center);

	// フラスタムカリング用バウンディング球：BOX半径 + エフェクトが届く全長を包む
	const float boxRadius = m_halfSize.Length();
	m_cullingRadius = boxRadius + m_length;

	// テクスチャ読込
	m_spTex = std::make_shared<KdTexture>();
	m_spTex->Load(WindBoxConst::ParticleTexPath);

	// ボックスモデル読込
	m_boxModel = std::make_shared<KdModelWork>();
	m_boxModel->SetModelData(WindBoxConst::BoxModelPath);

	// 影生成パス(GenerateDepthMapFromLight)で DrawLit を描かせる
	m_drawType = eDrawTypeLit;

	// COL ノードベースのコライダーを初期化（床＋壁両方として登録）
	m_pCollider = std::make_unique<KdCollider>();
	m_pCollider->RegisterCollisionShape(WindBoxConst::ColNodeName, m_boxModel,
		KdCollider::TypeGround | KdCollider::TypeBump);

	// 描画と当たり判定で共用するワールド行列を先に確定（押し出しは球vsOBBで別途行う）
	m_worldMat = BuildWorldMatrix();

	// プロペラノードの初期ローカル行列を丸ごと保存
	const KdModelWork::Node* propNode = m_boxModel->FindWorkNode(WindBoxConst::PropellerNodeName);
	if (propNode)
	{
		m_propellerInitLocal = propNode->m_localTransform;
	}

	// パーティクルを均等な初期位置で生成（m_length 全体を等分割）
	m_particles.resize(WindBoxConst::ParticleCount);
	for (int i = 0; i < WindBoxConst::ParticleCount; ++i)
	{
		InitParticle(m_particles[i],
			m_length * (static_cast<float>(i) / WindBoxConst::ParticleCount));
	}
}

//----------------------------------------------------------
void WindBox::InitParticle(WindParticle& p, float initialTravel) const
{
	// 螺旋半径：出口面の短辺（ModelFaceHalfH）基準
	const float faceShort = WindBoxConst::ModelFaceHalfH;
	p.orbitRadius = faceShort * WindBoxConst::SpiralRadiusScale
				  * (0.5f + WBRand01() * 0.5f);

	// 公転初期位相
	p.orbitPhase = WBRand01() * DirectX::XM_2PI;

	// 自転初期角度
	p.selfRot = WBRand01() * DirectX::XM_2PI;

	// 出口面（ModelFaceHalfW × ModelFaceHalfH の矩形）上のランダム位置
	p.startPerp = m_windRight * WBRandSym() * WindBoxConst::ModelFaceHalfW * 0.8f
				+ m_windUp    * WBRandSym() * WindBoxConst::ModelFaceHalfH * 0.8f;

	// 出口面から風向きに飛ぶ距離 = m_length（エディタで設定した風の届く距離）
	p.travelMax  = m_length;
	p.travelDist = initialTravel;

	// 移動速度：m_length 全体を一定時間で端まで流れるよう調整
	// 「length を N 秒で走り抜ける」→ speed = length / N
	constexpr float kTravelTimeSec = 1.5f;  // 端から端まで何秒かける
	p.speed = m_length / kTravelTimeSec * (0.8f + WBRand01() * 0.4f);

	// サイズランダム
	p.size = WindBoxConst::ParticleSizeMin
		   + WBRand01() * (WindBoxConst::ParticleSizeMax - WindBoxConst::ParticleSizeMin);
}

//----------------------------------------------------------
void WindBox::Update()
{
	if (!m_enabled) { return; }

	// 移動床に追従（アタッチ時のみ）。中心を動かせば下のワールド行列・判定が追従する。
	if (auto floor = m_attachFloor.lock())
	{
		m_center = m_baseCenter + (floor->GetPos() - floor->GetBaseCenter());
	}

	const float dt = KdFPSController::GetDt();

	// プロペラ回転角を積算（2πでラップ）
	m_propellerAngle += WindBoxConst::PropellerRotSpeed * dt;
	if (m_propellerAngle > DirectX::XM_2PI) { m_propellerAngle -= DirectX::XM_2PI; }

	for (auto& p : m_particles)
	{
		// 風向きに進む
		p.travelDist += p.speed * dt;

		// 公転・自転
		p.orbitPhase += WindBoxConst::OrbitSpeed * dt;
		p.selfRot    += WindBoxConst::SelfRotSpeed * dt;

		// ボックス端まで来たらリスポーン（風上端から再スタート）
		if (p.travelDist >= p.travelMax)
		{
			InitParticle(p, 0.0f);
		}
	}
}

//----------------------------------------------------------
void WindBox::DrawLit()
{
	if (!m_boxModel || !m_boxModel->IsEnable()) { return; }

	// ── プロペラノードの localTransform を更新 ──────────────
	KdModelWork::Node* propNode = m_boxModel->FindWorkNode(WindBoxConst::PropellerNodeName);
	if (propNode)
	{
		const Math::Matrix spinZ = Math::Matrix::CreateRotationZ(m_propellerAngle);
		propNode->m_localTransform = spinZ * m_propellerInitLocal;
	}

	// ── ワールド行列を構築（描画・当たり判定で共用）──
	m_worldMat = BuildWorldMatrix();

	KdShaderManager::Instance().m_StandardShader.DrawModel(*m_boxModel, m_worldMat);
}

//----------------------------------------------------------
// 描画・当たり判定で共用するワールド行列を構築。
// モデルの +Y → windDir になるよう回転させ、size に合わせて非等方スケール。
//----------------------------------------------------------
Math::Matrix WindBox::BuildWorldMatrix() const
{
	const float scaleXZ = m_halfSize.x / WindBoxConst::ModelNaturalXZ;
	const float scaleY  = m_halfSize.y / WindBoxConst::ModelNaturalY;
	const Math::Matrix scaleMat = Math::Matrix::CreateScale(scaleXZ, scaleY, scaleXZ);

	// +Y → windDir の最短回転
	const Math::Vector3 localUp{ 0.0f, 1.0f, 0.0f };
	Math::Vector3 rotAxis = localUp.Cross(m_windDir);
	Math::Matrix  rotMat;
	if (rotAxis.LengthSquared() < 1e-6f)
	{
		rotMat = (localUp.Dot(m_windDir) >= 0.0f)
			? Math::Matrix::Identity
			: Math::Matrix::CreateRotationZ(DirectX::XM_PI);
	}
	else
	{
		rotAxis.Normalize();
		const float angle = std::acosf(std::clamp(localUp.Dot(m_windDir), -1.0f, 1.0f));
		rotMat = Math::Matrix::CreateFromAxisAngle(rotAxis, angle);
	}

	return scaleMat * rotMat * Math::Matrix::CreateTranslation(m_center);
}

//----------------------------------------------------------
void WindBox::DrawOutline()
{
	if (!m_boxModel || !m_boxModel->IsEnable()) { return; }
	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::TerrainWidth);   // 大きいので太め
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(*m_boxModel, m_worldMat, c, Math::Vector3::Zero);
}

//----------------------------------------------------------
void WindBox::DrawEffect()
{
	if (!m_enabled || !m_spTex) { return; }

	// カメラ位置を取得してビルボード軸を求める
	const Math::Vector3 camPos = KdShaderManager::Instance().GetCameraCB().CamPos;

	auto& shader   = KdShaderManager::Instance().m_StandardShader;
	auto& shaderMgr = KdShaderManager::Instance();
	shaderMgr.ChangeBlendState(KdBlendState::Add);
	shaderMgr.ChangeDepthStencilState(KdDepthStencilState::ZWriteDisable);

	// 風の出口面の中心（モデルの風向き側の面）
	const Math::Vector3 exitFace = m_center + m_windDir * WindBoxConst::ModelFaceOffset;

	for (const auto& p : m_particles)
	{
		// ワールド位置
		const Math::Vector3 spiral = m_windRight * (p.orbitRadius * std::cosf(p.orbitPhase))
								   + m_windUp    * (p.orbitRadius * std::sinf(p.orbitPhase));
		const Math::Vector3 worldPos = exitFace
									 + p.startPerp
									 + spiral
									 + m_windDir * p.travelDist;

		// フェードイン・アウト
		const float lifeRatio = p.travelDist / p.travelMax;
		const float fadeIn    = std::min(lifeRatio / WindBoxConst::FadeEdge, 1.0f);
		const float fadeOut   = std::min((1.0f - lifeRatio) / WindBoxConst::FadeEdge, 1.0f);
		const float alpha     = fadeIn * fadeOut * WindBoxConst::ParticleAlphaMax;
		if (alpha <= 0.001f) { continue; }

		// カメラに向くビルボード基底を構築
		Math::Vector3 toBillboard = worldPos - camPos;
		if (toBillboard.LengthSquared() < 1e-6f) { continue; }
		toBillboard.Normalize();

		// ビルボードの右軸：worldUp × toBillboard
		// worldUp には風方向の上成分 or Y+ を使う
		const Math::Vector3 worldUp = { 0.0f, 1.0f, 0.0f };
		Math::Vector3 bRight = worldUp.Cross(toBillboard);
		if (bRight.LengthSquared() < 1e-4f)
			bRight = Math::Vector3{ 1.0f, 0.0f, 0.0f };
		bRight.Normalize();
		Math::Vector3 bUp = toBillboard.Cross(bRight);
		bUp.Normalize();

		// 自転を追加（bRight/bUp を selfRot で回転）
		const float cosR  = std::cosf(p.selfRot);
		const float sinR  = std::sinf(p.selfRot);
		const Math::Vector3 rotRight = bRight * cosR - bUp * sinR;
		const Math::Vector3 rotUp    = bRight * sinR + bUp * cosR;

		const float hs = p.size * 0.5f;
		const Math::Vector3 TL = worldPos + (-rotRight + rotUp) * hs;
		const Math::Vector3 TR = worldPos + ( rotRight + rotUp) * hs;
		const Math::Vector3 BL = worldPos + (-rotRight - rotUp) * hs;
		const Math::Vector3 BR = worldPos + ( rotRight - rotUp) * hs;

		// 頂点カラー（アルファはalphaで、emissiveで発光）
		const unsigned char a  = static_cast<unsigned char>(std::min(alpha * 255.0f, 255.0f));
		const unsigned int  vc = (a << 24) | 0x00FFFFFF;

		KdPolygon poly(m_spTex);
		auto& verts = const_cast<std::vector<KdPolygon::Vertex>&>(poly.GetVertices());
		verts.resize(6);

		auto makeV = [&](const Math::Vector3& p3, float u, float v) -> KdPolygon::Vertex
		{
			KdPolygon::Vertex vtx;
			vtx.pos    = p3;
			vtx.UV     = { u, v };
			vtx.normal = -toBillboard;
			vtx.tangent= rotRight;
			vtx.color  = vc;
			return vtx;
		};

		verts[0] = makeV(TL, 0.0f, 0.0f);
		verts[1] = makeV(TR, 1.0f, 0.0f);
		verts[2] = makeV(BL, 0.0f, 1.0f);
		verts[3] = makeV(TR, 1.0f, 0.0f);
		verts[4] = makeV(BR, 1.0f, 1.0f);
		verts[5] = makeV(BL, 0.0f, 1.0f);

		const Math::Color   col    { 1.0f, 1.0f, 1.0f, 1.0f };
		const Math::Vector3 emissive{ 0.5f, 0.75f, 1.0f };   // 水色の自己発光
		shader.DrawPolygon(poly, Math::Matrix::Identity, col, emissive);
	}

	shaderMgr.UndoBlendState();
	shaderMgr.UndoDepthStencilState();
}

//----------------------------------------------------------
void WindBox::DrawDebug()
{
	if (!m_pDebugWire)
	{
		m_pDebugWire = std::make_unique<KdDebugWireFrame>();
	}

	const Math::Vector3 boxSize = m_halfSize * 2.0f;
	m_pDebugWire->AddDebugBox(
		Math::Matrix::CreateTranslation(m_center),
		boxSize, Math::Vector3::Zero, false,
		{ 0.2f, 0.8f, 1.0f, 0.6f });

	const float arrowLen = std::min({ m_halfSize.x, m_halfSize.y, m_halfSize.z }) * 1.2f;
	const Math::Vector3 tip = m_center + m_windDir * arrowLen;
	m_pDebugWire->AddDebugLine(m_center, tip, { 0.0f, 1.0f, 0.4f, 1.0f });

	m_pDebugWire->Draw();
}

//----------------------------------------------------------
bool WindBox::IsInRange(const Math::Vector3& pos) const
{
	if (!m_enabled || !m_pCollider) { return false; }

	// ── ① COL メッシュ内チェック ────────────────────────
	const Math::Vector3 toPos = pos - m_center;
	const float dist = toPos.Length();
	if (dist < 1e-4f) { return true; }

	const KdCollider::RayInfo ray(
		KdCollider::TypeGround | KdCollider::TypeBump,
		m_center,
		toPos / dist,
		dist * 0.99f);

	std::list<KdCollider::CollisionResult> results;
	const bool hitWall = m_pCollider->Intersects(ray, m_worldMat, &results);

	if (!hitWall) { return true; }   // COL メッシュ内

	// ── ② 風向き延長範囲チェック（m_length 分だけ追加判定）────
	// COL 上端から風向きに m_length 進んだ円柱状の範囲を AABB 代替でなく
	// 「pos を COL 座標系に投影」して風向き成分と垂直成分で判定する
	// ※ AABB 禁止規則に従い、ここでもレイキャスト判定を使う。
	// 　 風口端（COL上端）から pos まで逆向きレイを飛ばして COL に当たれば外側。
	//   距離が m_length 以内かつ COL に当たらなければ延長範囲内と見なす。
	if (m_length < 1e-4f) { return false; }

	// COL 上端（近似: center + windDir * halfSize.y）から pos へのベクトル
	const Math::Vector3 topCenter = m_center + m_windDir * m_halfSize.y;
	const Math::Vector3 toFromTop = pos - topCenter;
	const float alongWind = toFromTop.Dot(m_windDir);

	// 風向き成分が 0〜m_length の外なら範囲外
	if (alongWind < 0.0f || alongWind > m_length) { return false; }

	// 垂直成分が COL の横幅以内かどうか: topCenter → pos の逆レイで COL に当たらなければ内側
	const float perpDist = (toFromTop - m_windDir * alongWind).Length();
	const float halfPerp = std::max(m_halfSize.x, m_halfSize.z);
	if (perpDist > halfPerp) { return false; }

	return true;
}

//----------------------------------------------------------
Math::Vector3 WindBox::RandomPerp() const
{
	return m_windRight * WBRandSym() * m_halfSize.x
		 + m_windUp    * WBRandSym() * m_halfSize.y;
}


