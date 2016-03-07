/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"
#include "MeshmoonCommonFwd.h"

#include "IModule.h"

/// Exposes common functionality for both the Meshmoon client and the server.
class MESHMOON_COMMON_API MeshmoonCommonPlugin : public IModule
{
    Q_OBJECT

public:
    MeshmoonCommonPlugin();
    virtual ~MeshmoonCommonPlugin();

private:
    /// IModule override.
    void Initialize();

    MeshmoonSpaceLoader *spaceLoader_;
};
