
#include "StableHeaders.h"

#include "common/MeshmoonCommon.h"

namespace Meshmoon
{
    SceneLayer::SceneLayer() :
        centerPosition(float3::zero),
        defaultVisible(false),
        downloaded(false),
        loaded(false),
        visible(false),
        id(0)
    {
    }

    SceneLayer::SceneLayer(uint _id, bool _visible, const QString &_name, const QString &_icon) :
        centerPosition(float3::zero),
        defaultVisible(false),
        downloaded(false),
        loaded(false),
        visible(_visible),
        id(_id),
        name(_name),
        iconUrl(_icon)
    {
    }
        
    bool SceneLayer::IsValid() const
    {
        return (id != 0);
    }

    void SceneLayer::Reset()
    {
        *this = SceneLayer();
    }

    QString SceneLayer::ToString() const
    {
        return QString("name = %1 id = %2").arg(name).arg(id);
    }

    QString SceneLayer::toString() const
    {
        return ToString();
    }
}
