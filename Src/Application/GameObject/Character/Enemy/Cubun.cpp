#include "../../../../Pch.h"
#include "Cubun.h"
#include "../Character.h"
#include "../../../Const/OutlineConst.h"

//==========================================================
// 初期化
//==========================================================
void Cubun::Init()
{
	// 基底クラスのモデルロード（hp も設定される）
	InitModel(CubunConst::ModelPath);

	// スポーン位置を記録
	m_spawnPos = GetPos();

	// 初期重力方向をキック・速度操作なしでスナップセット
	if (m_initGravDir != ManualGravityDir::None)
	{
		SetInitialGravityDir(m_initGravDir);
	}

	// 崖・壁で折り返す挙動を有効化（マリオの赤クリボーと同じ動き）
	m_useEdgeDetection = true;

	// 本体コライダー（プレイヤーの押し出し用）。
	// プレイヤーの壁レイは足元〜+0.9 までしか飛ばないので、COL ノード実寸
	// （+0.99〜+2.99 に浮いている）のままだとレイが底に届かず押し出しが効かない。
	// そこで足元(0)〜頭頂部までの「柱」として登録し、低いレイでも必ず当たるようにする。
	m_pCollider = std::make_unique<KdCollider>();
	const float bodyTop = CubunConst::CollisionCenterY + CubunConst::CollisionHalfH; // 頭頂部の高さ
	const DirectX::BoundingOrientedBox bodyObb(
		DirectX::XMFLOAT3(0.0f, bodyTop * 0.5f, 0.0f),       // 足元〜頭頂部の中心
		DirectX::XMFLOAT3(CubunConst::CollisionRadius,
						  bodyTop * 0.5f,                      // 足元まで覆う縦半径
						  CubunConst::CollisionRadius),
		DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
	m_pCollider->RegisterCollisionShape("Body", bodyObb, KdCollider::TypeBump);

	// 叩きつけコライダー：モデルの COL_Attack ノードのみをレイ判定対象に登録
	m_pAttackCollider = std::make_unique<KdCollider>();
	if (const auto spData = m_modelWork.GetData())
	{
		auto attackShape = std::make_unique<KdModelCollision>(spData, KdCollider::TypeDamage);

		// COL_Attack ノードのインデックスを探してフィルタに設定（COL 本体は除外）
		const auto& nodes = spData->GetOriginalNodes();
		for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
		{
			if (nodes[i].m_name == CubunConst::AttackNodeName)
			{
				attackShape->SetNodeFilter({ i });
				break;
			}
		}
		m_pAttackCollider->RegisterCollisionShape(CubunConst::AttackNodeName, std::move(attackShape));
	}

	m_hp = CubunConst::Hp;
}

//==========================================================
// 更新
//==========================================================
void Cubun::Update()
{
	// 初回死亡検知でバーストエフェクトをリクエスト
	if (IsDead() && !m_deathBounceActive)
	{
		m_requestBurstEffect = true;
		if (!m_rockDropped) { m_requestRockDrop = true; m_rockDropped = true; }
	}

	if (IsDead()) { return; }

	// ぺちゃんこ演出中：タイマーを進め、終わったら直接消す
	if (m_squashActive)
	{
		m_squashTimer += KdFPSController::GetDt();
		if (m_squashTimer >= CubunConst::SquashDuration)
		{
			// m_squashActive はリセットしない → DrawLit がぺちゃんこ状態のまま終わる
			m_requestBurstEffect = true;
			if (!m_rockDropped) { m_requestRockDrop = true; m_rockDropped = true; }
			Expire();  // death bounce をスキップして即消去
		}
		return;  // 演出中は移動・ジャンプを止める
	}

	// ジャンプ制御（着地後 JumpInterval 秒ごとにジャンプ）
	TryJump();

	// 移動・AI は Enemy::Update() に全部任せる
	Enemy::Update();

	// ── ドッスン風移動ゲート ───────────────────────────────
	// 地面にいる間は横移動を殺し、ジャンプ中（空中）だけ横移動を許可する。
	// これにより「その場で待機 → 跳ねてプレイヤーへ寄りながら落下」の挙動になる。
	if (m_isGround)
	{
		const float vUp = m_velocity.Dot(m_upDir);   // 上下成分（ジャンプ初速）は残す
		m_velocity     = m_upDir * vUp;
		m_moveVelocity = Math::Vector3::Zero;
	}
}

//==========================================================
// 後更新（物理位置を保持したままワールド行列を更新）
//
// Enemy::PostUpdate は m_mWorld に ModelOffsetY を含む平行移動を書き込む。
// GetPos() は m_mWorld.Translation() を返すため、Enemy::PostUpdate を呼ぶと
// 物理位置が毎フレーム -ModelOffsetY ずれる。
// Cubun の描画は DrawLit → CalcVisualMatrix() で行われ m_mWorld を使わないため、
// ここでは m_mWorld の平行移動成分にオフセットを入れず物理位置を保つ。
//==========================================================
void Cubun::PostUpdate()
{
	// Enemy::PostUpdate を呼ばず Character::PostUpdate だけ呼ぶ
	Character::PostUpdate();

	const Math::Vector3 pos   = GetPos();
	const float         scale = EnemyConst::ModelScale
		                      * (m_deathBounceActive ? std::max(0.0f, m_deathFadeAlpha) : 1.0f);
	const float         yaw   = std::atan2f(m_facingDir.x, m_facingDir.z);

	// オフセットなし：m_mWorld の Translation が物理位置そのままになる
	m_mWorld = DirectX::XMMatrixScaling(scale, scale, scale)
		     * DirectX::XMMatrixRotationY(yaw)
		     * DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
}

//==========================================================
// 描画
//==========================================================
void Cubun::DrawLit()
{
	if (!m_modelWork.IsEnable()) { return; }

	// ビジュアル行列（体は回転しない）
	KdShaderManager::Instance().m_StandardShader.DrawModel(m_modelWork, CalcVisualMatrix());
}

void Cubun::DrawOutline()
{
	if (!m_modelWork.IsEnable()) { return; }
	auto& shader = KdShaderManager::Instance().m_StandardShader;
	shader.SetOutlineWidth(OutlineConst::Width);
	const Math::Color c(OutlineConst::ColorMul, OutlineConst::ColorMul, OutlineConst::ColorMul, 1.0f);
	shader.DrawModel(m_modelWork, CalcVisualMatrix(), c, Math::Vector3::Zero);
}

void Cubun::DrawDebug()
{
	if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

	// 本体コリジョン（押し出し用の柱と一致：足元〜頭頂部）
	const float bodyTop = CubunConst::CollisionCenterY + CubunConst::CollisionHalfH;
	const Math::Vector3 bodyHalfExtents = {
		CubunConst::CollisionRadius,
		bodyTop * 0.5f,
		CubunConst::CollisionRadius
	};
	const Math::Matrix collisionMat = Math::Matrix::CreateTranslation(GetPos());
	m_pDebugWire->AddDebugBox(collisionMat, bodyHalfExtents,
		Math::Vector3{ 0.0f, bodyTop * 0.5f, 0.0f }, false, { 0.8f, 0.4f, 0.0f, 1.0f });

	// COL_Attack スラブ（叩きつけ判定の目安。実判定はレイキャスト）
	const Math::Vector3 attackHalfExtents = {
		CubunConst::AttackRadius,
		CubunConst::AttackHalfH,
		CubunConst::AttackRadius
	};
	m_pDebugWire->AddDebugBox(collisionMat, attackHalfExtents,
		Math::Vector3{ 0.0f, CubunConst::AttackCenterY, 0.0f }, false, { 1.0f, 0.0f, 0.0f, 1.0f });

	m_pDebugWire->Draw();
}

//==========================================================
// 攻撃（近接タックル）
//==========================================================
void Cubun::DoAttack()
{
	FaceTarget();

	if (m_attackCool > 0) { ChangeAnim("Idle"); return; }

	const auto spTarget = m_wpTarget.lock();
	if (spTarget)
	{
		if (auto* pChar = dynamic_cast<Character*>(spTarget.get()))
		{
			pChar->TakeDamageFrom(CubunConst::ContactDamage, GetPos());
		}
	}

	ChangeAnim("Idle");
	m_attackCool = static_cast<int>(CubunConst::JumpInterval * 60.0f);
}

//==========================================================
// 叩きつけ(COL_Attack)判定（外部から問い合わせ）
// COL_Attack はモデル底面＝ワールド下向きの薄い板。プレイヤーの真上へ
// 縦レイを飛ばし、スラブに当たれば「真下（フットプリント内）にいる＝当たり」
// とする。横に外れている（フットプリント外）ならレイが外れて当たらない。
// 当たり行列は描画と同じ CalcVisualMatrix を使い、見た目のスラブと一致させる。
//==========================================================
bool Cubun::IsSpikeHit(const Math::Vector3& playerPos) const
{
	if (!m_pAttackCollider) { return false; }

	// プレイヤーの少し下から真上(ワールド+Y)へレイを飛ばす
	const Math::Vector3 up     = { 0.0f, 1.0f, 0.0f };
	const Math::Vector3 origin = playerPos - up * CubunConst::AttackRayBackOffset;
	const float         range  = CubunConst::AttackRayBackOffset + CubunConst::AttackContactRange;
	const KdCollider::RayInfo ray(KdCollider::TypeDamage, origin, up, range);

	std::list<KdCollider::CollisionResult> results;
	return m_pAttackCollider->Intersects(ray, CalcVisualMatrix(), &results);
}

//==========================================================
// 踏みつけ判定（頭側＝安全面。重力＝足元基準）
// プレイヤーが体の「上」(+m_upDir)付近にいて、かつ Cubun の上方向に対して
// 降下中（踏みつけ動作）なら true
//==========================================================
bool Cubun::CheckStomp(const Math::Vector3& playerPos, const Math::Vector3& playerVel) const
{
	const Math::Vector3 lp        = playerPos - GetPos();
	const float         alongUp   = lp.Dot(m_upDir);        // +:頭側(安全) -:足側(棘)
	const Math::Vector3 horiz     = lp - m_upDir * alongUp; // m_upDir 垂直面への投影
	const float         horizDist = horiz.Length();

	// 頭頂部（上面）= 本体中心 + 縦半径（GetPos からの高さ）
	const float bodyTop = CubunConst::CollisionCenterY + CubunConst::CollisionHalfH;

	// 頭側（安全面）の上面付近にいるか
	if (alongUp < bodyTop - CubunConst::StompTolerance) { return false; } // 上面より内側すぎ
	if (alongUp > bodyTop + CubunConst::StompReach)     { return false; } // 上に離れすぎ
	if (horizDist > CubunConst::CollisionRadius + CubunConst::StompHorizMargin) { return false; }

	// プレイヤーが Cubun の上方向に対して降下中（= upDir 方向成分が負）なら踏みつけ
	const float approach = playerVel.Dot(m_upDir);
	if (approach > CubunConst::StompApproachMax) { return false; }

	return true;
}

bool Cubun::Intersects(const KdCollider::RayInfo& ray,
					   std::list<KdCollider::CollisionResult>* result)
{
	if (!m_pCollider) { return false; }
	return m_pCollider->Intersects(ray, m_mWorld, result);
}

//==========================================================
// 踏みつけぺちゃんこ演出開始（GameScene から呼ぶ）
//==========================================================
void Cubun::StartStomp()
{
	if (m_squashActive) { return; }
	m_squashActive = true;
	m_squashTimer  = 0.0f;
	m_velocity     = Math::Vector3::Zero;  // 踏まれた瞬間に停止
}

//==========================================================
// プライベート：ジャンプ（着地後 JumpInterval 秒後に再ジャンプ）
//==========================================================
void Cubun::TryJump()
{
	const float kDt = KdFPSController::GetDt();

	if (m_isGround)
	{
		m_jumpTimer += kDt;
		if (m_jumpTimer >= CubunConst::JumpInterval)
		{
			m_jumpTimer = 0.0f;
			// 上方向（m_upDir）にジャンプ速度を加算
			const float currentUp = m_velocity.Dot(m_upDir);
			m_velocity += m_upDir * (CubunConst::JumpPower - currentUp);
		}
	}
	else
	{
		m_jumpTimer = 0.0f;
	}
}

//==========================================================
// プライベート：ビジュアル行列（体の向きは重力に関係なく常に上向き固定）
//==========================================================
Math::Matrix Cubun::CalcVisualMatrix() const
{
	const Math::Vector3 pos = GetPos();
	const float         yaw = std::atan2f(m_facingDir.x, m_facingDir.z);

	float scaleXZ = EnemyConst::ModelScale;
	float scaleY  = EnemyConst::ModelScale;

	// ぺちゃんこ演出：t=0(元サイズ) → t=1(潰れ切り) でスケール補間
	if (m_squashActive)
	{
		const float t  = std::min(m_squashTimer / CubunConst::SquashDuration, 1.0f);
		scaleY  = EnemyConst::ModelScale * (1.0f - t * (1.0f - CubunConst::SquashScaleY));
		scaleXZ = EnemyConst::ModelScale * (1.0f + t * (CubunConst::SquashScaleXZ - 1.0f));
	}

	return DirectX::XMMatrixScaling(scaleXZ, scaleY, scaleXZ)
		 * DirectX::XMMatrixRotationY(yaw)
		 * DirectX::XMMatrixTranslation(pos.x,
			   pos.y + CubunConst::ModelOffsetY,
			   pos.z);
}

