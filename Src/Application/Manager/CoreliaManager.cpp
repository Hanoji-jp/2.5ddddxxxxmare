#include "../../Pch.h"
#include "CoreliaManager.h"
#include "StageManager.h"
#include "ModelManager.h"
#include "PlanetGravityManager.h"
#include "ManualGravityZoneManager.h"
#include "../Const/CoreliaConst.h"
#include "../Const/PlanetConst.h"
#include "../../Framework/Utility/KdDebug/KdDebugWireFrame.h"
#include "../../Framework/Utility/KdFPSController.h"
#include <fstream>
#include <sstream>

namespace
{
    const std::string kEmpty;
}

// モデルとアニメーター(Idle)をセットアップ（T字ポーズ防止）
static void SetupNpcModel(CoreliaNpc& npc)
{
    npc.modelWork = std::make_shared<KdModelWork>();
    const auto spModel = ModelManager::Instance().GetModel(CoreliaConst::ModelPath);
    if (spModel) { npc.modelWork->SetModelData(spModel); }

    npc.animator = std::make_shared<KdAnimator>();
    if (spModel)
    {
        auto anim = spModel->GetAnimation("Idle");
        if (!anim) { anim = spModel->GetAnimation(static_cast<uint32_t>(0)); }   // 無ければ先頭
        if (anim) { npc.animator->SetAnimation(anim, true); }
    }

    // 手持ち武器・背中の剣を非表示（NPCには持たせない）
    if (npc.modelWork->IsEnable())
    {
        auto& nodes = npc.modelWork->WorkNodes();
        for (auto& n : nodes)
        {
            for (const char* hn : CoreliaConst::HiddenNodeNames)
            {
                if (n.m_name == hn) { n.m_visible = false; break; }
            }
        }
    }
}

// pos から -up 方向へレイを撃ち、地面（上向き面）の表面点を返す。見つからなければ false。
static bool SnapToGround(const Math::Vector3& pos, const Math::Vector3& up, Math::Vector3& outGround)
{
    const Math::Vector3 rayStart = pos + up * CoreliaConst::GroundRayUp;
    const KdCollider::RayInfo ray(KdCollider::TypeGround, rayStart, -up, CoreliaConst::GroundRayLen);

    bool  found = false;
    float bestUp = 0.0f;
    Math::Vector3 bestPos;
    for (const auto& p : PlanetGravityManager::Instance().GetPlanets())
    {
        if (!p.pCollider) { continue; }
        std::list<KdCollider::CollisionResult> results;
        if (!p.pCollider->Intersects(ray, p.mWorld, &results)) { continue; }
        for (const auto& r : results)
        {
            if (r.m_hitNDir.Dot(up) < 0.5f) { continue; }   // 上向き面のみ
            const float hu = r.m_hitPos.Dot(up);
            if (!found || hu > bestUp) { bestUp = hu; bestPos = r.m_hitPos; found = true; }
        }
    }
    if (found) { outGround = bestPos; }   // 地面ぴったりに置く（余分な持ち上げなし）
    return found;
}

// NPC位置の重力上方向
Math::Vector3 CoreliaManager::GravityUpAt(const Math::Vector3& pos)
{
    const GravityInfluenceResult g = PlanetGravityManager::Instance().ComputeGravityInfluence(pos);
    Math::Vector3 up = g.hasInfluence ? g.dominantUpDir : Math::Vector3(0.0f, 1.0f, 0.0f);
    if (up.LengthSquared() < 1e-6f) { up = Math::Vector3(0.0f, 1.0f, 0.0f); }
    up.Normalize();
    return up;
}

namespace
{
    // up を軸とした a→b の符号付き角度(rad)
    float SignedAngleAround(const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& up)
    {
        const float d = std::clamp(a.Dot(b), -1.0f, 1.0f);
        float ang = acosf(d);
        Math::Vector3 c; a.Cross(b, c);
        if (c.Dot(up) < 0.0f) { ang = -ang; }
        return ang;
    }
    Math::Vector3 RotateAround(const Math::Vector3& v, const Math::Vector3& axis, float rad)
    {
        const Math::Matrix m = Math::Matrix::CreateFromAxisAngle(axis, rad);
        return Math::Vector3::TransformNormal(v, m);
    }
}

void CoreliaManager::Update(float dt)
{
    m_animTime += dt;

    // フレームレート非依存：60fps基準のフレーム換算
    const float dt60 = dt * 60.0f;
    const float bodyTurn = std::min(1.0f, CoreliaConst::BodyTurnLerp * dt60);
    const float headTurn = std::min(1.0f, CoreliaConst::HeadTurnLerp * dt60);

    const float neckLimit = CoreliaConst::NeckLimitDeg * 0.01745329f;  // deg→rad

    for (auto& npc : m_npcs)
    {
        // 重力上を決める。NormalGravityゾーン内（横重力などの部屋）では
        // ゾーンの重力方向の逆をUpにする＝NPCが壁向きに正しく傾く。
        // ゾーン外は従来どおり惑星重力から求める。
        Math::Vector3 up;
        Math::Vector3 zoneGravDir;
        if (ManualGravityZoneManager::Instance().IsInNormalGravityZone(npc.Pos, zoneGravDir))
        {
            up = -zoneGravDir;
            if (up.LengthSquared() < 1e-6f) { up = Math::Vector3(0.0f, 1.0f, 0.0f); }
            up.Normalize();
        }
        else
        {
            up = GravityUpAt(npc.Pos);
        }
        npc.Up = up;
        Math::Vector3 ground;
        npc.GroundPos = SnapToGround(npc.Pos, up, ground) ? ground : npc.Pos;

        // プレイヤー方向を地面平面に射影
        Math::Vector3 toP = m_playerPos - npc.GroundPos;
        Math::Vector3 targetFwd = toP - up * toP.Dot(up);
        const bool hasDir = (targetFwd.LengthSquared() > 1e-4f);
        if (hasDir) { targetFwd.Normalize(); }

        // BodyFwd 初期化／平面正規化
        npc.BodyFwd = npc.BodyFwd - up * npc.BodyFwd.Dot(up);
        if (npc.BodyFwd.LengthSquared() < 1e-4f) { npc.BodyFwd = hasDir ? targetFwd : Math::Vector3(0.0f, 0.0f, 1.0f); }
        npc.BodyFwd.Normalize();

        if (hasDir)
        {
            // 体に対する目標ヨー
            const float a = SignedAngleAround(npc.BodyFwd, targetFwd, up);
            // 首制限を超えた分だけ体が追従（ゆっくり）
            const float over = a - std::clamp(a, -neckLimit, neckLimit);
            if (fabsf(over) > 1e-4f)
            {
                npc.BodyFwd = RotateAround(npc.BodyFwd, up, over * bodyTurn);
                npc.BodyFwd.Normalize();
            }
            // 頭は残り角を首制限内で（速く）追従
            const float rem = SignedAngleAround(npc.BodyFwd, targetFwd, up);
            const float headTarget = std::clamp(rem, -neckLimit, neckLimit);
            npc.HeadYaw += (headTarget - npc.HeadYaw) * headTurn;
        }

        // アニメ更新 → ノード行列確定 → 頭ボーンをローカル変形で回す（子も追従・上書きされない）
        if (npc.animator && npc.modelWork && npc.modelWork->GetData())
        {
            npc.animator->AdvanceTime(npc.modelWork->WorkNodes(), dt60);
            npc.modelWork->CalcNodeMatrices();   // 頭ワールド行列を取得するため一旦確定

            if (fabsf(npc.HeadYaw) > 1e-4f)
            {
                auto& nodes = npc.modelWork->WorkNodes();   // 非const取得（needCalc=trueになる）
                int hi = -1;
                for (int i = 0; i < static_cast<int>(nodes.size()) && hi < 0; ++i)
                {
                    for (const char* nm : CoreliaConst::HeadBoneNames)
                    {
                        if (nodes[i].m_name == nm) { hi = i; break; }
                    }
                }
                if (hi >= 0)
                {
                    // m_worldTransform はモデルローカル空間（orient適用前）なので、
                    // 軸はワールドUpではなくモデルローカルの+Y(=orientの中段行に対応)を使う。
                    // これで横重力(orientが回転)でも首が正しく水平に回る。
                    const Math::Vector3 modelUp(0.0f, 1.0f, 0.0f);
                    Math::Vector3 axis = Math::Vector3::TransformNormal(modelUp, nodes[hi].m_worldTransform.Invert());
                    if (axis.LengthSquared() > 1e-8f) { axis.Normalize(); }
                    const Math::Matrix R = Math::Matrix::CreateFromAxisAngle(axis, npc.HeadYaw);
                    nodes[hi].m_localTransform = R * nodes[hi].m_localTransform;
                }
            }
            npc.modelWork->CalcNodeMatrices();   // 頭回転を反映して再確定（DrawModelは再計算しない）
        }
    }
}

int CoreliaManager::FindInteractable(const Math::Vector3& _playerPos) const
{
    int   best = -1;
    float bestDistSq = CoreliaConst::InteractRadius * CoreliaConst::InteractRadius;
    for (int i = 0; i < static_cast<int>(m_npcs.size()); ++i)
    {
        const Math::Vector3 d = m_npcs[i].GroundPos - _playerPos;
        const float distSq = d.LengthSquared();
        if (distSq <= bestDistSq)
        {
            bestDistSq = distSq;
            best = i;
        }
    }
    return best;
}

bool CoreliaManager::GetNpcPos(int index, Math::Vector3& out) const
{
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) { return false; }
    out = m_npcs[index].GroundPos + Math::Vector3{ 0.0f, CoreliaConst::FloatUp + 1.0f, 0.0f };
    return true;
}

bool CoreliaManager::GetNpcGroundPos(int index, Math::Vector3& out) const
{
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) { return false; }
    out = m_npcs[index].GroundPos;
    return true;
}

bool CoreliaManager::GetNpcUp(int index, Math::Vector3& out) const
{
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) { return false; }
    out = m_npcs[index].Up;   // 接地時に求めた実際の上方向（壁ならその法線）
    return true;
}

int CoreliaManager::GetNpcHintId(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) { return 0; }
    return m_npcs[index].HintId;
}

int CoreliaManager::GetNpcBubbleDir(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_npcs.size())) { return 0; }
    return m_npcs[index].BubbleDir;
}

const std::string& CoreliaManager::GetHint(int hintId) const
{
    if (hintId < 0 || hintId >= static_cast<int>(m_hints.size())) { return kEmpty; }
    return m_hints[hintId];
}

void CoreliaManager::DrawLit()
{
    auto& sm = KdShaderManager::Instance();
    auto& shader = sm.m_StandardShader;

    // ゴースト（死んだ霊）表現：半透明で描く。深度は書く（DoFでボケないように）。
    sm.ChangeBlendState(KdBlendState::Alpha);
    sm.ChangeDepthStencilState(KdDepthStencilState::ZEnable);

    const float bob = std::sinf(m_animTime * CoreliaConst::BobSpeed) * CoreliaConst::BobAmp;

    for (const auto& npc : m_npcs)
    {
        if (!npc.modelWork || !npc.modelWork->GetData()) { continue; }

        // Update() で求めた重力上＋体の向き（頭追尾は頭ボーンで適用済み）
        const Math::Vector3 up  = npc.Up;
        Math::Vector3 fwd = npc.BodyFwd;
        Math::Vector3 right; up.Cross(fwd, right);
        if (right.LengthSquared() > 1e-4f) { right.Normalize(); } else { right = Math::Vector3(0.0f, 0.0f, 1.0f); }

        const Math::Matrix orient(
            right.x, right.y, right.z, 0.0f,
            up.x,    up.y,    up.z,    0.0f,
            fwd.x,   fwd.y,   fwd.z,   0.0f,
            0.0f,    0.0f,    0.0f,    1.0f);

        // Update() で確定した接地位置を使う（判定とモデルがズレない）
        const Math::Vector3 drawPos = npc.GroundPos + up * (CoreliaConst::ModelOffsetY + CoreliaConst::FloatUp + bob);

        const Math::Matrix world =
            Math::Matrix::CreateScale(CoreliaConst::ModelScale) *
            orient *
            Math::Matrix::CreateTranslation(drawPos);

        // プレイヤーのクローンに見えないよう色を付ける（コアリアらしいシアン）＋半透明ゴースト
        const Math::Color tint(CoreliaConst::TintR, CoreliaConst::TintG, CoreliaConst::TintB,
                               CoreliaConst::GhostAlpha);
        shader.DrawModel(*npc.modelWork, world, tint);
    }

    sm.UndoDepthStencilState();
    sm.UndoBlendState();
}

void CoreliaManager::DrawDebugShapes() const
{
    for (int i = 0; i < static_cast<int>(m_npcs.size()); ++i)
    {
        const auto& npc = m_npcs[i];
        const bool selected = (i == m_selectedIndex);
        const Math::Color color = selected
            ? Math::Color(1.0f, 1.0f, 0.0f, 0.9f)
            : Math::Color(0.4f, 1.0f, 0.8f, 0.8f);

        // interact radius ring（重力に合わせて地面の平面上に描く）
        const Math::Vector3 up  = npc.Up;
        Math::Vector3 fwd = npc.BodyFwd;
        Math::Vector3 right; up.Cross(fwd, right);
        if (right.LengthSquared() > 1e-4f) { right.Normalize(); } else { right = Math::Vector3(0.0f, 0.0f, 1.0f); }

        KdDebugWireFrame wire;
        const int seg = 24;
        const float r = CoreliaConst::InteractRadius;
        Math::Vector3 prev = npc.GroundPos + right * r;
        for (int s = 1; s <= seg; ++s)
        {
            const float t = 6.2831853f * s / seg;
            const Math::Vector3 cur = npc.GroundPos + right * (r * cosf(t)) + fwd * (r * sinf(t));
            wire.AddDebugLine(prev, cur, color);
            prev = cur;
        }
        wire.Draw();
    }
}

void CoreliaManager::DrawGui()
{
    if (!ImGui::Begin(U8("コアリアNPC"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored({ 0.4f, 1.0f, 0.8f, 1.0f }, "Corelia (hint NPC) Editor");
    ImGui::Text(U8("読込ヒント数: %d"), GetHintCount());
    ImGui::Separator();

    if (ImGui::Button(U8("コアリア追加")))
    {
        CoreliaNpc npc;
        npc.Pos = { 0.0f, 0.0f, 0.0f };
        npc.HintId = 0;
        SetupNpcModel(npc);
        m_npcs.push_back(std::move(npc));
        m_selectedIndex = static_cast<int>(m_npcs.size()) - 1;
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_npcs.size()); ++i)
    {
        char label[64];
        sprintf_s(label, "Corelia %d (hint %d)", i, m_npcs[i].HintId);
        if (ImGui::Selectable(label, m_selectedIndex == i)) { m_selectedIndex = i; }
    }

    ImGui::Separator();

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_npcs.size()))
    {
        auto& npc = m_npcs[m_selectedIndex];

        float pos[3] = { npc.Pos.x, npc.Pos.y, npc.Pos.z };
        if (ImGui::DragFloat3(U8("位置"), pos, 0.5f, -1000.0f, 1000.0f))
        {
            npc.Pos = { pos[0], pos[1], pos[2] };
        }

        const int maxHint = (GetHintCount() > 0) ? GetHintCount() - 1 : 0;
        ImGui::DragInt(U8("ヒントID"), &npc.HintId, 0.1f, 0, maxHint);
        if (npc.HintId < 0) { npc.HintId = 0; }
        if (npc.HintId > maxHint) { npc.HintId = maxHint; }

        // 話す吹き出しの向き（見た目が横向きのNPCは Left/Right を選ぶ）
        const char* dirItems[] = { U8("上"), U8("下"), U8("左"), U8("右") };
        ImGui::Combo(U8("吹き出し向き"), &npc.BubbleDir, dirItems, 4);

        if (ImGui::Button(U8("削除")))
        {
            m_npcs.erase(m_npcs.begin() + m_selectedIndex);
            m_selectedIndex = -1;
        }
    }

    ImGui::Separator();
    if (ImGui::Button(U8("保存"))) { Save(); }
    ImGui::SameLine();
    if (ImGui::Button(U8("読込"))) { Load(); }
    ImGui::SameLine();
    if (ImGui::Button(U8("ヒント再読込"))) { LoadHints(); }

    ImGui::End();
}

void CoreliaManager::Save() const
{
    std::ofstream ofs(StageManager::Instance().ResolvePath("Corelias.csv"));
    if (!ofs) { return; }

    for (const auto& npc : m_npcs)
    {
        ofs << npc.Pos.x << ","
            << npc.Pos.y << ","
            << npc.Pos.z << ","
            << npc.HintId << ","
            << npc.BubbleDir << "\n";
    }
}

void CoreliaManager::Load()
{
    LoadHints();

    KdAssetIStream ifs(StageManager::Instance().ResolvePath("Corelias.csv"));

    // 新ステージのファイルが無くても空にする（前ステージのNPCを残さない）
    m_npcs.clear();
    if (!ifs) { return; }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty()) { continue; }

        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, ',')) { tokens.push_back(token); }
        if (tokens.size() < 4) { continue; }

        CoreliaNpc npc;
        npc.Pos.x  = std::stof(tokens[0]);
        npc.Pos.y  = std::stof(tokens[1]);
        npc.Pos.z  = std::stof(tokens[2]);
        npc.HintId = std::stoi(tokens[3]);
        if (tokens.size() >= 5) { npc.BubbleDir = std::stoi(tokens[4]); }   // 旧データは無→既定0(上)

        SetupNpcModel(npc);

        m_npcs.push_back(std::move(npc));
    }
}

void CoreliaManager::LoadHints()
{
    m_hints.clear();
    KdAssetIStream ifs(CoreliaConst::HintsPath);
    if (!ifs) { return; }

    std::string line;
    while (std::getline(ifs, line))
    {
        // strip trailing CR (file is CRLF)
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        m_hints.push_back(line);
    }
}
