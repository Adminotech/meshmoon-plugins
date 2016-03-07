/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#if defined (_WINDOWS)
#if defined(MeshmoonComponents_EXPORTS) 
#define MESHMOON_COMPONENTS_API __declspec(dllexport)
#else
#define MESHMOON_COMPONENTS_API __declspec(dllimport) 
#endif
#else
#define MESHMOON_COMPONENTS_API
#endif
