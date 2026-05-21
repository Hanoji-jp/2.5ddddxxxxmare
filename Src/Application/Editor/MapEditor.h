#pragma once
#include "MapObjectData.h"
#include "../Const/MapConst.h"

// マップエディター
// ImGui を使って配置オブジェクトの追加・編集・削除・セーブ・ロードを管理する
class MapEditor
{
public:
    MapEditor()  {}
    ~MapEditor() {}

    void Init();
    void Update();

    // ImGui ウィンドウを描画する（DrawDebug から呼ぶ）
    void DrawGui();

    // 配置データを取得（EditorScene がオブジェクト生成に使う）
    const std::vector<MapObjectData>& GetObjectDataList() const { return m_objectDataList; }

    // セーブ・ロード
    void Save() const;
    void Load();

    // 配置データが変化したか
    bool IsDirty() const { return m_dirty; }
    void ClearDirty()    { m_dirty = false; }

private:
    void DrawObjectList();
    void DrawInspector();
    void DrawModelSelector();

    // 選択中インデックス（-1 = 未選択）
    int  m_selectedIndex = -1;

    // 配置オブジェクトデータリスト
    std::vector<MapObjectData> m_objectDataList;

    // Asset/Data 以下の gltf ファイルリスト（モデル選択用）
    std::vector<std::string> m_modelFileList;

    // モデル選択リストの現在インデックス
    int m_modelSelectIndex = 0;

    bool m_dirty = false;
};
