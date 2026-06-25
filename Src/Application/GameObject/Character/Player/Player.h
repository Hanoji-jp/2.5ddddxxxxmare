#pragma once
#include "../Character.h"
#include "../AnimBlender.h"
#include "../../../Const/PlayerConst.h"
#include "../../../Const/WindBoxConst.h"
#include "../../Weapon/Sword.h"
#include "../../Weapon/Bow.h"
#include "../../Weapon/Arrow.h"
#include "../../Item/HitBox.h"

class Player : public Character
{
public:
    Player()          { Init(); }
    virtual ~Player() {}

    // m_animBlender が m_modelWork の生アドレスを持つためコピー・ムーブ禁止
    Player(const Player&)            = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&)                 = delete;
    Player& operator=(Player&&)      = delete;

    void Init()       override;
    void Update()     override;
    void PostUpdate() override;
    void DrawLit()     override;
    void DrawOutline() override;  // 原神式アウトライン（背面押し出し）
    void DrawBright()  override;  // 取得演出中の加算ブルーム発光

    bool IsVisible() const override { return true; }

    // プレイヤーは死亡(HP0)で Character::TakeDamage が m_isExpired を立てるが、
    // シーンのリストから除去されると復活しても表示・操作されない。プレイヤーは
    // GameScene が明示管理し死亡＝Dead状態で扱うので、自動除去の対象から外す。
    bool IsExpired() const override { return false; }

    void TakeDamage(int _damage) override;
    void TakeDamageFrom(int _damage, const Math::Vector3& sourcePos) override;

    // 即死（無敵を無視して HP を 0 にする。ドッスンの叩きつけ等）
    void InstantDeath();

    // 復活：モデルは作り直さず、HP・状態・速度・操作可否だけ初期化する
    void Revive();

    // 回復：HPを加算（MaxHpでクランプ）し、緑の取得発光を出す（緑石アイテム用）
    void Heal(int amount);

    // 会話中に顔（頭ボーン）を相手へ向ける（毎フレーム呼ぶ。首制限つき）
    void LookAtHead(const Math::Vector3& target);

    // GhostTrail 用：描画ワールド行列を公開
    const Math::Matrix& GetDrawWorld() const { return m_drawWorld; }
    // GhostTrail 用：モデルデータを公開
    std::shared_ptr<KdModelData> GetModelData() const { return m_modelWork.GetData(); }
    // FootDust 用：ダッシュ中かどうか
    bool IsDashing() const { return m_isDashing; }

    // アイテム取得用ヒットボックスを公開（ItemManager が毎フレーム参照）
    HitBox& GetPickupHitBox() { return m_pickupHitBox; }

    // 今フレームに惑星が切り替わったか（GameScene→カメラへのズームトリガー用）
    bool IsPlanetChanged() const { return m_planetChangedThisFrame; }
    void ResetPlanetChangedFlag()  { m_planetChangedThisFrame = false; }

    // パラソルアイテム取得
    void GiveParasol() { m_hasParasol = true; }

    // パラソルを開いて滑空中か（滑空中は場外落下死タイマーを止めるのに使う）
    bool IsParasolOpen() const { return m_isParasolOpen; }

    // 現在の手動重力方向（重力矢印を即切替するのに使う）
    ManualGravityDir GetManualGravityDir() const { return GetManualGravity(); }

    // ステージセレクト（ハブ）用：惑星がない平地で歩けるよう手動重力Downを与える。
    // 重力ゾーン外なら手動重力は維持されるので、ハブを惑星から離して置けば成立する。
    void SetHubGravityDown() { SetInitialGravityDir(ManualGravityDir::Down); }

    // 演出用：操作の有効/無効。false の間は入力を受けず重力落下のみ
    // （投げ出され→不時着などのカットシーン用）。velocity は外部から維持できる。
    void SetControlEnabled(bool e) { m_controlEnabled = e; }
    bool IsControlEnabled() const  { return m_controlEnabled; }

    // 演出中などダメージを無効化する（見せカメラ再生中の被弾防止）
    void SetDamageEnabled(bool e) { m_damageEnabled = e; }

    // 演出用：見た目のタンブル回転（描画のみ。コリジョンには影響しない）
    // 吹っ飛ばされてくるくる回る表現などに使う（ワールドZ軸まわりのロール）。
    void SetCutsceneSpin(float radians) { m_cutsceneSpin = radians; }

    // 演出用：多軸タンブル（描画のみ）。x=pitch, y=yaw, z=roll(rad)。いろんな軸でくるくる回す
    void SetCutsceneTumble(const Math::Vector3& anglesXYZ) { m_cutsceneTumble = anglesXYZ; }

    // 演出用：体を +Z 正面へ向ける（クリア演出の決めポーズ用。描画のみ）
    void SetCutsceneFaceZ(bool on) { m_cutsceneFaceZ = on; }

    // クリア演出：重力コア取得ポーズ（GetGravityCore アニメ）を再生する状態
    void SetClearHold(bool on) { m_clearHold = on; }

    // クリア演出：物理を動かさず、その場でアニメ(GetGravityCore)と姿勢だけ更新する。
    // （クリア中はオブジェクト更新が止まるので GameScene から毎フレーム呼ぶ）
    void UpdateClearPose(float dt);

    // 指定ボーンのワールド行列を返す（コアを GravityCoreBorn ボーンへ追従させる用）。
    // アニメ更新後（CalcNodeMatrices 済み）に呼ぶこと。
    Math::Matrix GetBoneWorld(const std::string& boneName) const;

    // 導入演出ポーズ：パラソル等のノードを隠す（落下中の見た目用）
    void SetIntroPose(bool on);

    // 着地のつぶれ演出を発動（ズサーバタンの「バタン」）
    void TriggerLandingSquash();

    // 演出用：重力スケール（1.0=通常、小さいほどゆっくり落ちて滞空が長い）
    void SetGravityScale(float s) { m_gravityScale = s; }

    // 取得演出の発光を開始（色指定）／ヒットストップ中も進める更新
    void TriggerPickupGlow(const Math::Color& color);
    void UpdatePickupGlow(float dt);

private:
    void Move();
    void Jump();
    void AttackMelee();
    void AttackRanged();
    // アニメーション切り替え
    void ChangeAnim(const std::string& _animName, bool _isLoop = true);
    bool ChangeAnimIfExist(const std::string& _animName, bool _isLoop = true);

    KdModelWork  m_modelWork;
    AnimBlender  m_animBlender;

    // アイテム取得用ヒットボックス
    HitBox       m_pickupHitBox;

    // 描画専用ワールド行列（ピボット補正オフセットを含む。コリジョンには使わない）
    Math::Matrix m_drawWorld;

    // 取得演出の発光（加算ブルーム）残り時間と色
    float        m_pickupGlowTimer = 0.0f;
    Math::Color  m_pickupGlowColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    // 現在再生中のアニメーション名（同アニメの再セット防止）
    std::string  m_currentAnimName;

    // 装備
    std::shared_ptr<Sword>  m_sword;
    std::shared_ptr<Bow>    m_bow;

    // デバッグ：フリーフライ（Fでトグル。WASDで移動・壁貫通）
    bool m_freeFly    = false;
    bool m_freeFlyKeyPrev = false;

    // 飛翔中の矢リスト（シーンへの追加は GameScene が行う想定）
    std::vector<std::shared_ptr<Arrow>> m_arrows;

    // 接線方向の左右符号（+1 = tangent 向き、-1 = 逆向き）
    float m_facingSign = 1.0f;

    // XZ移動の慣性速度（加減速に使用）
    Math::Vector3 m_moveVelocity  = {};

    // ── Z平面固定（2.5D）：移動床等で押されたZを戻す基準（ホームZ）──
    float m_homeZ     = 0.0f;
    bool  m_homeZInit = false;   // 初回フレームで現在Zをホームとして取り込む

    // ── ディゾルブ（0=通常 / 1=溶けきって消滅）──
    // 死亡で 0→1（溶ける）、復活で 1→0（逆再生で再構成）。
    float m_dissolve   = 0.0f;
    bool  m_respawning = false;   // 復活ディゾルブ（逆再生）中か

public:
    // 復活時：ディゾルブを満タン(1)にしてから逆再生で戻す（Revive 後に呼ぶ）
    void TriggerRespawnDissolve() { m_dissolve = 1.0f; m_respawning = true; }
private:

    // 被ダメージ後の無敵タイマー（フレーム。> 0 の間は再被弾しない）
    int m_invincibleTimer = 0;

    // 被ダメージ時に本体を赤くするフラッシュタイマー（フレーム）
    int m_damageFlashTimer = 0;

    // 会話中の頭(顔)向きヨー角（rad、首制限内・平滑化）
    float m_headLookYaw = 0.0f;
    bool  m_headLookActive = false;                       // 会話中に頭を回しているか
    Math::Matrix m_headBaseLocal = Math::Matrix::Identity; // 頭ボーンの基準ローカル（累積防止）

    // 近接攻撃クールダウン
    int m_meleeCooldown = 0;

    // 遠距離攻撃クールダウン
    int m_rangedCooldown = 0;

    // 着地スクワッシュ（0=なし、>0=スクワッシュ中）
    float m_squashTimer = 0.0f;

    // 前フレームの着地状態（エッジ検出用）
    bool m_wasGround = false;

    // ダッシュ中フラグ
    bool m_isDashing = false;
    bool m_dashJumping = false;   // ダッシュ中にジャンプした＝空中でもダッシュ速度を維持（飛距離が伸びる）

    // 操作の有効/無効（演出用カットシーン中は false）
    bool m_controlEnabled = true;
    bool m_damageEnabled  = true;   // false の間は一切ダメージを受けない（演出用）

    // 演出用タンブル回転角（描画のみ。ワールドZ軸ロール）
    float m_cutsceneSpin = 0.0f;

    // 演出用多軸タンブル（描画のみ）。x=pitch, y=yaw, z=roll
    Math::Vector3 m_cutsceneTumble = { 0.0f, 0.0f, 0.0f };

    // 演出用：体を +Z 正面へ固定（描画のみ）
    bool m_cutsceneFaceZ = false;

    // クリア演出：重力コア取得ポーズ再生中
    bool m_clearHold = false;

    // 攻撃モーション再生中フラグ（上半身 Attack 上書き管理用）
    bool m_isAttacking = false;

    // 傘アイテム所持中（未実装：デバッグキー P で取得/無）
    bool m_hasParasol = false;

    // 傘を開いているか（Fall 中のみ有効）
    bool m_isParasolOpen = false;

    // 今フレームに惑星切り替わりが発生したか
    bool m_planetChangedThisFrame = false;

    // アニメーション再生速度倍率（通常=1.0、ダッシュ時は DashAnimSpeedMul）
    float m_animSpeed = 1.0f;

    // ──── 風ギミック ──────────────────────────────────────────
    // 今フレームの風ベクトル（GameScene から ApplyWind() で積算、PostUpdate で消費）
    Math::Vector3 m_windAccum     = {};

    // 風による見た目の傾き角度（度）：正=右へ傾く
    float         m_windTiltAngle = 0.0f;

    // 風ボックス範囲内かどうかのフラグ（前フレーム）
    bool          m_wasInWind        = false;

    // 今フレームの風が左右方向かどうか
    bool          m_windIsHorizontal = false;

    // 水平方向の風速（Move() に上書きされないよう別管理）
    Math::Vector3 m_windHorizVel  = {};

public:
    // 風ボックスから毎フレーム呼ぶ（複数ボックスが重なっても加算で対応）
    void ApplyWind(const Math::Vector3& windDir, float power);

    // 風ボックス範囲外に出たときに呼ぶ（傾きと上昇フラグをリセット）
    void ClearWindState();
};


