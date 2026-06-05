#include "../../../Pch.h"
#include "MovingFloor.h"
#include "../../Manager/ModelManager.h"
#include "../../Const/PlanetConst.h"

void MovingFloor::Init()
{
m_pos = m_center;
// 開始位置: -1=負端, 0=中心, 1=正端
m_t   = m_range * static_cast<float>(m_startPhase);
m_dir = (m_startPhase >= 0) ? 1 : -1;
InitModel();
}

void MovingFloor::Update()
{
const Math::Vector3 prevPos = m_pos;

constexpr float kDt = 1.0f / 60.0f;

// 折り返し地点での一時停止中
if (m_waitTimer > 0.0f)
{
m_waitTimer -= kDt;
m_deltaMove  = {};
return;
}

// m_t = 移動軸上のオフセット（ワールド単位）。[-m_range, +m_range] を往復する
m_t += kDt * m_speed * static_cast<float>(m_dir);
bool atEnd = false;
if (m_t >= m_range)        { m_t =  m_range; m_dir = -1; atEnd = true; }
else if (m_t <= -m_range)  { m_t = -m_range; m_dir =  1; atEnd = true; }
if (atEnd && m_waitDuration > 0.0f) { m_waitTimer = m_waitDuration; }

Math::Vector3 offset = {};
switch (m_axis)
{
case Axis::X: offset.x = m_t; break;
case Axis::Y: offset.y = m_t; break;
case Axis::Z: offset.z = m_t; break;
}

m_pos       = m_center + offset;
m_deltaMove = m_pos - prevPos;
UpdateWorld();
}

void MovingFloor::DrawLit()
{
if (!m_modelWork) { return; }

auto& shader = KdShaderManager::Instance().m_StandardShader;

shader.SetTriplanarUV(true, PlanetConst::TriplanarScale);

Math::Vector3 gravUp = { m_mWorld._21, m_mWorld._22, m_mWorld._23 };
gravUp.Normalize();

shader.SetGrassTexture(m_spGrassTex);
shader.SetGrassNormalTexture(m_spGrassNormalTex);
shader.SetGrassEdgeTexture(m_spGrassEdgeTex,
   PlanetConst::GrassEdgeWidth,
   PlanetConst::GrassEdgeTexScale);
shader.SetGrassBlend(true, gravUp,
 PlanetConst::GrassBlendSharpness,
 PlanetConst::GrassTriplanarScale);
shader.SetFullEdgeStrength(PlanetConst::BoxFullEdgeStrength);

shader.DrawModel(*m_modelWork, m_mWorld);

shader.SetTriplanarUV(false);
shader.SetGrassBlend(false);
shader.SetFullEdgeStrength(0.0f);

// 各面のキャップを描画（面インデックス: 0=Top, 1=Bottom, 2=Left, 3=Right）
const Math::Vector3 capUpDirs[4] = {
{ 0.0f,  1.0f, 0.0f },   // Top
{ 0.0f, -1.0f, 0.0f },   // Bottom
{-1.0f,  0.0f, 0.0f },   // Left
{ 1.0f,  0.0f, 0.0f },   // Right
};
for (int i = 0; i < 4; ++i)
{
if (!m_grassCapWork[i]) { continue; }
shader.SetTriplanarUV(true, PlanetConst::GrassTriplanarScale);
shader.SetGrassTexture(m_spGrassTex);
shader.SetGrassNormalTexture(m_spGrassNormalTex);
shader.SetGrassEdgeTexture(m_spGrassTex,
   PlanetConst::GrassEdgeWidth,
   PlanetConst::GrassEdgeTexScale);
shader.SetGrassBlend(true, capUpDirs[i],
 PlanetConst::GrassBlendSharpness,
 PlanetConst::GrassTriplanarScale);
shader.DrawModel(*m_grassCapWork[i], m_grassCapWorld[i]);
shader.SetTriplanarUV(false);
shader.SetGrassBlend(false);
}
}

void MovingFloor::DrawUnLit()
{
}

void MovingFloor::DrawDebug()
{
if (!m_pDebugWire) { m_pDebugWire = std::make_unique<KdDebugWireFrame>(); }

// m_mWorld = CreateScale(m_size) * Translate なので、ローカル単位サイズ {1,1,1} を渡す
const Math::Vector3 boxSize = { 1.0f, 1.0f, 1.0f };
m_pDebugWire->AddDebugBox(m_mWorld, boxSize, Math::Vector3::Zero, false, { 0.2f, 0.9f, 0.2f, 1.0f });
m_pDebugWire->Draw();
}

void MovingFloor::InitModel()
{
const auto spData = ModelManager::Instance().GetModel(PlanetConst::BoxModelPath);
if (spData)
{
m_modelWork = std::make_shared<KdModelWork>();
m_modelWork->SetModelData(spData);
}

// 壁レイキャスト用コライダー（ローカル単位ボックス {1,1,1}、m_mWorld でスケール込み）
m_pCollider = std::make_unique<KdCollider>();
{
DirectX::BoundingBox localBox;
localBox.Center  = { 0.0f, 0.0f, 0.0f };
localBox.Extents = { 1.0f, 1.0f, 1.0f };
m_pCollider->RegisterCollisionShape("Body", localBox, KdCollider::TypeBump);
}

// キャップモデルを Inward な面の数だけ生成
const auto spCapData = ModelManager::Instance().GetModel(PlanetConst::GrassCapModelPath);
if (spCapData)
{
const BoxFaceGravityMode faceModes[4] = { m_faceTop, m_faceBottom, m_faceLeft, m_faceRight };
for (int i = 0; i < 4; ++i)
{
const bool inward = m_bNormalGravity
? (i == 0)  // NormalGravity は Top のみ
: (faceModes[i] == BoxFaceGravityMode::Inward);
if (inward)
{
m_grassCapWork[i] = std::make_shared<KdModelWork>();
m_grassCapWork[i]->SetModelData(spCapData);
}
else
{
m_grassCapWork[i] = nullptr;
}
}
}

m_spGrassTex       = std::make_shared<KdTexture>();
m_spGrassNormalTex = std::make_shared<KdTexture>();
m_spGrassEdgeTex   = std::make_shared<KdTexture>();
m_spGrassTex->Load(PlanetConst::GrassTexPath);
m_spGrassNormalTex->Load(PlanetConst::GrassNormalTexPath);
m_spGrassEdgeTex->Load(PlanetConst::GrassEdgeTexPath);

UpdateWorld();
}

void MovingFloor::UpdateWorld()
{
m_mWorld =
Math::Matrix::CreateScale(m_size) *
Math::Matrix::CreateTranslation(m_pos);

if (m_modelWork) { m_modelWork->CalcNodeMatrices(); }

// 各面のキャップワールド行列を更新
// 面インデックス: 0=Top(Y+), 1=Bottom(Y-), 2=Left(X-), 3=Right(X+)
const float capT = PlanetConst::GrassCapThickness;

// Top (Y+)
if (m_grassCapWork[0])
{
const Math::Vector3 capScale = { m_size.x * PlanetConst::GrassCapXZScale, capT, m_size.z * PlanetConst::GrassCapXZScale };
const float capY = m_pos.y + m_size.y * PlanetConst::GrassCapYOffset + capT;
m_grassCapWorld[0] = Math::Matrix::CreateScale(capScale) * Math::Matrix::CreateTranslation({ m_pos.x, capY, m_pos.z });
m_grassCapWork[0]->CalcNodeMatrices();
}

// Bottom (Y-)
if (m_grassCapWork[1])
{
const Math::Vector3 capScale = { m_size.x * PlanetConst::GrassCapXZScale, capT, m_size.z * PlanetConst::GrassCapXZScale };
const float capY = m_pos.y - m_size.y * PlanetConst::GrassCapYOffset - capT;
m_grassCapWorld[1] = Math::Matrix::CreateScale(capScale) * Math::Matrix::CreateTranslation({ m_pos.x, capY, m_pos.z });
m_grassCapWork[1]->CalcNodeMatrices();
}

// Left (X-)
if (m_grassCapWork[2])
{
// XZ → cap は XZ → YZ 面に貼る（X軸キャップ）→ Z/Y スワップ
const Math::Vector3 capScale = { capT, m_size.y * PlanetConst::GrassCapXZScale, m_size.z * PlanetConst::GrassCapXZScale };
const float capX = m_pos.x - m_size.x * PlanetConst::GrassCapYOffset - capT;
m_grassCapWorld[2] = Math::Matrix::CreateScale(capScale) * Math::Matrix::CreateTranslation({ capX, m_pos.y, m_pos.z });
m_grassCapWork[2]->CalcNodeMatrices();
}

// Right (X+)
if (m_grassCapWork[3])
{
const Math::Vector3 capScale = { capT, m_size.y * PlanetConst::GrassCapXZScale, m_size.z * PlanetConst::GrassCapXZScale };
const float capX = m_pos.x + m_size.x * PlanetConst::GrassCapYOffset + capT;
m_grassCapWorld[3] = Math::Matrix::CreateScale(capScale) * Math::Matrix::CreateTranslation({ capX, m_pos.y, m_pos.z });
m_grassCapWork[3]->CalcNodeMatrices();
}
}
