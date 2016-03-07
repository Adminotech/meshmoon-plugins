/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonHttpPlugin.h"
#include "MeshmoonHttpClient.h"
#include "MeshmoonHttpScriptTypeDefines.h"

#include "Framework.h"

MeshmoonHttpPlugin::MeshmoonHttpPlugin() :
    IModule("MeshmoonHttpPlugin")
{
}

MeshmoonHttpPlugin::~MeshmoonHttpPlugin()
{
}

void MeshmoonHttpPlugin::Load()
{
    GetFramework()->RegisterDynamicObject("http", this);

    client_ = MAKE_SHARED(MeshmoonHttpClient, GetFramework());
}

void MeshmoonHttpPlugin::Unload()
{
    client_.reset();
}

MeshmoonHttpClientPtr MeshmoonHttpPlugin::Client() const
{
    return client_;
}

void MeshmoonHttpPlugin::OnScriptEngineCreated(QScriptEngine *engine)
{
    RegisterMeshmoonHttpPluginMetaTypes(engine);
}

extern "C"
{
    DLLEXPORT void TundraPluginMain(Framework *fw)
    {
        Framework::SetInstance(fw);
        fw->RegisterModule(new MeshmoonHttpPlugin());
    }
}
