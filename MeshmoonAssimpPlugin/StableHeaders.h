/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   StableHeaders.h
    @brief  Stable headers for MeshmoonComponents. */

#pragma once

// If PCH is disabled, leave the contents of this whole file empty to avoid any compilation unit getting any unnecessary headers.
#ifdef PCH_ENABLED
#include "CoreTypes.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"
#include "Win.h"

#include <QtCore>

#include <assimp/vector3.h>
#include <assimp/matrix4x4.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#endif
