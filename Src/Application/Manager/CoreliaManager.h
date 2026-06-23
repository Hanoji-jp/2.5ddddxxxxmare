#pragma once
#include "../../Framework/Direct3D/KdModel.h"
#include "../../Framework/Math/KdAnimation.h"

//==========================================================
// CoreliaNpc
// Hint-giving companion NPC. Placed in the world; talk to get a hint.
//==========================================================
struct CoreliaNpc
{
    Math::Vector3 Pos      = { 0.0f, 0.0f, 0.0f };  // エディタで置く点
    Math::Vector3 GroundPos = { 0.0f, 0.0f, 0.0f };  // 接地後の実位置（描画・判定・会話で共通使用）
    int HintId = 0;
    int BubbleDir = 0;   // 話す吹き出しの向き（0=上,1=下,2=左,3=右）。見た目重力に合わせて手動指定

    // 追尾用の状態（Update で更新）
    Math::Vector3 Up      = { 0.0f, 1.0f, 0.0f };   // 重力上方向
    Math::Vector3 BodyFwd = { 0.0f, 0.0f, 0.0f };   // 体の向き（地面平面・ゆっくり追従）
    float         HeadYaw = 0.0f;                    // 体に対する頭のヨー角(rad、首制限内)

    // Drawing
    std::shared_ptr<KdModelWork> modelWork;
    std::shared_ptr<KdAnimator>  animator;   // Idle 再生（T字ポーズ防止）
};

//==========================================================
// CoreliaManager
// Manages Corelia NPCs (editor + CSV) and hint lookup.
// Mirrors DeadZoneManager / ManualGravityZoneManager.
//==========================================================
class CoreliaManager
{
public:
    static CoreliaManager& Instance()
    {
        static CoreliaManager s;
        return s;
    }

    // プレイヤー位置を渡す（コアリアが体・顔をプレイヤーへ向ける追尾に使う）
    void SetPlayerPos(const Math::Vector3& p) { m_playerPos = p; }

    // Advance idle animation time / per-NPC animators.
    void Update(float dt);

    // Find the nearest interactable NPC within range. Returns index or -1.
    int FindInteractable(const Math::Vector3& _playerPos) const;

    // NPC accessors
    int  GetNpcCount() const { return static_cast<int>(m_npcs.size()); }
    bool GetNpcPos(int index, Math::Vector3& out) const;
    bool GetNpcGroundPos(int index, Math::Vector3& out) const;   // 接地点の素の座標（Upオフセット無し）
    bool GetNpcUp(int index, Math::Vector3& out) const;   // 接地時の実際の上方向（吹き出し配置用）
    int  GetNpcHintId(int index) const;
    int  GetNpcBubbleDir(int index) const;                // 吹き出しの向き（0=上,1=下,2=左,3=右）

    // Hint body text for a hint id (Shift-JIS bytes; empty if missing).
    const std::string& GetHint(int hintId) const;
    int  GetHintCount() const { return static_cast<int>(m_hints.size()); }

    void ClearNpcs() { m_npcs.clear(); }

    // エディタ操作（マウスピッキング／生成／コピー）用のアクセサ
    CoreliaNpc* GetNpc(int _index)
    {
        if (_index < 0 || _index >= static_cast<int>(m_npcs.size())) { return nullptr; }
        return &m_npcs[_index];
    }
    void AddNpc(const CoreliaNpc& _npc) { m_npcs.push_back(_npc); }
    void SetSelected(int _i)            { m_selectedIndex = _i; }
    void RemoveLastNpc()                { if (!m_npcs.empty()) { m_npcs.pop_back(); } }

    // Drawing
    void DrawLit();
    void DrawDebugShapes() const;

    // ImGui editor
    void DrawGui();

    // CSV + hint file
    void Save() const;
    void Load();
    void LoadHints();

private:
    CoreliaManager() = default;

    // NPC位置の重力上方向を返す
    static Math::Vector3 GravityUpAt(const Math::Vector3& pos);

    std::vector<CoreliaNpc>  m_npcs;
    Math::Vector3            m_playerPos = { 0.0f, 0.0f, 0.0f };
    std::vector<std::string> m_hints;     // index = hintId
    float m_animTime = 0.0f;
    int   m_selectedIndex = -1;
};
