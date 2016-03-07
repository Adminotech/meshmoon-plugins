/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   StableHeaders.h
    @brief  Stable headers for AdminotechPlugin. */

#pragma once

// If PCH is disabled, leave the contents of this whole file empty to avoid any compilation unit getting any unnecessary headers.
#ifdef PCH_ENABLED
#include "CoreTypes.h"
#include "CoreDefines.h"
#include "Framework.h"
#include "LoggingFunctions.h"
#include "Win.h"

#include <QtCore>
#include <QtGui>
#endif
