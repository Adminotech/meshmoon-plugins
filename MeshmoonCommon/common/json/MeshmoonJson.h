
/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"
#include "common/MeshmoonCommon.h"

#include <QString>
#include <QByteArray>
#include <QVariant>
#include <QVariantMap>

#include "Math/float2.h"
#include "Math/float3.h"
#include "Math/float4.h"
#include "Color.h"

/// @cond PRIVATE

namespace Meshmoon
{
    class JSON
    {

    public:
        MESHMOON_COMMON_API static void PrettyPrint(const QVariant &source);
        MESHMOON_COMMON_API static void PrettyPrint(const QByteArray &source);
        
        MESHMOON_COMMON_API static bool DeserializeFloat2(const QVariant &source, float2 &dest);
        MESHMOON_COMMON_API static bool DeserializeFloat3(const QVariant &source, float3 &dest);
        MESHMOON_COMMON_API static bool DeserializeFloat4(const QVariant &source, float4 &dest);
        MESHMOON_COMMON_API static bool DeserializeColor(const QVariant &source, Color &dest);

        MESHMOON_COMMON_API static QVariant DeserializeFloatFromMap(const QVariantMap &source, QString key, float &dest);
        MESHMOON_COMMON_API static bool DeserializeColorChannel(const QVariantMap &source, QString key, float &dest);
        
        MESHMOON_COMMON_API static Meshmoon::SpaceList DeserializeSpaces(const QVariant &source);
        MESHMOON_COMMON_API static Meshmoon::LayerList DeserializeLayers(const QVariant &source);
    };
}

/// @endcond
