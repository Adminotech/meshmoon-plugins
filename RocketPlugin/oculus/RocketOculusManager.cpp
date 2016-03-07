/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

//#include "StableHeaders.h"

#include "CoreTypes.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"

#ifdef WIN32

// Already included Windows.h by some other means. This is an error, since including winsock2.h will fail after this.
#if defined(_WINDOWS_) && !defined(_WINSOCK2API_) && defined(FD_CLR)
#error Error: Trying to include winsock2.h after windows.h! This is not allowed! See this file for fix instructions.
#endif

// Remove the manually added #define if it exists so that winsock2.h includes OK.
#if !defined(_WINSOCK2API_) && defined(_WINSOCKAPI_)
#undef _WINSOCKAPI_
#endif

// Windows.h issue: Cannot include winsock2.h after windows.h, so include it before.

// MathGeoLib uses the symbol Polygon. Windows.h gives GDI function Polygon, which Tundra will never use, so kill it.
#define Polygon Polygon_unused

#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif

#endif

#ifdef ROCKET_OCULUS_ENABLED
    #ifdef Ptr
        #undef Ptr
    #endif

    #if defined(WIN32)
        #if defined(DIRECTX_ENABLED)
            #include <d3d9.h>
            #define OVR_D3D_VERSION 9
            #include <OVR_CAPI_D3D.h>
        #endif
    #endif

    #include <OVR.h>
    #ifndef Ptr
        #define Ptr(type) kNet::SharedPtr< type >
    #endif
#endif

#undef ROCKET_NO_WIN_H

#include "DebugOperatorNew.h"

#include "RocketOculusManager.h"
#include "RocketPlugin.h"

#ifdef ROCKET_OCULUS_ENABLED

#include "RocketLobbyWidget.h"
#include "RocketNotifications.h"

#include "Profiler.h"
#include "Framework.h"
#include "FrameAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "InputAPI.h"
#include "InputContext.h"
#include "Scene.h"

#include "ConfigAPI.h"

#include "EC_Placeable.h"
#include "EC_Camera.h"
#include "EC_Avatar.h"
#include "EC_Mesh.h"
#include "EC_Sky.h"
#include "EC_ParticleSystem.h"
#include "EC_Billboard.h"

#include "OgreWorld.h"
#include "OgreSceneManager.h"
#include "OgreRenderingModule.h"
#include "OgreD3D9RenderWindow.h"
#include "OgreD3D9Texture.h"

#include "Scene.h"
#include "TundraLogicModule.h"
#include "UiGraphicsView.h"

#include "Math/Quat.h"
#include "MemoryLeakCheck.h"
#include "Client.h"

#include <QDesktopWidget>
#include <QScreen>
#include <QMenuBar>

static uint OCULUS_ENTITY_MASK = 0xFFFFFFF0;

struct TrackingState
{
    Ogre::Quaternion orientation;
    Ogre::Vector3 position;

    TrackingState()
    {
        orientation = Ogre::Quaternion::IDENTITY;
        position = Ogre::Vector3::ZERO;
    }

    void SetState(ovrPosef pose)
    {
        orientation.w = pose.Orientation.w;
        orientation.x = pose.Orientation.x;
        orientation.y = pose.Orientation.y;
        orientation.z = pose.Orientation.z;

        position.x = pose.Position.x;
        position.y = pose.Position.y;
        position.z = pose.Position.z;
    }
};

struct TimeWarpMatrices
{
    Ogre::Matrix4 start;
    Ogre::Matrix4 end;
};

struct RocketOculusManager::OculusMembers
{
    enum Eye
    {
        EYE_LEFT = 0,
        EYE_RIGHT = 1
    };

    OculusMembers::OculusMembers(Framework* framework, OgreRenderer::RendererPtr renderer) :
        framework_(framework),
        renderer_(renderer),
        hmd_(0)
    {
        ovr_Initialize();

        int devicesDetected = ovrHmd_Detect();
        if (devicesDetected <= 0)
            return;

        hmd_ = ovrHmd_Create(0);

        int enabledCaps = ovrHmd_GetEnabledCaps(hmd_);
        useDirectMode = false;

        resolution_.w = 1920;
        resolution_.h = 1080;

        eyeViewports_[0].Pos.x = 0;
        eyeViewports_[0].Pos.y = 0;
        eyeViewports_[0].Size.w = resolution_.w / 2;
        eyeViewports_[0].Size.h = resolution_.h;
        eyeViewports_[1].Pos.x = resolution_.w / 2;
        eyeViewports_[1].Pos.y = 0;
        eyeViewports_[1].Size = eyeViewports_[0].Size;

        if (hmd_)
        {
            eyeFov_[0] = hmd_->DefaultEyeFov[0];
            eyeFov_[1] = hmd_->DefaultEyeFov[1];

            ovrHmd_SetEnabledCaps(hmd_, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

            ovrHmd_ConfigureTracking(hmd_,  ovrTrackingCap_Orientation |
                                            ovrTrackingCap_MagYawCorrection |
                                            ovrTrackingCap_Position, 0);
        }
    }

    OculusMembers::~OculusMembers()
    {
        ovrHmd_Destroy(hmd_);
        ovr_Shutdown();
    }

    bool HasDevice() const
    {
        return (hmd_ ? true : false);
    }

    int DeviceWidth()
    {
        return (hmd_ ? hmd_->Resolution.w : 0);
    }

    int DeviceHeight()
    {
        return (hmd_ ? hmd_->Resolution.h : 0);
    }

    bool UseDirectMode()
    {
        return useDirectMode;
    }

    void ConfigureRendering()
    {
        if (!HasDevice())
            return;

        Ogre::D3D9RenderWindow* mainWindow = dynamic_cast<Ogre::D3D9RenderWindow*>(renderer_->CurrentRenderWindow());
        if (!mainWindow)
            return;

        IDirect3DSwapChain9* swapChain = 0;
        mainWindow->getDevice()->getD3D9Device()->GetSwapChain(0, &swapChain);
        if (!swapChain)
            return;

        ovrD3D9Config renderConfig;
        renderConfig.D3D9.Header.API = ovrRenderAPI_D3D9;
        renderConfig.D3D9.Header.RTSize.w = resolution_.w;
        renderConfig.D3D9.Header.RTSize.h = resolution_.h;
        renderConfig.D3D9.Header.Multisample = mainWindow->getFSAA();
        renderConfig.D3D9.pDevice = mainWindow->getDevice()->getD3D9Device();
        renderConfig.D3D9.pSwapChain = swapChain;

        if (!ovrHmd_ConfigureRendering(hmd_, &renderConfig.Config, ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette | ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive, hmd_->DefaultEyeFov, eyeRenderDesc_))
            return;

        ovrHmd_SetEnabledCaps(hmd_, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);

        if (useDirectMode)
            ovrHmd_AttachToWindow(hmd_, mainWindow->getWindowHandle(), NULL, NULL);
    }

    void Attach()
    {
        Ogre::D3D9RenderWindow* mainWindow = dynamic_cast<Ogre::D3D9RenderWindow*>(renderer_->CurrentRenderWindow());
        if (!mainWindow)
            return;

        HWND winId = mainWindow->getWindowHandle();

        ovrHmd_AttachToWindow(hmd_, winId, NULL, NULL);
    }

    void SetResolution(int width, int height)
    {
        resolution_.w = width;
        resolution_.h = height;
    }

    Ogre::MeshPtr CreateDistortionMesh(Eye eyeIndex)
    {
        // Get distortion mesh
        ovrDistortionMesh distortionMesh;
        ovrHmd_CreateDistortionMesh(hmd_,
            eyeRenderDesc_[eyeIndex].Eye, eyeRenderDesc_[eyeIndex].Fov,
            ovrDistortionCap_Chromatic | ovrDistortionCap_Overdrive | ovrDistortionCap_Vignette, &distortionMesh);

        Ogre::MeshPtr ogreMesh = Ogre::MeshManager::getSingleton().createManual((eyeIndex == 0 ? "OculusDistortionMeshLeft" : "OculusDistortionMeshRight"), RocketPlugin::MESHMOON_RESOURCE_GROUP);
        Ogre::SubMesh* subMesh = ogreMesh->createSubMesh();

        ogreMesh->sharedVertexData = new Ogre::VertexData();
        ogreMesh->sharedVertexData->vertexCount = distortionMesh.VertexCount;

        // Create vertex declaration
        Ogre::VertexDeclaration* vertexDeclaration = ogreMesh->sharedVertexData->vertexDeclaration;

        size_t offset = 0;

        // Position + timewarp in z
        vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION, 0);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);

        // TexcoordR
        vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);

        // TexcoordG
        vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 1);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);

        // TexcoordB
        vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 2);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
        
        // Vignette
        vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT1, Ogre::VES_TEXTURE_COORDINATES, 3);
        offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT1);

        // Allocate vertexbuffer
        Ogre::HardwareVertexBufferSharedPtr vertexBuffer = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(offset, distortionMesh.VertexCount, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);

        // Set vertex buffer binding
        Ogre::VertexBufferBinding* binding = ogreMesh->sharedVertexData->vertexBufferBinding;
        binding->setBinding(0, vertexBuffer);

        // Upload vertex data to GPU
        float* data = new float[10 * distortionMesh.VertexCount];

        unsigned dataIndex = 0;
        ovrDistortionVertex *ovrVertex = distortionMesh.pVertexData;
        for (unsigned i = 0; i < distortionMesh.VertexCount; ++i)
        {
            // Position
            data[dataIndex + 0] = ovrVertex->ScreenPosNDC.x;
            data[dataIndex + 1] = ovrVertex->ScreenPosNDC.y;

            // Timewarp
            data[dataIndex + 2] = ovrVertex->TimeWarpFactor;

            // TangentEyeR
            data[dataIndex + 3] = ovrVertex->TanEyeAnglesR.x;
            data[dataIndex + 4] = ovrVertex->TanEyeAnglesR.y;

            // TangentEyeG
            data[dataIndex + 5] = ovrVertex->TanEyeAnglesG.x;
            data[dataIndex + 6] = ovrVertex->TanEyeAnglesG.y;

            // TangentEyeB
            data[dataIndex + 7] = ovrVertex->TanEyeAnglesB.x;
            data[dataIndex + 8] = ovrVertex->TanEyeAnglesB.y;

            // Vignette
            data[dataIndex + 9] = ovrVertex->VignetteFactor;

            dataIndex += 10;
            ovrVertex++;
        }

        vertexBuffer->writeData(0, vertexBuffer->getSizeInBytes(), data, true);

        // Allocate index buffer
        Ogre::HardwareIndexBufferSharedPtr indexBuffer = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT, distortionMesh.IndexCount, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);

        // Upload the index data to the GPU
        indexBuffer->writeData(0, indexBuffer->getSizeInBytes(), distortionMesh.pIndexData, true);

        for (uint i = 0; i < distortionMesh.IndexCount; i += 3)
        {
            short f1 = distortionMesh.pIndexData[i + 0] + 1;
            short f2 = distortionMesh.pIndexData[i + 1] + 1;
            short f3 = distortionMesh.pIndexData[i + 2] + 1;
        }

        // Set parameters of the submesh
        subMesh->useSharedVertices = true;
        subMesh->indexData->indexBuffer = indexBuffer;
        subMesh->indexData->indexCount = distortionMesh.IndexCount;
        subMesh->indexData->indexStart = 0;

        // Set bounding information (for culling);
        Ogre::AxisAlignedBox box(-1, -1, -1, 1, 1, 1);
        ogreMesh->_setBounds(box);
        ogreMesh->_setBoundingSphereRadius(2.0f);

        // Load mesh
        ogreMesh->load();

        // Update structs
        eyeRenderDesc_[eyeIndex] = ovrHmd_GetRenderDesc(hmd_, (ovrEyeType)eyeIndex,  eyeFov_[eyeIndex]);
        ovrHmd_GetRenderScaleAndOffset(eyeFov_[eyeIndex], resolution_, eyeViewports_[eyeIndex], uvScaleOffset_[eyeIndex]);

        ovrHmd_DestroyDistortionMesh(&distortionMesh);

        return ogreMesh;
    }

    void SetUVScaleOffset(Ogre::GpuProgramParametersSharedPtr params, Eye eye)
    {
        Ogre::Vector2 uvScale(uvScaleOffset_[eye][0].x, uvScaleOffset_[eye][0].y);
        Ogre::Vector2 uvOffset(uvScaleOffset_[eye][1].x, uvScaleOffset_[eye][1].y);

        params->setNamedConstant("EyeToSourceUVScale", &uvScale.x, 2, 2);
        params->setNamedConstant("EyeToSourceUVOffset", &uvOffset.x, 2, 2);
    }

    const TimeWarpMatrices& QueryTimewarpMatrices(Eye eyeIndex)
    {
        ovrPosef eyeRenderPose[2]; 
        for (int eyeIndex = 0; eyeIndex < ovrEye_Count; eyeIndex++)
        {
            ovrEyeType eye = hmd_->EyeRenderOrder[eyeIndex];
            eyeRenderPose[eye] = ovrHmd_GetEyePose(hmd_, eye);
        }

        ovrMatrix4f timeWarpMatrices[2];
        ovrHmd_GetEyeTimewarpMatrices(hmd_, (ovrEyeType)eyeIndex, eyeRenderPose[eyeIndex], timeWarpMatrices);

        TimeWarpMatrices matrices;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                matrices.start[i][j] = timeWarpMatrices[0].M[i][j];
                matrices.end[i][j] = timeWarpMatrices[1].M[i][j];
            }
        }

        return matrices;
    }

    const TrackingState& QueryTrackingState()
    {
        ovrTrackingState state = ovrHmd_GetTrackingState(hmd_, ovr_GetTimeInSeconds());
        if (state.StatusFlags & (ovrStatus_CameraPoseTracked | ovrStatus_PositionTracked | ovrStatus_OrientationTracked))
        {
            ovrPosef pose = state.HeadPose.ThePose;
            currentTrackingState_.SetState(pose);
        }

        /*for (int eyeIndex = 0; eyeIndex < 2; ++eyeIndex)
        {
            ovrEyeType eye = hmd_->EyeRenderOrder[eyeIndex];
            eyeRenderPose_[eyeIndex] = ovrHmd_GetEyePose(hmd_, eye);
        }*/

        return currentTrackingState_;
    }

    Ogre::Matrix4 LeftEyeProjection()
    {
        return EyeProjection(0);
    }

    Ogre::Matrix4 RightEyeProjection()
    {
        return EyeProjection(1);
    }

    Ogre::Vector3 LeftEyeAdjust()
    {
        return Ogre::Vector3(
            -eyeRenderDesc_[EYE_LEFT].ViewAdjust.x * 0.5,
            eyeRenderDesc_[EYE_LEFT].ViewAdjust.y,
            eyeRenderDesc_[EYE_LEFT].ViewAdjust.z
        );
    }

    Ogre::Vector3 RightEyeAdjust()
    {
        return Ogre::Vector3(
            -eyeRenderDesc_[EYE_RIGHT].ViewAdjust.x * 0.5f,
            eyeRenderDesc_[EYE_RIGHT].ViewAdjust.y,
            eyeRenderDesc_[EYE_RIGHT].ViewAdjust.z
        );
    }

    void Recenter()
    {
        if (!hmd_)
            return;

        ovrHmd_RecenterPose(hmd_);
    }

    void BeginFrame()
    {
        PROFILE(Oculus_Begin_Frame);

        ovrHmd_BeginFrameTiming(hmd_, 0); 
        // Retrieve data useful for handling the Health and Safety Warning - unused, but here for reference
        ovrHSWDisplayState hswDisplayState;
        ovrHmd_GetHSWDisplayState(hmd_, &hswDisplayState);
    }

    void EndFrame()
    {
        PROFILE(Oculus_End_Frame);

        ovrHmd_EndFrameTiming(hmd_);
    }

private:
    Ogre::Matrix4 EyeProjection(int eyeIndex)
    {
        ovrMatrix4f projection = ovrMatrix4f_Projection(eyeRenderDesc_[eyeIndex].Fov, 0.1f, 2000.0f, true);
        Ogre::Matrix4 result;

        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                result[i][j] = projection.M[i][j];

        return result;
    }

    bool useDirectMode;

    ovrSizei resolution_;

    ovrRecti eyeViewports_[2];

    ovrFovPort eyeFov_[2];
    ovrEyeRenderDesc eyeRenderDesc_[2];

    ovrPosef eyeRenderPose_[2];
    ovrVector2f uvScaleOffset_[2][2];

    TrackingState currentTrackingState_;
    ovrHmd hmd_;

    OgreRenderer::RendererPtr renderer_;
    Framework* framework_;
};

RocketOculusManager::RocketOculusManager(RocketPlugin* plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    oculus_(0),
    enabled_(false),
    lobbyInitialized_(false),
    oculusOverlay_(0),
    mainCameraEntity_(0),
    headNode_(0),
    mainCamera_(0),
    leftEyeCamera_(0),
    rightEyeCamera_(0),
    leftEyeViewport_(0),
    rightEyeViewport_(0),
    uiMeshEntity_(0),
    leftEyeMeshEntity_(0),
    rightEyeMeshEntity_(0),
    uiMeshNode_(0),
    leftMeshNode_(0),
    rightMeshNode_(0),
    oculusRenderTarget_(0),
    lastConnectionState_(0)
{
    QTimer::singleShot(1000, this, SLOT(OnShowRiftAvailableNotification()));

    renderer_ = framework_->Module<OgreRenderingModule>()->Renderer();
    if (!renderer_.get())
        return;

    oculus_ = new OculusMembers(framework_, renderer_);
    if (!oculus_->HasDevice())
        return;

    ConfigData config("adminotech", "clientplugin");
    enabled_ = framework_->Config()->Read(config, "oculus enabled", false).toBool();
    if (!enabled_)
        return;

    LoadSettings();

    connect(renderer_.get(), SIGNAL(MainCameraChanged(Entity*)), this, SLOT(OnActiveCameraChanged(Entity*)));
    connect(renderer_.get(), SIGNAL(PreRenderFrame()), this, SLOT(PreRenderFrame()));
    connect(renderer_.get(), SIGNAL(SwapBuffers()), this, SLOT(SwapBuffers()));
    connect(renderer_.get(), SIGNAL(PostRenderFrame()), this, SLOT(PostRenderFrame()));

    // Mouse events
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(MouseEventReceived(MouseEvent*)), this, SLOT(OnMouseEvent(MouseEvent*)), Qt::UniqueConnection);
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)), Qt::UniqueConnection);

    // Connect to update signal
    connect(framework_->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdated(float)), Qt::UniqueConnection);

    // Hook to connection changed
    connect(plugin_, SIGNAL(ConnectionStateChanged(bool)), this, SLOT(OnConnectionStateChanged(bool)));

    // Configure rendering
    oculus_->ConfigureRendering();

    CreateMeshes();

    // Create oculus texture
    CreateOculusTexture();

    // Setup lobby for Oculus rendering
    SetupLobby();
}

RocketOculusManager::~RocketOculusManager()
{
    disconnect(plugin_, SIGNAL(ConnectionStateChanged(bool)), this, SLOT(OnConnectionStateChanged(bool)));

    SAFE_DELETE(oculus_);
}

bool RocketOculusManager::HasDevice() const
{
    return (oculus_ ? oculus_->HasDevice() : false);
}

bool RocketOculusManager::IsEnabled() const
{
    return enabled_ && oculus_->HasDevice();
}

void RocketOculusManager::SetupLobby()
{
    Ogre::SceneManager* sceneManager = Ogre::Root::getSingletonPtr()->getSceneManager("DefaultEmptyScene");
    if (!sceneManager)
        return;

    CreateOculusUI(sceneManager);

    Ogre::TexturePtr uiTexture = Ogre::TextureManager::getSingletonPtr()->getByName("");

    Ogre::MaterialPtr uiMaterial = Ogre::MaterialManager::getSingletonPtr()->getByName("meshmoon/OculusRiftUi");
    if (!uiMaterial.get())
        return;

    Ogre::Pass *matPass = uiMaterial->getTechnique(0)->getPass(0);
    matPass->setLightingEnabled(false);

    uiMaterialParams_ = matPass->getFragmentProgramParameters();

    // Texture unit 0 for main rendering
    Ogre::TextureUnitState *textureUnit = 0;
    if (matPass->getNumTextureUnitStates() > 0)
        textureUnit = matPass->getTextureUnitState(0);
    else
        textureUnit = matPass->createTextureUnitState();
    textureUnit->setTextureName("MainWindow RTT");

    Ogre::Camera* camera = sceneManager->createCamera("OculusLobbyMainCamera");
    if (!camera)
        return;

    camera->setNearClipDistance(0.1f);
    camera->setFarClipDistance(100.0f);

    Ogre::RenderWindow* renderWindow = renderer_->CurrentRenderWindow();
    Ogre::Viewport* viewPort = renderWindow->addViewport(camera, 1);
    viewPort->setVisibilityMask(0x0000000F);

    headNode_ = sceneManager->getRootSceneNode()->createChildSceneNode();
    CreateOculusCameras(sceneManager, headNode_);

    CreateOculusMeshEntities(sceneManager, sceneManager->getRootSceneNode());

    if (Ogre::OverlayManager::getSingleton().hasOverlayElement("MainWindow Overlay Panel"))
        Ogre::OverlayManager::getSingleton().getOverlayElement("MainWindow Overlay Panel")->hide();

    lobbyInitialized_ = true;
}

void RocketOculusManager::DestroyLobby()
{
    Ogre::SceneManager* sceneManager = Ogre::Root::getSingletonPtr()->getSceneManager("DefaultEmptyScene");
    if (!sceneManager)
        return;

    try
    {
        // Destroy head node
        if (headNode_)
        {
            headNode_->detachAllObjects();
            sceneManager->destroySceneNode(headNode_);
            headNode_ = 0;
        }

        DestroyOculusUI(sceneManager);

        if (sceneManager->hasCamera("OculusLobbyMainCamera"))
        {
            Ogre::Camera* camera = sceneManager->getCamera("OculusLobbyMainCamera");
            camera->detachFromParent();
            sceneManager->destroyCamera(camera);
        }

        Ogre::RenderWindow* renderWindow = renderer_->CurrentRenderWindow();
        if (renderWindow)
        {
            if (renderWindow->hasViewportWithZOrder(1))
                renderWindow->removeViewport(1);
        }

        DestroyOculusCameras(sceneManager);
        DestroyOculusMeshEntities(sceneManager);
    }
    catch(const Ogre::Exception& e)
    {
        LogError("RocketOculusManager::OnActiveCameraChanged: " + std::string(e.what()));
    }

    /*Ogre::Overlay *oculusOverlay = Ogre::OverlayManager::getSingleton().getByName("OculusRiftOverlay");
    if (oculusOverlay)
        Ogre::OverlayManager::getSingleton().destroy(oculusOverlay);*/

    renderer_->MainViewport()->setAutoUpdated(true);
    mainCamera_ = 0;
    lobbyInitialized_ = false;
}

void RocketOculusManager::CreateMeshes()
{
    uiMesh_ = CreateUIMesh(1920, 1080);
    leftEyeMesh_ = oculus_->CreateDistortionMesh(OculusMembers::EYE_LEFT);
    rightEyeMesh_ = oculus_->CreateDistortionMesh(OculusMembers::EYE_RIGHT);
}

void RocketOculusManager::CreateOculusTexture()
{
    oculusTexture_ = Ogre::TextureManager::getSingleton().createManual("OculusRiftTexture",
        RocketPlugin::MESHMOON_RESOURCE_GROUP,
        Ogre::TEX_TYPE_2D, 1920, 1080,
        0, Ogre::PF_R8G8B8, Ogre::TU_RENDERTARGET);

    oculusRenderTarget_ = oculusTexture_->getBuffer(0)->getRenderTarget();
}

void RocketOculusManager::CreateOculusCameras(Ogre::SceneManager* ogreSceneManager, Ogre::SceneNode* parentNode)
{
    if (!ogreSceneManager)
        return;

    // Create cameras for Oculus rift.
    leftEyeCamera_ = ogreSceneManager->createCamera("OculusRiftLeftEye");
    leftEyeCamera_->setCustomProjectionMatrix(true, oculus_->LeftEyeProjection());
    leftEyeCamera_->setPosition(oculus_->LeftEyeAdjust());
    
    Ogre::Quaternion leftOrientation = Ogre::Quaternion::IDENTITY;
    leftOrientation.FromAngleAxis(Ogre::Degree(1.0f), Ogre::Vector3(0, 1, 0));
    leftEyeCamera_->setOrientation(leftOrientation);

    rightEyeCamera_ = ogreSceneManager->createCamera("OculusRiftRightEye");
    rightEyeCamera_->setCustomProjectionMatrix(true, oculus_->RightEyeProjection());
    rightEyeCamera_->setPosition(oculus_->RightEyeAdjust());

    Ogre::Quaternion rightOrientation = Ogre::Quaternion::IDENTITY;
    rightOrientation.FromAngleAxis(Ogre::Degree(-1.0f), Ogre::Vector3(0, 1, 0));
    rightEyeCamera_->setOrientation(rightOrientation);

    // Attach cameras to the rendertarget
    leftEyeViewport_ = oculusRenderTarget_->addViewport(leftEyeCamera_, 0);
    leftEyeViewport_->setDimensions(0.0f, 0.0f, 0.5f, 1.0f);
    leftEyeViewport_->setAutoUpdated(!oculus_->UseDirectMode());
    leftEyeViewport_->setOverlaysEnabled(false);
    leftEyeViewport_->setSkiesEnabled(true);
    leftEyeViewport_->setShadowsEnabled(false);
    leftEyeViewport_->setVisibilityMask(0xFFFFFF00);

    rightEyeViewport_ = oculusRenderTarget_->addViewport(rightEyeCamera_, 1);
    rightEyeViewport_->setDimensions(0.5f, 0.0f, 0.5f, 1.0f);
    rightEyeViewport_->setAutoUpdated(!oculus_->UseDirectMode());
    rightEyeViewport_->setOverlaysEnabled(false);
    rightEyeViewport_->setSkiesEnabled(true);
    rightEyeViewport_->setShadowsEnabled(false);
    rightEyeViewport_->setVisibilityMask(0xFFFFFF00);

    parentNode->attachObject(leftEyeCamera_);
    parentNode->attachObject(rightEyeCamera_);
}

void RocketOculusManager::DestroyOculusCameras(Ogre::SceneManager* ogreSceneManager)
{
    if (!ogreSceneManager)
        return;

    // Destroy cameras
    if (leftEyeCamera_)
    {
        leftEyeCamera_->detachFromParent();
        ogreSceneManager->destroyCamera(leftEyeCamera_);
        leftEyeCamera_ = 0;
        leftEyeViewport_ = 0;
    }
    if (rightEyeCamera_)
    {
        rightEyeCamera_->detachFromParent();
        ogreSceneManager->destroyCamera(rightEyeCamera_);
        rightEyeCamera_ = 0;
        rightEyeViewport_ = 0;
    }

    //Remove all viewports from rendertarget
    if (oculusRenderTarget_)
        oculusRenderTarget_->removeAllViewports();
}

void RocketOculusManager::DestroyOculusMeshEntities(Ogre::SceneManager* ogreSceneManager)
{
    if (!ogreSceneManager)
        return;

    if (leftMeshNode_)
    {
        leftMeshNode_->detachAllObjects();
        ogreSceneManager->destroySceneNode(leftMeshNode_);
        leftMeshNode_ = 0;
    }
    if (leftEyeMeshEntity_)
    {
        ogreSceneManager->destroyEntity(leftEyeMeshEntity_);
        leftEyeMeshEntity_ = 0;
    }

    if (rightMeshNode_)
    {
        rightMeshNode_->detachAllObjects();
        ogreSceneManager->destroySceneNode(rightMeshNode_);
        rightMeshNode_ = 0;
    }
    if (rightEyeMeshEntity_)
    {
        ogreSceneManager->destroyEntity(rightEyeMeshEntity_);
        rightEyeMeshEntity_ = 0;
    }
}

void RocketOculusManager::CreateOculusMeshEntities(Ogre::SceneManager* ogreSceneManager, Ogre::SceneNode* parentNode)
{
    if (!ogreSceneManager)
        return;

    // Create left eye entity
    if (!oculus_->UseDirectMode())
    {
        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName("meshmoon/OculusRiftOverlayLeft");
        Ogre::Pass *matPass = material->getTechnique(0)->getPass(0);
        matPass->setLightingEnabled(false);

        Ogre::GpuProgramParametersSharedPtr leftParams = matPass->getVertexProgramParameters();

        // Texture unit 0 for main rendering
        Ogre::TextureUnitState *textureUnit = 0;
        if (matPass->getNumTextureUnitStates() > 0)
            textureUnit = matPass->getTextureUnitState(0);
        else
            textureUnit = matPass->createTextureUnitState();
        textureUnit->setTexture(oculusTexture_);

        // Create entity
        leftEyeMeshEntity_ = ogreSceneManager->createEntity(leftEyeMesh_);
        leftEyeMeshEntity_->setMaterialName("meshmoon/OculusRiftOverlayLeft");
        leftEyeMeshEntity_->setVisible(true);
        leftEyeMeshEntity_->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);
        leftEyeMeshEntity_->setVisibilityFlags(0x0000000F);
        leftEyeMeshEntity_->setCastShadows(false);

        // Create scenenode
        leftMeshNode_ = parentNode->createChildSceneNode("OculusLeftEyeSceneNode");
        leftMeshNode_->setPosition(0.0f, 0.0f, 0.0f);
        leftMeshNode_->setOrientation(Ogre::Quaternion::IDENTITY);
        leftMeshNode_->setScale(1, 1, 1);
        leftMeshNode_->attachObject(leftEyeMeshEntity_);
        leftMeshNode_->setVisible(true);

        // Update material params
        oculus_->SetUVScaleOffset(leftParams, OculusMembers::EYE_LEFT);
    }

    // Create right eye entity
    if (!oculus_->UseDirectMode())
    {
        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName("meshmoon/OculusRiftOverlayRight");
        Ogre::Pass *matPass = material->getTechnique(0)->getPass(0);
        matPass->setLightingEnabled(false);

        Ogre::GpuProgramParametersSharedPtr rightParams = matPass->getVertexProgramParameters();

        // Texture unit 0 for main rendering
        Ogre::TextureUnitState *textureUnit = 0;
        if (matPass->getNumTextureUnitStates() > 0)
            textureUnit = matPass->getTextureUnitState(0);
        else
            textureUnit = matPass->createTextureUnitState();
        textureUnit->setTexture(oculusTexture_);

        // Create entity
        Ogre::MeshPtr rightEyeMesh = oculus_->CreateDistortionMesh(OculusMembers::EYE_RIGHT);
        rightEyeMeshEntity_ = ogreSceneManager->createEntity(rightEyeMesh_);
        rightEyeMeshEntity_->setMaterialName("meshmoon/OculusRiftOverlayRight");
        rightEyeMeshEntity_->setVisible(true);
        rightEyeMeshEntity_->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY);
        rightEyeMeshEntity_->setVisibilityFlags(0x0000000F);
        rightEyeMeshEntity_->setCastShadows(false);

        // Create scenenode
        rightMeshNode_ = parentNode->createChildSceneNode("OculusRightEyeSceneNode");
        rightMeshNode_->setPosition(0.0f, 0.0f, 0.0f);
        rightMeshNode_->setOrientation(Ogre::Quaternion::IDENTITY);
        rightMeshNode_->setScale(1, 1, 1);
        rightMeshNode_->attachObject(rightEyeMeshEntity_);
        rightMeshNode_->setVisible(true);
        // Update material params

        oculus_->SetUVScaleOffset(rightParams, OculusMembers::EYE_RIGHT);
    }
}

void RocketOculusManager::CreateOculusUI(Ogre::SceneManager* ogreSceneManager)
{
    uiMeshNode_ = ogreSceneManager->getRootSceneNode()->createChildSceneNode();
    if (!uiMeshNode_)
        return;

    uiMeshEntity_ = ogreSceneManager->createEntity(uiMesh_);
    uiMeshNode_->attachObject(uiMeshEntity_);
    uiMeshNode_->setPosition(0, 0, -0.75);
    uiMeshEntity_->setMaterialName("meshmoon/OculusRiftUi");
    uiMeshEntity_->setVisibilityFlags(0xFFFFFFF0);
}

void RocketOculusManager::DestroyOculusUI(Ogre::SceneManager* ogreSceneManager)
{
    if (uiMeshNode_)
    {
        uiMeshNode_->detachAllObjects();
        ogreSceneManager->destroySceneNode(uiMeshNode_);
        uiMeshNode_ = 0;
    }

    if (uiMeshEntity_)
    {
        ogreSceneManager->destroyEntity(uiMeshEntity_);
        uiMeshEntity_ = 0;
    }
}

void RocketOculusManager::OnConnectionStateChanged(bool connectedToServer)
{
    if (connectedToServer && !lastConnectionState_)
    {
        DestroyLobby();

        Scene* mainScene = renderer_->MainCameraScene();
        if (mainScene)
        {
            connect(mainScene, SIGNAL(EntityCreated(Entity*, AttributeChange::Type)), this, SLOT(OnEntityCreated(Entity*, AttributeChange::Type)), Qt::UniqueConnection);
            connect(mainScene, SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)), this, SLOT(OnComponentAdded(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
        }
    }
    else if(!connectedToServer && lastConnectionState_)
    {
        SetupLobby();

        Scene* mainScene = renderer_->MainCameraScene();
        if (mainScene)
        {
            disconnect(mainScene, SIGNAL(EntityCreated(Entity*, AttributeChange::Type)), this, SLOT(OnEntityCreated(Entity*, AttributeChange::Type)));
            disconnect(mainScene, SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)), this, SLOT(OnComponentAdded(Entity*, IComponent*, AttributeChange::Type)));
        }
    }
    lastConnectionState_ = connectedToServer;
}

void RocketOculusManager::OnActiveCameraChanged(Entity* newMainWindowCamera)
{
    if (!newMainWindowCamera)
    {
        DestroyOculusDisplay();
        SetupLobby();
        return;
    }

    if(!oculus_ || !oculus_->HasDevice())
        return;

    mainCameraEntity_ = newMainWindowCamera;

    //oculus_->Attach();
    SetupOculusDisplay();
}

void RocketOculusManager::SetupOculusDisplay()
{
    if (lobbyInitialized_)
        DestroyLobby();

    DestroyOculusDisplay();

    if (oculus_->UseDirectMode())
        renderer_->SetAutoSwapBuffersEnabled(false);

    OgreWorldWeakPtr ogreWorld = renderer_->MainCameraScene()->Subsystem<OgreWorld>();
    Ogre::SceneManager *ogreSceneManager = (!ogreWorld.expired() ? ogreWorld.lock()->OgreSceneManager() : 0);
    if (!ogreSceneManager)
        return;

    shared_ptr<EC_Placeable> placeable = mainCameraEntity_->Component<EC_Placeable>();
    shared_ptr<EC_Camera> camera = mainCameraEntity_->Component<EC_Camera>();
    if (!placeable || !camera)
        return;

    Ogre::Camera* ogreCamera = camera->OgreCamera();
    if (!ogreCamera)
        return;

    Ogre::SceneNode* placeableNode = placeable->OgreSceneNode();

    headNode_ = placeableNode->createChildSceneNode("OculusHeadNode");
    CreateOculusCameras(ogreSceneManager, headNode_);

    if (Ogre::OverlayManager::getSingleton().hasOverlayElement("MainWindow Overlay Panel"))
        Ogre::OverlayManager::getSingleton().getOverlayElement("MainWindow Overlay Panel")->hide();

    CreateOculusMeshEntities(ogreSceneManager, placeableNode);

    CreateOculusUI(ogreSceneManager);
    uiMeshNode_->getParentSceneNode()->removeChild(uiMeshNode_);
    uiMeshNode_->setPosition(0, 0.1, uiMeshNode_->getPosition().z);
    placeableNode->addChild(uiMeshNode_);

    // Render only oculus meshes in main camera
    ogreCamera->getViewport()->setVisibilityMask(0x0000000F);
    ogreCamera->getViewport()->setAutoUpdated(false);

    // Disable sky
    ogreCamera->getViewport()->setSkiesEnabled(false);
    ogreCamera->getViewport()->setBackgroundColour(Ogre::ColourValue::Black);

    ogreCamera->setCustomProjectionMatrix(true, leftEyeCamera_->getProjectionMatrix());
    mainCamera_ = ogreCamera;
}

/**
    Illuminati was here.
*/

void RocketOculusManager::DestroyOculusDisplay()
{
    //renderer_->SetAutoSwapBuffersEnabled(true);

    // Disconnect from update signal
    //disconnect(framework_->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdated(float)));

    /*if (Ogre::OverlayManager::getSingleton().hasOverlayElement("MainWindow Overlay Panel"))
            Ogre::OverlayManager::getSingleton().getOverlayElement("MainWindow Overlay Panel")->show();*/

    OgreWorldWeakPtr ogreWorld = renderer_->MainCameraScene()->Subsystem<OgreWorld>();
    Ogre::SceneManager *ogreSceneManager = (!ogreWorld.expired() ? ogreWorld.lock()->OgreSceneManager() : 0);
    if (!ogreSceneManager)
        return; // We shouldn't be here...

    try
    {
        // Destroy head node
        if (headNode_)
        {
            headNode_->detachAllObjects();
            ogreSceneManager->destroySceneNode(headNode_);
            headNode_ = 0;
        }

        DestroyOculusCameras(ogreSceneManager);
        DestroyOculusMeshEntities(ogreSceneManager);
        DestroyOculusUI(ogreSceneManager);
    }
    catch(const Ogre::Exception& e)
    {
        LogError("RocketOculusManager::OnActiveCameraChanged: " + std::string(e.what()));
    }

    /*Ogre::Overlay *oculusOverlay = Ogre::OverlayManager::getSingleton().getByName("OculusRiftOverlay");
    if (oculusOverlay)
        Ogre::OverlayManager::getSingleton().destroy(oculusOverlay);*/

    renderer_->MainViewport()->setAutoUpdated(true);
    mainCamera_ = 0;
}

void RocketOculusManager::OnUpdated(float /* elapsedTime */)
{
}

void RocketOculusManager::UpdateMaterialParams(const Ogre::Matrix4& eyeRotationStart, const Ogre::Matrix4& eyeRotationEnd, int eye)
{
    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(eye == 0 ? "meshmoon/OculusRiftOverlayLeft" : "meshmoon/OculusRiftOverlayRight");
    if (!material.get())
        return;

    Ogre::GpuProgramParametersSharedPtr params = material->getTechnique(0)->getPass(0)->getVertexProgramParameters();

    try
    {
        params->setNamedConstant("EyeRotationStart", eyeRotationStart);
        params->setNamedConstant("EyeRotationEnd", eyeRotationEnd);
    }
    catch (const Ogre::Exception& e)
    {}
}

void RocketOculusManager::PreRenderFrame()
{
    if (!oculus_ || !oculus_->HasDevice())
        return;

    oculus_->BeginFrame();

    //QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
}

void RocketOculusManager::SwapBuffers()
{
    if (!headNode_)
        return;

    TrackingState state = oculus_->QueryTrackingState();

    headNode_->setPosition(state.position);
    headNode_->setOrientation(state.orientation);

    //mainCamera_->setOrientation(state.orientation);

    if (!oculus_ || !oculus_->HasDevice() || !headNode_ || !leftEyeViewport_ || !rightEyeViewport_ || !mainCamera_)
        return;

    PROFILE(Oculus_Update_Eyes);
    leftEyeViewport_->update();
    rightEyeViewport_->update();
    ELIFORP(Oculus_Update_Eyes);

    PROFILE(Oculus_Update_MainView);
    mainCamera_->getViewport()->update();
    ELIFORP(Oculus_Update_MainView);

    if (oculus_->UseDirectMode())
        leftEyeViewport_->getTarget()->swapBuffers(Ogre::Root::getSingletonPtr()->getRenderSystem()->getWaitForVerticalBlank());
}

void RocketOculusManager::PostRenderFrame()
{
    if (!oculus_ || !oculus_->HasDevice())
        return;

    oculus_->EndFrame();
}

void RocketOculusManager::OnKeyPress(KeyEvent* e)
{
    if (!e || !enabled_ || !oculus_->HasDevice())
        return;

    QKeySequence recenterOculus = QKeySequence(Qt::ShiftModifier + Qt::Key_R);
    if (e->Sequence() == recenterOculus)
        oculus_->Recenter();
}

void RocketOculusManager::OnWindowResized(int width, int height)
{
    // @TODO. Implement.
}

void RocketOculusManager::OnMouseEvent(MouseEvent* e)
{
    if (!e || !oculus_ || !uiMaterialParams_.get())
        return;

    Ogre::Vector2 mouseToTexCoord(-1.0f, -1.0f);
    //framework_->Input()->SetMouseCursorVisible(false);
    //framework_->App()->changeOverrideCursor(QCursor(Qt::CursorShape::OpenHandCursor));
    
    QSizeF menuBarSize = framework_->Ui()->MainWindow()->menuBar() && framework_->Ui()->MainWindow()->menuBar()->isVisible() ? framework_->Ui()->MainWindow()->menuBar()->geometry().size() : QSizeF(0.f,0.f);
    QSizeF windowSize = framework_->Ui()->MainWindow()->size();

    mouseToTexCoord.x = static_cast<float>(e->x) / windowSize.width();
    mouseToTexCoord.y = static_cast<float>(e->y) / (windowSize.height() - menuBarSize.height());

    uiMaterialParams_->setNamedConstant("MousePosition", &mouseToTexCoord.x, 2, 2);
}
void RocketOculusManager::OnShowRiftAvailableNotification()
{
    if (enabled_ || !oculus_->HasDevice())
        return;

    RocketNotificationWidget* notificationWidget = plugin_->Notifications()->ShowNotification("Oculus Rift is available. You can enable it from settings", ":/images/icon-camera.png");
    /*QPushButton* enableRiftButton = notificationWidget->AddButton(true, "Open settings");

    connect(enableRiftButton, SIGNAL(pressed()), plugin_->Lobby(), SLOT(EditSettingsRequest()));*/

    notificationShowed_ = true;
}

void RocketOculusManager::LoadSettings()
{
    // @TODO. Implement.
}

#define PI 3.14159265358979323846

Ogre::MeshPtr RocketOculusManager::CreateUIMesh(int width, int height)
{
    struct ProceduralVertex
    {
        float3 position;
        float2 uv;
    };

    int xVertices = 20;
    int vertexCount = xVertices * 2;

    float aspectRatio = (float)width / (float)height;
    float meshHeight = 0.6f;
    float meshWidth = meshHeight * aspectRatio;

    float radius = 1.0f;
    float quarterDist = (radius * 2 * PI) * 0.25f;

    float xOffset = -(meshWidth * 0.5f);
    float vertexDistance = meshWidth / (float)(xVertices - 1);

    ProceduralVertex* vertices = new ProceduralVertex[vertexCount];

    // Generate vertices with position and uv.
    int currentVertex = 0;
    for (int i = 0; i < xVertices; ++i)
    {
        float xPos = xOffset + (vertexDistance * i);

        float xUv = 1.0f - ((xPos + xOffset) / (xOffset * 2));

        // Top vertex
        ProceduralVertex topVertex = {
            float3(xPos, (meshHeight * 0.5f), 0),
            float2(xUv, 0)
        };

        // Bottom vertex
        ProceduralVertex bottomVertex = {
            float3(xPos, -(meshHeight * 0.5f), 0),
            float2(xUv, 1)
        };

        float angle = ((xPos < 0 ? -xPos : xPos) / quarterDist);
        angle = (xPos < 0 ? angle : -angle);
        angle = DegToRad(angle * 90.0f);

        // Rotate vertices
        Quat quat(float3::unitY, angle);

        topVertex.position = quat * topVertex.position;
        bottomVertex.position = quat * bottomVertex.position;

        vertices[currentVertex + 0] = topVertex;
        vertices[currentVertex + 1] = bottomVertex;

        currentVertex += 2;
    }

    // Quad count
    int quadCount = xVertices - 1;
    int indexCount = (quadCount * 2) * 3;

    short* indices = new short[indexCount];

    // Generate indices
    int firstIndex = 0;
    for (int i = 0; i < indexCount; i += 3)
    {
        if ((i % 2) == 0)
        {
            indices[i + 0] = firstIndex + 0;
            indices[i + 1] = firstIndex + 1;
            indices[i + 2] = firstIndex + 2;
        }
        else
        {
            indices[i + 0] = firstIndex + 2;
            indices[i + 1] = firstIndex + 1;
            indices[i + 2] = firstIndex + 0;
        }

        firstIndex = indices[i + 1];
    }

    QFile objFile("uiMesh.obj");
    if (!objFile.open(QIODevice::WriteOnly))
        return Ogre::MeshPtr();

    QTextStream stream(&objFile);
    stream << "o Ui\n";

    for (int i = 0; i < vertexCount; ++i)
        stream << "v " << vertices[i].position.x << " " << vertices[i].position.y << " " << vertices[i].position.z << "\n";

    for (int i = 0; i < indexCount; i += 3)
    {
        short index0 = indices[i + 0] + 1;
        short index1 = indices[i + 1] + 1;
        short index2 = indices[i + 2] + 1;
        stream << "f " << index0 << " " << index1 << " " << index2 << "\n";
    }
    objFile.close();

    Ogre::MeshPtr ogreMesh = Ogre::MeshManager::getSingleton().createManual("OculusUIMesh", RocketPlugin::MESHMOON_RESOURCE_GROUP);
    Ogre::SubMesh* subMesh = ogreMesh->createSubMesh();

    ogreMesh->sharedVertexData = new Ogre::VertexData();
    ogreMesh->sharedVertexData->vertexCount = vertexCount;

    // Create vertex declaration
    Ogre::VertexDeclaration* vertexDeclaration = ogreMesh->sharedVertexData->vertexDeclaration;

    size_t offset = 0;

    // Position
    vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION, 0);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT3);

    // Texcoord
    vertexDeclaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0);
    offset += Ogre::VertexElement::getTypeSize(Ogre::VET_FLOAT2);

    // Allocate vertexbuffer
    Ogre::HardwareVertexBufferSharedPtr vertexBuffer = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(offset, vertexCount, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);

    // Set vertex buffer binding
    Ogre::VertexBufferBinding* binding = ogreMesh->sharedVertexData->vertexBufferBinding;
    binding->setBinding(0, vertexBuffer);

    // Upload vertex data to GPU
    vertexBuffer->writeData(0, vertexBuffer->getSizeInBytes(), &vertices[0].position.x, true);

    // Allocate index buffer
    Ogre::HardwareIndexBufferSharedPtr indexBuffer = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(Ogre::HardwareIndexBuffer::IT_16BIT, indexCount, Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);

    // Upload the index data to the GPU
    indexBuffer->writeData(0, indexBuffer->getSizeInBytes(), indices, true);

    // Set parameters of the submesh
    subMesh->useSharedVertices = true;
    subMesh->indexData->indexBuffer = indexBuffer;
    subMesh->indexData->indexCount = indexCount;
    subMesh->indexData->indexStart = 0;

    // Set bounding information (for culling);
    Ogre::AxisAlignedBox box(-1, -1, -1, 1, 1, 1);
    ogreMesh->_setBounds(box);
    ogreMesh->_setBoundingSphereRadius(3.0f);

    // Load mesh
    ogreMesh->load();

    return ogreMesh;
}

void RocketOculusManager::OnEntityCreated(Entity* entity, AttributeChange::Type change)
{
    if (!entity)
        return;

    if (!entity->Name().startsWith("Avatar"))
        return;

    QString connectionId = entity->Name().mid(6);

    bool ok = false;
    int connId = connectionId.toInt(&ok);
    if (!ok)
        return;

    TundraLogic::Client *client = framework_->GetModule<TundraLogic::TundraLogicModule>()->GetClient().get();
    if (!client)
        return;

    if (connId != client->ConnectionId())
        return;

    shared_ptr<EC_Placeable> placeable = static_pointer_cast<EC_Placeable>(entity->Component<EC_Placeable>());
    if (!placeable)
        return;

    placeable->visible.Set(false, AttributeChange::LocalOnly);
}

void RocketOculusManager::OnComponentAdded(Entity* entity, IComponent* comp, AttributeChange::Type change)
{
    if (!entity || !comp)
        return;

    EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(comp);
    if (mesh)
    {
        connect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnMeshChanged()), Qt::UniqueConnection);
    }
}

void RocketOculusManager::OnMeshChanged()
{
    QObject* sender = QObject::sender();
    if (!sender)
        return;

    EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(sender);
    if (!mesh)
        return;

    Ogre::Entity* ogreEntity = mesh->OgreEntity();
    if (!ogreEntity)
        return;

    ogreEntity->setVisibilityFlags(OCULUS_ENTITY_MASK);
}

#else

RocketOculusManager::RocketOculusManager(RocketPlugin* plugin) { }

RocketOculusManager::~RocketOculusManager() { }

bool RocketOculusManager::HasDevice() const
{
    return false;
}

bool RocketOculusManager::IsEnabled() const
{
    return false;
}

void RocketOculusManager::PreRenderFrame() { }
void RocketOculusManager::SwapBuffers()  { }
void RocketOculusManager::PostRenderFrame() { }

void RocketOculusManager::OnConnectionStateChanged(bool /* connectedToServer */)  { }
void RocketOculusManager::OnActiveCameraChanged(Entity* /* newMainWindowCamera */)  { }

void RocketOculusManager::OnKeyPress(KeyEvent* /* e */)  { }
void RocketOculusManager::OnMouseEvent(MouseEvent* /* e */)  { }
void RocketOculusManager::OnWindowResized(int /* width */, int  /* height */)  { }

void RocketOculusManager::OnShowRiftAvailableNotification()  { }
    
void RocketOculusManager::OnEntityCreated(Entity* entity, AttributeChange::Type change)  { }
void RocketOculusManager::OnComponentAdded(Entity* entity, IComponent* comp, AttributeChange::Type change)  { }
void RocketOculusManager::OnMeshChanged()  { }

void RocketOculusManager::OnUpdated(float /* elapsedTime */) { }

#endif
