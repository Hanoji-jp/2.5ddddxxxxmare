#include "SaveData.h"
#include "SettingsManager.h"
#include "StageManager.h"
#include "../Const/SettingsConst.h"
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <algorithm>

namespace
{
    const char* kSavePath = "save.dat";   // exe と同じ場所（CWD直下）。Assetフォルダは作らない

    template<class T> void WriteBin(std::ostream& o, const T& v)
    {
        o.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }
    template<class T> bool ReadBin(std::istream& i, T& v)
    {
        return static_cast<bool>(i.read(reinterpret_cast<char*>(&v), sizeof(T)));
    }
}

namespace SaveData
{
    bool Exists()
    {
        std::error_code ec;
        return std::filesystem::exists(kSavePath, ec);
    }

    void Save()
    {
        std::ofstream o(kSavePath, std::ios::binary | std::ios::trunc);
        if (!o) { return; }

        auto& s  = SettingsManager::Instance();
        auto& st = StageManager::Instance();

        const char magic[4] = { 'C', 'S', 'A', 'V' };
        o.write(magic, 4);
        uint32_t ver = 1; WriteBin(o, ver);

        // 設定
        WriteBin(o, s.m_master);
        WriteBin(o, s.m_bgm);
        WriteBin(o, s.m_se);
        WriteBin<int32_t>(o, s.m_fullscreen ? 1 : 0);
        WriteBin<int32_t>(o, s.m_resIndex);
        WriteBin<int32_t>(o, s.m_fpsIndex);
        WriteBin<int32_t>(o, s.m_showFps ? 1 : 0);

        // 合計
        WriteBin<int32_t>(o, st.GetTotalCoins());
        WriteBin<int32_t>(o, st.GetTotalRocks());

        // 初回起動フラグ
        WriteBin<int32_t>(o, st.GetLaunched() ? 1 : 0);

        // ステージ記録
        const int32_t n = StageManager::kMaxStages;
        WriteBin(o, n);
        for (int i = 0; i < n; ++i)
        {
            const auto& r = st.GetRecord(i);
            WriteBin<int32_t>(o, r.cleared ? 1 : 0);
            WriteBin<int32_t>(o, r.bestCoins);
            WriteBin<float>  (o, r.bestTime);
        }
    }

    void Load()
    {
        auto& s  = SettingsManager::Instance();
        auto& st = StageManager::Instance();

        std::ifstream in(kSavePath, std::ios::binary);
        if (!in) { return; }   // 無ければ既定値のまま

        char magic[4] = {};
        if (!in.read(magic, 4)) { return; }
        if (magic[0] != 'C' || magic[1] != 'S' || magic[2] != 'A' || magic[3] != 'V') { return; }

        uint32_t ver = 0;
        if (!ReadBin(in, ver)) { return; }

        float fm = 1.0f, fb = 1.0f, fse = 1.0f;
        int32_t fs = 0, ri = 0, fi = 1, sf = 0, tc = 0, tr = 0, lf = 0;
        if (!ReadBin(in, fm) || !ReadBin(in, fb) || !ReadBin(in, fse)) { return; }
        if (!ReadBin(in, fs) || !ReadBin(in, ri) || !ReadBin(in, fi) || !ReadBin(in, sf)) { return; }
        if (!ReadBin(in, tc) || !ReadBin(in, tr)) { return; }
        if (!ReadBin(in, lf)) { return; }

        // 設定（クランプ）
        s.m_master = std::clamp(fm, 0.0f, 1.0f);
        s.m_bgm    = std::clamp(fb, 0.0f, 1.0f);
        s.m_se     = std::clamp(fse, 0.0f, 1.0f);
        s.m_fullscreen = (fs != 0);
        s.m_resIndex = (ri >= 0 && ri < SettingsConst::ResolutionCount) ? ri : 0;
        s.m_fpsIndex = (fi >= 0 && fi < SettingsConst::FpsCount) ? fi : 1;
        s.m_showFps  = (sf != 0);

        // 合計・初回フラグ
        st.SetTotals((tc < 0) ? 0 : tc, (tr < 0) ? 0 : tr);
        st.SetLaunched(lf != 0);

        // ステージ記録
        int32_t n = 0;
        if (ReadBin(in, n))
        {
            for (int i = 0; i < n; ++i)
            {
                int32_t c = 0, bc = 0; float bt = 0.0f;
                if (!ReadBin(in, c) || !ReadBin(in, bc) || !ReadBin(in, bt)) { break; }
                StageManager::StageRecord rec;
                rec.cleared   = (c != 0);
                rec.bestCoins = (bc < 0) ? 0 : bc;
                rec.bestTime  = (bt < 0.0f) ? 0.0f : bt;
                st.SetRecord(i, rec);
            }
        }
    }
}
