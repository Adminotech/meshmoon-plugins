/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonCommonPlugin.h"
#include "Framework.h"

#include "common/loaders/MeshmoonSpaceLoader.h"

MeshmoonCommonPlugin::MeshmoonCommonPlugin() :
    IModule("MeshmoonCommonPlugin"),
    spaceLoader_(0)
{
}

MeshmoonCommonPlugin::~MeshmoonCommonPlugin()
{
    SAFE_DELETE(spaceLoader_);
}

void MeshmoonCommonPlugin::Initialize()
{
    if (Fw()->HasCommandLineParameter("--meshmoonLoadSpaces"))
    {
        QStringList source = Fw()->CommandLineParameters("--meshmoonLoadSpaces");
        if (!source.isEmpty() && !source.first().trimmed().isEmpty())
            spaceLoader_ = new MeshmoonSpaceLoader(Fw(), source.first().trimmed());
    }
}

extern "C"
{
    DLLEXPORT void TundraPluginMain(Framework *fw)
    {
        Framework::SetInstance(fw);
        fw->RegisterModule(new MeshmoonCommonPlugin());
    }
}
