#include "AssetPack.h"

#ifdef DISTRIBUTE_BUILD

#include <Windows.h>
#include <filesystem>

namespace AssetPack
{
    // 全アセットは VFS(AssetVault) からメモリ読みするため、ディスクへは一切展開しない。
    // ここでは CWD を exe フォルダへ固定するだけ（セーブ等の相対書き込み先を揃える）。
    void Prepare()
    {
        try
        {
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, buf, MAX_PATH);
            const std::filesystem::path exeDir = std::filesystem::path(buf).parent_path();
            SetCurrentDirectoryW(exeDir.c_str());
        }
        catch (...) {}
    }
}

#else // !DISTRIBUTE_BUILD

namespace AssetPack
{
    void Prepare() {}   // 開発ビルドでは何もしない
}

#endif
