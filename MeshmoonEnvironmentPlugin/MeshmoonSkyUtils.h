/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief  */

#pragma once

#ifndef MESHMOON_SERVER_BUILD

#include "MeshmoonEnvironmentPluginFwd.h"

#include "Math/float3.h"
#include "OgreModuleFwd.h"

#include <Ogre.h>

/// @cond PRIVATE

static inline Ogre::Vector3 ToOgreVector(const float3 &vector)
{
    return Ogre::Vector3(vector.x, vector.y, vector.z);
}

static inline float3 FromOgreVector(const Ogre::Vector3 &vector)
{
    return float3(vector.x, vector.y, vector.z);
}

class MeshmoonSkyRenderQueueListener : public Ogre::RenderQueueListener
{
public:
    MeshmoonSkyRenderQueueListener(EC_MeshmoonSky* sky, int visibilityMask);
    ~MeshmoonSkyRenderQueueListener();

    void renderQueueStarted(Ogre::uint8 queueGroupId, const Ogre::String &invocation, bool &skipThisInvocation);

private:
    float SetMatrices();
    void SetFog();

    Ogre::SceneManager *sceneManager_;
    Ogre::RenderSystem *renderSystem_;
    int visibilityMask_;

    EC_MeshmoonSky *sky_;
};

class MeshmoonSkyRenderSystemListener : public Ogre::RenderSystem::Listener
{
public:
    MeshmoonSkyRenderSystemListener(EC_MeshmoonSky *component);
    ~MeshmoonSkyRenderSystemListener();

    void eventOccurred(const Ogre::String& eventName, const Ogre::NameValuePairList* parameters = 0);

private:
    EC_MeshmoonSky* parentComponent_;
};
#endif // ~ifndef MESHMOON_SERVER_BUILD
/// @endcond
