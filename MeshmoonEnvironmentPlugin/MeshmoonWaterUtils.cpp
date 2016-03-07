/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#include "StableHeaders.h"

#ifdef MESHMOON_TRITON
#include "MeshmoonWaterUtils.h"
#include "EC_MeshmoonSky.h"
#include "EC_MeshmoonWater.h"

#include "EC_EnvironmentLight.h"
#include "EC_SkyX.h"
#include "EC_Camera.h"
#include "EC_Placeable.h"
#include "SceneAPI.h"
#include "OgreWorld.h"
#include "Scene.h"
#include "Profiler.h"
#include "HighPerfClock.h"
#include "Math/MathFunc.h"

#include <Triton.h>

#if OGRE_VERSION_MAJOR >= 1 && OGRE_VERSION_MINOR >= 9
#include <OgreOverlaySystem.h>
#endif

#ifdef DIRECTX_ENABLED
#undef SAFE_DELETE
#undef SAFE_DELETE_ARRAY
#include <OgreD3D9Texture.h>
#elif defined(__APPLE__)
/// @todo Include this on Windows also.
#include <OgreGLTexture.h>
#endif

int  MeshmoonWaterRenderTargetListener::staticCubeMapId_ = 0;
bool MeshmoonWaterRenderTargetListener::renderingReflections = false;
Ogre::Camera *MeshmoonWaterRenderTargetListener::reflectionCamera = 0;
Ogre::RenderTarget *MeshmoonWaterRenderTargetListener::reflectionRenderTarget = 0;
Ogre::MovablePlane *MeshmoonWaterRenderTargetListener::reflectionPlane = 0;

Ogre::uint8 MeshmoonWaterRenderQueueListener::renderQueueId = Ogre::RENDER_QUEUE_SKIES_EARLY + 1;

MeshmoonWaterRenderTargetListener::MeshmoonWaterRenderTargetListener(EC_MeshmoonWater* component) :
    parentComponent_(component),
    sceneManager_(0),
    lastRender_(0),
    cameraPlaneDiff_(0)
{
    reflectionCamera = 0;
    reflectionRenderTarget = 0;
    reflectionPlane = 0;
    renderingReflections = false;
    
    if (!parentComponent_ || !parentComponent_->reflectionsEnabled.Get() || !parentComponent_->visible.Get())
        return;

    OgreWorld *ogreWorld = parentComponent_->ParentScene() ? parentComponent_->ParentScene()->Subsystem<OgreWorld>().get() : 0;
    if (!ogreWorld)
        return;
    sceneManager_ = ogreWorld->OgreSceneManager();
    if (!sceneManager_)
        return;

    tritonResourceId_ = staticCubeMapId_;
    ++staticCubeMapId_;

    try
    {
        reflectionCamera = sceneManager_->createCamera(AppendTritonResourceId("TritonReflectionCamera"));
        reflectionCamera->setFOVy(Ogre::Degree(90));
        reflectionCamera->setAspectRatio(1);
        reflectionCamera->setFixedYawAxis(false);
        reflectionCamera->setFarClipDistance(0);
        reflectionCamera->setNearClipDistance(5);
        
        // Setup reflection
        reflectionSceneNode_ = sceneManager_->createSceneNode(AppendTritonResourceId("TritonReflectionSceneNode"));
        reflectionSceneNode_->setPosition(0, 0, 0);

        UpdateReflectionPlane();

        reflectionTexture_ = Ogre::TextureManager::getSingleton().createManual(AppendTritonResourceId("TritonReflectionMap"),
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D,
            1024, 1024,
            0,
            Ogre::PF_R8G8B8,
            Ogre::TU_RENDERTARGET);
        
#ifndef DIRECTX_ENABLED
        flippedTexture_ = Ogre::TextureManager::getSingleton().createManual(AppendTritonResourceId("TritonFlippedReflectionMap"),
                                                                            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                                                                            Ogre::TEX_TYPE_2D,
                                                                            1024, 1024,
                                                                            0,
                                                                            Ogre::PF_R8G8B8,
                                                                            Ogre::TU_DYNAMIC);
#endif
        
        reflectionRenderTarget = reflectionTexture_->getBuffer(0)->getRenderTarget();

        Ogre::Viewport *vp = reflectionRenderTarget->addViewport(reflectionCamera);
        vp->setAutoUpdated(true);
        vp->setOverlaysEnabled(false);
        vp->setClearEveryFrame(true);
        vp->setSkiesEnabled(true);

        reflectionRenderTarget->addListener(this);
    }
    catch(const Ogre::Exception& ex)
    {
        LogError("EC_MeshmoonWater: " + ex.getDescription());
    }

    // Create overlay for debugging reflections
    bool debugging = false;
    if (debugging)
    {
        Ogre::Overlay *overlay = Ogre::OverlayManager::getSingleton().create(AppendTritonResourceId("TritonReflectionOverlay"));
        
        Ogre::MaterialPtr overlayMat = Ogre::MaterialManager::getSingleton().create(
            AppendTritonResourceId("TritonReflectionOverlayMaterial"), 
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        overlayMat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
        Ogre::TextureUnitState *t = overlayMat->getTechnique(0)->getPass(0)->createTextureUnitState(reflectionTexture_->getName());
        t->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);

        Ogre::OverlayContainer* debugPanel = dynamic_cast<Ogre::OverlayContainer*>(Ogre::OverlayManager::getSingleton()
            .createOverlayElement("Panel", AppendTritonResourceId("TritonReflectionDebugPanel")));
        if (debugPanel)
        {
            debugPanel->_setPosition(0.0, 0.0);
            debugPanel->_setDimensions((Ogre::Real)0.4, (Ogre::Real)0.4);
            debugPanel->setMaterialName(overlayMat->getName());

            overlay->add2D(debugPanel);
            overlay->show();
        }
    }
}

MeshmoonWaterRenderTargetListener::~MeshmoonWaterRenderTargetListener()
{
    try
    {
        if (reflectionRenderTarget)
        {
            reflectionRenderTarget->removeAllViewports();
            reflectionRenderTarget->removeAllListeners();
        }

        /// @note These might have been created if not debugging!
        Ogre::Overlay *reflectionOverlay = Ogre::OverlayManager::getSingleton().getByName(AppendTritonResourceId("TritonReflectionOverlay"));
        if (reflectionOverlay)
            Ogre::OverlayManager::getSingleton().destroy(reflectionOverlay);
        if (Ogre::MaterialManager::getSingleton().getByName(AppendTritonResourceId("TritonReflectionOverlayMaterial")).get())
            Ogre::MaterialManager::getSingleton().unload(AppendTritonResourceId("TritonReflectionOverlayMaterial"));

        if (reflectionTexture_.get())
            Ogre::TextureManager::getSingleton().remove(reflectionTexture_->getHandle());

        if (sceneManager_)
        {
            if (reflectionCamera)
                sceneManager_->destroyCamera(reflectionCamera);
            if (reflectionSceneNode_)
                sceneManager_->destroySceneNode(reflectionSceneNode_);
        }

        if (reflectionPlane)
            delete reflectionPlane;

        if (Ogre::TextureManager::getSingleton().getByName(AppendTritonResourceId("TritonReflectionMap")).get())
            LogWarning("EC_MeshmoonWater: Reflection map not succesfully removed.");
    }
    catch(const Ogre::Exception& ex)
    {
        LogError("[EC_MeshmoonWater]: " + ex.getDescription());
    }
    
    renderingReflections = false;
    reflectionRenderTarget = 0;
    reflectionCamera = 0;
    reflectionPlane = 0;
}

std::string MeshmoonWaterRenderTargetListener::AppendTritonResourceId(const QString &resource)
{
    return QString("%1%2").arg(resource).arg(tritonResourceId_).toStdString();
}

void MeshmoonWaterRenderTargetListener::preRenderTargetUpdate(const Ogre::RenderTargetEvent& evt)
{
    if (!parentComponent_ || !parentComponent_->State().ocean || !parentComponent_->ActiveCamera() || !parentComponent_->RenderQueueListener())
        return;
    if (!sceneManager_ || !reflectionRenderTarget || !reflectionCamera)
        return;

    PROFILE(EC_MeshmoonWater_PreRenderReflections);

    Ogre::Camera *mainCamera = parentComponent_->ActiveCamera()->OgreCamera();
    if (!mainCamera)
        return;

    parentComponent_->RenderQueueListener()->SetMatrices();

    Triton::Matrix4 reflectionMatrix;
    Triton::Matrix3 textureMatrix;
    parentComponent_->State().ocean->ComputeReflectionMatrices(reflectionMatrix, textureMatrix);

    textureMatrix_ = textureMatrix;

    Ogre::Vector3 mainCameraPos = mainCamera->getDerivedPosition();
    reflectionCamera->setPosition(mainCameraPos);
    reflectionCamera->setOrientation(mainCamera->getDerivedOrientation());
    reflectionCamera->setFOVy(mainCamera->getFOVy());
    reflectionCamera->setAspectRatio(mainCamera->getAspectRatio());
    reflectionCamera->disableCustomNearClipPlane();

    renderingReflections = true;
}

void MeshmoonWaterRenderTargetListener::postRenderTargetUpdate(const Ogre::RenderTargetEvent& evt)
{
    if(!sceneManager_ || !parentComponent_ || !parentComponent_->visible.Get())
        return;

    PROFILE(EC_MeshmoonWater_PostRenderReflections);

    MeshmoonWaterRenderQueueListener* listener = parentComponent_->renderQueueListener_;
    if (!listener)
        return;

    renderingReflections = false;
    listener->SetReflectionMap();
}

void MeshmoonWaterRenderTargetListener::UpdateReflectionPlane()
{
    if (!parentComponent_ || !reflectionCamera)
        return;
    
    reflectionSceneNode_->detachAllObjects();
    delete reflectionPlane;

    reflectionPlane = new Ogre::MovablePlane(AppendTritonResourceId("TritonReflectionPlane"));
    reflectionPlane->normal = Ogre::Vector3::UNIT_Y;
    reflectionPlane->d = 0;
    reflectionSceneNode_->attachObject(reflectionPlane);
    reflectionSceneNode_->setPosition(0, parentComponent_->seaLevel.Get(), 0);
    reflectionCamera->enableReflection(reflectionPlane);
}

void MeshmoonWaterRenderTargetListener::SetReflectionClipPlane()
{
    if (renderingReflections && reflectionCamera && reflectionPlane)
        reflectionCamera->enableCustomNearClipPlane(reflectionPlane);
}

void MeshmoonWaterRenderTargetListener::DisableClipPlane()
{
    if (reflectionCamera)
        reflectionCamera->disableCustomNearClipPlane();
}

void MeshmoonWaterRenderTargetListener::UpdateRenderTarget()
{
    if (reflectionRenderTarget)
        reflectionRenderTarget->update();
}

bool MeshmoonWaterRenderTargetListener::IsRenderingReflections() const
{
    return renderingReflections;
}

MeshmoonWaterRenderQueueListener::MeshmoonWaterRenderQueueListener(EC_MeshmoonWater *component, int visibilityMask) :
    Ogre::RenderQueueListener(),
#ifndef DIRECTX_ENABLED
    data_(0),
#endif
    visibilityMask_(visibilityMask),
    parentComponent_(component),
    tundraScene_(component->ParentScene()),
    sceneManager_(0),
    tSunAndMoon_(0.0f)
{
    sceneManager_ = parentComponent_->OgreSceneManager();
    if (!sceneManager_)
        return;

    renderSystem_ = Ogre::Root::getSingletonPtr()->getRenderSystem();
    if (!renderSystem_)
        return;
        
    sceneLights_.direction = ToTritonVector3(float3::unitY);
    sceneLights_.diffuseColor = ToTritonVector3(float3::one);
    sceneLights_.ambientColor = ToTritonVector3(OgreWorld::DefaultSceneAmbientLightColor());
    
#if defined(__APPLE__)
    data_ = new GLubyte[1024 * 1024 * 4];
#endif
}

MeshmoonWaterRenderQueueListener::~MeshmoonWaterRenderQueueListener()
{
    sceneManager_ = 0;
    parentComponent_ = 0;
    
#ifndef DIRECTX_ENABLED
    delete[] data_;
#endif
}

void MeshmoonWaterRenderQueueListener::renderQueueEnded(Ogre::uint8 queueGroupId, const Ogre::String& invocation, bool& repeatThisInvocation)
{
    if (queueGroupId == Ogre::RENDER_QUEUE_SKIES_EARLY)
        MeshmoonWaterRenderTargetListener::SetReflectionClipPlane();
}

void MeshmoonWaterRenderQueueListener::renderQueueStarted(Ogre::uint8 queueGroupId, const Ogre::String& invocation, bool& repeatThisInvocation)
{
    if (!parentComponent_ || !renderSystem_)
        return;

    Ogre::Viewport* viewPort = renderSystem_->_getViewport();
    if (!viewPort)
        return;

    if (!(viewPort->getVisibilityMask() & visibilityMask_))
        return;

    if (queueGroupId == Ogre::RENDER_QUEUE_SKIES_EARLY)
    {
        MeshmoonWaterRenderTargetListener::DisableClipPlane();
        return;
    }

    MeshmoonWaterRenderTargetListener *targetListener = parentComponent_->RenderTargetListener();
    if (targetListener && targetListener->IsRenderingReflections())
    {
        MeshmoonWaterRenderTargetListener::SetReflectionClipPlane();
        return;
    }

    if (queueGroupId != renderQueueId || invocation == "SHADOWS")
        return;
    if (!tundraScene_) 
        return;
    if (!parentComponent_->State().environment || !parentComponent_->State().ocean || !parentComponent_->ActiveCamera())
        return;

    PROFILE(EC_MeshmoonWater_OceanUpdate);   
    SetMatrices();
    SetLighting();

    try
    {
        PROFILE(EC_MeshmoonWater_TritonOceanDraw);
        Ogre::Timer *timer = Ogre::Root::getSingleton().getTimer();
        const double t = (timer != 0 ? timer->getMilliseconds() * 0.001 : 0.0f);
        parentComponent_->State().ocean->Draw(t);
    }
    catch (const Ogre::Exception& ex)
    {
        LogError("[EC_MeshmoonWater]: " + QString::fromStdString(ex.getDescription()));
    }
}

void MeshmoonWaterRenderQueueListener::SetMatrices()
{
    PROFILE(EC_MeshmoonWater_SetMatrices);

    if (!renderSystem_)
        return;

    Ogre::Viewport* viewport = renderSystem_->_getViewport();
    if (!viewport)
        return;

    Ogre::Camera* ogreCamera = viewport->getCamera();
    if (!ogreCamera)
        return;

    Ogre::Matrix4 projectionMatrix = ogreCamera->getProjectionMatrixWithRSDepth();
    Ogre::Matrix4 viewMatrix = ogreCamera->getViewMatrix();

    double tritonProjection[16], tritonView[16];

    int index = 0;
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            tritonProjection[index] = projectionMatrix[j][i];
            tritonView[index] = viewMatrix[j][i];
            ++index;
        }
    }

    Triton::Environment *environment = parentComponent_->State().environment;
    if (environment)
    {
        environment->SetProjectionMatrix(tritonProjection);
        environment->SetCameraMatrix(tritonView);
    }
}

void MeshmoonWaterRenderQueueListener::SetLighting()
{   
    if (!sceneManager_ || !parentComponent_ || !parentComponent_->State().environment)
        return;

    PROFILE(EC_MeshmoonWater_UpdateSceneLighting);

    UpdateSceneLighting(); // Updates sceneLights_
    parentComponent_->State().environment->SetDirectionalLight(sceneLights_.direction, sceneLights_.diffuseColor);
    parentComponent_->State().environment->SetAmbientLight(sceneLights_.ambientColor);
}

void MeshmoonWaterRenderQueueListener::UpdateSceneLighting()
{
    if (!sceneManager_ || !tundraScene_)
        return;

    EntityList entities = tundraScene_->GetEntitiesWithComponent(EC_MeshmoonSky::TypeNameStatic());
    if (entities.size() > 0)
    {
        for(EntityList::iterator it = entities.begin(); it != entities.end(); ++it)
        {
            EntityPtr ent = (*it);
            EC_MeshmoonSky *meshmoonSky = ent.get() ? ent->Component<EC_MeshmoonSky>().get() : 0;
            if (!meshmoonSky)
                continue;
                
            Ogre::Light *light = meshmoonSky->OgreLight();
            if (!light)
                continue;

            // light->getDirection() is LightSource-To-Earth, flip to Earth-To-LightSource.
            sceneLights_.direction = ToTritonVector3(-(light->getDirection()));
            sceneLights_.diffuseColor = ToTritonVector3(light->getDiffuseColour());
            sceneLights_.ambientColor = ToTritonVector3(sceneManager_->getAmbientLight());
            return;
        }
    }
 
    entities = tundraScene_->GetEntitiesWithComponent(EC_SkyX::TypeNameStatic());
    if (entities.size() > 0)
    {
        for(EntityList::iterator it = entities.begin(); it != entities.end(); ++it)
        {
            EntityPtr ent = (*it);
            EC_SkyX *skyx = ent.get() ? ent->Component<EC_SkyX>().get() : 0;
            if (!skyx)
                continue;

            // Check if time is progressing
            float lightSourceTimeMultiplier = skyx->timeMultiplier.Get();
            float lightSourceDirMultiplier = (lightSourceTimeMultiplier > 0.0f ? 1.0f : 1.0f); /// @bug wtf?
            bool lightSourceMoving = (lightSourceTimeMultiplier != 0.0f);
            
            // Calculate frametime
            static tick_t timerFrequency = GetCurrentClockFreq();
            static tick_t lightsTimePrev = GetCurrentClockTime();

            tick_t timeNow = GetCurrentClockTime();
            //float msecsOccurred = (float)(timeNow - lightsTimePrev) * 1000.0 / timerFrequency;
            float msecsOccurred = Clamp(0.1f * lightSourceTimeMultiplier, 0.005f, 0.01f); // Hack away
            lightsTimePrev = timeNow;

            sceneLights_.ambientColor = ToTritonVector3(skyx->getambientLightColor());

            if (skyx->IsSunVisible() && skyx->IsMoonVisible())
            {
                sceneLights_.diffuseColor = ToTritonVector3(skyx->getsunlightDiffuseColor());
                sceneLights_.direction = ToTritonVector3(skyx->SunDirection());
                
                bool sunset = !lightSourceMoving ? false : ((sceneLights_.direction.z < 0.0f && lightSourceTimeMultiplier > 0.0f) || 
                                                            (sceneLights_.direction.z > 0.0f && lightSourceTimeMultiplier < 0.0f));
                sceneLights_.direction = ToTritonVector3((lightSourceMoving 
                    ? (sunset ? skyx->SunDirection().Add(lightSourceDirMultiplier * tSunAndMoon_ * float3(0, -1, 0))
                              : skyx->MoonDirection().Add(lightSourceDirMultiplier * tSunAndMoon_ * float3(0, -1, 0)))
                    : skyx->SunDirection()));
                
                // Trigger the below t step
                if (tSunAndMoon_ == 0.0f)
                    tSunAndMoon_ = 0.0001f;
            }
            else if (skyx->IsSunVisible())
            {
                sceneLights_.diffuseColor = ToTritonVector3(skyx->getsunlightDiffuseColor());
                sceneLights_.direction = ToTritonVector3((lightSourceMoving && tSunAndMoon_ > 0.0f 
                    ? skyx->SunDirection().Add(lightSourceDirMultiplier * tSunAndMoon_ * float3(0, -1, 0))
                    : skyx->SunDirection()));
            }
            else if (skyx->IsMoonVisible())
            {
                sceneLights_.diffuseColor = ToTritonVector3(skyx->getmoonlightDiffuseColor());
                sceneLights_.direction = ToTritonVector3((lightSourceMoving && tSunAndMoon_ > 0.0f 
                    ? skyx->MoonDirection().Add(lightSourceDirMultiplier * tSunAndMoon_ * float3(0, -1, 0))
                    : skyx->MoonDirection()));
            }
            else
                break;

            sceneLights_.direction.Normalize();

            if (lightSourceMoving && tSunAndMoon_ > 0.0f)
            {                           
                tSunAndMoon_ += (skyx->IsSunVisible() && skyx->IsMoonVisible() ? msecsOccurred : -msecsOccurred);
                if (tSunAndMoon_ < 0.0f)         tSunAndMoon_ = 0.0f;
                else if (tSunAndMoon_ > 1.0f)    tSunAndMoon_ = 1.0f;
            }
            return;
        }
    }

    // No SkyX, check for Ogre environment lightSource
    entities = tundraScene_->GetEntitiesWithComponent(EC_EnvironmentLight::TypeNameStatic());
    if (entities.size() > 0)
    {
        for(EntityList::iterator it = entities.begin(); it != entities.end(); ++it)
        {
            EntityPtr ent = (*it);
            EC_EnvironmentLight *sun = ent.get() ? ent->Component<EC_EnvironmentLight>().get() : 0;
            if (!sun)
                continue;

            sceneLights_.direction = ToTritonVector3(sun->sunDirection.Get().Neg()); // Flip to Earth-to-Sun
            sceneLights_.diffuseColor = ToTritonVector3(sun->getsunColor());
            sceneLights_.ambientColor = ToTritonVector3(sun->getambientColor());
            return;
        }
    }

    // No light source components, return white from above.
    sceneLights_.direction = ToTritonVector3(float3::unitY);
    sceneLights_.diffuseColor = ToTritonVector3(float3::one);
    sceneLights_.ambientColor = ToTritonVector3(OgreWorld::DefaultSceneAmbientLightColor());
}

void MeshmoonWaterRenderQueueListener::SetReflectionMap()
{
    if (!parentComponent_ || !parentComponent_->reflectionsEnabled.Get() || !parentComponent_->visible.Get())
        return;
    
    MeshmoonWaterRenderTargetListener *renderTargetListener = parentComponent_->renderTargetListener_;
    if (!renderTargetListener)
        return;

    PROFILE(EC_MeshmoonWater_SetReflectionMap);
    
    if (parentComponent_->State().ocean)
        parentComponent_->State().ocean->SetPlanarReflectionBlend(Min(Max(parentComponent_->reflectionIntensity.Get(), 0.0f), 1.0f));

#ifdef DIRECTX_ENABLED
    Ogre::D3D9Texture *d3d9Tex = dynamic_cast<Ogre::D3D9Texture*>(renderTargetListener->reflectionTexture_.get());
    if (d3d9Tex)
    {
        IDirect3DBaseTexture9 *d3d9BaseTex = d3d9Tex->getTexture();
        if (d3d9BaseTex)
            parentComponent_->State().environment->SetPlanarReflectionMap(d3d9BaseTex, parentComponent_->renderTargetListener_->textureMatrix_);
    }
#elif defined(__APPLE__)
    Ogre::GLTexture *glTexture = dynamic_cast<Ogre::GLTexture*>(renderTargetListener->reflectionTexture_.get());
    Ogre::GLTexture *flippedTexture = dynamic_cast<Ogre::GLTexture*>(renderTargetListener->flippedTexture_.get());
    if (glTexture && flippedTexture)
    {
        GLuint glTexId = glTexture->getGLID();
        GLuint glFlippedId = flippedTexture->getGLID();
        if (glTexId)
        {
            // Get pixel data from reflection texture
            uint dataSize = 1024 * 1024 * 4;
            glBindTexture(GL_TEXTURE_2D, glTexId);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_);
            glBindTexture(GL_TEXTURE_2D, glFlippedId);
            
            for(int i = 0; i < 1024; ++i)
            {
                int index = i * 1024 * 4;
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1023 - i, 1024, 1, GL_RGBA, GL_UNSIGNED_BYTE, &data_[index]);
            }
            
            glBindTexture(GL_TEXTURE_2D, 0);
            
            parentComponent_->State().environment->SetPlanarReflectionMap(glFlippedId, parentComponent_->renderTargetListener_->textureMatrix_);
        }
    }
#endif
}

MeshmoonWaterRenderSystemListener::MeshmoonWaterRenderSystemListener(EC_MeshmoonWater *component) :
    parentComponent_(component)
{
}

MeshmoonWaterRenderSystemListener::~MeshmoonWaterRenderSystemListener()
{
    parentComponent_ = 0;
}

void MeshmoonWaterRenderSystemListener::eventOccurred(const Ogre::String& eventName, const Ogre::NameValuePairList* parameters)
{
    if (!parentComponent_)
        return;

    if (eventName == "DeviceLost")
        parentComponent_->DeviceLost();
    else if (eventName == "DeviceRestored") 
        parentComponent_->DeviceRestored();
}
#endif // ~ifdef MESHMOON_TRITON
