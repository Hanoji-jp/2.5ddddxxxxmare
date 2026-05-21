#pragma once
#include "../../Editor/MapObjectData.h"

// MapEditor の配置データ1件を表示するオブジェクト
class MapObject : public KdGameObject
{
public:
    MapObject()          {}
    virtual ~MapObject() {}

    void Init(const MapObjectData& _data);
    void DrawLit() override;

    bool IsVisible() const override { return true; }

    void Expire() { m_isExpired = true; }

private:
    KdModelWork m_modelWork;
};
