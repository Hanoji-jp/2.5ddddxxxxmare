#include "../../Pch.h"
#include "MapEditor.h"

void MapEditor::Init()
{
    // Asset/Data 以下の .gltf ファイルをスキャンしてリスト化
    m_modelFileList.clear();
    const std::string searchPath = std::string(MapConst::ModelListPath) + "*.gltf";

    WIN32_FIND_DATAA findData{};
    const HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            m_modelFileList.push_back(std::string(MapConst::ModelListPath) + findData.cFileName);
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
    }

    Load();
}

void MapEditor::Update()
{
}

void MapEditor::DrawGui()
{
    ImGui::Begin("Map Editor");

    DrawModelSelector();
    ImGui::Separator();
    DrawObjectList();
    ImGui::Separator();
    DrawInspector();
    ImGui::Separator();

    if (ImGui::Button("Save"))
    {
        Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        Load();
    }

    ImGui::End();
}

void MapEditor::DrawModelSelector()
{
    ImGui::Text("Model");

    // リストボックス用のラベル配列を構築
    std::vector<const char*> labels;
    labels.reserve(m_modelFileList.size());
    for (const auto& path : m_modelFileList)
    {
        labels.push_back(path.c_str());
    }

    if (!labels.empty())
    {
        ImGui::ListBox("##models", &m_modelSelectIndex,
            labels.data(), static_cast<int>(labels.size()), 5);
    }

    if (ImGui::Button("Add Object"))
    {
        if (!m_modelFileList.empty())
        {
            MapObjectData data;
            data.modelPath = m_modelFileList[m_modelSelectIndex];
            m_objectDataList.push_back(data);
            m_selectedIndex = static_cast<int>(m_objectDataList.size()) - 1;
            m_dirty = true;
        }
    }
}

void MapEditor::DrawObjectList()
{
    ImGui::Text("Object List");

    for (int i = 0; i < static_cast<int>(m_objectDataList.size()); ++i)
    {
        const auto& obj = m_objectDataList[i];

        // ファイル名のみ表示
        std::string label = obj.modelPath;
        const size_t slashPos = label.find_last_of("/\\");
        if (slashPos != std::string::npos)
        {
            label = label.substr(slashPos + 1);
        }
        label += "##" + std::to_string(i);

        const bool selected = (m_selectedIndex == i);
        if (ImGui::Selectable(label.c_str(), selected))
        {
            m_selectedIndex = i;
        }
    }
}

void MapEditor::DrawInspector()
{
    if (m_selectedIndex < 0 ||
        m_selectedIndex >= static_cast<int>(m_objectDataList.size()))
    {
        ImGui::Text("No object selected");
        return;
    }

    MapObjectData& obj = m_objectDataList[m_selectedIndex];

    ImGui::Text("Inspector");

    float pos[3] = { obj.position.x, obj.position.y, obj.position.z };
    if (ImGui::DragFloat3("Position", pos, MapConst::SnapSize))
    {
        obj.position = { pos[0], pos[1], pos[2] };
        m_dirty = true;
    }

    float rot[3] = { obj.rotation.x, obj.rotation.y, obj.rotation.z };
    if (ImGui::DragFloat3("Rotation", rot, 0.01f))
    {
        obj.rotation = { rot[0], rot[1], rot[2] };
        m_dirty = true;
    }

    float scl[3] = { obj.scale.x, obj.scale.y, obj.scale.z };
    if (ImGui::DragFloat3("Scale", scl, 0.01f))
    {
        obj.scale = { scl[0], scl[1], scl[2] };
        m_dirty = true;
    }

    if (ImGui::Button("Delete"))
    {
        m_objectDataList.erase(m_objectDataList.begin() + m_selectedIndex);
        m_selectedIndex = -1;
        m_dirty = true;
    }
}

void MapEditor::Save() const
{
    std::ofstream ofs(MapConst::SaveFilePath);
    if (!ofs) { return; }

    for (const auto& obj : m_objectDataList)
    {
        ofs << obj.modelPath << " "
            << obj.position.x << " " << obj.position.y << " " << obj.position.z << " "
            << obj.rotation.x << " " << obj.rotation.y << " " << obj.rotation.z << " "
            << obj.scale.x    << " " << obj.scale.y    << " " << obj.scale.z
            << "\n";
    }
}

void MapEditor::Load()
{
    std::ifstream ifs(MapConst::SaveFilePath);
    if (!ifs) { return; }

    m_objectDataList.clear();

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty()) { continue; }

        std::istringstream ss(line);
        MapObjectData obj;
        ss >> obj.modelPath
           >> obj.position.x >> obj.position.y >> obj.position.z
           >> obj.rotation.x >> obj.rotation.y >> obj.rotation.z
           >> obj.scale.x    >> obj.scale.y    >> obj.scale.z;

        m_objectDataList.push_back(obj);
    }

    m_dirty = true;
}
