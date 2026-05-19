#include "../../Pch.h"
#include "ModelManager.h"

std::shared_ptr<KdModelData> ModelManager::GetModel(const std::string& _filePath)
{
    // キャッシュに存在すればそれを返す
    auto it = m_modelCache.find(_filePath);
    if (it != m_modelCache.end())
    {
        return it->second;
    }

    // 新規ロード
    auto spModel = std::make_shared<KdModelData>();
    if (!spModel->Load(_filePath))
    {
        return nullptr;
    }

    m_modelCache[_filePath] = spModel;
    return spModel;
}

void ModelManager::AllRelease()
{
    m_modelCache.clear();
}
