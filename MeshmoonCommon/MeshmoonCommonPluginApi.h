/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#if defined (_WINDOWS)
#if defined(MeshmoonCommon_EXPORTS) 
#define MESHMOON_COMMON_API __declspec(dllexport)
#else
#define MESHMOON_COMMON_API __declspec(dllimport) 
#endif
#else
#define MESHMOON_COMMON_API
#endif
