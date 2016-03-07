/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   StableHeaders.h
    @brief  Stable headers for MeshmoonEnvironmentPlugin. */

#pragma once

// If PCH is disabled, leave the contents of this whole file empty to avoid any compilation unit getting any unnecessary headers.
#ifdef PCH_ENABLED
#include "Win.h"
#include "CoreTypes.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"

#include <QtCore>

#include <Ogre.h>

#ifdef MESHMOON_TRITON
#include <Triton.h>
#include <TritonCommon.h>
#endif

#endif
