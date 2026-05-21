#pragma once

// マップ上に配置する1オブジェクト分のデータ
struct MapObjectData
{
    std::string   modelPath;              // モデルファイルのパス
    Math::Vector3 position = { 0.0f, 0.0f, 0.0f };
    Math::Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // オイラー角（ラジアン）
    Math::Vector3 scale    = { 1.0f, 1.0f, 1.0f };
};
