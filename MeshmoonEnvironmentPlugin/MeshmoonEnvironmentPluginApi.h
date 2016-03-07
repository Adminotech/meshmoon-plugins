/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#if defined (_WINDOWS)
#if defined(MeshmoonEnvironmentPlugin_EXPORTS) 
#define MESHMOON_ENVIRONMENT_API __declspec(dllexport)
#else
#define MESHMOON_ENVIRONMENT_API __declspec(dllimport) 
#endif
#else
#define MESHMOON_ENVIRONMENT_API
#endif
