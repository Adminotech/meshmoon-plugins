/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonEnvironmentPlugin.cpp
    @brief  Registration of the Meshmoon environment ECs. */

#include "StableHeaders.h"
#include "EC_MeshmoonWater.h"
#include "EC_MeshmoonSky.h"
#include "EC_MeshmoonCloudLayer.h"

#include "Framework.h"
#include "SceneAPI.h"
#include "IComponentFactory.h"

extern "C" DLLEXPORT void TundraPluginMain(Framework *fw)
{
    Framework::SetInstance(fw); // Inside this DLL, remember the pointer to the global framework object.
    fw->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonWater>));
    fw->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonSky>));
    fw->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonCloudLayer>));
}
