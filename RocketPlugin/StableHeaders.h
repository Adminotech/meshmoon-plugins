/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   StableHeaders.h
    @brief  Stable headers for RocketPlugin. */

#pragma once

// If PCH is disabled, leave the contents of this whole file empty to avoid any compilation unit getting any unnecessary headers.
#ifdef PCH_ENABLED
#include "CoreTypes.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"
#include "Win.h"

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QtWebKit>

#include <kNet.h>

#include <Ogre.h>
#endif
