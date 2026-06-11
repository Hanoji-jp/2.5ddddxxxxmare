#include "../../Pch.h"
#include "CameraSettings.h"
#include <fstream>
#include <sstream>

//----------------------------------------------------------
// ImGui ウィンドウ描画
//----------------------------------------------------------
void CameraSettings::DrawGui()
{
    if (!ImGui::Begin("Camera Settings"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Save CSV")) { Save(); }
    ImGui::SameLine();
    if (ImGui::Button("Load CSV")) { Load(); }

    ImGui::Separator();

    ImGui::Text("-- Basic Offset --");
    ImGui::DragFloat("OffsetX (right)",   &OffsetX,          0.05f, -50.0f,  50.0f);
    ImGui::DragFloat("OffsetY (up)",      &OffsetY,          0.05f, -50.0f,  50.0f);
    ImGui::DragFloat("OffsetZ (depth)",   &OffsetZ,          0.1f,  -100.0f, -1.0f);

    ImGui::Separator();
    ImGui::Text("-- Follow --");
    ImGui::DragFloat("PosLerp",           &PosLerp,          0.005f, 0.01f, 1.0f);
    ImGui::DragFloat("LookAtLerp",        &LookAtLerp,       0.005f, 0.01f, 1.0f);
    ImGui::DragFloat("FollowWeightX",     &FollowWeightX,    0.01f,  0.0f,  1.0f);
    ImGui::DragFloat("FollowWeightY",     &FollowWeightY,    0.01f,  0.0f,  1.0f);
    ImGui::DragFloat("MinFollowWeightX",  &MinFollowWeightX, 0.005f, 0.0f,  1.0f);
    ImGui::DragFloat("MinFollowWeightY",  &MinFollowWeightY, 0.005f, 0.0f,  1.0f);

    ImGui::Separator();
    ImGui::Text("-- LookAt --");
    ImGui::DragFloat("LookAtHeight",      &LookAtHeight,     0.05f, -10.0f, 20.0f);
    ImGui::DragFloat("LookAheadX",        &LookAheadX,       0.05f, -20.0f, 20.0f);
    ImGui::DragFloat("LookAheadY",        &LookAheadY,       0.05f, -10.0f, 10.0f);

    ImGui::Separator();
    ImGui::Text("-- Float Bob --");
    ImGui::DragFloat("FloatAmplitude",    &FloatAmplitude,   0.01f,  0.0f,  5.0f);
    ImGui::DragFloat("FloatSpeed",        &FloatSpeed,       0.01f,  0.0f,  10.0f);

    ImGui::Separator();
    ImGui::Text("-- Roll --");
    ImGui::DragFloat("RollMaxDeg",        &RollMaxDeg,       0.1f,   0.0f,  30.0f);
    ImGui::DragFloat("RollLerp",          &RollLerp,         0.005f, 0.01f, 1.0f);
    ImGui::DragFloat("RollSensitivity",   &RollSensitivity,  0.005f, 0.0f,  2.0f);

    ImGui::Separator();
    ImGui::Text("-- Wall Clamp Zoom --");
    ImGui::DragFloat("WallZoomMaxRatio",   &WallZoomMaxRatio,   0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("WallZoomLerp",       &WallZoomLerp,       0.005f,0.01f,1.0f);
    ImGui::DragFloat("WallZoomSensitivity",&WallZoomSensitivity,0.05f, 0.0f,10.0f);

    ImGui::End();
}

//----------------------------------------------------------
// CSV 保存
//  key,value 形式で1行ずつ
//----------------------------------------------------------
void CameraSettings::Save() const
{
    std::ofstream ofs(SavePath);
    if (!ofs) { return; }

    ofs << "OffsetX,"          << OffsetX          << "\n";
    ofs << "OffsetY,"          << OffsetY          << "\n";
    ofs << "OffsetZ,"          << OffsetZ          << "\n";
    ofs << "PosLerp,"          << PosLerp          << "\n";
    ofs << "LookAtLerp,"       << LookAtLerp       << "\n";
    ofs << "FollowWeightX,"    << FollowWeightX    << "\n";
    ofs << "FollowWeightY,"    << FollowWeightY    << "\n";
    ofs << "MinFollowWeightX," << MinFollowWeightX << "\n";
    ofs << "MinFollowWeightY," << MinFollowWeightY << "\n";
    ofs << "LookAtHeight,"     << LookAtHeight     << "\n";
    ofs << "LookAheadX,"       << LookAheadX       << "\n";
    ofs << "LookAheadY,"       << LookAheadY       << "\n";
    ofs << "FloatAmplitude,"   << FloatAmplitude   << "\n";
    ofs << "FloatSpeed,"       << FloatSpeed       << "\n";
    ofs << "RollMaxDeg,"       << RollMaxDeg       << "\n";
    ofs << "RollLerp,"         << RollLerp         << "\n";
    ofs << "RollSensitivity,"  << RollSensitivity  << "\n";
    ofs << "WallZoomMaxRatio,"   << WallZoomMaxRatio   << "\n";
    ofs << "WallZoomLerp,"       << WallZoomLerp       << "\n";
    ofs << "WallZoomSensitivity,"<< WallZoomSensitivity<< "\n";
}

//----------------------------------------------------------
// CSV 読込
//----------------------------------------------------------
void CameraSettings::Load()
{
    std::ifstream ifs(SavePath);
    if (!ifs) { return; }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty()) { continue; }

        const auto comma = line.find(',');
        if (comma == std::string::npos) { continue; }

        const std::string key   = line.substr(0, comma);
        float             value = 0.0f;
        try { value = std::stof(line.substr(comma + 1)); }
        catch (...) { continue; }

        if      (key == "OffsetX")           OffsetX          = value;
        else if (key == "OffsetY")           OffsetY          = value;
        else if (key == "OffsetZ")           OffsetZ          = value;
        else if (key == "PosLerp")           PosLerp          = value;
        else if (key == "LookAtLerp")        LookAtLerp       = value;
        else if (key == "FollowWeightX")     FollowWeightX    = value;
        else if (key == "FollowWeightY")     FollowWeightY    = value;
        else if (key == "MinFollowWeightX")  MinFollowWeightX = value;
        else if (key == "MinFollowWeightY")  MinFollowWeightY = value;
        else if (key == "LookAtHeight")      LookAtHeight     = value;
        else if (key == "LookAheadX")        LookAheadX       = value;
        else if (key == "LookAheadY")        LookAheadY       = value;
        else if (key == "FloatAmplitude")    FloatAmplitude   = value;
        else if (key == "FloatSpeed")        FloatSpeed       = value;
        else if (key == "RollMaxDeg")        RollMaxDeg       = value;
        else if (key == "RollLerp")          RollLerp         = value;
        else if (key == "RollSensitivity")   RollSensitivity  = value;
        else if (key == "WallZoomMaxRatio")   WallZoomMaxRatio   = value;
        else if (key == "WallZoomLerp")        WallZoomLerp       = value;
        else if (key == "WallZoomSensitivity") WallZoomSensitivity= value;
    }
}

