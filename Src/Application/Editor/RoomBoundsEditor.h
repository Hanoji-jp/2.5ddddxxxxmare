#pragma once
#include "../GameObject/Camera/RoomBounds.h"
#include <vector>
#include <string>

//==========================================================
// RoomBoundsEditor
// カメラルーム境界を ImGui で編集し CSV で保存・読込する
//
// 各ルームは以下のパラメータを持つ:
//   minX / maxX  : カメラが映せるX範囲（ドア等で制限）
//   minY / maxY  : カメラY範囲
//   triggerX     : 次ルームへの遷移トリガーX
//   blendX       : 遷移ブレンド幅
//==========================================================
class RoomBoundsEditor
{
public:
    RoomBoundsEditor()  {}
    ~RoomBoundsEditor() {}

    // ImGui ウィンドウを描画する（DrawGui から呼ぶ）
    void DrawGui();

    // ルーム一覧を取得（SideScrollCamera::SetRooms に渡す）
    const std::vector<RoomBounds>& GetRooms() const { return m_rooms; }

    // 外部からルームをセット（GameScene 側の既存ハードコードから移行用）
    void SetRooms(const std::vector<RoomBounds>& _rooms) { m_rooms = _rooms; m_dirty = true; }

    // 変更があったか
    bool IsDirty() const { return m_dirty; }
    void ClearDirty()    { m_dirty = false; }

    // CSV ファイルパス
    static constexpr const char* SavePath = "Asset/Data/rooms.csv";

    void Save() const;
    void Load();

    // 3D空間にルーム境界線を描画する（DrawDebugExtra から呼ぶ）
    void DrawDebugLines() const;

private:
    std::vector<RoomBounds> m_rooms;
    bool                    m_dirty       = false;
    int                     m_selectedIdx = -1;
};
