/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketOgreSceneRenderer.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "Profiler.h"
#include "FrameAPI.h"
#include "ConfigAPI.h"
#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "Math/MathFunc.h"

#include "OgreRenderingModule.h"
#include "OgreMeshAsset.h"
#include "OgreMaterialAsset.h"
#include "TextureAsset.h"
#include "OgreWorld.h"
#include "Renderer.h"

#include <OgreRoot.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>
#include <OgreEntity.h>
#include <OgreCamera.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreOverlayManager.h>
#include <OgrePanelOverlayElement.h>

#include <QUuid>

#include <utility>

#ifdef Q_WS_X11
#include <QX11Info>
#endif

#ifdef Q_WS_MAC
#include "SystemInfo.h"
#endif

#include "MemoryLeakCheck.h"

const QString RocketOgreSceneRenderer::LC = "[RocketOgreSceneRenderer]: ";

RocketOgreSceneRenderer::RocketOgreSceneRenderer(RocketPlugin *plugin, QWidget *renderTarget, QObject *parent) :
    QObject(parent),
    plugin_(plugin),
    renderTarget_(renderTarget),
    renderMode_(RM_RenderWidget),
    format_(Ogre::PF_R8G8B8)
{
    if (!Initialize())
        return;
    if (renderTarget_.isNull())
    {
        LogError(LC + "Render target QWidget* is null!");
        return;
    }

    CreateRenderWindow(renderTarget_, Id("RenderWindow"), renderTarget_->width(), renderTarget_->height(), 0, 0, false);

    // Setup render target widget
    renderTarget_->setUpdatesEnabled(false);
    renderTarget_->setAttribute(Qt::WA_DontShowOnScreen, false);
    renderTarget_->setAttribute(Qt::WA_PaintOnScreen, false);
    renderTarget_->setAttribute(Qt::WA_NoSystemBackground, true);
    renderTarget_->setAttribute(Qt::WA_OpaquePaintEvent, true);
    renderTarget_->installEventFilter(this);
    
    // Timer to do delayed resize
    resizeTimer_.setSingleShot(true);
    connect(&resizeTimer_, SIGNAL(timeout()), SLOT(OnRenderTargetResized()));

    CreateViewport(true, false, QColor(20,20,20));
}

RocketOgreSceneRenderer::RocketOgreSceneRenderer(RocketPlugin *plugin, const QSize &size, Ogre::PixelFormat format, QObject *parent) :
    QObject(parent),
    plugin_(plugin),
    renderMode_(RM_RenderTexture),
    format_(format)
{
    if (Initialize())
        Resize(size);
}

RocketOgreSceneRenderer::~RocketOgreSceneRenderer()
{
    DestroyOverlay();

    // Destroys Ogre::RenderTexture for RM_RenderTexture
    // and the  Ogre::RenderWindow  for RM_RenderWidget.
    Ogre::RenderTarget *renderTarget = RenderTarget();
    if (renderTarget)
    {
        renderTarget->removeAllViewports();
        renderTarget->removeAllListeners();
        /* @bug Qt will crash on next QApplication::processEvents call if 'destroyRenderTarget' is used in case of Mac
                We are deliberately leaking memory here. TODO: Check if this is fixed in Ogre 2.0 or so. */
#ifdef __APPLE__
        Ogre::Root::getSingleton().detachRenderTarget(renderTarget);
#else
        Ogre::Root::getSingleton().destroyRenderTarget(renderTarget);
#endif

    }
    renderTarget = 0;
    renderWindow_ = 0;
    viewPort_ = 0;

    // Delete render texture (RM_RenderTexture only)
    Ogre::Texture *renderTexture = RenderTexture();
    if (renderTexture)
    {
        try { Ogre::TextureManager::getSingleton().remove(renderTexture->getName()); } catch(...) {}
    }

    // Detach everything from root, without deleting them.
    if (rootNode_)
    {
        rootNode_->detachAllObjects();
        rootNode_->removeAllChildren();
    }
    rootNode_ = 0;

    // Debug visualization
    SAFE_DELETE(debugLines_);
    SAFE_DELETE(debugLinesNoDepth_);

    if (sceneManager_)
    {
        // Delete all created Ogre::SceneNodes
        for(int i=0, num=createdNodes_.size(); i<num; i++)
        {
            Ogre::SceneNode *ptr = createdNodes_[i];
            if (!ptr)
                continue;

            ptr->detachAllObjects();
            ptr->removeAllChildren();
            
            sceneManager_->destroySceneNode(ptr);
        }
        createdNodes_.clear();

        // Delete all created Ogre::MovableObjects
        for(int i=0, num=createdObjects_.size(); i<num; i++)
        {
            Ogre::MovableObject *ptr = createdObjects_[i];
            if (!ptr)
                continue;

            sceneManager_->destroyMovableObject(ptr);
        }
        createdObjects_.clear();

        // Destroy camera
        if (camera_)
            sceneManager_->destroyCamera(camera_);
        camera_ = 0;
        
        // Lights
        sceneManager_->destroyAllLights();

        // Destroy scene
        Ogre::Root::getSingleton().destroySceneManager(sceneManager_);
    }
    sceneManager_ = 0;
}

bool RocketOgreSceneRenderer::Initialize()
{
    // Initialize member data
    id_ = "RocketOgreSceneRenderer_" + QUuid::createUuid().toString().replace("{", "").replace("}", "");
    
    autoMeshFocus_ = true;
    cameraDistance_ = 10.0f;
    updateInterval_ = -1.0f;
    t_ = 0.0f;

    renderWindow_ = 0;
    sceneManager_ = 0;
    rootNode_ = 0;
    camera_ = 0;
    viewPort_ = 0;
    overlay_ = 0;
    overlayElement_ = 0;
    debugLines_ = 0;
    debugLinesNoDepth_ = 0;

    // Init rendering
    OgreRenderingModule *renderingModule = plugin_->GetFramework()->Module<OgreRenderingModule>();
    renderer_ = (renderingModule ? renderingModule->Renderer() : OgreRenderer::RendererPtr());
    sceneManager_ = (renderer_ && renderer_->OgreRoot() ? renderer_->OgreRoot()->createSceneManager(Ogre::ST_GENERIC, IdStd()) : 0);
    rootNode_ = (sceneManager_ ? sceneManager_->getRootSceneNode() : 0);
    camera_ = (sceneManager_ ? sceneManager_->createCamera(IdStd("Camera")) : 0);

    if (!IsValid())
    {
        LogError(LC + "Ogre scene initialization failed");
        return false;
    }

    sceneManager_->setAmbientLight(OgreWorld::DefaultSceneAmbientLightColor());

    camera_->setAutoAspectRatio(true);
    camera_->setNearClipDistance(0.1f);
    camera_->setFarClipDistance(10000.0f);
    camera_->setFOVy(Ogre::Radian(Ogre::Math::DegreesToRadians(60.0f)));

    CreateSceneNode()->attachObject(camera_);
    
#include "DisableMemoryLeakCheck.h"
    debugLines_ = new DebugLines("PhysicsDebug");
    debugLinesNoDepth_ = new DebugLines("PhysicsDebugNoDepth");
#include "EnableMemoryLeakCheck.h"
    rootNode_->attachObject(debugLines_);
    rootNode_->attachObject(debugLinesNoDepth_);
    debugLinesNoDepth_->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);
    
    connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), SLOT(OnStateUpdate(float)));
    
    return true;
}

bool RocketOgreSceneRenderer::IsValid() const
{
    return (sceneManager_ != 0 && camera_ != 0 && rootNode_ != 0);
}

bool RocketOgreSceneRenderer::IsCameraValid() const
{
    return (IsValid() && camera_->getParentSceneNode() != 0);
}

QString RocketOgreSceneRenderer::Id(const QString &postfix) const
{
    if (!postfix.isEmpty())
        return QString("%1_%2").arg(id_).arg(postfix);
    else
        return id_;
}

QString RocketOgreSceneRenderer::MaterialId(const QString &postfix) const
{
    if (!postfix.isEmpty())
        return QString("%1_%2").arg(id_).arg(postfix);
    else
        return id_;
}

QString RocketOgreSceneRenderer::OverlayId(const QString &postfix) const
{
    if (!postfix.isEmpty())
        return QString("%1_%2").arg(id_).arg(postfix);
    else
        return id_;
}

std::string RocketOgreSceneRenderer::IdStd(const std::string &postfix) const
{
    return Id(QString::fromStdString(postfix)).toStdString();
}

std::string RocketOgreSceneRenderer::MaterialIdStd(const std::string &postfix) const
{
    return MaterialId(QString::fromStdString(postfix)).toStdString();
}

std::string RocketOgreSceneRenderer::OverlayIdStd(const std::string &postfix) const
{
    return MaterialId(QString::fromStdString(postfix)).toStdString();
}

void RocketOgreSceneRenderer::CreateRenderWindow(QWidget *targetWindow, const QString &name, int width, int height, int left, int top, bool fullscreen)
{
    Ogre::NameValuePairList params;

    /** See http://www.ogre3d.org/tikiwiki/RenderWindowParameters
        @note All the values must be strings. Assigning int/bool etc. directly to the map produces a empty string value!
        @bug Check if the keys and values are valid from Ogre::RenderSystem::getConfigOptions(). If you feed carbage to the map outside the accepted values
        (eg. FSAA) you may crash when the render window is created. Additionally if the map keys are case sensitive we might have bugs below (eg. vsync vs. VSync)! */
    Framework *fw = plugin_->GetFramework();
    if (fw->CommandLineParameters("--vsyncFrequency").length() > 0) // "Display frequency rate; only applies if fullScreen is set."
        params["displayFrequency"] = fw->CommandLineParameters("--vsyncFrequency").first().toStdString();
    if (fw->CommandLineParameters("--vsync").length() > 0) // "Synchronize buffer swaps to monitor vsync, eliminating tearing at the expense of a fixed frame rate"
        params["vsync"] = fw->CommandLineParameters("--vsync").first().toStdString();
    else if (fw->Config()->HasKey(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "vsync"))
    {
        QString value = fw->Config()->Get(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "vsync", "").toString();
        if (!value.isEmpty())
            params["vsync"] = value.toStdString();
    }
    if (fw->CommandLineParameters("--antialias").length() > 0) // "Full screen antialiasing factor"
        params["FSAA"] = fw->CommandLineParameters("--antialias").first().toStdString();
    else if (fw->Config()->HasKey(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "antialias"))
    {
        // Accepted FSAA values can be more than just a number, allow any string from config file. 
        QString value = fw->Config()->Get(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "antialias", "").toString();
        if (!value.isEmpty())
            params["FSAA"] = value.toStdString();
    }

#ifdef WIN32
    if (targetWindow)
        params["externalWindowHandle"] = Ogre::StringConverter::toString((unsigned int)targetWindow->winId());
#endif

#ifdef Q_WS_MAC
    Ogre::String winhandle;

    QWidget* nativewin = targetWindow;

    // according to
    // http://www.ogre3d.org/forums/viewtopic.php?f=2&t=27027 does
    winhandle = Ogre::StringConverter::toString(
        (unsigned long)nativewin ? nativewin->winId() : 0);

    //Add the external window handle parameters to the existing params set.
    params["externalWindowHandle"] = winhandle;
    
    /* According to http://doc.qt.nokia.com/stable/qwidget.html#winId
       "On Mac OS X, the type returned depends on which framework Qt was linked against. 
       -If Qt is using Carbon, the {WId} is actually an HIViewRef. 
       -If Qt is using Cocoa, {WId} is a pointer to an NSView."
      Ogre needs to know that a NSView handle will be passed to its' externalWindowHandle parameter,
      otherwise it assumes that NSWindow will be used 
    */
    params["macAPI"] = "cocoa";
    params["macAPICocoaUseNSView"] = "true";
#endif

#ifdef Q_WS_X11
    QWidget *parent = targetWindow;
    while(parent && parent->parentWidget())
        parent = parent->parentWidget();

    // GLX - According to Ogre Docs:
    // poslong:posint:poslong:poslong (display*:screen:windowHandle:XVisualInfo*)
    QX11Info info = targetWindow->x11Info();

    Ogre::String winhandle = Ogre::StringConverter::toString((unsigned long)(info.display()));
    winhandle += ":" + Ogre::StringConverter::toString((unsigned int)(info.screen()));
    winhandle += ":" + Ogre::StringConverter::toString((unsigned long)parent ? parent->winId() : 0);

    //Add the external window handle parameters to the existing params set.
    params["parentWindowHandle"] = winhandle;
#endif

    // Window position to params
    if (left != -1)
        params["left"] = Ogre::StringConverter::toString(left);
    if (top != -1)
        params["top"] = Ogre::StringConverter::toString(top);

#ifdef WIN32
    if (fw->HasCommandLineParameter("--perfHud"))
    {
        params["useNVPerfHUD"] = "true";
        params["Rendering Device"] = "NVIDIA PerfHUD";
    }
#endif

    renderWindow_ = Ogre::Root::getSingletonPtr()->createRenderWindow(name.toStdString().c_str(), width, height, fullscreen, &params);
    renderWindow_->setDeactivateOnFocusChange(false);
}

void RocketOgreSceneRenderer::CreateViewport(bool autoUpdate, bool overlayEnabled, Color backgroundColor)
{
    Ogre::RenderTarget *renderTarget = RenderTarget();
    if (!renderTarget || !camera_)
    {
        LogError(LC + "Failed to create viewport");
        return;
    }
    
    /// Initialize active camera and listen to camera changes.
    OgreRenderingModule* renderingModule = plugin_->GetFramework()->Module<OgreRenderingModule>();
    if (renderingModule && renderingModule->Renderer())
        connect(renderingModule->Renderer().get(), SIGNAL(DeviceCreated()), this, SLOT(OnDeviceCreated()), Qt::UniqueConnection);

    renderTarget->removeAllViewports();

    viewPort_ = renderTarget->addViewport(camera_);
    viewPort_->setAutoUpdated(autoUpdate);
    viewPort_->setOverlaysEnabled(overlayEnabled);
    viewPort_->setBackgroundColour(backgroundColor);
    viewPort_->_updateDimensions();
}

Ogre::OverlayElement *RocketOgreSceneRenderer::CreateOverlay(float2 size, float2 position, bool autoUpdate)
{
    Ogre::Texture *renderTexture = RenderTexture();
    if (!renderTexture || !OgreViewport())
        return 0;

    OgreViewport()->setAutoUpdated(autoUpdate);

    size = size.Clamp(float2(1.f, 1.f), float2(10000.f, 10000.f));
    position = position.Clamp(float2(0.f, 0.f), float2(10000.f, 10000.f));

    overlaySize_ = size;
    overlayPosition_ = position;
    
    DestroyOverlay();

    // Create material
    Ogre::MaterialPtr rttMaterial = Ogre::MaterialManager::getSingleton().create(MaterialIdStd(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

    Ogre::TextureUnitState *rttTuState = rttMaterial->getTechnique(0)->getPass(0)->createTextureUnitState();
    rttTuState->setTextureName(renderTexture->getName());
    rttTuState->setTextureFiltering(Ogre::TFO_NONE);
    rttTuState->setNumMipmaps(1);
    rttTuState->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);

    rttMaterial->setFog(true, Ogre::FOG_NONE); ///\todo Check, shouldn't here be false?
    rttMaterial->setReceiveShadows(false);
    rttMaterial->setTransparencyCastsShadows(false);

    rttMaterial->getTechnique(0)->getPass(0)->setSceneBlending(Ogre::SBF_SOURCE_ALPHA, Ogre::SBF_ONE_MINUS_SOURCE_ALPHA);
    rttMaterial->getTechnique(0)->getPass(0)->setDepthWriteEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setDepthCheckEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setLightingEnabled(false);
    rttMaterial->getTechnique(0)->getPass(0)->setCullingMode(Ogre::CULL_NONE);

    // Create container
    overlayElement_ = Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", OverlayIdStd("Overlay_Panel"));
    overlayElement_->setMaterialName(MaterialIdStd());
    overlayElement_->setMetricsMode(Ogre::GMM_PIXELS);
    overlayElement_->setDimensions(size.x, size.y);
    overlayElement_->setPosition(position.x, position.y);

    // Create overlay
    overlay_ = Ogre::OverlayManager::getSingleton().create(OverlayIdStd());
    overlay_->add2D(static_cast<Ogre::OverlayContainer*>(overlayElement_));
    overlay_->setZOrder(501);
    overlay_->show();

    return overlayElement_;
}

void RocketOgreSceneRenderer::SetOverlayPosition(float2 position)
{
    if (overlayElement_)
    {
        position = position.Clamp(float2(0.f, 0.f), float2(10000.f, 10000.f));
        overlayPosition_ = position;
        overlayElement_->setPosition(position.x, position.y);
    }
}

void RocketOgreSceneRenderer::SetOverlayPosition(float x, float y)
{
    SetOverlayPosition(float2(x,y));
}

void RocketOgreSceneRenderer::SetOverlaySize(float2 size)
{
    if (overlayElement_)
    {
        size = size.Clamp(float2(1.f, 1.f), float2(10000.f, 10000.f));
        overlaySize_ = size;
        overlayElement_->setDimensions(size.x, size.y);
    }
}

void RocketOgreSceneRenderer::SetOverlaySize(float width, float height)
{
    SetOverlaySize(float2(width,height));
}

void RocketOgreSceneRenderer::DestroyOverlay()
{
    if (Ogre::MaterialManager::getSingleton().getByName(MaterialIdStd()).get())
        Ogre::MaterialManager::getSingleton().remove(MaterialIdStd());
    if (Ogre::OverlayManager::getSingleton().hasOverlayElement(OverlayIdStd("Overlay_Panel")))
        Ogre::OverlayManager::getSingleton().destroyOverlayElement(OverlayIdStd("Overlay_Panel"));
    if (Ogre::OverlayManager::getSingleton().getByName(OverlayIdStd()))
        Ogre::OverlayManager::getSingleton().destroy(OverlayIdStd());

    overlayElement_ = 0;
    overlay_ = 0;
}

bool RocketOgreSceneRenderer::AttachObject(Ogre::MovableObject *object, Ogre::SceneNode *newParent)
{
    if (!object || !newParent)
        return false;
        
    Ogre::SceneNode *currentParent = object->getParentSceneNode();
    if (currentParent)
        currentParent->detachObject(object);
    newParent->attachObject(object);
    return true;
}

bool RocketOgreSceneRenderer::DestroyObject(Ogre::MovableObject *object)
{
    if (!sceneManager_ || !object)
        return false;

    createdObjects_.removeAll(object);

    Ogre::SceneNode *parentNode = object->getParentSceneNode();
    if (parentNode)
    {
        parentNode->detachObject(object);

        // We want to destroy the node only if this is a "root level" scene node
        if (parentNode->numAttachedObjects() == 0 && createdNodes_.contains(parentNode))
        {
            createdNodes_.removeAll(parentNode);
            nodeRotationCache_.remove(parentNode);

            if (rootNode_)
                rootNode_->removeChild(parentNode);
            
            sceneManager_->destroySceneNode(parentNode);
        }
    }

    sceneManager_->destroyMovableObject(object);

    return true;
}

Ogre::SceneNode *RocketOgreSceneRenderer::CreateSceneNode()
{
    if (!IsValid())
        return 0;

    Ogre::SceneNode *sceneNode = sceneManager_->createSceneNode(renderer_->GenerateUniqueObjectName(IdStd("Node")));
    
    // Setup defaults
    sceneNode->setVisible(true);
    SetNodeTransform(sceneNode, Transform(float3::zero, float3::zero, float3::one));

    // Store for later destruction
    createdNodes_ << sceneNode;

    rootNode_->addChild(sceneNode);
    return sceneNode;
}

void RocketOgreSceneRenderer::CreateMesh(const QString &meshAssetRef, const QStringList &materialAssetRefs, Transform transform)
{
    AssetTransferPtr transfer = plugin_->GetFramework()->Asset()->RequestAsset(meshAssetRef, "OgreMesh");
    if (transfer.get())
        NewPendingObjectCreation(transfer, materialAssetRefs, transform);
}

Ogre::Entity *RocketOgreSceneRenderer::CreateMesh(OgreMeshAsset *asset)
{
    if (!IsValid() || !asset || !asset->IsLoaded() || asset->OgreMeshName().isEmpty())
        return 0;

    Ogre::SceneNode *sceneNode = CreateSceneNode();
    Ogre::Entity *meshEntity = sceneManager_->createEntity(sceneNode->getName() + "_Mesh", asset->OgreMeshName().toStdString());
    meshEntity->setMaterialName("BaseWhite");
    sceneNode->attachObject(meshEntity);

    // Store for later destruction
    createdObjects_ << meshEntity;
    
    return meshEntity;
}

bool RocketOgreSceneRenderer::SetMaterial(Ogre::Entity *entity, OgreMaterialAsset *asset, uint submeshIndex)
{
    if (!entity || !asset || !asset->IsLoaded() || !asset->ogreMaterial.get())
        return false;
        
    if (submeshIndex >= entity->getNumSubEntities())
        return false;
        
    entity->getSubEntity(submeshIndex)->setMaterial(asset->ogreMaterial);
    return true;
}

bool RocketOgreSceneRenderer::SetMaterials(Ogre::Entity *entity, QList<OgreMaterialAsset*> assets)
{
    bool anyFailed = false;
    for(int i=0; i<assets.size(); ++i)
    {
        if (!SetMaterial(entity, assets[i], static_cast<uint>(i)))
            anyFailed = true;
    }
    return !anyFailed;
}

Ogre::Entity *RocketOgreSceneRenderer::CreateMesh(Ogre::SceneManager::PrefabType type)
{
    if (!IsValid())
        return 0;

    Ogre::SceneNode *sceneNode = CreateSceneNode();
    Ogre::Entity *meshEntity = sceneManager_->createEntity(sceneNode->getName() + "_Mesh_Prefab", type);
    meshEntity->setMaterialName("BaseWhite");
    sceneNode->attachObject(meshEntity);

    // Store for later destruction
    createdObjects_ << meshEntity;

    return meshEntity;
}

Ogre::Light *RocketOgreSceneRenderer::CreateLight(Ogre::Light::LightTypes type, const Color &diffuse)
{
    if (!IsValid())
        return 0;

    Ogre::SceneNode *sceneNode = CreateSceneNode();
    Ogre::Light *light = sceneManager_->createLight(sceneNode->getName() + "_Light");
    light->setType(type);
    light->setDiffuseColour(diffuse);
    sceneNode->attachObject(light);
    
    // Store for later destruction
    createdObjects_ << light;

    return light;
}

Ogre::SceneManager *RocketOgreSceneRenderer::SceneManager() const
{
    return sceneManager_;
}

Ogre::Camera *RocketOgreSceneRenderer::OgreCamera() const
{
    return camera_;
}

Ogre::Viewport *RocketOgreSceneRenderer::OgreViewport() const
{
    return viewPort_;
}

Ogre::Overlay *RocketOgreSceneRenderer::OgreOverlay() const
{
    return overlay_;
}

Ogre::OverlayElement *RocketOgreSceneRenderer::OgreOverlayElement() const
{
    return overlayElement_;
}

float3 RocketOgreSceneRenderer::CameraPosition()
{
    if (camera_ && camera_->getParentSceneNode())
        return camera_->getParentSceneNode()->getPosition();
    return float3::zero;
}

Transform RocketOgreSceneRenderer::CameraTransform()
{
    return NodeTransform((camera_ && camera_->getParentSceneNode() ? camera_->getParentSceneNode() : 0));
}

float RocketOgreSceneRenderer::CameraDistance() const
{
    return cameraDistance_;
}

void RocketOgreSceneRenderer::SetCameraDistance(float distance)
{
    autoMeshFocus_ = true;
    cameraDistance_ = Max(distance, 0.0f);
}

void RocketOgreSceneRenderer::SetCameraTransform(const Transform &transform)
{
    autoMeshFocus_ = false;
    SetNodeTransform((camera_ && camera_->getParentSceneNode() ? camera_->getParentSceneNode() : 0), transform);
}

void RocketOgreSceneRenderer::SetCameraPosition(float3 position)
{
    if (camera_ && camera_->getParentSceneNode())
    {
        autoMeshFocus_ = false;
        camera_->getParentSceneNode()->setPosition(position);
    }
}

void RocketOgreSceneRenderer::SetCameraLookAt(float3 position)
{
    if (camera_)
    {
        autoMeshFocus_ = false;
        camera_->lookAt(position);
    }
}

void RocketOgreSceneRenderer::SetBackgroundColor(const Color &color)
{
    if (OgreViewport())
        OgreViewport()->setBackgroundColour(color);
}

void RocketOgreSceneRenderer::SetAmbientLightColor(const Color &color)
{
    if (SceneManager())
        SceneManager()->setAmbientLight(color);
}

Transform RocketOgreSceneRenderer::NodeTransform(Ogre::SceneNode *node)
{
    Transform transform;
    if (node)
    {
        transform.pos = node->getPosition();
        transform.scale = node->getScale();
        
        if (nodeRotationCache_.contains(node))
            transform.rot = nodeRotationCache_[node];
        else
        {
            LogWarning(LC + "NodeTransform() Failed to find rotation in degrees from cache for node. Did you use CreateSceneNode() to create it?!");
            transform.SetOrientation(node->getOrientation());
        }
    }
    return transform;
}

void RocketOgreSceneRenderer::SetNodeTransform(Ogre::SceneNode *node, const Transform &transform)
{
    if (!node)
        return;

    if (transform.pos.IsFinite())
        node->setPosition(transform.pos);

    Quat orientation = transform.Orientation();
    if (orientation.IsFinite())
    {
        node->setOrientation(orientation);
        nodeRotationCache_[node] = transform.rot;
    }

    // Prevent Ogre exception from zero scale
    float3 scale = transform.scale;
    if (scale.IsZero())
    {
        LogWarning(LC + "SetNodeTransform scale too close to zero, correcting to 0.0001.");
        scale = float3(0.0001f, 0.0001f, 0.0001f);
    }
    node->setScale(scale);
}

void RocketOgreSceneRenderer::OnRenderTargetResized()
{
    if (renderMode_ == RM_RenderWidget && renderTarget_)
        Resize(renderTarget_->size());
}

void RocketOgreSceneRenderer::OnDeviceCreated()
{
    Ogre::Texture *texture = RenderTexture();
    if (!texture)
        return;

    // Current viewport options
    bool autoUpdate = (OgreViewport() ? OgreViewport()->isAutoUpdated() : false);
    Ogre::ColourValue bgColor = (OgreViewport() ? OgreViewport()->getBackgroundColour() : Ogre::ColourValue::Black);

    // Recreate texture
    RecreateTexture(texture, QSize(texture->getWidth(), texture->getHeight()));

    // Recreate viewport
    CreateViewport(false, false, bgColor);

    // Recreate overlay
    if (overlayElement_)
        CreateOverlay(overlaySize_, overlayPosition_, autoUpdate);
}

RocketOgreSceneRenderer::PendingObjectCreation *RocketOgreSceneRenderer::NewPendingObjectCreation(AssetTransferPtr transfer, const QStringList &materialAssetRefs, Transform transform)
{
    PendingObjectCreation *pending = new PendingObjectCreation(this);
    pending->UUID = QUuid::createUuid().toString().replace("{","").replace("}","");
    pending->transform = transform;
    pending->materialAssetRefs = materialAssetRefs;
    
    pending->Listen(transfer.get());
    connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnMeshAssetLoaded(AssetPtr)), Qt::UniqueConnection);
    connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnMeshAssetFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);

    pendingCreations_ << pending;    
    return pending;
}

RocketOgreSceneRenderer::PendingObjectCreationList RocketOgreSceneRenderer::PendingCreation(IAssetTransfer *transfer)
{
    QList<RocketOgreSceneRenderer::PendingObjectCreation*> creations;
    if (!transfer)
        return creations;
    
    QStringList uuids = transfer->property("PendingObjectCreationUUID").toStringList();
    foreach(const QString &uuid, uuids)
    {
        RocketOgreSceneRenderer::PendingObjectCreation *pending = PendingCreationByUUID(uuid);
        if (pending)
            creations << pending;
    }
    return creations;
}

RocketOgreSceneRenderer::PendingObjectCreation *RocketOgreSceneRenderer::PendingCreationByUUID(const QString &UUID)
{
    if (UUID.isEmpty())
        return 0;
    foreach(PendingObjectCreation *pending, pendingCreations_)
    {
        if (pending && pending->UUID == UUID)
            return pending;
    }
    return 0;
}

void RocketOgreSceneRenderer::ApplyPendingMaterial(RocketOgreSceneRenderer::PendingObjectCreation *pending, IAssetTransfer *transfer)
{
    if (!pending || !transfer)
        return;

    for (int i=0; i<pending->materialAssetRefsTransfer.size(); ++i)
    {
        if (pending->materialAssetRefsTransfer[i] == transfer->SourceUrl())
        {
            SetMaterial(pending->entity, dynamic_cast<OgreMaterialAsset*>(transfer->Asset().get()), i);

            // Reuse the material refs list for tracking completion.
            if (!pending->materialAssetRefs.isEmpty())
                pending->materialAssetRefs.takeFirst();
            if (pending->materialAssetRefs.isEmpty())
                RemovePendingCreation(pending);
            return;
        }
    }
}

bool RocketOgreSceneRenderer::RemovePendingCreation(RocketOgreSceneRenderer::PendingObjectCreation *pending)
{
    if (pending)
        return RemovePendingCreation(pending->UUID);
    return false;
}

bool RocketOgreSceneRenderer::RemovePendingCreation(const QString &UUID)
{
    for (int i=0; i<pendingCreations_.size(); ++i)
    {
        if (pendingCreations_[i] && pendingCreations_[i]->UUID == UUID)
        {
            pendingCreations_.removeAt(i);
            return true;
        }
    }
    return false;
}

void RocketOgreSceneRenderer::OnMeshAssetLoaded(AssetPtr asset)
{
    PendingObjectCreationList pendings = PendingCreation(dynamic_cast<IAssetTransfer*>(sender()));
    if (pendings.isEmpty())
        return;
    
    OgreMeshAsset *meshAsset = dynamic_cast<OgreMeshAsset*>(asset.get());
    foreach(PendingObjectCreation *pending, pendings)
    {
        if (!meshAsset)
        {
            RemovePendingCreation(pending);
            continue;
        }

        pending->entity = CreateMesh(meshAsset);
        SetNodeTransform(pending->entity->getParentSceneNode(), pending->transform);
        if (pending->materialAssetRefs.isEmpty())
        {
            RemovePendingCreation(pending);
            continue;
        }

        pending->materialAssetRefsTransfer.clear();
        foreach(const QString &materialRef, pending->materialAssetRefs)
        {
            AssetTransferPtr transfer = plugin_->GetFramework()->Asset()->RequestAsset(materialRef, "OgreMaterial");
            if (transfer.get())
            {
                pending->Listen(transfer.get());
                pending->materialAssetRefsTransfer << transfer->SourceUrl();
                connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnMaterialAssetLoaded(AssetPtr)), Qt::UniqueConnection);
                connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnMaterialAssetFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);
            }
            else
                pending->materialAssetRefsTransfer << "";    
        }
    }
}

void RocketOgreSceneRenderer::OnMaterialAssetLoaded(AssetPtr asset)
{
    IAssetTransfer *transfer = dynamic_cast<IAssetTransfer*>(sender());
    if (!transfer)
        return;
    PendingObjectCreationList pendings = PendingCreation(transfer);
    foreach(PendingObjectCreation *pending, pendings)
        if (pending->ApplyMaterial(transfer->SourceUrl(), transfer->Asset()))
            RemovePendingCreation(pending->UUID);
}

void RocketOgreSceneRenderer::OnMeshAssetFailed(IAssetTransfer *transfer, QString reason)
{
    PendingObjectCreationList pendings = PendingCreation(dynamic_cast<IAssetTransfer*>(sender()));
    foreach(PendingObjectCreation *pending, pendings)
        RemovePendingCreation(pending);
}

void RocketOgreSceneRenderer::OnMaterialAssetFailed(IAssetTransfer *transfer, QString reason)
{
    PendingObjectCreationList pendings = PendingCreation(dynamic_cast<IAssetTransfer*>(sender()));
    foreach(PendingObjectCreation *pending, pendings)
        if (pending->ApplyMaterial(transfer->SourceUrl(), AssetPtr()))
            RemovePendingCreation(pending->UUID);
}

void RocketOgreSceneRenderer::Resize(const QSize &size)
{
    if (!IsValid())
        return;

    PROFILE(RocketOgreSceneRenderer_Resize);
    
    if (renderMode_ == RM_RenderTexture)
    {
        Ogre::Texture *texture = RenderTexture();
        if (!texture)
        {
            texture = Ogre::TextureManager::getSingleton().createManual(IdStd("RenderTexture"), 
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
                size.width(), size.height(), 0, format_, Ogre::TU_RENDERTARGET, 0, false, ReadFSAASetting()).get();
        }        
        else if (static_cast<int>(texture->getWidth()) != size.width() || static_cast<int>(texture->getHeight()) != size.height())
        {
            RecreateTexture(texture, size);
        }

        CreateViewport(false, false, QColor(20,20,20));
    }
    else if (renderMode_ == RM_RenderWidget)
    {
#ifdef Q_WS_MAC
        s32 majorVersion, minorVersion;
        OSXVersionInfo(&majorVersion, &minorVersion, 0);

        // For non-obvious reasons, on Mac OS X Maverick, renderWindow->getWidth() and width (same applies for height) is always equals,
        // thus it will fail to resize the UI texture. Bypass this check in case of OS X Maverick (10.9.X)
        if (majorVersion <= 10 && minorVersion < 9)
#endif
        if (size.width() == static_cast<int>(renderWindow_->getWidth()) && size.height() == static_cast<int>(renderWindow_->getHeight()))
            return; // Avoid recreating resources if the size didn't actually change.

#ifndef Q_WS_MAC
        renderWindow_->resize(size.width(), size.height());
        renderWindow_->windowMovedOrResized();
#endif
    }
}

uint RocketOgreSceneRenderer::ReadFSAASetting()
{
    // FSAA 1) cmd line 2) config
    QString FSAA = (plugin_->GetFramework()->CommandLineParameters("--antialias").length() > 0 ? 
        plugin_->GetFramework()->CommandLineParameters("--antialias").first() : "");
    if (FSAA.isEmpty() && plugin_->GetFramework()->Config()->HasKey(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "antialias"))
        FSAA = plugin_->GetFramework()->Config()->Get(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING, "antialias", "").toString();
    if (!FSAA.isEmpty())
    {
        bool ok = false;
        int numFSAA = FSAA.toInt(&ok);
        if (ok && numFSAA >= 0)
            return static_cast<uint>(numFSAA);
    }
    return 0;
}

void RocketOgreSceneRenderer::RecreateTexture(Ogre::Texture *&texture, const QSize &size)
{
    if (!texture)
        return;

    // Render target textures seemingly cannot be resized after creation, lets recreated it!
    try 
    { 
        Ogre::RenderTarget *renderTarget = RenderTarget();
        if (renderTarget)
        {
            renderTarget->removeAllViewports();
            renderTarget->removeAllListeners();
            Ogre::Root::getSingleton().destroyRenderTarget(renderTarget);
        }

        Ogre::TextureManager::getSingleton().remove(texture->getName());
    } 
    catch(Ogre::Exception &e)
    {
        LogError(QString("Failed to destroy render target and its texture: ") + e.what());
    }
    
    texture = Ogre::TextureManager::getSingleton().createManual(IdStd("RenderTexture"), 
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, Ogre::TEX_TYPE_2D,
        size.width(), size.height(), 0, format_, Ogre::TU_RENDERTARGET, 0, false, ReadFSAASetting()).get();

    /* This does not work for render target textures!
    texture->freeInternalResources();
    texture->setWidth(size.width());
    texture->setHeight(size.height());
    texture->createInternalResources();*/
}

Ogre::Texture *RocketOgreSceneRenderer::RenderTexture() const
{
    if (renderMode_ == RM_RenderTexture)
        return dynamic_cast<Ogre::Texture*>(Ogre::TextureManager::getSingleton().getByName(IdStd("RenderTexture")).get());
    else
        return 0;
}

Ogre::RenderTarget *RocketOgreSceneRenderer::RenderTarget() const
{
    if (renderMode_ == RM_RenderTexture)
    {
        Ogre::Texture *texture = RenderTexture();
        return (texture && texture->getBuffer().get() ? texture->getBuffer()->getRenderTarget() : 0);
    }
    else if (renderMode_ == RM_RenderWidget)
        return renderWindow_;
    return 0;
}

bool RocketOgreSceneRenderer::eventFilter(QObject *obj, QEvent *e)
{
    if (!renderTarget_.isNull() && obj == renderTarget_)
    {
        // Suppress Qt events that make the rendering flicker
        if (e->type() == QEvent::Paint || e->type() == QEvent::UpdateRequest || 
            e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave || 
            e->type() == QEvent::Enter || e->type() == QEvent::Leave ||
            e->type() == QEvent::WindowActivate || e->type() == QEvent::WindowDeactivate)
        {
            e->accept();
            return true;
        }
        else if (e->type() == QEvent::Resize)
            resizeTimer_.start(100);
    }
    return false;
}

void RocketOgreSceneRenderer::SetAutoUpdateInterval(int updateIntervalMsec)
{
    float fps = 1000.0f / static_cast<float>(updateIntervalMsec);
    SetAutoUpdateInterval(1.0f / fps);
}

void RocketOgreSceneRenderer::SetAutoUpdateInterval(float updateIntervalSeconds)
{
    if (updateIntervalSeconds <= 0.0f)
    {
        disconnect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
        updateInterval_ = -1.0f;
        return;
    }
    updateInterval_ = updateIntervalSeconds;
    t_ = 0.0f;
    connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
}

void RocketOgreSceneRenderer::Update()
{
    // Auto update disabled, update now
    if (updateInterval_ <= 0.0f)
        OnUpdate(0.0f);
}

void RocketOgreSceneRenderer::OnStateUpdate(float frametime)
{
    PROFILE(RocketOgreSceneRenderer_DrawDebug);  

    if (autoMeshFocus_)
    {
        foreach(Ogre::MovableObject *obj, createdObjects_)
        {
            Ogre::Entity *ent = dynamic_cast<Ogre::Entity*>(obj);
            if (ent)
            {
                AABB aabb = ent->getWorldBoundingBox();
                
                // Zoom
                if (camera_ && camera_->getParentSceneNode())
                    camera_->getParentSceneNode()->setPosition(aabb.CenterPoint() + float3(cameraDistance_, 0.f, 0.f));

                // Lookat
                if (camera_)
                    camera_->lookAt(aabb.CenterPoint());

                break;
            }
        }
    }

    if (debugLines_)
        debugLines_->draw();
    if (debugLinesNoDepth_)
        debugLinesNoDepth_->draw();
}

void RocketOgreSceneRenderer::OnUpdate(float frametime)
{
    // Interval choking if enabled
    if (updateInterval_ > 0.0f)
    {
        t_ += frametime;
        if (t_ < updateInterval_)
            return;
        t_ = 0.0f;
    }
    
    PROFILE(RocketOgreSceneRenderer_Update);

    if (renderMode_ == RM_RenderTexture)
    {
        Ogre::RenderTarget *renderTarget = RenderTarget();
        if (!renderTarget || !viewPort_)
        {
            LogError(LC + "Update failed to get render target");
            return;
        }
        if (!viewPort_->isAutoUpdated())
            viewPort_->update();
    }
}

QImage RocketOgreSceneRenderer::ToQImage()
{
    PROFILE(RocketOgreSceneRenderer_ToQImage);
    
    if (renderMode_ == RM_RenderTexture)
    {
        Ogre::Texture *renderTexture = RenderTexture();
        if (renderTexture)
            return TextureAsset::ToQImage(renderTexture);
        return QImage();
    }
    else
    {
        LogError(LC + "QImage rendering not supported for RM_RenderWidget mode. Create me with the RM_RenderTexture contructor.");
        return QImage();
    }
}

/// Debug visualization

void RocketOgreSceneRenderer::AddDebugDrawAABB(const AABB &aabb, const Color &clr, bool depthTest)
{
    for(int i = 0; i < 12; ++i)
        AddDebugDrawLineSegment(aabb.Edge(i), clr, depthTest);
}

void RocketOgreSceneRenderer::AddDebugDrawOBB(const OBB &obb, const Color &clr, bool depthTest)
{
    for(int i = 0; i < 12; ++i)
        AddDebugDrawLineSegment(obb.Edge(i), clr, depthTest);
}

void RocketOgreSceneRenderer::AddDebugDrawLineSegment(const LineSegment &l, const Color &clr, bool depthTest)
{
    if (depthTest)
    {
        if (debugLines_)
            debugLines_->addLine(l.a, l.b, clr);
    }
    else
    {
        if (debugLinesNoDepth_)
            debugLines_->addLine(l.a, l.b, clr);
    }
}

// PendingObjectCreation

bool RocketOgreSceneRenderer::PendingObjectCreation::ApplyMaterial(const QString &sourceUrl, AssetPtr asset)
{
    for (int i=0; i<materialAssetRefsTransfer.size(); ++i)
    {
        if (materialAssetRefsTransfer[i] == sourceUrl)
        {
            if (entity && asset)
                sceneRenderer->SetMaterial(entity, dynamic_cast<OgreMaterialAsset*>(asset.get()), i);

            // Reuse the material refs list for tracking completion.
            if (!materialAssetRefs.isEmpty())
                materialAssetRefs.takeFirst();
            break;
        }
    }
    return (materialAssetRefs.isEmpty());
}

void RocketOgreSceneRenderer::PendingObjectCreation::Listen(IAssetTransfer *transfer)
{
    if (!transfer)
        return;

    QStringList uuids = transfer->property("PendingObjectCreationUUID").toStringList();
    foreach(const QString &listener, uuids)
        if (UUID == listener)
            return;
    uuids << UUID;
    transfer->setProperty("PendingObjectCreationUUID", uuids);
}

bool RocketOgreSceneRenderer::PendingObjectCreation::Listening(IAssetTransfer *transfer)
{
    QStringList uuids = transfer->property("PendingObjectCreationUUID").toStringList();
    foreach(const QString &listener, uuids)
        if (UUID == listener)
            return true;
    return false;
}
