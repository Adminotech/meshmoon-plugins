/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketNetworking.h"
#include "RocketPlugin.h"
#include "common/MeshmoonCommon.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "TundraLogicModule.h"
#include "Client.h"

#include <kNet/MessageConnection.h>

#include "MemoryLeakCheck.h"

RocketNetworking::RocketNetworking(RocketPlugin *plugin) :
    plugin_(plugin),
    LC("[RocketNetworking]: ")
{
}

RocketNetworking::~RocketNetworking()
{
}

void RocketNetworking::SendAssetReloadMsg(const QString &assetReference)
{
}

void RocketNetworking::SendAssetReloadMsg(const QStringList &assetReferences)
{
}

void RocketNetworking::SendLayerUpdateMsg(u32 layerId, bool visible)
{
}
