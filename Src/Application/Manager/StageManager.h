#pragma once
#include <string>
#include <cstdio>
#include <fstream>
#include <array>
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

    //------------------------------------------------------
    // Stage-clear result, carried from GameScene to StageSelect.
    //------------------------------------------------------
    struct StageResult
    {
        bool  pending = false;   // a fresh clear is waiting to be shown
        int   stageId = 0;       // 0-based id (for stage-name lookup)
        int   coins   = 0;       // coins collected in the run
        int   rocks   = 0;       // rock items collected in the run
        int   deaths  = 0;       // times died in the run
        float time    = 0.0f;    // play time until clear (seconds)
    };

    void SetResult(int _stageId, int _coins, int _rocks, int _deaths, float _time)
    {
        m_result = { true, _stageId, _coins, _rocks, _deaths, _time };
    }
    // Read the result once; clears the pending flag so it only shows one time.
    StageResult ConsumeResult()
    {
        StageResult r = m_result;
        m_result.pending = false;
        return r;
    }

    //------------------------------------------------------
    // Lifetime totals (coins / rocks), saved across sessions.
    //------------------------------------------------------
    int  GetTotalCoins() const { return m_totalCoins; }
    int  GetTotalRocks() const { return m_totalRocks; }
    void AddTotalCoins(int n)  { m_totalCoins += n; SaveTotals(); }
    void AddTotalRocks(int n)  { m_totalRocks += n; SaveTotals(); }

    //------------------------------------------------------
    // 初回起動判定（セーブデータ＝フラグファイルの有無で判定）。
    // 初回のみ Story → Stage1 へ直行。以降は Story を出さない。
    //------------------------------------------------------
    bool IsFirstLaunch() const
    {
        return !std::filesystem::exists("Asset/Data/launched.flag");
    }
    void MarkLaunched() const
    {
        std::error_code ec;
        std::filesystem::create_directories("Asset/Data/", ec);
        std::ofstream ofs("Asset/Data/launched.flag");
        if (ofs) { ofs << "1"; }
    }

    //------------------------------------------------------
    // セーブデータを全消去（デバッグ用）。初回起動フラグ・合計・ステージ記録をリセット。
    //------------------------------------------------------
    void ResetSaveData()
    {
        std::error_code ec;
        std::filesystem::remove("Asset/Data/launched.flag", ec);
        std::filesystem::remove(TotalsPath(), ec);
        std::filesystem::remove(RecordsPath(), ec);
        m_totalCoins = 0;
        m_totalRocks = 0;
        m_records = std::array<StageRecord, kMaxStages>{};
    }

    //------------------------------------------------------
    // Per-stage best records (cleared / best coins / best time), saved across sessions.
    //------------------------------------------------------
    static constexpr int kMaxStages = 16;
    struct StageRecord
    {
        bool  cleared   = false;
        int   bestCoins = 0;
        float bestTime  = 0.0f;   // 0 = no record yet
    };

    const StageRecord& GetRecord(int stageId) const
    {
        static const StageRecord empty;
        if (stageId < 0 || stageId >= kMaxStages) { return empty; }
        return m_records[stageId];
    }

    // Call on stage clear: marks cleared and keeps the best coins/time.
    void RecordClear(int stageId, int coins, float time)
    {
        if (stageId < 0 || stageId >= kMaxStages) { return; }
        StageRecord& r = m_records[stageId];
        r.cleared = true;
        if (coins > r.bestCoins) { r.bestCoins = coins; }
        if (r.bestTime <= 0.0f || time < r.bestTime) { r.bestTime = time; }
        SaveRecords();
    }

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
    StageManager() { LoadTotals(); LoadRecords(); }
    ~StageManager() = default;
    StageManager(const StageManager&)            = delete;
    StageManager& operator=(const StageManager&) = delete;

    // Totals are saved in a single global file (not per-stage).
    std::string TotalsPath() const { return "Asset/Data/totals.csv"; }

    void LoadTotals()
    {
        std::ifstream ifs(TotalsPath());
        if (!ifs) { return; }
        char comma = ',';
        ifs >> m_totalCoins >> comma >> m_totalRocks;
        if (m_totalCoins < 0) { m_totalCoins = 0; }
        if (m_totalRocks < 0) { m_totalRocks = 0; }
    }
    void SaveTotals() const
    {
        std::error_code ec;
        std::filesystem::create_directories("Asset/Data/", ec);
        std::ofstream ofs(TotalsPath());
        if (!ofs) { return; }
        ofs << m_totalCoins << ',' << m_totalRocks;
    }

    std::string RecordsPath() const { return "Asset/Data/stage_records.csv"; }

    void LoadRecords()
    {
        std::ifstream ifs(RecordsPath());
        if (!ifs) { return; }
        // one line per stage (in stageId order): "cleared,bestCoins,bestTime"
        for (int i = 0; i < kMaxStages; ++i)
        {
            int c = 0, coins = 0; float t = 0.0f; char comma = ',';
            if (!(ifs >> c >> comma >> coins >> comma >> t)) { break; }
            m_records[i].cleared   = (c != 0);
            m_records[i].bestCoins = (coins < 0) ? 0 : coins;
            m_records[i].bestTime  = (t < 0.0f) ? 0.0f : t;
        }
    }
    void SaveRecords() const
    {
        std::error_code ec;
        std::filesystem::create_directories("Asset/Data/", ec);
        std::ofstream ofs(RecordsPath());
        if (!ofs) { return; }
        for (int i = 0; i < kMaxStages; ++i)
        {
            ofs << (m_records[i].cleared ? 1 : 0) << ','
                << m_records[i].bestCoins << ','
                << m_records[i].bestTime << '\n';
        }
    }

    int m_stageIndex = 1;
    StageResult m_result;
    int m_totalCoins = 0;
    int m_totalRocks = 0;
    std::array<StageRecord, kMaxStages> m_records{};
};
