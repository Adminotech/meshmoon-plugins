/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"

#include "MeshmoonCommonFwd.h"
#include "FrameworkFwd.h"
#include "SceneFwd.h"

/// @cond PRIVATE

class MESHMOON_COMMON_API MeshmoonSceneValidator
{
public:
    /// Rocket Environment Entity name.
    static QString RocketEnvironmentEntityName;

    /// Validates Meshmoon scene.
    static void Validate(Scene *scene);
};

/// @endcond
