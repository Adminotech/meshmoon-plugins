/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#pragma once

#include "CoreTypes.h"

/// @cond PRIVATE

// Components
class EC_MeshmoonSky;
class EC_MeshmoonWater;
class EC_MeshmoonCloudLayer;

// Plugin
class MeshmoonEnvironmentPlugin;

// Water
class MeshmoonWaterRenderQueueListener;
class MeshmoonWaterRenderTargetListener;
class MeshmoonWaterRenderSystemListener;

namespace Triton
{
    class Ocean;
    class Environment;
    class ResourceLoader;
}

// Sky
class MeshmoonSkyRenderQueueListener;

namespace SilverLining
{
    class Atmosphere;
    class CloudLayer;
}

// Typedefs
typedef shared_ptr<EC_MeshmoonSky> MeshmoonSkyPtr;
typedef weak_ptr<EC_MeshmoonSky> MeshmoonSkyWeakPtr;
typedef shared_ptr<EC_MeshmoonCloudLayer> MeshmoonCloudLayerPtr;
typedef weak_ptr<EC_MeshmoonCloudLayer> MeshmoonCloudLayerWeakPtr;

/// @endcond