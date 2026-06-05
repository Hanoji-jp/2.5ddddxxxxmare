#pragma once
#include <string>

//==========================================================
// CameraSettings
// カメラの全パラメータをランタイムで保持するシングルトン
// ImGui で編集し CSV で保存・読込できる
//==========================================================
struct CameraSettings
{
    // ── 基本オフセット ──────────────────────────────────────
    float OffsetX         =  0.0f;   // プレイヤーより右にずらす量
    float OffsetY         =  3.0f;   // プレイヤーより上にずらす量
    float OffsetZ         = -15.0f;  // プレイヤーより手前にずらす量（奥行き距離）

    // ── 追従 ────────────────────────────────────────────────
    float PosLerp         =  0.10f;  // カメラ位置補間速度（小さいほどふわふわ）
    float LookAtLerp      =  0.06f;  // 注視点補間速度
    float FollowWeightX   =  0.60f;  // ルーム中心→プレイヤー 横割合
    float FollowWeightY   =  0.35f;  // 縦割合
    float MinFollowWeightX=  0.10f;  // 完全停止防止の最低追従量（横）
    float MinFollowWeightY=  0.05f;  // 縦

    // ── 注視点 ──────────────────────────────────────────────
    float LookAtHeight    =  1.5f;   // プレイヤー頭上オフセット
    float LookAheadX      =  1.5f;   // 進行方向への先読みオフセット
    float LookAheadY      =  0.2f;

    // ── 浮遊ボブ ────────────────────────────────────────────
    float FloatAmplitude  =  0.005f;  // 上下の浮遊幅（極小）
    float FloatSpeed      =  0.5f;   // 浮遊の周期速度（ゆったり）

    // ── ロール（左右傾き） ───────────────────────────────────
    float RollMaxDeg      =  4.0f;   // 最大傾き角度
    float RollLerp        =  0.05f;  // ロール補間速度
    float RollSensitivity =  0.15f;  // オフセット→ロール変換係数

    // ── 壁クランプ時ズーム ───────────────────────────────────
    float WallZoomMaxRatio    =  0.08f;  // 最大ズーム率（ほんのり）
    float WallZoomLerp        =  0.008f; // ズームの補間速度（かなりゆっくり）
    float WallZoomSensitivity =  0.03f;  // overX → ズーム率の感度（緩め）

    // ── CSV パス ─────────────────────────────────────────────
    static constexpr const char* SavePath = "Asset/Data/camera_settings.csv";

    // ── シングルトン ─────────────────────────────────────────
    static CameraSettings& Instance()
    {
        static CameraSettings s;
        return s;
    }

    void DrawGui();
    void Save() const;
    void Load();

private:
    CameraSettings()  = default;
    ~CameraSettings() = default;
};
