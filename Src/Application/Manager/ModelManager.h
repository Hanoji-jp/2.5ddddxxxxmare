#pragma once

// モデルデータをキャッシュして管理するマネージャー
// 同じファイルパスのモデルは1度しかロードしない
class ModelManager
{
public:
    static ModelManager& Instance()
    {
        static ModelManager instance;
        return instance;
    }

    // モデルデータを取得（未ロードなら読み込んでキャッシュする）
    std::shared_ptr<KdModelData> GetModel(const std::string& _filePath);

    // キャッシュを全て解放する
    void AllRelease();

private:
    ModelManager()  {}
    ~ModelManager() {}

    // ファイルパスをキーにモデルデータをキャッシュ
    std::unordered_map<std::string, std::shared_ptr<KdModelData>> m_modelCache;
};
