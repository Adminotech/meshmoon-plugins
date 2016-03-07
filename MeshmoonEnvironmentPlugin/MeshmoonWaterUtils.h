/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#pragma once

#ifdef MESHMOON_TRITON

#include "MeshmoonEnvironmentPluginFwd.h"

#include "SceneFwd.h"
#include "OgreModuleFwd.h"
#include "Math/float3.h"
#include "Math/float4.h"
#include "Color.h"

#include <QString>
#include <Ogre.h>
#include <Triton.h>

/// @cond PRIVATE

static inline Triton::Vector3 ToTritonVector3(const float3 &vector)
{
    return Triton::Vector3(vector.x, vector.y, vector.z);
}

static inline Triton::Vector3 ToTritonVector3(const float4 &vector)
{
    return Triton::Vector3(vector.x, vector.y, vector.z);
}

static inline Triton::Vector3 ToTritonVector3(const Ogre::Vector3 &vector)
{
    return Triton::Vector3(vector.x, vector.y, vector.z);
}

static inline Triton::Vector3 ToTritonVector3(const Ogre::ColourValue &color)
{
    return Triton::Vector3(color.r, color.g, color.b);
}

static inline Triton::Vector3 ToTritonVector3(const Color &color)
{
    return Triton::Vector3(color.r, color.g, color.b);
}

static inline float3 ToMathVector(const Triton::Vector3 &vector)
{
    return float3(vector.x, vector.y, vector.z);
}

static inline Ogre::Vector3 ToOgreVector(const Triton::Vector3 &vector)
{
    return Ogre::Vector3(vector.x, vector.y, vector.z);
}

struct LightSource
{
    Triton::Vector3 direction; ///< Earth-to-Sun dir vector
    Triton::Vector3 diffuseColor;
    Triton::Vector3 ambientColor;
};

class MeshmoonWaterRenderTargetListener : public Ogre::RenderTargetListener
{

public:
    MeshmoonWaterRenderTargetListener(EC_MeshmoonWater* component);
    ~MeshmoonWaterRenderTargetListener();

    void preRenderTargetUpdate(const Ogre::RenderTargetEvent& evt);
    void postRenderTargetUpdate(const Ogre::RenderTargetEvent& evt);
    
    void UpdateReflectionPlane();
    void UpdateRenderTarget();
    bool IsRenderingReflections() const;

    static void SetReflectionClipPlane();
    static void DisableClipPlane();

    std::string AppendTritonResourceId(const QString &resource);
    
    Ogre::TexturePtr reflectionTexture_;
    Triton::Matrix3 textureMatrix_;
    
#ifndef DIRECTX_ENABLED
    Ogre::TexturePtr flippedTexture_;
#endif

    static bool renderingReflections;
    static Ogre::Camera *reflectionCamera;
    static Ogre::RenderTarget *reflectionRenderTarget;
    static Ogre::MovablePlane *reflectionPlane;

protected:
    EC_MeshmoonWater* parentComponent_;

    Ogre::SceneManager* sceneManager_;
    Ogre::SceneNode *reflectionSceneNode_;

    float lastRender_;
    float cameraPlaneDiff_;
    int tritonResourceId_;
    
    static int staticCubeMapId_;
};

class MeshmoonWaterRenderQueueListener : public Ogre::RenderQueueListener
{
public:
    MeshmoonWaterRenderQueueListener(EC_MeshmoonWater *component, int visiblityMask);
    ~MeshmoonWaterRenderQueueListener();

    void renderQueueStarted(Ogre::uint8 queueGroupId, const Ogre::String& invocation, bool& repeatThisInvocation);
    void renderQueueEnded(Ogre::uint8 queueGroupId, const Ogre::String& invocation, bool& repeatThisInvocation);

    static Ogre::uint8 renderQueueId;

    void SetMatrices();
    void SetLighting();
    void SetReflectionMap();
    void UpdateSceneLighting();

protected:
#ifndef DIRECTX_ENABLED
    unsigned char *data_;
#endif
    
    Scene* tundraScene_;
    Ogre::SceneManager* sceneManager_;
    Ogre::RenderSystem* renderSystem_;
    int visibilityMask_;
    EC_MeshmoonWater* parentComponent_;

    LightSource sceneLights_;
    float tSunAndMoon_;

    friend class MeshmoonWaterRenderTargetListener;
};

class MeshmoonWaterRenderSystemListener : public Ogre::RenderSystem::Listener
{
public:
    MeshmoonWaterRenderSystemListener(EC_MeshmoonWater *component);
    ~MeshmoonWaterRenderSystemListener();

    void eventOccurred(const Ogre::String& eventName, const Ogre::NameValuePairList* parameters = 0);

private:
    EC_MeshmoonWater* parentComponent_;
};

#endif // ~ifndef MESHMOON_SERVER_BUILD
/// @endcond
