/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.cpp
    @brief  The Meshmoon Rocket client plugin. */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketPlugin.h"

#include "MeshmoonBackend.h"
#include "MeshmoonAssetLibrary.h"
#include "MeshmoonUser.h"
#include "MeshmoonWorld.h"

#include "RocketNetworking.h"
#include "RocketLobbyWidget.h"
#include "RocketOfflineLobby.h"
#include "RocketLayersWidget.h"
#include "RocketTaskbar.h"
#include "RocketMenu.h"
#include "RocketAvatarEditor.h"
#include "RocketSounds.h"
#include "RocketAssetMonitor.h"
#include "RocketSettings.h"
#include "RocketReporter.h"
#include "RocketNotifications.h"

#include "buildmode/RocketBuildEditor.h"
#include "buildmode/RocketBuildWidget.h"
#include "presis/RocketPresis.h"
#include "cave/RocketCaveManager.h"
#include "updater/RocketUpdater.h"
#include "storage/MeshmoonStorage.h"
#include "occlusion/RocketOcclusionManager.h"
#include "oculus/RocketOculusManager.h"
#include "utils/RocketFileSystem.h"
#include "rendering/RocketGPUProgramGenerator.h"

#include "common/script/MeshmoonScriptTypeDefines.h"
#include "RocketScriptTypeDefines.h"
#include "MeshmoonComponents.h"

#include "Framework.h"
#include "Application.h"
#include "LoggingFunctions.h"
#include "CoreDefines.h"
#include "IRenderer.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"
#include "UiProxyWidget.h"
#include "TundraLogicModule.h"
#include "KristalliProtocolModule.h"
#include "UserConnectedResponseData.h"
#include "MsgClientJoined.h"
#include "MsgClientLeft.h"
#include "Client.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"
#include "EC_Mesh.h"
#include "EC_MeshmoonTeleport.h"
#include "InputAPI.h"
#include "ConsoleAPI.h"
#include "Math/float3.h"
#include "Renderer.h"
#include "OgreRenderingModule.h"
#include "OgreWorld.h"
#include "OgreMaterialAsset.h"
#include "SceneStructureModule.h"
#include "EC_Camera.h"
#include "JavascriptModule.h"

#include <QPixmap>
#include <QAction>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPushButton>
#include <QDesktopServices>

#include <OgreResourceGroupManager.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreGpuProgramManager.h>
#include <OgreGpuProgramParams.h>

#include <kNet/PolledTimer.h>

#include "MemoryLeakCheck.h"

const std::string RocketPlugin::MESHMOON_RESOURCE_GROUP = "Meshmoon";

RocketPlugin::RocketPlugin() :
    IModule("RocketPlugin"),
    LC("[Rocket]: "),
    avatarDetection_(true),
    noPermissionNotified_(false),
    scenePreviewWidget_(0),
    backend_(0),
    lobby_(0),
    offlineLobby_(0),
    layersWidget_(0),
    taskbar_(0),
    menu_(0),
    avatarEditor_(0),
    user_(0),
    world_(0),
    sounds_(0),
    settings_(0),
    assetMonitor_(0),
    updater_(0),
    storage_(0),
    networking_(0),
    presis_(0),
    caveManager_(0),
    buildEditor_(0),
    library_(0),
    reporter_(0),
    notifications_(0),
    occlusionManager_(0),
    oculusManager_(0),
    fileSystem_(0),
    currentView_(MainView)
{
    // Register custom types
    qRegisterMetaType<MeshmoonLibrarySource>("MeshmoonLibrarySource");
    qRegisterMetaType<Meshmoon::SceneLayer>("Meshmoon::SceneLayer");
    qRegisterMetaType<QList<Meshmoon::SceneLayer> >("QList<Meshmoon::SceneLayer>"); /// @todo Remove once all script facing stuff is using Meshmoon::SceneLayerList?
    qRegisterMetaType<Meshmoon::SceneLayerList >("Meshmoon::SceneLayerList");
}

RocketPlugin::~RocketPlugin()
{
}

void RocketPlugin::Initialize()
{
    // Don't do anything if headless or a server.
    if (framework_->IsHeadless() || framework_->HasCommandLineParameter("--server"))
    {
        LogError(LC + "This plugin cannot be ran in headless or server mode.");
        return;
    }

    InitializeRendering();

    framework_->RegisterDynamicObject("adminotech", this); /**< @todo This will be removed when all the scripts have been migrated to use "rocket" instead. */
    framework_->RegisterDynamicObject("rocket", this);
    
    // Hook to client connected/disconnected signals.
    TundraLogic::Client *client = GetTundraClient();
    if (client)
    {
        connect(client, SIGNAL(AboutToConnect()), this, SLOT(OnAboutToConnect()));
        connect(client, SIGNAL(Connected(UserConnectedResponseData*)), this, SLOT(OnConnected()));
        connect(client, SIGNAL(Disconnected()), this, SLOT(OnDisconnected()));
        connect(client, SIGNAL(LoginFailed(const QString&)), this, SLOT(OnConnectionFailed(const QString&)));
    }
    else
        LogError(LC + "Could not find TundraLogicModule, cannot do full functionality.");

    // Hook custom Meshmoon networking messages.
    KristalliProtocolModule *kristalli = framework_->Module<KristalliProtocolModule>();
    if (!connect(kristalli, SIGNAL(NetworkMessageReceived(kNet::MessageConnection *, kNet::packet_id_t, kNet::message_id_t, const char *, size_t)), 
            this, SLOT(HandleKristalliMessage(kNet::MessageConnection*, kNet::packet_id_t, kNet::message_id_t, const char*, size_t))))
    {
        LogError(LC + "Failed to hook to custom kNet protocol messages. Functionality will be limited!");
    }

    // Hook scene creation
    connect(framework_->Scene(), SIGNAL(SceneAdded(const QString&)), SLOT(OnSceneCreated(const QString&)), Qt::UniqueConnection);

    /// @note Don't change the init order here if you are not sure what you are doing.
    networking_     = new RocketNetworking(this);
    settings_       = new RocketSettings(this);
    storage_        = new MeshmoonStorage(this);
    library_        = new MeshmoonAssetLibrary(this);
    backend_        = new MeshmoonBackend(this);
    world_          = new MeshmoonWorld(this);
    user_           = new MeshmoonUser();
    
    sounds_         = new RocketSounds(this);
    notifications_  = new RocketNotifications(this);
    menu_           = new RocketMenu(this);
    reporter_       = new RocketReporter(this);
    presis_         = new RocketPresis(this);
    lobby_          = new RocketLobby(this);
    offlineLobby_   = new RocketOfflineLobby(this);
    avatarEditor_   = new RocketAvatarEditor(this);
    layersWidget_   = new RocketLayersWidget(this);
    assetMonitor_   = new RocketAssetMonitor(this);
    buildEditor_    = new RocketBuildEditor(this);
    caveManager_    = new RocketCaveManager(this);
    updater_        = new RocketUpdater(this);
    fileSystem_     = new RocketFileSystem(this);
    
    occlusionManager_ = new RocketOcclusionManager(this);
    oculusManager_    = new RocketOculusManager(this);

    // Portal widget
    connect(lobby_, SIGNAL(LogoutRequest()), backend_, SLOT(Unauthenticate()));
    connect(lobby_, SIGNAL(LogoutRequest()), storage_, SLOT(Unauthenticate()));
    connect(lobby_, SIGNAL(LogoutRequest()), this, SLOT(OnCancelLogin()));

    connect(lobby_, SIGNAL(EditProfileRequest()), this, SLOT(SetProfileEditorVisible()));
    connect(lobby_, SIGNAL(EditSettingsRequest()), this, SLOT(SetSettingsVisible()));

    connect(lobby_, SIGNAL(UpdateServersRequest()), backend_, SLOT(RequestServers()));
    connect(lobby_, SIGNAL(UpdateServersRequest()), this, SLOT(RestoreMainView()));
    connect(lobby_, SIGNAL(UpdatePromotedRequest()), backend_, SLOT(RequestPromoted()));

    connect(lobby_, SIGNAL(StartupRequest(const Meshmoon::Server&)), backend_, SLOT(RequestStartup(const Meshmoon::Server&)));
    connect(lobby_, SIGNAL(DisconnectRequest()), this, SLOT(OnCancelLogin()));
    connect(lobby_, SIGNAL(CancelRequest(const Meshmoon::Server&)), this, SLOT(OnCancelLogin()));
    connect(lobby_, SIGNAL(DirectLoginRequest(const QString&, unsigned short, const QString&, const QString&, const QString&)), 
        client, SLOT(Login(const QString&, unsigned short, const QString&, const QString&, const QString&)));

    // Backend
    connect(backend_, SIGNAL(WebpageLoadStarted(QGraphicsWebView*, const QUrl&)), lobby_, SLOT(OnWebpageLoadStarted(QGraphicsWebView*, const QUrl&)));
    connect(backend_, SIGNAL(Response(const QUrl&, const QByteArray&, int, const QString&)), this, SLOT(OnBackendResponse(const QUrl&, const QByteArray&, int, const QString&)));
    connect(backend_, SIGNAL(FilterRequest(QStringList)), this, SLOT(OnFilterRequest(QStringList)));
    connect(backend_, SIGNAL(LoaderMessage(const QString&)), this, SLOT(OnStartLoader(const QString&)));
    connect(backend_, SIGNAL(ExtraLoginQuery(Meshmoon::EncodedQueryItems)), this, SLOT(OnExtraLoginQuery(Meshmoon::EncodedQueryItems)));

    // Auth
    connect(backend_, SIGNAL(AuthCompleted(const Meshmoon::User&)), this, SLOT(OnAuthenticated(const Meshmoon::User&)));
    connect(backend_, SIGNAL(AuthReset()), this, SLOT(OnAuthReset()));

    // Avatar editor and Presis
    connect(lobby_, SIGNAL(OpenAvatarEditorRequest()), this, SLOT(SetAvatarEditorVisible()));
    connect(lobby_, SIGNAL(OpenPresisRequest()), this, SLOT(SetPresisVisible()));

    // Sounds
    connect(lobby_, SIGNAL(PlaySoundRequest(const QString&)), sounds_, SLOT(PlaySound(const QString&)));

    // Auto hide for UI if no avatar is found.
    fallBackHide_.setSingleShot(true);
    connect(&fallBackHide_, SIGNAL(timeout()), SLOT(Hide()));

    // Delayed resize signal.
    resizeDelayTimer_.setSingleShot(true);
    resizeDelayTimer_.setInterval(50);
    connect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), &resizeDelayTimer_, SLOT(start()));
    connect(&resizeDelayTimer_, SIGNAL(timeout()), SLOT(OnDelayedResize()));

    // Media load delay
    mediaLoadDelay_.setSingleShot(true);
    connect(&mediaLoadDelay_, SIGNAL(timeout()), SLOT(LoadPersistentMedia()));
    
    // Admino comps teleport request from EC_MeshmoonTeleport
    MeshmoonComponents *meshmoonComponents = framework_->Module<MeshmoonComponents>();
    if (meshmoonComponents)
    {
        connect(meshmoonComponents, SIGNAL(TeleportRequest(const QString&, const QString&, const QString&)),
            this, SLOT(OnTeleportRequest(const QString&, const QString&, const QString&)));
    }

    // Boot up!
    const QStringList loginParams = framework_->CommandLineParameters("--login");
    if (loginParams.empty() && !framework_->HasCommandLineParameter("--file"))
    {
        // Start auth process if no login url is given on startup.
        lobby_->Show();
        backend_->Authenticate();
    }
    else
    {
        // --login
        if (!loginParams.empty())
        {
            bool rocketLaunch = backend_->ProcessLoginUrl(loginParams.first());

            // No animations for showing main ui, start loader widget.
            lobby_->Show(false);
            if (!rocketLaunch)
                lobby_->StartLoader("Connecting to server...");

            // Show shortcuts that would otherwise be tied to AuthCompleted signal.
            avatarEditor_->SetShortcutVisible(true);
        }
        // --file
        else
        {           
            QString style = QString("QPushButton { background-color: %1; color: white; border: 0px; border-radius: 2px; font-size: 18px; font-family: 'Arial'; padding: 8px; } QPushButton:hover { background-color: %2; }")
                    .arg(Meshmoon::Theme::ColorWithAlpha(Meshmoon::Theme::Blue, 200)).arg(Meshmoon::Theme::BlueStr);
            
            scenePreviewWidget_ = new QFrame();
            scenePreviewWidget_->setObjectName("parentFrame");
            scenePreviewWidget_->setContentsMargins(0, 0, 0, 0);
            scenePreviewWidget_->setFrameShape(QFrame::NoFrame);
            scenePreviewWidget_->setStyleSheet("QFrame#parentFrame { background-color: transparent; border: 0px; }");
            
            QHBoxLayout *l = new QHBoxLayout();
            l->setContentsMargins(10, 10, 10, 10);
            l->setSpacing(10);
            
            QPushButton *buttonLobby = new QPushButton("Rocket Lobby");        
            buttonLobby->setStyleSheet(style);
            connect(buttonLobby, SIGNAL(clicked()), this, SLOT(OnReturnToLobby()), Qt::QueuedConnection);
            
            QPushButton *buttonExit = new QPushButton("Exit Rocket");
            buttonExit->setStyleSheet(style);
            connect(buttonExit, SIGNAL(clicked()), framework_, SLOT(Exit()), Qt::QueuedConnection);
            
            l->addWidget(buttonExit);
            l->addWidget(buttonLobby);
            scenePreviewWidget_->setLayout(l);
            
            UiProxyWidget *proxy = framework_->Ui()->AddWidgetToScene(scenePreviewWidget_, Qt::Widget);
            proxy->setPos(0, 0);
            proxy->setVisible(true);
        }
    }

    // Menus
    QAction *actSettings = new QAction(QIcon(":/images/icon-settings.png"), tr("Settings"), this);
    connect(actSettings, SIGNAL(triggered()), this, SLOT(SetSettingsVisible()));
    menu_->AddAction(actSettings, tr("Utilities"));
    
    QAction *actCache = new QAction(QIcon(":/images/icon-recycle.png"), tr("Clear Asset Cache"), this);
    connect(actCache, SIGNAL(triggered()), assetMonitor_, SLOT(OnClearAssetCache()));
    menu_->AddAction(actCache, tr("Utilities"));
    
    QAction *actUpdate = new QAction(QIcon(":/images/icon-update.png"), tr("Check Updates"), this);
    connect(actUpdate, SIGNAL(triggered()), updater_, SLOT(CheckUpdates()));
    menu_->AddAction(actUpdate, tr("Utilities"));

    if (framework_->HasCommandLineParameter("--rocket-no-update"))
        actUpdate->setEnabled(false);
    
    // Load startup settings. This emits the signal that listening parties will react to.
    settings_->EmitSettings();
}

void RocketPlugin::InitializeRendering()
{
    if (framework_->IsHeadless())
        return;
    
    OgreRenderingModule* renderingModule = framework_->Module<OgreRenderingModule>();
    OgreRendererPtr renderer = (renderingModule != 0 ? renderingModule->Renderer() : OgreRendererPtr());
    if (!renderer)
        return;
        
    kNet::PolledTimer t;
    t.Start();

    // Add Meshmoon resource location
    QDir meshmoonDir(Application::InstallationDirectory() + "media/meshmoon");
    if (!meshmoonDir.exists())
    {
        LogError(LC + "Meshmoon resource location does not exist: " + meshmoonDir.absolutePath());
        return;
    }

    Ogre::ResourceGroupManager::getSingleton().createResourceGroup(MESHMOON_RESOURCE_GROUP, true);

    // Load everything except materials before shaders.
    QList<QString> resourceLocations;
    resourceLocations << meshmoonDir.absolutePath()
                      << meshmoonDir.absoluteFilePath("meshes")
                      << meshmoonDir.absoluteFilePath("textures")
                      << meshmoonDir.absoluteFilePath("programs")
                      << meshmoonDir.absoluteFilePath("scripts");

    switch (renderer->ShadowQuality())
    {
        case OgreRenderer::Renderer::Shadows_Off:
            resourceLocations << meshmoonDir.absoluteFilePath("scripts/shadows_off");
            break;
        case OgreRenderer::Renderer::Shadows_High:
            resourceLocations << meshmoonDir.absoluteFilePath("scripts/shadows_high");
            break;
        default:
            resourceLocations << meshmoonDir.absoluteFilePath("scripts/shadows_low");
            break;
    }

    foreach(const QString &location, resourceLocations)
        Ogre::ResourceGroupManager::getSingleton().addResourceLocation(location.toStdString(), "FileSystem", MESHMOON_RESOURCE_GROUP);

    LogDebug(LC + QString("Meshmoon media locations added in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
    t.Start();

    // Create meshmoon shaders.
    RocketGPUProgramGenerator::CreateShaders(framework_);
    LogDebug(LC + QString("Meshmoon shaders generated in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
    t.Start();

    // Load materials now that shaders are available.
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(meshmoonDir.absoluteFilePath("materials").toStdString(), "FileSystem", MESHMOON_RESOURCE_GROUP);

    Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup(MESHMOON_RESOURCE_GROUP);
    LogDebug(LC + QString("Meshmoon resources initialized in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
    t.Start();

    /** @todo Make this smarter in core. We should be able to tell extra resource locations to Renderer before Ogre is initialized. 
        Now we do resource locations our selves and we need to call RenderingCreateInstancingShaders() even if its already done once in Renderer. */    
    RenderingCreateInstancingShaders();
    LogDebug(LC + QString("Meshmoon instanced shaders generated in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
    t.Start();

    LoadPersistentMedia();
}

void RocketPlugin::LoadPersistentMedia()
{
    // Tundra unloads everything from Ogre when disconnected from a server.
    // This function handles loading Ogre resources that should always be present.

    // Default material
    OgreRenderer::Renderer::DefaultMaterialName = "";

    OgreMaterialAssetPtr matAsset = framework_->Asset()->FindAsset<OgreMaterialAsset>("Ogre Media:MeshmoonEmptyMaterial.material");
    if (!matAsset.get() || !matAsset->IsLoaded())
    {
        AssetTransferPtr transfer = framework_->Asset()->RequestAsset("Ogre Media:MeshmoonEmptyMaterial.material");
        if (transfer.get())
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(LoadPersistentMedia()), Qt::UniqueConnection);
    }
    else
    {
        // Set out material to be used if no material is set to a submesh.
        // Hook to the unloaded signal as delayed to reload the asset back.
        LogDebug(LC + "Setting Renderer default material to '" + matAsset->ogreAssetName + "'");
        OgreRenderer::Renderer::DefaultMaterialName = matAsset->ogreAssetName.toStdString();
        connect(matAsset.get(), SIGNAL(Unloaded(IAsset*)), this, SLOT(ReloadPersistentMedia()), Qt::UniqueConnection);
    }

    // Error material
    OgreRenderer::Renderer::ErrorMaterialName = "";

    matAsset = framework_->Asset()->FindAsset<OgreMaterialAsset>("Ogre Media:MeshmoonErrorMaterial.material");
    if (!matAsset.get() || !matAsset->IsLoaded())
    {
        AssetTransferPtr transfer = framework_->Asset()->RequestAsset("Ogre Media:MeshmoonErrorMaterial.material");
        if (transfer.get())
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(LoadPersistentMedia()), Qt::UniqueConnection);
    }
    else
    {
        // Set out material to be used if no material is set to a submesh.
        // Hook to the unloaded signal as delayed to reload the asset back.
        LogDebug(LC + "Setting Renderer error material to '" + matAsset->ogreAssetName + "'");
        OgreRenderer::Renderer::ErrorMaterialName = matAsset->ogreAssetName.toStdString();
        connect(matAsset.get(), SIGNAL(Unloaded(IAsset*)), this, SLOT(ReloadPersistentMedia()), Qt::UniqueConnection);
    }
}

void RocketPlugin::ReloadPersistentMedia()
{
    if (mediaLoadDelay_.isActive())
        mediaLoadDelay_.stop();
    mediaLoadDelay_.start(500);
}

void RocketPlugin::RenderingCreateInstancingShaders()
{
    QStringList dontClone;
    dontClone << "meshmoon/MultiDiffShadowVP";

    Ogre::ResourceManager::ResourceMapIterator shader_iter = ((Ogre::ResourceManager*)Ogre::HighLevelGpuProgramManager::getSingletonPtr())->getResourceIterator();
    while(shader_iter.hasMoreElements())
    {
        Ogre::ResourcePtr resource = shader_iter.getNext();
        if (!resource.get())
            continue;
        if (resource->getGroup() != MESHMOON_RESOURCE_GROUP)
            continue;
            
        Ogre::HighLevelGpuProgram* program = dynamic_cast<Ogre::HighLevelGpuProgram*>(resource.get());
        if (program)
        {    
            if (program->getType() == Ogre::GPT_VERTEX_PROGRAM && program->getLanguage() == "cg")
            {
                QString name = QString::fromStdString(program->getName());
                if (!name.contains("instanced", Qt::CaseInsensitive) && !name.contains("instancing", Qt::CaseInsensitive))
                {
                    try
                    {
                        // Check if Renderer has already cloned this shader, as it has for most!
                        Ogre::ResourcePtr existingClone = Ogre::HighLevelGpuProgramManager::getSingletonPtr()->getByName(program->getName() 
                            + "/Instanced", resource->getGroup());
                        if (existingClone.get())
                            continue;

                        // Check if we are ignoring this shader for now that wont compile. Will give warning 
                        // for user when he enabled instancing for this particular shader.
                        if (dontClone.contains(name))
                            continue;

                        // Create the clone.
                        Ogre::HighLevelGpuProgram* cloneProgram = Ogre::HighLevelGpuProgramManager::getSingletonPtr()->createProgram(program->getName()
                            + "/Instanced", resource->getGroup(), "cg", Ogre::GPT_VERTEX_PROGRAM).get();
                        if (cloneProgram)
                        {
                            cloneProgram->setSourceFile(program->getSourceFile());
                            cloneProgram->setParameter("profiles", program->getParameter("profiles"));
                            cloneProgram->setParameter("entry_point", program->getParameter("entry_point"));
                            cloneProgram->setParameter("compile_arguments", program->getParameter("compile_arguments") + " -DINSTANCING");
                            cloneProgram->load();

                            Ogre::GpuProgramParametersSharedPtr srcParams = program->getDefaultParameters();
                            Ogre::GpuProgramParametersSharedPtr destParams = cloneProgram->getDefaultParameters();
                            destParams->copyMatchingNamedConstantsFrom(*srcParams);

                            // Add viewproj matrix parameter for SuperShader
                            if (destParams->_findNamedConstantDefinition("viewProjMatrix"))
                                destParams->setNamedAutoConstant("viewProjMatrix", Ogre::GpuProgramParameters::ACT_VIEWPROJ_MATRIX);

                            // If doing shadow mapping, map the lightViewProj matrices to not include world transform
                            if (destParams->_findNamedConstantDefinition("lightViewProj0"))
                                destParams->setNamedAutoConstant("lightViewProj0", Ogre::GpuProgramParameters::ACT_TEXTURE_VIEWPROJ_MATRIX, 0);
                            if (destParams->_findNamedConstantDefinition("lightViewProj1"))
                                destParams->setNamedAutoConstant("lightViewProj1", Ogre::GpuProgramParameters::ACT_TEXTURE_VIEWPROJ_MATRIX, 1);
                            if (destParams->_findNamedConstantDefinition("lightViewProj2"))
                                destParams->setNamedAutoConstant("lightViewProj2", Ogre::GpuProgramParameters::ACT_TEXTURE_VIEWPROJ_MATRIX, 2);
                        }
                        else
                            LogError(LC + "Could not clone vertex program " + name + " for instancing");
                    }
                    catch (Ogre::Exception& /*e*/)
                    {
                        LogError(LC + "Could not clone vertex program " + name + " for instancing");
                    }
                }
            }
        }
    }
}

void RocketPlugin::Uninitialize()
{
    RemoveTaskbar();

    SAFE_DELETE(scenePreviewWidget_);
    SAFE_DELETE(networking_);
    SAFE_DELETE(layersWidget_);
    SAFE_DELETE(avatarEditor_);
    SAFE_DELETE(storage_);
    SAFE_DELETE(sounds_);
    SAFE_DELETE(lobby_);
    SAFE_DELETE(offlineLobby_);
    SAFE_DELETE(backend_);
    SAFE_DELETE(assetMonitor_);
    SAFE_DELETE(updater_);
    SAFE_DELETE(menu_);
    SAFE_DELETE(user_);
    SAFE_DELETE(world_);
    SAFE_DELETE(settings_);
    SAFE_DELETE(presis_);
    SAFE_DELETE(reporter_);
    SAFE_DELETE(notifications_);
    SAFE_DELETE(caveManager_);
    SAFE_DELETE(buildEditor_);
    SAFE_DELETE(library_);
    SAFE_DELETE(fileSystem_);
    SAFE_DELETE(occlusionManager_);
    SAFE_DELETE(oculusManager_);
}

// Public slots

void RocketPlugin::SetProgressMessage(const QString &message)
{
    if (framework_->IsHeadless())
        return;
        
    // If any script update arrives, assume calling party will hide the UI once ready.
    if (fallBackHide_.isActive())
        fallBackHide_.stop();
        
    SetProgressMessageInternal(message);
}

void RocketPlugin::SetAvatarDetection(bool enabled)
{
    if (framework_->IsHeadless())
        return;

    // If av detection is disabled, assume calling party will hide the UI once ready.
    if (fallBackHide_.isActive())
        fallBackHide_.stop();
        
    if (avatarDetection_ == enabled)
        return;
            
    avatarDetection_ = enabled;
    HookAvatarDetection(avatarDetection_, true);
}

bool RocketPlugin::IsConnectedToServer() const
{
    TundraLogic::Client *client = GetTundraClient();
    return (client ? client->IsConnected() : false);
}

void RocketPlugin::Show(bool animate, uint msecDelay)
{
    if (!framework_->IsHeadless() && lobby_)
    {
        currentView_ = MainView;

        bool wasVisible = lobby_->isVisible();
        lobby_->Show(animate);
        if (!wasVisible)
            emit VisibilityChanged(true);
        
        if (!backend_->IsAuthenticated())
        {
            if (!animate)
                backend_->Authenticate();
            else
                QTimer::singleShot(msecDelay, backend_, SLOT(Authenticate()));
        }
        else
        {
            if (lobby_->NumServers() == 0)
                backend_->RequestServers();
            if (lobby_->NumPromoted() == 0)
                backend_->RequestPromoted();
            if (lobby_->NumServers() > 0 && lobby_->NumPromoted() > 0)
                lobby_->ApplyCurrentFilterAndSorting();
        }
    }
}

void RocketPlugin::ShowCenterContent()
{
    currentView_ = MainView;

    if (lobby_)
        lobby_->ShowCenterContent();

    if (!backend_->IsAuthenticated())
        backend_->Authenticate();
}

void RocketPlugin::Hide()
{
    if (fallBackHide_.isActive())
        fallBackHide_.stop();

    if (!framework_->IsHeadless())
    {
        // Hide the full screen main widget.
        if (lobby_)
        {
            bool wasVisible = lobby_->isVisible();
            lobby_->Hide();
            if (wasVisible)
                emit VisibilityChanged(false);
        }
            
        // Close settings
        if (settings_ && settings_->IsVisible())
            settings_->Close();

        // Show taskbar only if a script has not already called Hide()
        // on the taskbar indicating that the scene wants it hidden.
        if (taskbar_ && !taskbar_->HasHideRequest())
            taskbar_->Show();
    }
    
    // Unhook avatar detection as its only there for automatic hiding of the main widget.
    HookAvatarDetection(false, true);
}

void RocketPlugin::HideCenterContent()
{
    if (lobby_)
        lobby_->HideCenterContent();
}

RocketTaskbar *RocketPlugin::Taskbar() const
{
    return taskbar_;
}

RocketMenu *RocketPlugin::Menu() const
{
    return menu_;
}

MeshmoonUser *RocketPlugin::User() const
{
    return user_;
}

MeshmoonWorld *RocketPlugin::World() const
{
    return world_;
}

RocketLayersWidget *RocketPlugin::LayersWidget() const
{
    return layersWidget_;
}

RocketSounds *RocketPlugin::Sounds() const
{
    return sounds_;
}

RocketAvatarEditor *RocketPlugin::AvatarEditor() const
{
    return avatarEditor_;
}

MeshmoonStorage* RocketPlugin::Storage() const
{
    return storage_;
}

RocketBuildEditor *RocketPlugin::BuildEditor() const
{
    return buildEditor_;
}

RocketSettings *RocketPlugin::Settings() const
{
    return settings_;
}

MeshmoonAssetLibrary *RocketPlugin::AssetLibrary() const
{
    return library_;
}

RocketNotifications *RocketPlugin::Notifications() const
{
    return notifications_;
}

RocketAssetMonitor *RocketPlugin::AssetMonitor() const
{
    return assetMonitor_;
}

RocketFileSystem *RocketPlugin::FileSystem() const
{
    return fileSystem_;
}

RocketReporter *RocketPlugin::Reporter() const
{
    return reporter_;
}

void RocketPlugin::SetLayerVisibility(const Meshmoon::SceneLayer &layer, bool visible)
{
    SetLayerVisibility(layer.id, visible);
}

void RocketPlugin::SetLayerVisibility(u32 layerId, bool visible)
{
    if (networking_)
        networking_->SendLayerUpdateMsg(layerId, visible);
}

bool RocketPlugin::OpenUrlWithWebBrowser(const QString &url)
{
    if (!notifications_)
        return false;
    if (url.trimmed().isEmpty())
    {
        LogError(LC + "OpenUrlWithWebBrowser: given URL is an empty string: " + url.trimmed());
        return false;
    }

    QUrl checkUrl = QUrl::fromUserInput(url.trimmed());
    if (!checkUrl.isValid())
    {
        LogError(LC + "OpenUrlWithWebBrowser: failed to resolve input as a valid URL: " + url.trimmed());
        return false;
    }
    if (checkUrl.toString().startsWith("file://"))
    {
        LogError(LC + "OpenUrlWithWebBrowser: does not allow to open file:// protocol URLs: " + url.trimmed());
        return false;
    }
    
    // Check if this url is already showing or pending a show.
    foreach (RocketNotificationWidget *existing, notifications_->Notifications())
    {
        QVariant variant = existing ? existing->property("RocketOpenUrlWithWebBrowser") : QVariant();
        if (variant.isValid() && variant.toString() == url.trimmed())
            return true;
    }

    // Don't trim https:// as it might be useful info for the user to make the decision.
    QString prettyUrl = url.trimmed();
    if (prettyUrl.startsWith("http://"))
        prettyUrl = prettyUrl.mid(7);
    if (prettyUrl.length() > 35)
        prettyUrl = prettyUrl.left(35) + "...";

    RocketNotificationWidget *notification = notifications_->ShowNotification(
        QString("The Meshmoon world wants to open a web page <b>%1</b>").arg(prettyUrl), ":/images/server-anon.png");
    if (!notification)
    {
        LogError(LC + "OpenUrlWithWebBrowser: Failed to show confirmation notification to the user.");
        return false;
    }
    notification->setProperty("RocketOpenUrlWithWebBrowser", url.trimmed());

    QPushButton *openButton = notification->AddButton(true, "Allow");
    openButton->setProperty("RocketOpenUrlWithWebBrowser", url.trimmed());
    connect(openButton, SIGNAL(clicked()), SLOT(OnOpenUrlWithWebBrowserAuthorized()));
    notification->AddButton(true, "Ignore");
    return true;
}

void RocketPlugin::OnOpenUrlWithWebBrowserAuthorized()
{
    QPushButton *senderButton = qobject_cast<QPushButton*>(sender());
    if (!senderButton)
        return;
    QString url = senderButton->property("RocketOpenUrlWithWebBrowser").toString();
    if (url.isEmpty())
        return;

    if (!QDesktopServices::openUrl(QUrl::fromUserInput(url.trimmed())))
        LogError(LC + "Failed to open OpenUrlWithWebBrowser initiated URL: " + url);
}

MeshmoonBackend *RocketPlugin::Backend() const
{
    return backend_;
}

RocketNetworking *RocketPlugin::Networking() const
{
    return networking_;
}

RocketLobby *RocketPlugin::Lobby() const
{
    return lobby_;
}

RocketOfflineLobby *RocketPlugin::OfflineLobby() const
{
    return offlineLobby_;
}

RocketOculusManager *RocketPlugin::OculusManager() const
{
    return oculusManager_;
}

// Private

void RocketPlugin::AddTaskbar()
{
    if (!framework_->IsHeadless() && !taskbar_)
    {
        // Create taskbar
        taskbar_ = new RocketTaskbar(framework_, lobby_);
        connect(taskbar_, SIGNAL(DisconnectRequest()), this, SLOT(OnCancelLogin()), Qt::UniqueConnection);

        // Hook asset monitor to update taskbar load state
        if (assetMonitor_)
            assetMonitor_->StartMonitoringForTaskbar(taskbar_);
        
        // Hook all actions added to menu to also add them into top menu.    
        if (menu_)
            connect(taskbar_, SIGNAL(ActionAdded(QAction*)), menu_, SLOT(OnTaskbarActionAdded(QAction*)), Qt::UniqueConnection);
        
        // Update core taskbar actions.
        if (storage_)
            storage_->UpdateTaskbarEntry(taskbar_);
        if (layersWidget_)
            layersWidget_->UpdateTaskbarEntry(taskbar_, !layers_.isEmpty());
        if (buildEditor_)
            buildEditor_->UpdateTaskbarEntry(taskbar_);
    }
}

void RocketPlugin::RemoveTaskbar()
{
    if (!framework_->IsHeadless() && taskbar_)
    {
        if (assetMonitor_)
            assetMonitor_->StopMonitoringForTaskbar();
        if (storage_)
            storage_->UpdateTaskbarEntry(0);
        if (layersWidget_)
            layersWidget_->UpdateTaskbarEntry(0, false);
        if (buildEditor_)
            buildEditor_->UpdateTaskbarEntry(0);

        if (menu_)
            disconnect(taskbar_, SIGNAL(ActionAdded(QAction*)), menu_, SLOT(OnTaskbarActionAdded(QAction*)));
        disconnect(taskbar_, SIGNAL(DisconnectRequest()), this, SLOT(OnCancelLogin()));
        
        SAFE_DELETE(taskbar_);
    }
}

// Private slots

void RocketPlugin::OnAuthenticated(const Meshmoon::User &user)
{
    user_->OnAuthenticated(user);
    lobby_->OnAuthenticated(user);
    avatarEditor_->OnAuthenticated(user);
}

void RocketPlugin::OnAuthReset()
{
    RestoreMainView();

    user_->OnAuthReset();

    JavascriptModule *jsModule = Fw()->Module<JavascriptModule>();
    if (jsModule)
        jsModule->SetDefaultEngineCoreApiAccessEnabled(true);
}

void RocketPlugin::OnFilterRequest(QStringList sceneIds)
{
    filterIds_ = sceneIds;
}

void RocketPlugin::OnStartLoader(const QString &message)
{
    if (lobby_ && lobby_->State() != RocketLobby::Loader && !lobby_->IsLoaderVisible())
        lobby_->StartLoader(message, true, true, true);
    else if (lobby_)
        SetProgressMessageInternal(message);
}

void RocketPlugin::SetProgressMessageInternal(const QString &message, bool stopLoadAnimation)
{
    if (framework_->IsHeadless())
        return;

    if (lobby_ && lobby_->isVisible())
        lobby_->UpdateLogin(message, stopLoadAnimation);
}

void RocketPlugin::OnExtraLoginQuery(Meshmoon::EncodedQueryItems queryItems)
{
    for(int i=0; i<queryItems.size(); ++i)
        SetOrCreateExtraQueryItem(queryItems[i]);
}

void RocketPlugin::OnBackendResponse(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString &error)
{
    // If receiving any backend data, web browser cannot be open any more.
    // Closing the webview panel (auth, edit profile) cannot be detected more intelligently atm.
    if (CurrentView() == WebBrowserView)
        currentView_ = MainView;

    if (url == Meshmoon::UrlServers)
    {
        if (error.isEmpty())
        {
            lobby_->SetServers(Meshmoon::Json::DeserializeServers(data, framework_, filterIds_), CurrentView() == MainView);
            
            // Fetch scores once per Rocket run.
            if (!lobby_->HasServerScores())
                backend_->RequestScores();
        }
        else
            LogError(LC + QString("Failed to request server information: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlUsers)
    {
        if (error.isEmpty())
            lobby_->SetOnlineStats(Meshmoon::Json::DeserializeOnlineStats(data));
        else
            LogError(LC + QString("Failed to request stats information: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlPromoted)
    {
        if (error.isEmpty())
            lobby_->SetPromoted(Meshmoon::Json::DeserializePromoted(data, framework_, lobby_->PromotedRef()), CurrentView() == MainView);
        else
            LogError(LC + QString("Failed to request promoted information: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlNews)
    {
        if (error.isEmpty())
            lobby_->SetNews(Meshmoon::Json::DeserializeNews(data));
        else
            LogError(LC + QString("Failed to request news: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlStats)
    {
        if (error.isEmpty())
            lobby_->SetStats(Meshmoon::Json::DeserializeStats(data));
        else
            LogError(LC + QString("Failed to request stats information: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlScores)
    {
        if (error.isEmpty())
            lobby_->SetScores(Meshmoon::Json::DeserializeScores(data));
        else
            LogError(LC + QString("Failed to request stats information: %1 [%2]").arg(error).arg(httpStatusCode));
    }
    else if (url == Meshmoon::UrlStart)
    {
        if (error.isEmpty())
        {
            Meshmoon::ServerStartReply startReply = Meshmoon::Json::DeserializeServerStart(data);
            if (startReply.loginUrl.isValid())
            {
                // Update loader UI.
                if (lobby_->State() != RocketLobby::Loader && !lobby_->IsLoaderVisible())
                    lobby_->StartLoader("", true, true, true);
                SetProgressMessageInternal("Login information received...");

                // Read response login url and apply our extra query items to it.
                loginUrl_ = startReply.loginUrl;
                SetExtraQueryItems(loginUrl_);

                // If the server was started now, delay actually login by 3 seconds so it has time to boot up.
                QTimer::singleShot(startReply.started ? 3000 : 5, this, SLOT(OnConnect()));
            }
            else
            {
                if (sounds_)
                    sounds_->PlaySound(RocketSounds::SoundErrorBeep);
                    
                SetProgressMessageInternal("Login information not valid, cannot continue login", true);
                LogError(LC + "Login URL is not valid after strict parsing: " + startReply.loginUrl.toString());
            }
        }
        else
        {
            if (sounds_)
                sounds_->PlaySound(RocketSounds::SoundErrorBeep);
                
            // Normal login
            if (!HasExtraQueryItem("teleport"))
            {
                QString structuredError = QString("Failed to request login information: %1 [%2]").arg(error).arg(httpStatusCode);
                LogError(LC + structuredError);
                
                if (httpStatusCode == 500 || httpStatusCode == 404)
                {
                    structuredError = data;
                    QString imagePath = lobby_->GetFocusedImagePath();
                    Notifications()->ShowSplashDialog(structuredError, !imagePath.isEmpty() ? imagePath : ":/images/server-anon.png");
                    OnDisconnected(); // Return to the server list, dialog will show on top.
                }
                else if (httpStatusCode != 401)
                    SetProgressMessageInternal(structuredError, true);
            }
            // Teleporting
            else
            {
                TeleportationFailed(!data.isEmpty() ? data : "Teleportation failed");
                return;
            }
        }
    }
    else if (url == Meshmoon::UrlSupportRequest)
    {
        if (httpStatusCode != 200)
            Notifications()->ShowSplashDialog("Error occurred while sending support request, please try again later.", ":/images/icon-exit.png");
    }
    
    // Status code 401 means our session has expired, we need to reauth.
    if (httpStatusCode == 401)
    {
        SetProgressMessageInternal("Authentication Expired", true);
        Notifications()->ShowSplashDialog("<b>" + tr("Sessions authentication has expired: reauthentication required") + 
            "</b><br><br>" + tr("Expiration may happen if you signed in to any Meshmoon web page with the same credentials while running Rocket."), ":/images/profile-anon.png");
        lobby_->EmitLogoutRequest();
    }
}

void RocketPlugin::SetOrCreateExtraQueryItem(const Meshmoon::EncodedQueryItem &item)
{
    for(int i=0; i<loginQueryItems_.size(); ++i)
    {
        Meshmoon::EncodedQueryItem &existing = loginQueryItems_[i];
        if (existing.first == item.first)
        {
            existing.second = item.second;
            return;
        }
    }
    loginQueryItems_ << item;
}

void RocketPlugin::SetExtraQueryItems(QUrl &url)
{
    if (loginQueryItems_.isEmpty())
        return;

    Meshmoon::EncodedQueryItems queryItems = url.encodedQueryItems();

    for(int i=0; i<loginQueryItems_.size(); ++i)
    {
        Meshmoon::EncodedQueryItem &item = loginQueryItems_[i];
        loginUrl_.removeEncodedQueryItem(item.first);

        bool processed = false;
        for(int k=0; k<queryItems.size(); ++k)
        {
            Meshmoon::EncodedQueryItem &existing = queryItems[k];
            if (existing.first == item.first)
            {
                existing.second = item.second;
                processed = true;
                break;
            }
        } 
        if (!processed)
            queryItems << item;
            
    }
    url.setEncodedQueryItems(queryItems);
}

bool RocketPlugin::HasExtraQueryItem(const QString &key) const
{
    if (loginQueryItems_.isEmpty())
        return false;
    foreach(const Meshmoon::EncodedQueryItem &item, loginQueryItems_)
        if (item.first == key)
            return true;
    return false;
}

void RocketPlugin::HandleKristalliMessage(kNet::MessageConnection* source, kNet::packet_id_t packetId, kNet::message_id_t messageId, const char* data, size_t numBytes)
{
    if (messageId == MsgClientJoined::messageID)
    {
        if (world_)
            world_->ClientJoined(data, numBytes);
    }
    else if (messageId == MsgClientLeft::messageID)
    {
        if (world_)
            world_->ClientLeft(data, numBytes);
    }
}

void RocketPlugin::OnReturnToLobby()
{
    if (!lobby_)
        return;
        
    currentView_ = MainView;

    if (scenePreviewWidget_)
    {
        scenePreviewWidget_->close();
        SAFE_DELETE_LATER(scenePreviewWidget_);
    }
    framework_->Scene()->RemoveScene("TundraServer");

    lobby_->Show();
    backend_->Authenticate();
}

void RocketPlugin::SetSettingsVisible(bool visible)
{
    if (!settings_)
        return;

    if (visible && !settings_->IsVisible())
    {
        SetAvatarEditorVisible(false);
        SetProfileEditorVisible(false);
        SetPresisVisible(false);

        settings_->Open();
        currentView_ = SettingsView;
    }
    else if (!visible && settings_->IsVisible())
        settings_->Close(false, false);
}

void RocketPlugin::SetAvatarEditorVisible(bool visible)
{
    if (!avatarEditor_)
        return;

    if (visible && !avatarEditor_->IsVisible())
    {
        SetSettingsVisible(false);
        SetProfileEditorVisible(false);
        SetPresisVisible(false);

        avatarEditor_->Open();
        currentView_ = AvatarEditorView;
    }
    else if (!visible && avatarEditor_->IsVisible())
        avatarEditor_->Close(false, false);
}

void RocketPlugin::SetProfileEditorVisible(bool visible)
{
    if (!backend_)
        return;

    if (visible)
    {
        SetSettingsVisible(false);
        SetAvatarEditorVisible(false);
        SetPresisVisible(false);
        
        backend_->OpenEditProfile();
        currentView_ = WebBrowserView;
    }
}

void RocketPlugin::SetPresisVisible(bool visible)
{
    if (!presis_)
        return;
           
    if (visible && !presis_->IsVisible())
    {
        SetSettingsVisible(false);
        SetProfileEditorVisible(false);
        SetAvatarEditorVisible(false);
        
        presis_->Open();
        currentView_ = PresisView;
    }
    else if (!visible && presis_->IsVisible())
        presis_->Close(false);
}

void RocketPlugin::RestoreMainView()
{
    SetSettingsVisible(false);
    SetProfileEditorVisible(false);
    SetAvatarEditorVisible(false);
    SetPresisVisible(false);
    currentView_ = MainView;
}

void RocketPlugin::OnBasicUserConnected()
{
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), 
        this, SLOT(OnBasicUserKeyPress(KeyEvent*)), Qt::UniqueConnection);

    // Disable menu items
    menu_->SetActionEnabled("Tools", "Scene", false);
    menu_->SetActionEnabled("Tools", "Assets", false);
    menu_->SetActionEnabled("Tools", "EC Editor", false);

    framework_->Console()->UnregisterCommand("scene");
    framework_->Console()->UnregisterCommand("assets");
    framework_->Console()->UnregisterCommand("savescene");
    framework_->Console()->UnregisterCommand("loadscene");
    framework_->Console()->UnregisterCommand("importscene");
    framework_->Console()->UnregisterCommand("importmesh");
}

void RocketPlugin::OnConnect()
{
    if (!loginUrl_.isValid())
        return;

    TundraLogic::Client *client = GetTundraClient();
    if (client)
    {
        // loginUrl_ will get cleared on disconnect
        QUrl currentLoginUrl = QUrl(loginUrl_);
        if (client->IsConnected())
            client->DoLogout();
        client->Login(currentLoginUrl);
    }
    else
        LogError(LC + "Could not obtain Tundra Client object, cannot continue login.");

    loginQueryItems_.clear();
}

void RocketPlugin::OnCancelLogin()
{
    TundraLogic::Client *client = GetTundraClient();
    if (client && client->LoginState() != TundraLogic::Client::NotConnected)
    {
        client->Logout();
        return;
    }

    if (avatarEditor_ && avatarEditor_->scene() && avatarEditor_->isVisible())
        return;

    OnDisconnected();
}

void RocketPlugin::OnAboutToConnect()
{
    // Ongoing teleport, don't do anything.
    TundraLogic::Client *client = GetTundraClient();
    if (client && client->HasLoginProperty("teleport"))
        return;

    SetProgressMessageInternal(tr("Connecting to server..."));
}

void RocketPlugin::OnConnected()
{
    AddTaskbar();
    
    /** @todo This is not a reliable way to get server information where we are currently loggin into.
        If we are using a Rocket Launcher etc. that does not involve the UI in loggin in! */
    Meshmoon::Server serverInfo = lobby_->GetFocusedServerInfo();
    if (world_)
        world_->SetServer(serverInfo);
    if (reporter_)
        reporter_->SetServerInfo(loginUrl_, serverInfo);

    /** Loading screen hide logic
        1) Avatar is detected and its mesh it loaded. Hidden by RocketPlugin.
        2) All assets are loaded to the system (no avatar), hidden by RocketAssetMonitor.
        3) Fallback hide after 20 seconds. Meaning that there is no avatar (or it did not have time to load)
           and there are so many assets in the scene that they have not loaded in 20 seconds.
        @todo Evaluate the need for 20 sec fallback hide now that we monitor asset completions and screen is
        hidden once all transfers complete or fail. */
    fallBackHide_.start(20000);

    TundraLogic::Client *client = GetTundraClient();
    if (client)
    {
        permissions_.connectionId = client->ConnectionId();
        permissions_.username = client->LoginProperty("username").toString();
        permissions_.userhash = client->LoginProperty("userhash").toString();
        
        if (!client->HasLoginProperty("teleport"))
            SetProgressMessageInternal(tr("Loading 3D space..."));
        
        if (user_)
            user_->SetClientConnectionId(permissions_.connectionId);
        if (backend_)
            backend_->user.connectionId = permissions_.connectionId;
    }

    HookAvatarDetection(avatarDetection_);
    
    emit ConnectionStateChanged(true);
}

void RocketPlugin::OnDisconnected()
{
    /** @todo This is getting out of hand. Refactor this state into a sensible 
        object or do something to make this handling not so horrible */

    // Clear all session data
    layers_.clear();
    permissions_.Reset();
    loginUrl_ = QUrl();

    // Clear user and world
    if (user_)
        user_->Clear();
    if (world_)
        world_->Clear();

    // Boolean if we have showed a no permission notification
    noPermissionNotified_ = false;

    // Stop avatar hide/show detection logic
    avatarDetection_ = true;
    fallBackHide_.stop();

    // Destroy taskbar and unhook signals.
    RemoveTaskbar();

    // Clear and hide layers widget.
    if (layersWidget_)
    {
        if (layersWidget_->isVisible())
            layersWidget_->hide();
        layersWidget_->ClearLayers();
    }

    // Close and reset storage.
    if (storage_)
    {
        storage_->Close();
        storage_->ClearStorageInformation();
    }

    // Close build editor.
    if (buildEditor_ && buildEditor_->BuildWidget())
        buildEditor_->BuildWidget()->Hide();

    // Close all notifications.
    if (notifications_)
        notifications_->CloseNotifications();

    // Are we teleporting?    
    bool aboutToTeleport = HasExtraQueryItem("teleport");

    // Authenticate and update server view.
    if (lobby_)
    {
        lobby_->Show(false);
        if (aboutToTeleport)
            lobby_->StartLoader("Teleporting...", true, false, true);
    }

    // Don't reset auth if we are about to teleport
    if (!aboutToTeleport)
    {
        if (backend_)
        {
            backend_->user.connectionId = 0;
            backend_->Authenticate();
        }

        // We need to reload sounds as forget all assets was called on disconnect!
        if (sounds_)
            QTimer::singleShot(250, sounds_, SLOT(PlayErrorBeep()));
    }

    // Always do this to reset the ui/console command handling.
    OnBasicUserDisconnect();

    JavascriptModule *jsModule = Fw()->Module<JavascriptModule>();
    if (jsModule)
        jsModule->SetDefaultEngineCoreApiAccessEnabled(true);

    emit ConnectionStateChanged(false);
}

void RocketPlugin::OnBasicUserDisconnect()
{
    // Remove basic user input events
    disconnect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnBasicUserKeyPress(KeyEvent*)));

    // Enable menu items
    menu_->SetActionEnabled("Tools", "Scene", true);
    menu_->SetActionEnabled("Tools", "Assets", true);
    menu_->SetActionEnabled("Tools", "EC Editor", true);

    // Get list of current commands
    const ConsoleAPI::CommandMap &commands = framework_->Console()->Commands();
    ConsoleAPI::CommandMap::const_iterator it;

    // Restore SceneStructureModule console commands
    SceneStructureModule* sceneStructureModule = framework_->Module<SceneStructureModule>();
    if (sceneStructureModule)
    {
        it = commands.find("scene");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("scene", "Shows the Scene Structure window, hides it if it's visible.", 
                sceneStructureModule, SLOT(ToggleSceneStructureWindow()));

        it = commands.find("assets");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("assets", "Shows the Assets window, hides it if it's visible.", 
                sceneStructureModule, SLOT(ToggleAssetsWindow()));
    }
    
    // Restore TundraLogicModule console commands
    TundraLogicModule *tundraLogicModule = framework_->Module<TundraLogicModule>();
    if (tundraLogicModule)
    {
        it = commands.find("saveScene");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("saveScene", "Saves scene into XML or binary. Usage: saveScene(filename,asBinary=false,saveTemporaryEntities=false,saveLocalEntities=true)", 
                tundraLogicModule, SLOT(SaveScene(QString, bool, bool, bool)), SLOT(SaveScene(QString)));

        it = commands.find("loadscene");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("loadScene", "Loads scene from XML or binary. Usage: loadScene(filename,clearScene=true,useEntityIDsFromFile=true)", 
                tundraLogicModule, SLOT(LoadScene(QString, bool, bool)));

        it = commands.find("importScene");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("importScene", "Loads scene from a dotscene file. Optionally clears the existing scene. Replace-mode can be optionally disabled. Usage: importScene(filename,clearScene=false,replace=true)", 
                tundraLogicModule, SLOT(ImportScene(QString, bool, bool)), SLOT(ImportScene(QString)));

        it = commands.find("importMesh");
        if (it == commands.end())
            framework_->Console()->RegisterCommand("importMesh", "Imports a single mesh as a new entity. Position, rotation, and scale can be specified optionally. Usage: importMesh(filename, pos = 0 0 0, rot = 0 0 0, scale = 1 1 1, inspectForMaterialsAndSkeleton=true)", 
                tundraLogicModule, SLOT(ImportMesh(QString, const float3 &, const float3 &, const float3 &, bool)), SLOT(ImportMesh(QString)));
    }
}

void RocketPlugin::OnConnectionFailed(const QString &reason)
{
    Notifications()->ShowSplashDialog(reason, ":/images/icon-exit.png");
}

void RocketPlugin::OnTeleportRequest(const QString &sceneId, const QString &pos, const QString &rot)
{
    if (!backend_)
        return;

    loginQueryItems_.clear();
    if (backend_->RequestStartup(sceneId))
    {
        SetOrCreateExtraQueryItem(Meshmoon::EncodedQueryItem("teleport", "true"));
        if (!pos.isEmpty())
            SetOrCreateExtraQueryItem(Meshmoon::EncodedQueryItem("pos", pos.toUtf8()));
        if (!rot.isEmpty())
            SetOrCreateExtraQueryItem(Meshmoon::EncodedQueryItem("rot", rot.toUtf8()));
    }
    else
        TeleportationFailed(backend_->user.hash.isEmpty() ? "Authentication is required for teleportation" : "Teleportation failed");
}

void RocketPlugin::OnEntityCreated(Entity *entity, AttributeChange::Type change)
{
    const QString avatarName = QString("Avatar%1").arg(User()->ConnectionId());
    if (entity->Name() == avatarName)
    {
        if (fallBackHide_.isActive())
            fallBackHide_.stop();

        EC_Mesh *mesh = entity->GetComponent<EC_Mesh>().get();
        if (mesh)
        {
            if (mesh->HasMesh())
            {
                OnAvatarMeshCreated();
                return;
            }
            else
                connect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnAvatarMeshCreated()), Qt::UniqueConnection);
        }
        else
        {
            connect(entity, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), 
                this, SLOT(OnEntityComponentCreated(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
        }
        
        HookAvatarDetection(false, false);
    }
}

void RocketPlugin::OnSceneCreated(const QString &name)
{
    ScenePtr scene = framework_->Scene()->SceneByName(name);
    if (scene.get())
        connect(scene.get(), SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)),
            SLOT(OnSceneComponentCreated(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
}

void RocketPlugin::OnSceneComponentCreated(Entity *entity, IComponent* component, AttributeChange::Type change)
{
    if (component && component->TypeId() == EC_Camera::TypeIdStatic())
    {
        EC_Camera *camera = dynamic_cast<EC_Camera*>(component);
        if (camera && settings_)
            camera->farPlane.Set(static_cast<float>(settings_->ViewDistance()), AttributeChange::LocalOnly);
    }
}

void RocketPlugin::OnEntityComponentCreated(IComponent* component, AttributeChange::Type change)
{
    if (component->TypeName() != EC_Mesh::TypeNameStatic())
        return;
        
    const QString avatarName = QString("Avatar%1").arg(User()->ConnectionId());
    if (component->ParentEntity() && component->ParentEntity()->Name() == avatarName)
    {
        EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(component);
        if (mesh)
        {
            if (mesh->HasMesh())
                OnAvatarMeshCreated();
            else
                connect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnAvatarMeshCreated()), Qt::UniqueConnection);
        }
        else
        {
            LogWarning(LC + "Failed to cast IComponent to EC_Mesh, assuming avatar created.");
            OnAvatarMeshCreated();
        }
    }
}

void RocketPlugin::OnAvatarMeshCreated()
{
    HookAvatarDetection(false, true);
    QTimer::singleShot(1, this, SLOT(Hide()));
}

void RocketPlugin::OnScriptEngineCreated(QScriptEngine *engine)
{
    RegisterRocketPluginMetaTypes(engine);
    RegisterMeshmoonCommonMetaTypes(engine);
}

void RocketPlugin::OnBasicUserKeyPress(KeyEvent *e)
{
    if (!e || e->eventType != KeyEvent::KeyPressed || e->IsRepeat())
        return;

    InputAPI &input = *framework_->Input();
    const QKeySequence showSceneStruct = input.KeyBinding("ShowSceneStructureWindow", QKeySequence(Qt::ShiftModifier + Qt::Key_S));
    const QKeySequence showAssets = input.KeyBinding("ShowAssetsWindow", QKeySequence(Qt::ShiftModifier + Qt::Key_A));
    const QKeySequence showEcEditor = input.KeyBinding("ShowECEditor", QKeySequence(Qt::ShiftModifier + Qt::Key_E));
    
    if (e->sequence == showEcEditor)
    {
        LogWarning(LC + "You don't have permissions to access Entity Editor in this scene.");
        e->Suppress();
    }
    else if (e->Sequence()== showSceneStruct)
    {
        LogWarning(LC + "You don't have permissions to access Scene Editor in this scene.");
        e->Suppress();
    }
    else if (e->Sequence() == showAssets)
    {
        LogWarning(LC + "You don't have permissions to access Assets Window in this scene.");
        e->Suppress();
    }
}

void RocketPlugin::OnDelayedResize()
{
    if (framework_ && !framework_->IsHeadless() && framework_->Ui()->GraphicsScene())
    {
        QRectF sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
        emit SceneResized(sceneRect);
    }
}

void RocketPlugin::TeleportationFailed(const QString &message)
{
    loginQueryItems_.clear();

    OgreRenderingModule* renderingModule = framework_->Module<OgreRenderingModule>();
    OgreRendererPtr renderer = (renderingModule != 0 ? renderingModule->Renderer() : OgreRendererPtr());
    if (!renderer || !renderer->MainCamera() || !renderer->MainCamera()->ParentScene())
        return;

    std::vector<shared_ptr<EC_MeshmoonTeleport> > teleports = renderer->MainCamera()->ParentScene()->Components<EC_MeshmoonTeleport>();
    for(size_t i = 0; i < teleports.size(); ++i)
        if (teleports[i]->IsShowingDialog())
            teleports[i]->TeleportationFailed(message);
}

void RocketPlugin::HookAvatarDetection(bool enabled, bool unhookComponents)
{
    if (framework_->IsHeadless() || !backend_)
        return;
    
    TundraLogicModule *tundraModule = framework_->Module<TundraLogicModule>();
    if (tundraModule && tundraModule->GetClient().get())
    {
        if (tundraModule->GetClient()->IsConnected())
        {
            Scene *scene = framework_->Renderer()->MainCameraScene();
            if (scene)
            {
                if (enabled)
                {
                    // Check if avatar already exists.
                    Entity *avatarCheck = scene->GetEntityByName(QString("Avatar%1").arg(backend_->user.connectionId)).get();
                    if (avatarCheck)
                        OnEntityCreated(avatarCheck, AttributeChange::LocalOnly);
                    else
                        connect(scene, SIGNAL(EntityCreated(Entity*, AttributeChange::Type)), 
                            this, SLOT(OnEntityCreated(Entity*, AttributeChange::Type)), Qt::UniqueConnection);
                }
                else
                {
                    disconnect(scene, SIGNAL(EntityCreated(Entity*, AttributeChange::Type)), 
                        this, SLOT(OnEntityCreated(Entity*, AttributeChange::Type)));
                    disconnect(this, SLOT(OnEntityCreated(Entity*, AttributeChange::Type)));
                    
                    // If requested, unhook all relevant component signals for avatar detection.
                    if (unhookComponents)
                    {
                        Entity *avatarEntity = scene->GetEntityByName(QString("Avatar%1").arg(backend_->user.connectionId)).get();
                        if (avatarEntity)
                        {
                            disconnect(avatarEntity, SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), 
                                this, SLOT(OnEntityComponentCreated(IComponent*, AttributeChange::Type)));
                                
                            EC_Mesh *mesh = avatarEntity->GetComponent<EC_Mesh>().get();
                            if (mesh)   
                                disconnect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnAvatarMeshCreated()));
                        }
                            
                        disconnect(this, SLOT(OnEntityComponentCreated(IComponent*, AttributeChange::Type)));
                        disconnect(this, SLOT(OnAvatarMeshCreated()));
                    }
                }
            }
            else
                LogError(LC + "Active scene is null, cannot hook avatar detection.");
        }
    }
    else
        LogError(LC + "Could not find TundraLogicModule, cannot hook avatar detection.");
}

TundraLogic::Client *RocketPlugin::GetTundraClient() const
{
    TundraLogicModule *tundraModule = framework_->Module<TundraLogicModule>();
    return (tundraModule ? tundraModule->GetClient().get() : 0);
}

extern "C" DLLEXPORT void TundraPluginMain(Framework *fw)
{
    Framework::SetInstance(fw); // Inside this DLL, remember the pointer to the global framework object.
    fw->RegisterModule(new RocketPlugin());
}
