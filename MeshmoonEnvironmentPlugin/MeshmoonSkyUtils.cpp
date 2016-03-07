/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#include "StableHeaders.h"

#ifdef MESHMOON_SILVERLINING
#include "EC_MeshmoonSky.h"
#include "MeshmoonSkyUtils.h"
#include "MeshmoonWaterUtils.h"

#include "EC_Camera.h"
#include "Profiler.h"
#include "Scene.h"
#include "OgreWorld.h"
#include "Math/Quat.h"

// Suppress warnings leaking from SilverLining.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) 
#endif
#include <SilverLining.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static const Ogre::uint8 EC_MeshmoonSkyQueueId = Ogre::RENDER_QUEUE_SKIES_EARLY;

MeshmoonSkyRenderQueueListener::MeshmoonSkyRenderQueueListener(EC_MeshmoonSky* sky, int visibilityMask) :
    sky_(sky),
    sceneManager_(0),
    renderSystem_(0),
    visibilityMask_(visibilityMask)
{
    sceneManager_ = sky_->OgreSceneManager();
    if (!sceneManager_)
        return;

    renderSystem_ = Ogre::Root::getSingleton().getRenderSystem();
}

MeshmoonSkyRenderQueueListener::~MeshmoonSkyRenderQueueListener()
{
}

void MeshmoonSkyRenderQueueListener::renderQueueStarted(Ogre::uint8 queueGroupId, const Ogre::String &invocation, bool &skipThisInvocation)
{
    if (!sky_ || !sky_->ActiveCamera() || !sky_->Atmosphere())
        return;
    if (!sceneManager_ || !renderSystem_)
        return;
    if (queueGroupId != EC_MeshmoonSkyQueueId || invocation == "SHADOWS")
        return;

    Ogre::Viewport* viewPort = renderSystem_->_getViewport();
    if (!viewPort)
        return;

    if (!(viewPort->getVisibilityMask() & visibilityMask_))
        return;

    PROFILE(EC_MeshmoonSky_SkyUpdate);

    float linesWithSun = SetMatrices();

    /* Handle this properly! SL draws the fog, we would need to put a fog to the
       horizon and ask the color sky_->atmosphere_->GetHorizonColor(). 
       This should be done in EC_MeshmoonSky::OnUpdate instead!
    PROFILE(SilverLining_Set_Fog);
    SetFog();
    ELIFORP(SilverLining_Set_Fog); */

    PROFILE(EC_MeshmoonSky_DrawSky);
    sky_->Atmosphere()->DrawSky(true, false, 0, true, true, true);
    ELIFORP(EC_MeshmoonSky_DrawSky);

    PROFILE(EC_MeshmoonSky_OgreGpuUnbind);
    renderSystem_->unbindGpuProgram(Ogre::GPT_FRAGMENT_PROGRAM);
    renderSystem_->unbindGpuProgram(Ogre::GPT_VERTEX_PROGRAM);
    for (int i = 0; i < 8; ++i)
    {
        renderSystem_->_setTexture(i, false, "");
        renderSystem_->_setTextureCoordCalculation(i, Ogre::TEXCALC_NONE);
    }
    ELIFORP(EC_MeshmoonSky_OgreGpuUnbind);

    PROFILE(EC_MeshmoonSky_DrawObjects)
    sky_->Atmosphere()->DrawObjects(true, false, false, (linesWithSun >= 0.0 ?  sky_->GodRayIntensity() : 0.0f));
    ELIFORP(EC_MeshmoonSky_DrawObjects);
}

float MeshmoonSkyRenderQueueListener::SetMatrices()
{
    if (!sky_ || !sky_->Atmosphere())
        return 0.0f;

    PROFILE(EC_MeshmoonSky_SetMatrices);

    float3 lightPos, cameraDir;
    sky_->Atmosphere()->GetSunOrMoonPosition(&lightPos.x, &lightPos.y, &lightPos.z);

    Ogre::Matrix4 proj, view;
    if (!MeshmoonWaterRenderTargetListener::renderingReflections)
    {
        Ogre::RenderSystem* renderSystem = Ogre::Root::getSingletonPtr()->getRenderSystem();
        if (!renderSystem)
            return 0.0f;

        Ogre::Viewport* viewport = renderSystem->_getViewport();
        if (!viewport)
            return 0.0f;

        Ogre::Camera* ogreCamera = viewport->getCamera();
        if (!ogreCamera)
            return 0.0f;

        cameraDir = FromOgreVector(ogreCamera->getDerivedDirection());

        Ogre::Real farClip = ogreCamera->getFarClipDistance();
        ogreCamera->setFarClipDistance(100000);

        proj = ogreCamera->getProjectionMatrixWithRSDepth();
        view = ogreCamera->getViewMatrix();

        ogreCamera->setFarClipDistance(farClip);
    }
    else
    {
        Ogre::Camera* reflectionCamera = MeshmoonWaterRenderTargetListener::reflectionCamera;
        if (!reflectionCamera)
            return 0.0f;

        cameraDir = FromOgreVector(reflectionCamera->getDerivedDirection());

        MeshmoonWaterRenderTargetListener::DisableClipPlane();

        proj = reflectionCamera->getProjectionMatrixWithRSDepth();
        view = reflectionCamera->getViewMatrix();

        MeshmoonWaterRenderTargetListener::SetReflectionClipPlane();
    }

    double slProj[16], slView[16];
    for(int i=0, index=0; i<4; ++i)
    {
        for(int j=0; j<4; ++j, ++index)
        {
            slProj[index] = proj[j][i];
            slView[index] = view[j][i];
        }
    }

    sky_->Atmosphere()->SetProjectionMatrix(slProj);
    sky_->Atmosphere()->SetCameraMatrix(slView);
    
    return lightPos.Dot(cameraDir);
}

void MeshmoonSkyRenderQueueListener::SetFog()
{
    float density, r, g, b;
    if (sky_->Atmosphere()->GetFogEnabled())
    {
        float ar, ag, ab;
        sky_->Atmosphere()->GetSunOrMoonColor(&ar, &ag, &ab);
        sky_->Atmosphere()->GetFogSettings(&density, &r, &g, &b);
    }
    else
    {
        sky_->Atmosphere()->GetHorizonColor(0, &r, &g, &b);
        //density = (float)(1.0 / sky_->Atmosphere()->GetConditions()->GetVisibility());

        //static const double H = 8435.0;
        //double isothermalEffect = exp(-(sky_->Atmosphere()->GetConditions()->GetLocation().GetAltitude() / H));
        //if(isothermalEffect <= 0) isothermalEffect = 1E-9;
        //if(isothermalEffect > 1.0) isothermalEffect = 1.0;
        //density *= isothermalEffect;
    }
    //sceneManager_->setFog(Ogre::FOG_EXP, Ogre::ColourValue(r, g, b), density);
}

// MeshmoonSkyRenderSystemListener

MeshmoonSkyRenderSystemListener::MeshmoonSkyRenderSystemListener(EC_MeshmoonSky *component) :
    parentComponent_(component)
{
}

MeshmoonSkyRenderSystemListener::~MeshmoonSkyRenderSystemListener()
{
    parentComponent_ = 0;
}

void MeshmoonSkyRenderSystemListener::eventOccurred(const Ogre::String& eventName, const Ogre::NameValuePairList* parameters)
{
    if (!parentComponent_)
        return;

    if (eventName == "DeviceLost")
        parentComponent_->DeviceLost();
    else if (eventName == "DeviceRestored")
        parentComponent_->DeviceRestored();
}
#endif // ~ifdef MESHMOON_SILVERLINING
