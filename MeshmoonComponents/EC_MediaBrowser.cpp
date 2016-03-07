/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "EC_MediaBrowser.h"
#include "MeshmoonComponents.h"

#include "Framework.h"
#include "Application.h"
#include "IRenderer.h"
#include "Profiler.h"
#include "LoggingFunctions.h"

#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"

#include "FrameAPI.h"

#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"

#include "InputAPI.h"
#include "InputContext.h"
#include "MouseEvent.h"

#include "SceneInteract.h"
#include "Scene/Scene.h"
#include "Entity.h"

#include "Renderer.h"
#include "OgreWorld.h"
#include "OgreMaterialAsset.h"

#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Camera.h"
#include "EC_SoundListener.h"

#include "Math/MathFunc.h"
#include "Geometry/Ray.h"
#include "AudioAPI.h"

#include <QWidget>
#include <QKeyEvent>
#include <QApplication>
#include <QTcpSocket>
#include <QProcess>
#include <QHostAddress>
#include <QDataStream>
#include <QGraphicsSceneMouseEvent>

#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreTextureManager.h>
#include <OgreMaterialManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreTechnique.h>
#include <OgrePixelFormat.h>

#if !defined(MESHMOON_SERVER_BUILD)
#if defined(WIN32)
#if defined(DIRECTX_ENABLED)
#ifdef SAFE_DELETE
#undef SAFE_DELETE
#endif
#ifdef SAFE_DELETE_ARRAY
#undef SAFE_DELETE_ARRAY
#endif
#include <d3d9.h>
#include <d3dx9tex.h>
#include <OgreD3D9RenderSystem.h>
#include <OgreD3D9HardwarePixelBuffer.h>
#endif
#else
#include <OgreGLTexture.h>
#endif
#endif

#include "MemoryLeakCheck.h"

const QString EC_MediaBrowser::LC("[EC_MediaBrowser]: ");

EC_MediaBrowser::EC_MediaBrowser(Scene *scene) :
    IComponent(scene),
    // Attributes
    INIT_ATTRIBUTE_VALUE(enabled, "Enabled", true),
    INIT_ATTRIBUTE_VALUE(audioOnly, "Audio Only", false),
    INIT_ATTRIBUTE_VALUE(playOnLoad, "Play On Load", false),
    INIT_ATTRIBUTE_VALUE(spatialRadius, "Spatial Radius", 0.0f),
    INIT_ATTRIBUTE_VALUE(sourceRef, "Source Media", QString()),
    INIT_ATTRIBUTE_VALUE(renderTarget, "Target Entity", EntityReference()),
    INIT_ATTRIBUTE_VALUE(renderSubmeshIndex, "Target Submesh", 0),
    INIT_ATTRIBUTE_VALUE(illuminating, "Illuminating", true),
    INIT_ATTRIBUTE_VALUE(mouseInput, "3D Mouse Input", true),
    INIT_ATTRIBUTE_VALUE(looping, "Loop Media Playback", false),
    // Members
    currentSubmesh_(0xffffffff),
    currentDistance_(-1),
    inView_(false),
    inDistance_(false),
    renderedOnFrame_(false),
    permissionRun_(false),
    vlcLogging_(false),
    updateDelta_(0.0f),
    volume_(-1),
    ipcProcess(0),
    ipcSocket(0),
    size(QSize(0,0)),
    widget_(0),
    graphicsScene_(0),
    graphicsView_(0),
    imgPaused_(":/images/pause-single-line.png", "PNG")
{
    connect(this, SIGNAL(ParentEntitySet()), SLOT(OnParentEntitySet()));
}

EC_MediaBrowser::~EC_MediaBrowser()
{
    if (framework)
        Reset(true);
}

quint16 EC_MediaBrowser::IpcPort()
{
    quint16 port = 0;
    if (ipcSocket)
    {
        if (ipcSocket->state() == QAbstractSocket::ConnectedState)
            port = ipcSocket->peerPort();
        else
            port = static_cast<quint16>(ipcSocket->property("ipcport").toUInt());
    }
    return port;
}

quint16 EC_MediaBrowser::FreeIpcPort()
{
    if (!ParentScene())
        return 0;

    std::vector<shared_ptr<EC_MediaBrowser> > mediaBrowsers = ParentScene()->Components<EC_MediaBrowser>();
    quint16 port = 40100;

    for(;;)
    {
        bool taken = false;
        for(size_t i = 0; i < mediaBrowsers.size(); ++i)
            if (mediaBrowsers[i]->IpcPort() == port)
            {
                taken = true;
                break;
            }

        if (!taken)
            break;
        else
            port++;

        // Infinite loop guard
        if (port > 45100)
            return 0;
    }
    
    return port;
}

void EC_MediaBrowser::OnIpcData()
{
    if (!ipcSocket || ipcSocket->bytesAvailable() == 0)
        return;

    QByteArray data = ipcSocket->readAll();
    QDataStream s(&data, QIODevice::ReadWrite);

    while(!s.atEnd())
    {
        quint8 typeInt = 0; s >> typeInt;
        MediaPlayerProtocol::MessageType type = (MediaPlayerProtocol::MessageType)typeInt;

        switch(type)
        {
            case MediaPlayerProtocol::Resize:
            {
                MediaPlayerProtocol::ResizeMessage msg(s);
                
                if (ipcMemory.isAttached())
                    ipcMemory.detach();

                // This is the reported texture/video size and id.
                ipcMemory.setKey(msg.ipcMemoryId);
                size = QSize(msg.width, msg.height); 

                if (!audioOnly.Get())
                {
                    if (ipcMemory.attach())
                    {
                        // Init state so that correct material is loaded below.
                        //state_.state = MediaPlayerProtocol::Opening;
                        totalTime_ = "";

                        // Initialize rendering material and texture then apply it.
                        InitRendering();
                        ResizeTexture(size.width(), size.height());
                        ApplyMaterial(true);

                        // Initialize overlay UI.
                        ResetUi();
                        InitUi();
                    }
                    else
                        LogError(LC + "Failed to attach required memory block: " + ipcMemory.errorString());
                }
                break;
            }
            case MediaPlayerProtocol::PlayerError:
            {
                MediaPlayerProtocol::PlayerErrorMessage msg(s);
                LogError(LC + msg.error);
                break;
            }
            case MediaPlayerProtocol::StateUpdate:
            {
                MediaPlayerProtocol::StateUpdateMessage msg(s);
                bool stateChanged = (msg.state.state != state_.state);
                bool applyMaterial = (msg.state.state == MediaPlayerProtocol::Stopped || state_.state == MediaPlayerProtocol::Stopped);
                state_ = msg.state;

                if (stateChanged)
                    emit StateChanged(this, state_.state);
                if (applyMaterial)
                    ApplyMaterial(true);

                UpdateWidgetState();
                break;
            }
            case MediaPlayerProtocol::Dirty:
            {
                BlitToTexture();
                break;
            }
        }
    }
}

EntityPtr EC_MediaBrowser::RenderTarget()
{
    EntityPtr target;
    if (!ParentScene())
        return target;
        
    // Return render target if set, found and EC_Mesh is present.
    const EntityReference &renderTargetRef = renderTarget.Get();
    if (!renderTargetRef.ref.trimmed().isEmpty() && !renderTargetRef.IsEmpty())
    {
        target = renderTargetRef.Lookup(ParentScene());
        if (target && !target->Component<EC_Mesh>())
        {
            connect(target.get(), SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), SLOT(OnComponentAdded(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
            target.reset();
        }
        return target;
    }

    // Return parent if EC_Mesh is present.
    if (!target && ParentEntity()->Component<EC_Mesh>())
        target = ParentEntity()->shared_from_this();
    return target;
}

void EC_MediaBrowser::LoadSource(const QString &source)
{
    if (source.trimmed().isEmpty())
        return;

    if (source.toLower().startsWith("local://"))
    {
        LogWarning(LC + QString("Loading local:// refs (%1) not supported. If you are trying to load a local file use the absolute path instead.").arg(source));
        Reset(false);
        return;
    }

    if (ipcSocket)
    {
        size = QSize(0, 0);
        totalTime_ = "";
        
        // We need to prepend file:// if this is a path on disk.
        QString applySource = source.trimmed();
        AssetAPI::AssetRefType sourceType = AssetAPI::ParseAssetRef(applySource);
        if ((sourceType == AssetAPI::AssetRefLocalPath || sourceType == AssetAPI::AssetRefLocalUrl) && !source.startsWith("file://", Qt::CaseInsensitive))
            applySource = "file://" + source;

        // Load source
        MediaPlayerProtocol::SourceMessage msg(applySource);
        ipcSocket->write(msg.Serialize());

        // Reset volume, if spatial make it some negative non-minus-one value, it will be raised in time with the distance.
        SetVolume(spatialRadius.Get() < 1.f ? 100 : -2);

        // Set looping boolean
        SendLooping();

        // Play now if enabled
        if (playOnLoad.Get())
            Play();
    }
    else
        Init();
}

bool EC_MediaBrowser::SendLooping()
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::TypedBooleanMessage msg(MediaPlayerProtocol::Looping, looping.Get());
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::Play()
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::TypedMessage msg(MediaPlayerProtocol::Play);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::Pause()
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::TypedMessage msg(MediaPlayerProtocol::Pause);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::TogglePlay()
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::TypedMessage msg(MediaPlayerProtocol::PlayPauseToggle);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::Stop()
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::TypedMessage msg(MediaPlayerProtocol::Stop);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::Seek(qint64 msec)
{
    if (ipcSocket)
    {
        MediaPlayerProtocol::SeekMessage msg(msec);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

void EC_MediaBrowser::SeekWithSlider()
{
    Seek(static_cast<qint64>(ui_.timeSlider->value()));
    if (state_.state == MediaPlayerProtocol::Paused)
        QTimer::singleShot(10, this, SLOT(BlitToTexture()));
}

bool EC_MediaBrowser::SetVolume(int vol)
{
    if (ipcSocket && vol != volume_)
    {
        volume_ = vol;
        MediaPlayerProtocol::VolumeMessage msg(volume_);
        ipcSocket->write(msg.Serialize());
    }
    return (ipcSocket != 0);
}

bool EC_MediaBrowser::IsSeekable() const
{
    return state_.seekable;
}

bool EC_MediaBrowser::IsPausable() const
{
    return state_.pausable;
}

MediaPlayerProtocol::PlaybackState EC_MediaBrowser::PlaybackState() const
{
    return state_.state;
}

int EC_MediaBrowser::PlaybackStateInt() const
{
    return (int)PlaybackState();
}

void EC_MediaBrowser::OnParentEntitySet()
{
    if (framework->IsHeadless())
        return;

    resizeTimer_.setSingleShot(true);
    connect(&resizeTimer_, SIGNAL(timeout()), this, SLOT(BlitToTexture()));

    connect(framework->Frame(), SIGNAL(Updated(float)), SLOT(OnUpdate(float)));

    connect(ParentEntity(), SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)), SLOT(OnComponentAdded(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    connect(ParentEntity(), SIGNAL(ComponentRemoved(IComponent*, AttributeChange::Type)), SLOT(OnComponentRemoved(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    
    if (ParentScene())
    {
        OgreWorldPtr world = ParentScene()->GetWorld<OgreWorld>();
        if (world.get())
        {
            inView_ = world->IsEntityVisible(ParentEntity());
            
            world->StartViewTracking(ParentEntity());
            connect(world.get(), SIGNAL(EntityEnterView(Entity*)), SLOT(OnEntityEnterView(Entity*)), Qt::UniqueConnection);
            connect(world.get(), SIGNAL(EntityLeaveView(Entity*)), SLOT(OnEntityLeaveView(Entity*)), Qt::UniqueConnection);
        }   
    }
}

void EC_MediaBrowser::OnComponentAdded(IComponent *component, AttributeChange::Type change)
{
    if (component->TypeId() == EC_Mesh::TypeIdStatic() && component->ParentEntity() == RenderTarget().get())
        ApplyMaterial(true);
}

void EC_MediaBrowser::OnComponentRemoved(IComponent *component, AttributeChange::Type change)
{
    if (component == this || (component->TypeId() == EC_Mesh::TypeIdStatic() && component->ParentEntity() == RenderTarget().get()))
        Reset(false);
}

void EC_MediaBrowser::OnMeshAssetLoaded()
{
    // Mesh is now ready, apply the material.
    ApplyMaterial(true);
}

void EC_MediaBrowser::OnMeshMaterialChanged(uint index, const QString &materialName)
{   
    // Something tried to override the material index we are using to
    // render the media content. Reapply our material immediately.
    if (index == renderSubmeshIndex.Get())
        ApplyMaterial(true);
}

void EC_MediaBrowser::AttributesChanged()
{
    if (framework->IsHeadless())
        return;
    if (!enabled.Get())
    {
        Reset(false);
        return;
    }
    else if (enabled.ValueChanged())
        LoadSource(sourceRef.Get());

    // Update rendering.
    if (enabled.ValueChanged() || renderTarget.ValueChanged() || renderSubmeshIndex.ValueChanged())
        ApplyMaterial(true);
    if (illuminating.ValueChanged())
        SetMaterialIllumination(illuminating.Get());

    // Update browser if initialized at this point.
    if (sourceRef.ValueChanged() && !enabled.ValueChanged())
        LoadSource(sourceRef.Get());

    // Update looping if not previously sent via LoadSource.
    if (looping.ValueChanged() && !sourceRef.ValueChanged() && !enabled.ValueChanged())
        SendLooping();
    
    // Update input.
    if (mouseInput.ValueChanged())
        CheckInput();

    // Audio only
    if (audioOnly.ValueChanged())
    {
        // Reset all rendering
        if (audioOnly.Get())
        {
            RemoveMaterial();
            ResetRendering();
            ResetUi();
            ResetInput();
        }
    }
}

void EC_MediaBrowser::Init()
{
    if (framework->IsHeadless())
        return;
        
    if (!permissionRun_)
        return;

    InitPlayer();
    CheckInput();
    
    connect(framework->Ui()->MainWindow(), SIGNAL(WindowResizeEvent(int, int)), this, SLOT(OnWindowResized(int, int)), Qt::UniqueConnection);
}

void EC_MediaBrowser::Reset(bool immediateShutdown)
{
    if (framework->IsHeadless())
        return;

    RemoveMaterial();
    ResetRendering();
    ResetPlayer(immediateShutdown);
    ResetUi();
    ResetInput();

    state_ = MediaPlayerProtocol::PlayerState();
        
    disconnect(framework->Ui()->MainWindow(), SIGNAL(WindowResizeEvent(int, int)), this, SLOT(OnWindowResized(int, int)));
}

void EC_MediaBrowser::InitInput()
{
    if (inputContext_.get())
        return;

    inputContext_ = framework->Input()->RegisterInputContext("EC_MediaBrowser", 1000);
    inputContext_->SetTakeMouseEventsOverQt(true);

    connect(inputContext_.get(), SIGNAL(MouseEventReceived(MouseEvent*)), this, SLOT(OnMouseEventReceived(MouseEvent*)), Qt::UniqueConnection);
}

void EC_MediaBrowser::CheckInput()
{
    if (mouseInput.Get())
        InitInput();
    else
        ResetInput();
}

void EC_MediaBrowser::ResetInput()
{
    inputContext_.reset();
}

void EC_MediaBrowser::InitPlayer()
{
    if (framework->IsHeadless())
        return;

    if (!permissionRun_)
        return;

    if (!ipcProcess && !ipcSocket)
    {
        quint16 port = FreeIpcPort();
        if (port == 0)
        {
            LogError(LC + "Cannot start a media player no free ports for RocketMediaPlayer process!");
            return;
        }

        if (ipcMemory.isAttached())
            ipcMemory.detach();

        QStringList args;
        args << "--port" << QString::number(port);

        ipcSocket = new QTcpSocket();
        ipcSocket->setProperty("ipcport", port);
        connect(ipcSocket, SIGNAL(connected()), this, SLOT(OnIpcConnected()), Qt::UniqueConnection);
        connect(ipcSocket, SIGNAL(readyRead()), this, SLOT(OnIpcData()), Qt::UniqueConnection);
        connect(ipcSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(OnIpcError(QAbstractSocket::SocketError)), Qt::UniqueConnection);

        ipcProcess = new QProcess();
        if (vlcLogging_ || IsLogChannelEnabled(LogChannelDebug))
        {
            args << "--debug";
            ipcProcess->setProcessChannelMode(QProcess::ForwardedChannels);
        }

#ifdef __APPLE__
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("VLC_PLUGIN_PATH", Application::InstallationDirectory() + "plugins/vlcplugins"); // Add an environment variable
        ipcProcess->setProcessEnvironment(env);
#endif

        connect(ipcProcess, SIGNAL(started()), this, SLOT(OnIpcProcessStarted()), Qt::UniqueConnection);
        connect(ipcProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(OnIpcProcessClosed(int, QProcess::ExitStatus)), Qt::UniqueConnection);
        connect(ipcProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(OnIpcProcessError(QProcess::ProcessError)), Qt::UniqueConnection);

        ipcProcess->start(QDir::fromNativeSeparators(Application::InstallationDirectory()) + "RocketMediaPlayer", args);
    }
}

void EC_MediaBrowser::OnIpcProcessStarted()
{
    if (ipcSocket)
    {
        quint16 port = static_cast<quint16>(ipcSocket->property("ipcport").toUInt());
        ipcSocket->connectToHost(QHostAddress::LocalHost, port);
    }
}

void EC_MediaBrowser::OnIpcProcessClosed(int exitCode, QProcess::ExitStatus)
{
    if (ipcProcess)
    {
        if (ipcMemory.isAttached())
            ipcMemory.detach();

        ipcProcess->deleteLater();
    }
    ipcProcess = 0;
}

void EC_MediaBrowser::OnIpcProcessError(QProcess::ProcessError error)
{
    switch(error)
    {
        case QProcess::FailedToStart: LogError(LC + "OnIpcProcessError: FailedToStart"); break;
        case QProcess::Crashed: LogError(LC + "OnIpcProcessError: Crashed"); break;
        case QProcess::Timedout: LogError(LC + "OnIpcProcessError: Timedout"); break;
        case QProcess::ReadError: LogError(LC + "OnIpcProcessError: ReadError"); break;
        case QProcess::WriteError: LogError(LC + "OnIpcProcessError: WriteError"); break;
        case QProcess::UnknownError: LogError(LC + "OnIpcProcessError: UnknownError"); break;
    }

    if ((error == QProcess::UnknownError || error == QProcess::Crashed || error == QProcess::FailedToStart) && ipcProcess)
    {
        ipcProcess->deleteLater();
        ipcProcess = 0;
    }
}

void EC_MediaBrowser::OnIpcConnected()
{
    emit PlayerCreated(this);

    LoadSource(sourceRef.Get());
}

void EC_MediaBrowser::ResetPlayer(bool immediateShutdown)
{
    // Release our ref of the shared mem block.
    if (ipcMemory.isAttached())
        ipcMemory.detach();
        
    // Closing the socket should make the process exit automatically.
    if (ipcSocket)
    {
        if (ipcSocket->state() != QAbstractSocket::UnconnectedState)
            ipcSocket->disconnectFromHost();
        ipcSocket->deleteLater();
    }
    ipcSocket = 0;

    // Terminate process
    if (ipcProcess)
    {
        ipcProcess->terminate();
        if (immediateShutdown && ipcProcess && ipcProcess->state() != QProcess::NotRunning)
        {
            // If the process does not cleanly exit in 100 msec
            // avoid the memory leak and free immediately.
            if (!ipcProcess->waitForFinished(100))
            {
                ipcProcess->deleteLater();
                ipcProcess = 0;
            }
        }
    }
        
    emit PlayerClosed(this);
}

void EC_MediaBrowser::InitUi()
{
    if (widget_)
        return;
   
    graphicsScene_ = new QGraphicsScene();
    graphicsView_ = new QGraphicsView(graphicsScene_);

    QWidget *viewport = new QWidget();
    viewport->setAttribute(Qt::WA_DontShowOnScreen, true);
    graphicsView_->setViewport(viewport);

    graphicsView_->setAttribute(Qt::WA_DontShowOnScreen, true);
    graphicsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setLineWidth(0);
    graphicsView_->hide();

    widget_ = new QWidget(0, Qt::Widget);
    widget_->setAttribute(Qt::WA_DontShowOnScreen, true);
    ui_.setupUi(widget_);
    
    connect(ui_.playButton, SIGNAL(clicked()), SLOT(Play()));
    connect(ui_.pauseButton, SIGNAL(clicked()), SLOT(Pause()));
    connect(ui_.stopButton, SIGNAL(clicked()), SLOT(Stop()));
    connect(ui_.timeSlider, SIGNAL(sliderReleased()), SLOT(SeekWithSlider()));

    graphicsScene_->addWidget(widget_, Qt::Widget);
    widget_->hide();
    
    widget_->installEventFilter(this);
    ui_.timeSlider->installEventFilter(this);
    ui_.playButton->installEventFilter(this);
    ui_.pauseButton->installEventFilter(this);
    ui_.stopButton->installEventFilter(this);
    
    // Width from the video. Height a hardcoded one.
    int width = size.width();
    int height = 32;

    widget_->setFixedSize(width, height);
    graphicsView_->setFixedSize(width, height);
    graphicsScene_->setSceneRect(0, 0, width, height);
}

void EC_MediaBrowser::ResetUi()
{
    // The widget inside the scene will 
    // get destroyed automatically
    // when the scene is destroyed.
    widget_ = 0;

    SAFE_DELETE(graphicsView_);
    SAFE_DELETE(graphicsScene_);
}

void EC_MediaBrowser::InitRendering()
{
    if (framework->IsHeadless())
        return;

    if (audioOnly.Get())
        return;
        
    // Create texture
    bool createTexture = true;
    if (!textureName_.empty())
        createTexture = Ogre::TextureManager::getSingleton().getByName(textureName_).isNull();
    if (createTexture)
    {
        QSize textureSize = ActualTextureSize();
        textureName_ = framework->Renderer()->GetUniqueObjectName("EC_MediaBrowser_texture_");
        Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(textureName_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, Max(1, textureSize.width()), Max(1, textureSize.height()), 0, Ogre::PF_X8R8G8B8, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
        if (!texture.get())
            LogError(LC + "Failed to create target texture: " + textureName_.c_str());
    }
    
    // Create material
    bool createMaterial = true;
    if (!materialName_.empty())
        createMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName_).isNull();
    if (createMaterial)
    {
        materialName_ = framework->Renderer()->GetUniqueObjectName("EC_MediaBrowser_material_");
        Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().create(materialName_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (material.get())
        {
            if (material->getNumTechniques() == 0)
                material->createTechnique();
            if (material->getTechnique(0) && 
                material->getTechnique(0)->getNumPasses() == 0)
                material->getTechnique(0)->createPass();
            Ogre::Pass *targetPass = material->getTechnique(0)->getPass(0);
            if (targetPass)
            {
                targetPass->setSelfIllumination((illuminating.Get() ? Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f) : Ogre::ColourValue(0.0f, 0.0f, 0.0f, 0.0f)));
                if (targetPass->getNumTextureUnitStates() == 0)
                    targetPass->createTextureUnitState(textureName_);
            }
        }
        else
            LogError(LC + "Failed to create target material: " + materialName_.c_str());
    }
}

void EC_MediaBrowser::ResetRendering()
{
    if (framework->IsHeadless())
        return;

    if (!materialName_.empty())
    {
        try { Ogre::MaterialManager::getSingleton().remove(materialName_); }
        catch (...) {}
        
        materialName_ = "";
    }
    if (!textureName_.empty())
    {
        try { Ogre::TextureManager::getSingleton().remove(textureName_); }
        catch (...) {}
        
        textureName_ = "";
    }
}

void EC_MediaBrowser::ApplyMaterial(bool force)
{
    if (audioOnly.Get())
        return;

    EntityPtr target = RenderTarget();
    if (!target.get())
    {
        RemoveMaterial();
        return;
    }
    
    // Avoid removing and reapplying to exact same target.
    if (!force && currentTarget_.lock() == target && currentSubmesh_ == renderSubmeshIndex.Get())
        return;
    
    EC_Mesh *mesh = target->GetComponent<EC_Mesh>().get();
    if (!mesh)
        return;   
    
    // Mesh not yet loaded.
    if (!mesh->GetEntity())
    {
        connect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnMeshAssetLoaded()), Qt::UniqueConnection);
        return;
    }
            
    // Removes currently applied material from previous target 
    // entity or previous submesh index in the same target entity.
    RemoveMaterial();
   
    // Validate submesh index.
    if (renderSubmeshIndex.Get() >= mesh->GetEntity()->getNumSubEntities() || !mesh->GetEntity()->getSubEntity(renderSubmeshIndex.Get()))
    {
        LogWarning(LC + "Submesh index " + QString::number(renderSubmeshIndex.Get()) + " does not exist in the target mesh. (Entity id=" + QString::number(target->Id()) + " name=" + target->Name() + ")");
        return;
    }

    // Apply our material to the target mesh.
    try
    {
        if (state_.state != MediaPlayerProtocol::Stopped)
            mesh->GetEntity()->getSubEntity(renderSubmeshIndex.Get())->setMaterialName(AssetAPI::SanitateAssetRef(materialName_));
        else
        {
            OgreMaterialAsset *matAsset = dynamic_cast<OgreMaterialAsset*>(framework->Asset()->GetAsset("Ogre Media:EC_MediaBrowser.material").get());
            if (!matAsset || !matAsset->IsLoaded())
            {
                // Request our "waiting" asset
                AssetTransferPtr transfer = framework->Asset()->RequestAsset("Ogre Media:EC_MediaBrowser.material");
                connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(ApplyMaterial()));
                return;
            }
            mesh->GetEntity()->getSubEntity(renderSubmeshIndex.Get())->setMaterialName(matAsset->ogreAssetName.toStdString());
        }
        
        // Connect to mesh and material changes to react and reapply.
        connect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnMeshAssetLoaded()), Qt::UniqueConnection);
        connect(mesh, SIGNAL(MaterialChanged(uint, const QString&)), this, SLOT(OnMeshMaterialChanged(uint, const QString&)), Qt::UniqueConnection);
    }
    catch(Ogre::Exception& e)
    {
        LogError(LC + "Failed to set material to target mesh '" + materialName_.c_str() + "': " + e.what());
        return;
    }
    
    // Remember current target so we can restore 
    // on component remove or target switch.
    currentTarget_ = target;
    currentSubmesh_ = renderSubmeshIndex.Get();
}

void EC_MediaBrowser::RemoveMaterial()
{
    // Previous target: when target entity changes.
    EC_Mesh *previousTarget = currentTarget_.lock().get() ? currentTarget_.lock()->GetComponent<EC_Mesh>().get() : 0;
    if (previousTarget)
        RestoreMaterials(previousTarget);
    
    // Current target: when submesh index changes.
    EntityPtr target = RenderTarget();
    EC_Mesh *curretMesh = target.get() ? target->GetComponent<EC_Mesh>().get() : 0;
    if (curretMesh && curretMesh != previousTarget)
        RestoreMaterials(curretMesh);

    // Reset tracking data, everything should be restored.    
    currentTarget_.reset();
    currentSubmesh_ = 0xffffffff;
}

void EC_MediaBrowser::RestoreMaterials(EC_Mesh *mesh)
{
    if (!mesh)
        return;
    
    // Disconnect targets signals. We must not be connected to MaterialChanged when calling ApplyMaterial, result is a infinite loop.
    disconnect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnMeshAssetLoaded()));
    disconnect(mesh, SIGNAL(MaterialChanged(uint, const QString&)), this, SLOT(OnMeshMaterialChanged(uint, const QString&)));
    
    uint targetIndex = currentSubmesh_ < 0xffffffff ? currentSubmesh_ : renderSubmeshIndex.Get();
    const AssetReferenceList &materials = mesh->materialRefs.Get();
    
    bool restoreEmptyMaterial = ((int)targetIndex >= materials.Size());
    if (!restoreEmptyMaterial)
        if (materials[targetIndex].ref.trimmed().isEmpty())
            restoreEmptyMaterial = true;

    if (!restoreEmptyMaterial)
        mesh->ApplyMaterial();
    else
    {
        if (mesh->GetEntity() && targetIndex < mesh->GetEntity()->getNumSubEntities())
        {
            try
            {
                mesh->GetEntity()->getSubEntity(targetIndex)->setMaterialName(OgreRenderer::Renderer::DefaultMaterialName);
            }
            catch(Ogre::Exception& e)
            {
                LogError(LC + "Failed to set material to target mesh '" + materialName_.c_str() + "': " + e.what());
                return;
            }
        }
    }
}

bool EC_MediaBrowser::IsInView() const
{
    return inView_;
}

bool EC_MediaBrowser::IsInDistance() const
{
    return inDistance_;
}

int EC_MediaBrowser::DistanceToCamera(Entity *renderTarget)
{
    PROFILE(EC_MediaBrowser_DistanceToCamera)

    if (!renderTarget)
        renderTarget = RenderTarget().get();
    if (framework->IsHeadless() || !renderTarget)
    {
        currentDistance_ = -1;
        return -1;
    }
        
    Entity *cameraEnt = framework->Renderer()->MainCamera();
    EC_Placeable *cameraPlaceable = cameraEnt ? cameraEnt->GetComponent<EC_Placeable>().get() : 0;
    EC_Placeable *placeable = renderTarget->GetComponent<EC_Placeable>().get();
    if (!placeable || !cameraPlaceable)
    {
        currentDistance_ = -1;
        return -1;
    }

    currentDistance_ = RoundInt(cameraPlaceable->WorldPosition().Distance(placeable->WorldPosition()));
    return currentDistance_;
}

float EC_MediaBrowser::ActualTextureScale(int width, int height)
{
    float scale = 1.0f;
    if (framework->IsHeadless() || !ParentEntity() || currentDistance_ < 0)
        return scale;

    if (currentDistance_ > 20 && currentDistance_ <= 40)
        scale /= 2.0f;
    else if (currentDistance_ > 40 && currentDistance_ <= 60)
        scale /= 4.0f;
    else if (currentDistance_ > 60)
        scale /= 8.0f;
    else if (currentDistance_ > 85)
        scale /= 16.0f;

    return scale;
}

QSize EC_MediaBrowser::ActualTextureSize(int width, int height)
{
    QSize actualSize(width > 0 ? width : size.width(), height > 0 ? height : size.height());
    if (framework->IsHeadless() || !ParentEntity() || currentDistance_ < 0)
        return actualSize;

    if (currentDistance_ > 20 && currentDistance_ <= 40)
        actualSize /= 2.0f;
    else if (currentDistance_ > 40 && currentDistance_ <= 60)
        actualSize /= 4.0f;
    else if (currentDistance_ > 60)
        actualSize /= 8.0f;
    else if (currentDistance_ > 85)
        actualSize /= 16.0f;

    return actualSize;
}

void EC_MediaBrowser::FocusPlayer()
{
    if (widget_ && !widget_->isVisible())
    {
        widget_->show();
        UpdateWidgetState();
    }
}

void EC_MediaBrowser::UnfocusPlayer()
{
    if (widget_ && widget_->isVisible())
        widget_->hide();
}

void EC_MediaBrowser::SetMaterialIllumination(bool illuminating_)
{
    if (materialName_.empty())
        return;
    Ogre::MaterialPtr material = Ogre::MaterialManager::getSingleton().getByName(materialName_);
    if (!material.get())
        return;
    
    Ogre::Material::TechniqueIterator iter = material->getTechniqueIterator();
    while(iter.hasMoreElements())
    {
        Ogre::Technique *tech = iter.getNext();
        if (!tech)
            continue;
        Ogre::Technique::PassIterator passIter = tech->getPassIterator();
        while(passIter.hasMoreElements())
        {
            Ogre::Pass *pass = passIter.getNext();
            if (pass)
                pass->setSelfIllumination((illuminating_ ? Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f) : Ogre::ColourValue(0.0f, 0.0f, 0.0f, 0.0f)));
        }
    }
}

void EC_MediaBrowser::SetVlcLoggingEnabled(bool enabled)
{
    vlcLogging_ = enabled;
}

QString EC_MediaBrowser::PlaybackStateToString(int playbackState)
{
    return MediaPlayerProtocol::PlaybackStateToString(static_cast<MediaPlayerProtocol::PlaybackState>(playbackState));
}

void EC_MediaBrowser::ResizeTexture(int width, int height)
{
    if (audioOnly.Get())
        return;

    // Not initialized yet: Do nothing as it will be done when the time is right.
    if (textureName_.empty())
        return;

    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(textureName_);
    if (!texture.get())
        return;

    QSize textureSize = ActualTextureSize(Max(1, width), Max(1, height));
    if ((int)texture->getWidth() != textureSize.width() || (int)texture->getHeight() != textureSize.height())
    {
        texture->freeInternalResources();
        texture->setWidth(textureSize.width());
        texture->setHeight(textureSize.height());
        texture->createInternalResources();
        
        BlitToTexture();
    }
}

void EC_MediaBrowser::UpdateWidgetState()
{
    if (widget_ && widget_->isVisible())
    {                
        if (ui_.timeSlider->maximum() != state_.mediaLengthMsec)
            ui_.timeSlider->setMaximum(state_.mediaLengthMsec);
        if (!ui_.timeSlider->isSliderDown())
            ui_.timeSlider->setValue(state_.mediaTimeMsec);

        if (totalTime_.isEmpty())
        {
            QTime tTotal;
            tTotal = tTotal.addMSecs(state_.mediaLengthMsec);
            totalTime_ =  " / " + tTotal.toString((tTotal.hour() > 0 ? "H:m:ss" : "m:ss"));
        }

        QTime tNow;
        tNow = tNow.addMSecs(state_.mediaTimeMsec);
        ui_.timeLabel->setText(tNow.toString((tNow.hour() > 0 ? "H:m:ss" : "m:ss")) + totalTime_);

        if (state_.state == MediaPlayerProtocol::Playing)
        {
            if (ui_.playButton->isVisible())
                ui_.playButton->hide();
            if (!ui_.pauseButton->isVisible())
                ui_.pauseButton->show();
            if (!ui_.stopButton->isVisible())
                ui_.stopButton->show();
            ui_.timeSlider->setEnabled(true);
        }
        else if (state_.state == MediaPlayerProtocol::Paused)
        {
            if (!ui_.playButton->isVisible())
                ui_.playButton->show();
            if (ui_.pauseButton->isVisible())
                ui_.pauseButton->hide();
            if (!ui_.stopButton->isVisible())
                ui_.stopButton->show();
            ui_.timeSlider->setEnabled(true);
            
            QTimer::singleShot(20, this, SLOT(BlitToTexture()));
        }
        else if (state_.state == MediaPlayerProtocol::Stopped)
        {
            if (!ui_.playButton->isVisible())
                ui_.playButton->show();
            if (ui_.pauseButton->isVisible())
                ui_.pauseButton->hide();
            if (ui_.stopButton->isVisible())
                ui_.stopButton->hide();
            ui_.timeSlider->setEnabled(false);
            ui_.timeSlider->setValue(0);
            ui_.timeLabel->setText("0:00" + totalTime_);
        }
    }
}

void EC_MediaBrowser::RenderWidget(bool & locked)
{
    // Render widget
    if (widget_ && widget_->isVisible())
    {
        PROFILE(EC_MediaBrowser_RenderWidget)

        // Dock controls to the bottom
        int posY = size.height() - widget_->height();
        if (posY >= 0)
        {
            QImage img(widget_->size(), QImage::Format_RGB32);
            QPainter p(&img);
            widget_->render(&p);
            p.end();

            ipcMemory.lock();
            locked = true;

            uchar *data = (uchar*)ipcMemory.data();
            for(int y=0; y<img.height(); ++y)
                memcpy(data + ((posY+y)*img.bytesPerLine()), img.bits() + (y*img.bytesPerLine()), img.bytesPerLine());
        }

        ELIFORP(EC_MediaBrowser_RenderWidget)
    }

    if (state_.state == MediaPlayerProtocol::Paused && !imgPaused_.isNull() && size.height() > 200 && size.width() > 200)
    {
        PROFILE(EC_MediaBrowser_RenderPaused)

        int bytesPerImgLine = imgPaused_.bytesPerLine();
        int bytesPerVideoLine = size.width() * 4;

        int posY = 20;
        int posXBytes = 20 * 4;

        if (!locked)
        {
            ipcMemory.lock();
            locked = true;
        }

        // There is no transparency (too heavy to start masking
        // below pixels) so we must paint single pause block twice.
        uchar *data = (uchar*)ipcMemory.data();
        for(int y=0; y<imgPaused_.height(); ++y)
            memcpy(data + ((posY+y)*bytesPerVideoLine) + posXBytes, imgPaused_.bits() + (y*bytesPerImgLine), bytesPerImgLine);
        posXBytes += (bytesPerImgLine + 28 * 4);
        for(int y=0; y<imgPaused_.height(); ++y)
            memcpy(data + ((posY+y)*bytesPerVideoLine) + posXBytes, imgPaused_.bits() + (y*bytesPerImgLine), bytesPerImgLine);

        ELIFORP(EC_MediaBrowser_RenderPaused)
    }
}

void EC_MediaBrowser::BlitToTexture()
{
#if !defined(MESHMOON_SERVER_BUILD)
    if (state_.state == MediaPlayerProtocol::Stopped)
        return;
    if (audioOnly.Get())
        return;
    if (!IsInView() || !ipcProcess || !ipcMemory.isAttached())
        return;
    if (renderedOnFrame_)
        return;
    renderedOnFrame_ = true;
     
    if (textureName_.empty())
        return;
    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(textureName_);
    if (!texture.get())
        return;
    Ogre::HardwarePixelBufferSharedPtr pb = texture->getBuffer();
    if (!pb.get())
        return;

    PROFILE(EC_MediaBrowser_BlitToTexture)
    
    int width = size.width();
    int height = size.height();
#endif

#if defined(WIN32) && !defined(MESHMOON_SERVER_BUILD)
    // DirectX  
    Ogre::D3D9HardwarePixelBuffer *pixelBuffer = dynamic_cast<Ogre::D3D9HardwarePixelBuffer*>(pb.get());
    if (pixelBuffer)
    {
        LPDIRECT3DSURFACE9 surface = pixelBuffer->getSurface(Ogre::D3D9RenderSystem::getActiveD3D9Device());
        if (surface)
        {
            D3DSURFACE_DESC desc;
            HRESULT hr = surface->GetDesc(&desc);
            if (SUCCEEDED(hr))
            {
                bool locked = false;
                RenderWidget(locked);

                RECT dxRect = { 0, 0, width, height };
                
                if ((int)texture->getWidth() == width && (int)texture->getHeight() == height)
                {
                    if (!locked)
                    {
                        ipcMemory.lock();
                        locked = true;
                    }
                    D3DXLoadSurfaceFromMemory(surface, NULL, NULL, ipcMemory.data(), 
                        D3DFMT_X8R8G8B8, static_cast<UINT>(width*4), NULL, &dxRect, D3DX_DEFAULT, 0);
                }
                // Texture is different size than the media rendering size. We need to scale.
                else
                {
                    PROFILE(EC_MediaBrowser_ScaleToTexture)

                    float scale = ActualTextureScale();
                    Ogre::PixelBox srcBox (width, height, 1, Ogre::PF_X8R8G8B8, ipcMemory.data());
                    Ogre::PixelBox destBox(FloorInt(width*scale), FloorInt(height*scale), 1, Ogre::PF_X8R8G8B8, 0);
                    if (destBox.getWidth() <= 0 || destBox.getHeight() <= 0)
                        return; // Empty on a axis

                    dxRect.right = (LONG)destBox.getWidth();
                    dxRect.bottom = (LONG)destBox.getHeight();
                    
                    QByteArray scaledData((int)destBox.getConsecutiveSize(), Qt::Uninitialized);
                    destBox.data = (void*)scaledData.constData();
                    
                    // Do not change FILTER_NEAREST! This is a lot faster than the other modes and the 
                    // quality does not matter if we are scaling. We are already so far away from the texture.
                    if (!locked)
                    {
                        ipcMemory.lock();
                        locked = true;
                    }
                    Ogre::Image::scale(srcBox, destBox, Ogre::Image::FILTER_NEAREST);
                    
                    ELIFORP(EC_MediaBrowser_ScaleToTexture)
                    
                    D3DXLoadSurfaceFromMemory(surface, NULL, NULL, destBox.data, 
                        D3DFMT_X8R8G8B8, static_cast<UINT>(destBox.getWidth()*4), NULL, &dxRect, D3DX_DEFAULT, 0);
                }
                
                if (locked)
                    ipcMemory.unlock();
                return;
            }
        }
    }
#elif !defined(MESHMOON_SERVER_BUILD)
    // OpenGL
    Ogre::GLTexture *glTexture = dynamic_cast<Ogre::GLTexture*>(texture.get());
    if (glTexture)
    {
        bool locked = false;
        RenderWidget(locked);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, glTexture->getGLID());
        if ((int)texture->getWidth() == width && (int)texture->getHeight() == height)
        {
            if (!locked)
            {
                ipcMemory.lock();
                locked = true;
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size.width(), size.height(),
                            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ipcMemory.data());
        }
        else
        {
            if (!locked)
            {
                ipcMemory.lock();
                locked = true;
            }

            float scale = ActualTextureScale();
            QSize newSize(FloorInt(width * scale), FloorInt(height * scale));

            QImage sourceImage((const uchar*)ipcMemory.data(), width, height, QImage::Format_ARGB32);
            QImage destImage = sourceImage.scaled(newSize);

            QRect scaledRect(destImage.rect());

            glTexSubImage2D(GL_TEXTURE_2D, 0, scaledRect.x(), scaledRect.y(), scaledRect.width(), scaledRect.height(),
                            GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (void*) destImage.bits());
        }

        if (locked)
            ipcMemory.unlock();

        glDisable(GL_TEXTURE_2D);
        return;
    }
#endif
}

void EC_MediaBrowser::OnWindowResized(int newWidth, int newHeight)
{
    resizeTimer_.start(50);
}

bool EC_MediaBrowser::eventFilter(QObject *obj, QEvent *e)
{
    if (widget_)
    {
        if (obj == widget_)
        {
            QEvent::Type type = e->type();
            if (type == QEvent::Paint || type == QEvent::UpdateRequest)
            {
                BlitToTexture();
                return true;
            }
            if (type == QEvent::Show || type == QEvent::Hide)
                QTimer::singleShot(20, this, SLOT(BlitToTexture()));
        }
        else if (state_.state == MediaPlayerProtocol::Paused)
        {
            if (obj == ui_.timeSlider || obj == ui_.playButton || obj == ui_.pauseButton || obj == ui_.stopButton)
            {
                if (e->type() == QEvent::HoverMove)
                    QTimer::singleShot(1, this, SLOT(BlitToTexture()));
            }
        }
    }
    return false;
}

void EC_MediaBrowser::SetRunPermission(bool permitted)
{
    if (permissionRun_ == permitted)
        return;

    permissionRun_ = permitted;
    if (permissionRun_)
        Init();
    else
        Reset(false); /// @todo Leave the current texture on the object.
    ApplyMaterial(true);
}

void EC_MediaBrowser::OnMouseEventReceived(MouseEvent *mouseEvent)
{
    if (!ipcSocket)
        return;
    if (!mouseInput.Get())
        return;
    // If we are too far away, don't handle input.
    if (!IsInDistance())
    {
        UnfocusPlayer();
        return;
    }
    EntityPtr target = RenderTarget();
    if (!target.get() || !ParentScene())
        return;
    EC_Mesh *mesh = target->Component<EC_Mesh>().get();
    if (!mesh || !mesh->HasMesh())
        return;
    OgreWorldPtr world = ParentScene()->GetWorld<OgreWorld>();
    if (!world)
        return;
    EC_Camera *mainCamera = world->Renderer()->MainCameraComponent();
    if (!mainCamera)
        return;
    if (framework->Ui()->GraphicsView()->VisibleItemAtCoords(mouseEvent->x, mouseEvent->y) != 0)
        return;

    PROFILE(EC_MediaBrowser_OnMouseEventReceived)

    Ray mouseRay = mainCamera->ScreenPointToRay(mouseEvent->x, mouseEvent->y);
    if (!mouseRay.Intersects(mesh->WorldOBB()))
    {
        UnfocusPlayer();
        return;
    }

    RaycastResult *result = 0;
    SceneInteract *sceneInteract = framework->Module<SceneInteract>();
    if (sceneInteract) /// @todo Only use common raycast is targets placeable is in layer 0?
        result = sceneInteract->CurrentMouseRaycastResult();
    else
        result = world->Raycast(mouseRay, 0xffffffff); /// @todo Get layer from targets placeable?
    if (!result || result->entity != target.get() || (uint)result->submesh != renderSubmeshIndex.Get() || !framework->Input()->IsMouseCursorVisible())
    {
        UnfocusPlayer();
        return;
    }

    // If we don't yet know the size of the texture, we cannot map the clicks to it.
    // Only accept left click to toggle playback state until texture size is know.
    if (size.isNull())
    {
        if (mouseEvent->eventType == MouseEvent::MousePressed && mouseEvent->IsLeftButtonDown())
        {
            TogglePlay();
            mouseEvent->Suppress();
        }
        return;
    }

    if (widget_ && !widget_->isVisible())
        FocusPlayer();
        
    QPoint posPlayer((float)size.width()*result->u, (float)size.height()*result->v);
    bool isOnControls = (posPlayer.y() >= size.height() - 32);
    
    switch(mouseEvent->eventType)
    {
        case MouseEvent::MouseMove:
        {
            if (isOnControls)
            {
                QPointF scenePos(posPlayer.x(), posPlayer.y() - size.height() + 32);
                SendMouseEvent(QEvent::GraphicsSceneMouseMove, scenePos, mouseEvent);
            }
            break;
        }
        case MouseEvent::MousePressed:
        case MouseEvent::MouseReleased:
        {
            if (isOnControls)
            {
                QPointF scenePos(posPlayer.x(), posPlayer.y() - size.height() + 32);
                SendMouseEvent((mouseEvent->eventType == MouseEvent::MousePressed ? QEvent::GraphicsSceneMousePress : QEvent::GraphicsSceneMouseRelease), scenePos, mouseEvent);
            }
            else if (mouseEvent->eventType == MouseEvent::MousePressed && mouseEvent->IsLeftButtonDown())
            {
                TogglePlay();
                mouseEvent->Suppress();
            }
            break;
        }
        default:
            break;
    }
}

void EC_MediaBrowser::SendMouseEvent(QEvent::Type type, const QPointF &point, MouseEvent *e)
{
    if (!graphicsView_)
        return;

    Qt::MouseButton button = (type == QEvent::GraphicsSceneMouseMove ? Qt::NoButton : (Qt::MouseButton)e->button);
    QGraphicsSceneMouseEvent mouseEvent(type);
    mouseEvent.setButtonDownScenePos(button, point);
    mouseEvent.setButtonDownScreenPos(button, point.toPoint());

    mouseEvent.setScenePos(point);
    mouseEvent.setScreenPos(point.toPoint());
    mouseEvent.setLastScenePos(point);
    mouseEvent.setLastScreenPos(point.toPoint());
    mouseEvent.setButton(button);
    mouseEvent.setButtons((Qt::MouseButtons)e->otherButtons);

    mouseEvent.setModifiers((Qt::KeyboardModifiers)e->modifiers);
    mouseEvent.setAccepted(false);

    QApplication::sendEvent(graphicsView_->scene(), &mouseEvent);
    
    e->Suppress();
}

void EC_MediaBrowser::OnEntityEnterView(Entity *ent)
{
    if (ent == ParentEntity() && !inView_)
    {
        inView_ = true;
        BlitToTexture();
        OnUpdate(1.0f); // Checks distance
    }
}

void EC_MediaBrowser::OnEntityLeaveView(Entity *ent)
{
    if (ent == ParentEntity())
        inView_ = false;
}

void EC_MediaBrowser::OnUpdate(float frametime)
{   
    if (state_.state == MediaPlayerProtocol::Playing && ipcSocket && ParentScene())
    {
        PROFILE(EC_MediaBrowser_Update_Spatial_Volume)
        UpdateVolume();
    }
    
    if (!inView_)
        return;
    renderedOnFrame_ = false;
    
    updateDelta_ += frametime;
    if (updateDelta_ < 0.5f)
        return;
    updateDelta_ = 0.0f;
    
    // Check distance
    int distance = DistanceToCamera();
    bool inDistanceNow = (distance <= 100 && distance >= 0);
    if (inDistance_ != inDistanceNow)
    {
        inDistance_ = inDistanceNow;
        if (inDistance_)
            BlitToTexture();
    }
    
    // Resizes texture if there are changes.
    ResizeTexture(size.width(), size.height());
}

void EC_MediaBrowser::UpdateVolume()
{
    if (spatialRadius.Get() < 1.f)
    {
        if (volume_ != 100)
            SetVolume(100);
        return;
    }
    
    // Use the render target as the audio source location. Even if 
    // audioOnly is enabled this should still be respected.
    EntityPtr targetEnt = RenderTarget();
    if (!targetEnt.get())
        return;
    EC_Placeable *sourcePlaceable = targetEnt->Component<EC_Placeable>().get();
    if (!sourcePlaceable)
        return;

    float distance = sourcePlaceable->WorldPosition().Distance(framework->Audio()->ListenerPosition());
    int vol = Clamp((int)(100.0f - distance / spatialRadius.Get() * 100.0f), 0, 100);
    if (vol != volume_)
        SetVolume(vol);
}

void EC_MediaBrowser::OnIpcError(QAbstractSocket::SocketError socketError)
{
    LogDebug(LC + "An error occured while connecting to RocketMediaBrowser process: " + QAbstractSocketErrorToString(socketError) + ".");
}
