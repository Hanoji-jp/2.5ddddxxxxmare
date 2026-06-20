#pragma once
#include <string>
#include <cstdio>
#include <filesystem>

//==========================================================
// StageManager
// Per-stage map data router. Resolves a base file name to a
// stage-specific path:  Asset/Data/Stage{NN}/<fileName>.
// Set the stage index before switching to GameScene; every
// editor / manager loads & saves its CSV through ResolvePath().
//==========================================================
class StageManager
{
public:
    static StageManager& Instance()
    {
        static StageManager s;
        return s;
    }

    // 1-based stage index (clamped to >= 1).
    void SetStageIndex(int _i) { m_stageIndex = (_i < 1) ? 1 : _i; }
    int  GetStageIndex() const { return m_stageIndex; }

    // Stage data folder (with trailing slash), e.g. "Asset/Data/Stage01/".
    std::string Dir() const
    {
        char buf[64] = {};
        std::snprintf(buf, sizeof(buf), "Asset/Data/Stage%02d/", m_stageIndex);
        return std::string(buf);
    }

    // Resolve a base file name to the current stage's full path.
    // Ensures the stage folder exists so saves never fail.
    std::string ResolvePath(const char* _fileName) const
    {
        std::error_code ec;
        std::filesystem::create_directories(Dir(), ec);
        return Dir() + _fileName;
    }

private:
    StageManager() = default;
    ~StageManager() = default;
    StageManager(const StageManager&)            = delete;
    StageManager& operator=(const StageManager&) = delete;

    int m_stageIndex = 1;
};
