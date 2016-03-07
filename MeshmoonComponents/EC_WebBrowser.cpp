/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "EC_WebBrowser.h"
#include "rocketwebbrowser/BrowserNetworkMessages.h"
#include "MeshmoonComponents.h"

#include "Framework.h"
#include "Application.h"
#include "IRenderer.h"
#include "Profiler.h"
#include "LoggingFunctions.h"
#include "FrameAPI.h"

#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"

#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"

#include "InputAPI.h"
#include "InputContext.h"
#include "MouseEvent.h"

#include "Scene/Scene.h"
#include "Entity.h"

#include "Renderer.h"
#include "OgreMaterialAsset.h"
#include "OgreWorld.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Camera.h"

#include "Math/MathFunc.h"
#include "Geometry/Ray.h"

#include "TundraLogicModule.h"
#include "Server.h"
#include "Client.h"
#include "UserConnection.h"

#include <QWidget>
#include <QKeyEvent>
#include <QDebug>
#include <QApplication>
#include <QTcpSocket>
#include <QProcess>
#include <QHostAddress>
#include <QDataStream>

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

const int numberOfRetries = 100;

#include "MemoryLeakCheck.h"

EC_WebBrowser::EC_WebBrowser(Scene *scene) :
    IComponent(scene),
    // Attributes
    INIT_ATTRIBUTE_VALUE(enabled, "Enabled", true),
    INIT_ATTRIBUTE_VALUE(url, "URL", QString()),
    INIT_ATTRIBUTE_VALUE(size, "Size", QPoint(1024, 1024)),
    INIT_ATTRIBUTE_VALUE(renderTarget, "Target Entity", EntityReference()),
    INIT_ATTRIBUTE_VALUE(renderSubmeshIndex, "Target Submesh", 0),
    INIT_ATTRIBUTE_VALUE(illuminating, "Illuminating", true),
    INIT_ATTRIBUTE_VALUE(mouseInput, "3D Mouse Input", true),
    INIT_ATTRIBUTE_VALUE(keyboardInput, "3D Keyboard Input", true),
    INIT_ATTRIBUTE_VALUE(synchronizedClient, "Synchronized Client", -1),
    // Members
    LC("[EC_WebBrowser]: "),
    textureName_(""),
    materialName_(""),
    currentSubmesh_(-1),
    currentDistance_(-1),
    connectionRetries_(numberOfRetries),
    inView_(false),
    inDistance_(false),
    controlling_(false),
    permissionRun_(false),
    updateDelta_(0.0f),
    syncDelta_(0.0f),
    ipcProcess(0),
    ipcSocket(0)
{
    connect(this, SIGNAL(ParentEntitySet()), SLOT(OnParentEntitySet()));
}

EC_WebBrowser::~EC_WebBrowser()
{
    if (framework)
        Reset(true);
}

quint16 EC_WebBrowser::IpcPort()
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

quint16 EC_WebBrowser::FreeIpcPort()
{
    if (!ParentEntity() || !ParentScene())
        return 0;

    /** @todo Implement pinging the port before thinking its free.
        Currently the starting process or the existing process with same port will crash. */

    std::vector<shared_ptr<EC_WebBrowser> > browsers = ParentScene()->Components<EC_WebBrowser>();
    quint16 port = 40000;
    
    /// Hack for testing shared input with multiple clients on the same machine.
    //if (TundraClient()->ConnectionId() > 1)
    //    port += 1000;
    
    for(;;)
    {
        bool taken = false;
        for(size_t i = 0; i < browsers.size(); ++i)
        {
            if (browsers[i]->IpcPort() == port)
            {
                taken = true;
                break;
            }
        }
        
        if (!taken)
            break;
        else
            port++;
            
        // Infinite loop guard
        if (port > 45000)
            return 0;
    }
    
    return port;
}

QString EC_WebBrowser::IpcId(quint16 port)
{
    return QString("cef_ipc_mem_%1_%2x%3").arg(port).arg(size.Get().x()).arg(size.Get().y());
}

void EC_WebBrowser::OnIpcData()
{
    if (!ipcSocket || ipcSocket->bytesAvailable() == 0)
        return;

    QByteArray data = ipcSocket->readAll();
    QDataStream s(&data, QIODevice::ReadWrite);

    while(!s.atEnd())
    {
        quint8 typeInt = 0; s >> typeInt;
        BrowserProtocol::MessageType type = (BrowserProtocol::MessageType)typeInt;

        switch(type)
        {
            case BrowserProtocol::DirtyRects:
            {
                BrowserProtocol::DirtyRectsMessage msg(s);
                OnIpcPaint(msg.rects);
                break;
            }
            case BrowserProtocol::LoadStart:
            case BrowserProtocol::LoadEnd:
            {
                BrowserProtocol::TypedStringMessage msg(s);
                if (type == BrowserProtocol::LoadStart)
                {
                    currentUrl_ = msg.str.trimmed();
                    emit LoadStarted(this, msg.str);
                    
                    // Make everyone open our url if we are controlling
                    if (controlling_)
                    {
                        TundraLogic::Client *client = TundraClient();
                        if (client && client->GetConnection() && ParentEntity())
                        {
                            ECWebBrowserNetwork::MsgBrowserAction msgOut;
                            msgOut.entId = ParentEntity()->Id();
                            msgOut.compId = Id();
                            msgOut.type = static_cast<u8>(BrowserProtocol::Url);

                            msgOut.payload.resize(msg.str.size());
                            memcpy(&msgOut.payload[0], msg.str.data(), msg.str.size());

                            client->GetConnection()->Send(msgOut);
                        }
                    }
                }
                else if (type == BrowserProtocol::LoadEnd)
                    emit LoadCompleted(this, msg.str);
                break;
            }
            default:
                return;
        }
    }
}

void EC_WebBrowser::OnIpcPaint(const QList<QRect> &dirtyRects)
{
    PROFILE(Admino_Cef_IPC_Paint)

    // The following view/distance checks are done here instead inside EC_Browser::BlitToTexture.
    // to optimize away the data copy&pasting below.
    if (!IsInView() || !ipcProcess || !ipcMemory.isAttached())
        return;
    if (!IsInDistance())
    {
        // If we are too far away to render, still allow full rect paints.
        bool shouldRender = false;
        if (dirtyRects.size() == 1)
        {
            const QRect &mainRect = dirtyRects[0];
            if (mainRect.width() == size.Get().x() && mainRect.height() == size.Get().y())
                shouldRender = true;
        }
        if (!shouldRender)
            return;
    }

    ipcMemory.lock();

    // Copy rects to their own buffers from the big main buffer.
    // This suits best for EC_WebBrowser to feed the data into ogre.
    QList<QByteArray> outBufs;

    int viewWidth = size.Get().x();
    const unsigned char *buf = static_cast<const unsigned char*>(ipcMemory.constData());

    for(int i=0; i<dirtyRects.size(); ++i)
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);

        // The CEF docs say BGRA format but it seems to be ARGB which is good 
        // as OGRE wont let us use BGRA format directly so we would need to swap pixels around.
        const QRect &rect = dirtyRects[i];
        int lineBytes = rect.width() * 4;
        for (int y=0; y<rect.height(); ++y)
        {
            int index = (viewWidth * (rect.y()+y) * 4) + (rect.x()) * 4;
            if (stream.writeRawData((const char*)&buf[index], lineBytes) != lineBytes)
            {
                LogError("[CefClient]: Failed to prepare dirty rect data for texture blitting!");
                return;
            }
        }

        outBufs << data;
    }
    
    ipcMemory.unlock();

    if (!outBufs.isEmpty())
        BlitToTexture(dirtyRects, outBufs);
}

QString EC_WebBrowser::CurrentUrl() const
{
    if (!currentUrl_.isEmpty())
        return currentUrl_;
    return url.Get();
}

EntityPtr EC_WebBrowser::RenderTarget()
{
    EntityPtr target;
    if (!ParentScene())
        return target;
        
    // Return render target if set, found and EC_Mesh is present.
    const EntityReference &renderTargetRef = renderTarget.Get();
    if (!renderTargetRef.ref.trimmed().isEmpty() && !renderTargetRef.IsEmpty())
    {
        target = renderTargetRef.Lookup(ParentScene());
        if (target.get() && !target->Component<EC_Mesh>().get())
        {
            connect(target.get(), SIGNAL(ComponentAdded(IComponent*, AttributeChange::Type)),
                SLOT(OnComponentAdded(IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
            target.reset();
        }
        return target;
    }

    // Return parent if EC_Mesh is present.
    if (!target.get() && ParentEntity() && ParentEntity()->Component<EC_Mesh>().get())
        target = ParentScene()->EntityById(ParentEntity()->Id());
    return target;
}

void EC_WebBrowser::LoadUrl(const QString &url)
{
    if (ipcSocket)
    {
        BrowserProtocol::UrlMessage msg(url.trimmed());
        ipcSocket->write(msg.Serialize());
    }
}

void EC_WebBrowser::ResizeBrowser(int width, int height)
{
    if (ipcSocket)
    {
        // Size changes impact our shared memory segment size as well.
        // If IPC id is already attached, we are on a correct sized block, nothing to do then.
        QString id = IpcId(static_cast<quint16>(ipcSocket->property("ipcport").toUInt()));
        if (ipcMemory.key().compare(id, Qt::CaseInsensitive) != 0)
        {
            if (ipcMemory.isAttached())
                ipcMemory.detach();
            ipcMemory.setKey(id);

            uint sizeBytes = width * height * 4; // RGBA
            bool created = ipcMemory.create(sizeBytes);
            if (!created && ipcMemory.error() == QSharedMemory::AlreadyExists)
                ipcMemory.attach();
            if (!ipcMemory.isAttached())
            {
                LogError(LC + "Failed to attach to resized IPC memory.");
                Reset(false);
            }
            else
            {
                // Write full white to the texture to remove garbage if it for some reason ends up on screen.
                ipcMemory.lock();
                memset((char*)ipcMemory.data(), qRgba(255,255,255,255), (size_t)sizeBytes);
                ipcMemory.unlock();
            }
        }

        BrowserProtocol::ResizeMessage msg(width, height);
        ipcSocket->write(msg.Serialize());
    }
}

void EC_WebBrowser::OnParentEntitySet()
{
    // Server custom networking
    TundraLogic::Server *server = TundraServer();
    if (server && server->IsRunning())
    {
        connect(server, SIGNAL(MessageReceived(UserConnection *, kNet::packet_id_t, kNet::message_id_t, const char*, size_t)), 
            this, SLOT(OnServerMessageReceived(UserConnection *, kNet::packet_id_t, kNet::message_id_t, const char*, size_t)), Qt::UniqueConnection);
    }

    if (framework->IsHeadless())
        return;

    // Client init
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
        else
        {
            LogError(LC + "Rendering world is null, cannot start view tracking. Setting in view to true.");
            inView_ = true;
        }
    }
}

void EC_WebBrowser::OnComponentAdded(IComponent *component, AttributeChange::Type change)
{
    if (component->TypeId() == EC_Mesh::TypeIdStatic() && component->ParentEntity() == RenderTarget().get())
        ApplyMaterial(true);
}

void EC_WebBrowser::OnComponentRemoved(IComponent *component, AttributeChange::Type change)
{
    if (component == this || (component->TypeId() == EC_Mesh::TypeIdStatic() && component->ParentEntity() == RenderTarget().get()))
        Reset(false);
}

void EC_WebBrowser::OnMeshAssetLoaded()
{
    // Mesh is now ready, apply the material.
    ApplyMaterial(true);
}

void EC_WebBrowser::OnMeshMaterialChanged(uint index, const QString &materialName)
{   
    // Something tried to override the material index we are using to
    // render the web page content. Reapply our material immediately.
    if (index == renderSubmeshIndex.Get())
        ApplyMaterial(true);
}

void EC_WebBrowser::AttributesChanged()
{
    if (framework->IsHeadless())
        return;
    if (!enabled.Get())
    {
        Reset(false);
        return;
    }
    else if (enabled.ValueChanged())
        ApplyMaterial(true);

    // Update rendering.
    if (renderTarget.ValueChanged() || renderSubmeshIndex.ValueChanged())
        ApplyMaterial();
    if (illuminating.ValueChanged())
        SetMaterialIllumination(illuminating.Get());

    // Update browser if initialized at this point.
    if (url.ValueChanged())
        LoadUrl(url.Get());
    if (size.ValueChanged())
    {
        // Do not allow to be less than 10x10
        if (size.Get().x() < 10 || size.Get().y() < 10)
        {
            QPoint s = size.Get();
            s.setX(Max<int>(s.x(), 10));
            s.setY(Max<int>(s.y(), 10));

            // Look out for endless loop, must use AttributeChange::Disconnected.
            size.Set(s, AttributeChange::Disconnected);
        }

        int width = size.Get().x();
        int height = size.Get().y();
        ResizeBrowser(width, height);
        ResizeTexture(width, height, false);
    }

    // Update input.
    if (mouseInput.ValueChanged() || keyboardInput.ValueChanged())
        CheckInput();

    if (IsReplicated() && synchronizedClient.ValueChanged())
    {
        TundraLogic::Client *client = TundraClient();
        controlling_ = (client ? ((int)client->ConnectionId() == synchronizedClient.Get()) : false);
        
        if (client)
        {
            if (synchronizedClient.Get() != -1)
            {
                connect(client, SIGNAL(NetworkMessageReceived(kNet::packet_id_t, kNet::message_id_t, const char*, size_t)), 
                    this, SLOT(OnClientMessageReceived(kNet::packet_id_t, kNet::message_id_t, const char*, size_t)), Qt::UniqueConnection);
            }
            else
            {
                disconnect(client, SIGNAL(NetworkMessageReceived(kNet::packet_id_t, kNet::message_id_t, const char*, size_t)), 
                    this, SLOT(OnClientMessageReceived(kNet::packet_id_t, kNet::message_id_t, const char*, size_t)));
            }
        }
        
        // Make everyone refresh the page if we are controlling
        if (controlling_)
        {
            TundraLogic::Client *client = TundraClient();
            if (client && client->GetConnection() && ParentEntity())
            {
                LoadUrl(url.Get());
                
                ECWebBrowserNetwork::MsgBrowserAction msgOut;
                msgOut.entId = ParentEntity()->Id();
                msgOut.compId = Id();
                msgOut.type = static_cast<u8>(BrowserProtocol::Url);
                
                QByteArray utf8 = url.Get().toUtf8();
                msgOut.payload.resize(utf8.size());
                memcpy(&msgOut.payload[0], utf8.data(), utf8.size());
                
                client->GetConnection()->Send(msgOut);
            }
        }
    }
}

TundraLogic::TundraLogicModule *EC_WebBrowser::TundraLogic() const
{
    return framework->GetModule<TundraLogic::TundraLogicModule>();
}

TundraLogic::Client *EC_WebBrowser::TundraClient() const
{
    TundraLogic::TundraLogicModule *logic = TundraLogic();
    return (logic ? logic->GetClient().get() : 0);
}

TundraLogic::Server *EC_WebBrowser::TundraServer() const
{
    TundraLogic::TundraLogicModule *logic = TundraLogic();
    return (logic ? logic->GetServer().get() : 0);
}

void EC_WebBrowser::Init()
{
    if (framework->IsHeadless())
        return;
        
    if (!permissionRun_)
        return;
        
    InitRendering();
    InitBrowser();
    CheckInput();
    
    connect(framework->Ui()->MainWindow(), SIGNAL(WindowResizeEvent(int, int)), this, SLOT(OnWindowResized(int, int)), Qt::UniqueConnection);
}

void EC_WebBrowser::Reset(bool immediateShutdown)
{
    if (framework->IsHeadless())
        return;

    RemoveMaterial();
    ResetRendering();
    ResetBrowser(immediateShutdown);
    ResetInput();
    
    disconnect(framework->Ui()->MainWindow(), SIGNAL(WindowResizeEvent(int, int)), this, SLOT(OnWindowResized(int, int)));
}

void EC_WebBrowser::InitInput()
{
    if (inputState_.inputContext.get())
        return;

    inputState_.Reset();
    inputState_.inputContext = framework->Input()->RegisterInputContext("EC_WebBrowser", 1000);
    inputState_.inputContext->SetTakeMouseEventsOverQt(true);
    inputState_.inputContext->SetTakeKeyboardEventsOverQt(true);

    connect(inputState_.inputContext.get(), SIGNAL(MouseEventReceived(MouseEvent*)), this, SLOT(OnMouseEventReceived(MouseEvent*)), Qt::UniqueConnection);
    connect(inputState_.inputContext.get(), SIGNAL(KeyEventReceived(KeyEvent*)), this, SLOT(OnKeyEventReceived(KeyEvent*)), Qt::UniqueConnection);
}

void EC_WebBrowser::CheckInput()
{
    if (keyboardInput.Get() || mouseInput.Get())
        InitInput();
    else
        ResetInput();
}

void EC_WebBrowser::ResetInput()
{
    if (inputState_.inputContext.get())
    {
        disconnect(inputState_.inputContext.get(), SIGNAL(MouseEventReceived(MouseEvent*)), this, SLOT(OnMouseEventReceived(MouseEvent*)));
        disconnect(inputState_.inputContext.get(), SIGNAL(KeyEventReceived(KeyEvent*)), this, SLOT(OnKeyEventReceived(KeyEvent*)));
    }

    inputState_.Reset();
}

void EC_WebBrowser::InitBrowser()
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
            LogError(LC + "Cannot start a browser no free ports for RocketWebBrowser process!");
            return;
        }

        if (ipcMemory.isAttached())
            ipcMemory.detach();
        
        QString id = IpcId(port);
        ipcMemory.setKey(id);
        
        uint sizeBytes = size.Get().x() * size.Get().y() * 4; // RGBA
        
        bool created = ipcMemory.create(sizeBytes);
        if (!created && ipcMemory.error() == QSharedMemory::AlreadyExists)
            ipcMemory.attach();
        if (ipcMemory.isAttached())
        {
            QStringList args;
            args << "--port" << QString::number(port) << "--id" << ipcMemory.key() << "--pid" << QString::number(QCoreApplication::applicationPid());

            connectionRetries_ = numberOfRetries;
            ipcSocket = new QTcpSocket();
            ipcSocket->setProperty("ipcport", port);
            connect(ipcSocket, SIGNAL(connected()), this, SLOT(OnIpcConnected()), Qt::UniqueConnection);
            connect(ipcSocket, SIGNAL(readyRead()), this, SLOT(OnIpcData()), Qt::UniqueConnection);
            connect(ipcSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(OnIpcError(QAbstractSocket::SocketError)), Qt::UniqueConnection);

            ipcProcess = new QProcess();
            if (IsLogChannelEnabled(LogChannelDebug))
            {
                args << "--debug";
                ipcProcess->setProcessChannelMode(QProcess::ForwardedChannels);
            }
            connect(ipcProcess, SIGNAL(started()), this, SLOT(OnIpcProcessStarted()), Qt::UniqueConnection);
            connect(ipcProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(OnIpcProcessClosed(int, QProcess::ExitStatus)), Qt::UniqueConnection);
            connect(ipcProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(OnIpcProcessError(QProcess::ProcessError)), Qt::UniqueConnection);

            QString webBrowserExePath = "RocketWebBrowser";
#ifdef __APPLE__
            // If RocketWebBrowser is in "bin" folder, it will be shown into the Mac dockbar, where users can quit the process with a simple right-click-Quit
            webBrowserExePath = "../Resources/" + webBrowserExePath;
#endif
            ipcProcess->start(QDir::fromNativeSeparators(Application::InstallationDirectory()) + webBrowserExePath, args);
        }
        else
            LogError(LC + "Failed to create required memory block of size: " + QString::number(sizeBytes) + " Error: " + ipcMemory.errorString());
    }
}

void EC_WebBrowser::OnIpcError(QAbstractSocket::SocketError socketError)
{
    LogDebug(LC + "An error occured while connecting to RocketWebBrowser process: " + QAbstractSocketErrorToString(socketError) + ".");
    if (!ipcProcess)
        return;

    // Try reconnecting
    if (ipcProcess->state() != QProcess::Running)
        return;

    if (connectionRetries_ != 0)
    {
        LogDebug(LC + "Retrying connection in 50ms, number of retries left: " + QString::number(connectionRetries_) + ".");
        QTimer::singleShot(50, this, SLOT(OnIpcProcessStarted()));
        connectionRetries_--;
    }
    else
    {
        LogError(LC + "Unable to connect to RocketWebBrowser process after " + QString::number(numberOfRetries) + " retries: " + QAbstractSocketErrorToString(socketError) + ".");
    }
}

void EC_WebBrowser::OnIpcProcessStarted()
{
    if (ipcSocket)
    {
        quint16 port = static_cast<quint16>(ipcSocket->property("ipcport").toUInt());
        ipcSocket->connectToHost(QHostAddress::LocalHost, port);
    }
}

void EC_WebBrowser::OnIpcProcessClosed(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (ipcProcess)
    {
        if (ipcMemory.isAttached())
            ipcMemory.detach();

        ipcProcess->deleteLater();
    }
    ipcProcess = 0;
}

void EC_WebBrowser::OnIpcProcessError(QProcess::ProcessError error)
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

void EC_WebBrowser::OnIpcConnected()
{
    emit BrowserCreated(this);

    ResizeBrowser(size.Get().x(), size.Get().y());
    LoadUrl(url.Get());
}

void EC_WebBrowser::ResetBrowser(bool immediateShutdown)
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

    currentUrl_ = "";
    emit BrowserClosed(this);
}

void EC_WebBrowser::InitRendering()
{
    if (framework->IsHeadless())
        return;
        
    // Create texture
    bool createTexture = true;
    if (!textureName_.empty())
        createTexture = Ogre::TextureManager::getSingleton().getByName(textureName_).isNull();
    if (createTexture)
    {
        QSize textureSize = ActualTextureSize();
        textureName_ = framework->Renderer()->GetUniqueObjectName("EC_WebBrowser_texture_");
        Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().createManual(textureName_, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, Max(1, textureSize.width()), Max(1, textureSize.height()), 0, Ogre::PF_A8R8G8B8, Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
        if (!texture.get())
            LogError(LC + "Failed to create target texture: " + textureName_.c_str());
    }
    
    // Create material
    bool createMaterial = true;
    if (!materialName_.empty())
        createMaterial = Ogre::MaterialManager::getSingleton().getByName(materialName_).isNull();
    if (createMaterial)
    {
        materialName_ = framework->Renderer()->GetUniqueObjectName("EC_WebBrowser_material_");
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

void EC_WebBrowser::ResetRendering()
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

void EC_WebBrowser::ApplyMaterial(bool force)
{
    EntityPtr target = RenderTarget();
    if (!target.get())
    {
        RemoveMaterial();
        return;
    }
    
    // Avoid removing and reapplying to exact same target.
    if (!force && currentTarget_.lock() == target && currentSubmesh_ == static_cast<int>(renderSubmeshIndex.Get()))
        return;
    
    EC_Mesh *mesh = target->Component<EC_Mesh>().get();
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
        if (!materialName_.empty())
            mesh->GetEntity()->getSubEntity(renderSubmeshIndex.Get())->setMaterialName(AssetAPI::SanitateAssetRef(materialName_));
        else
        {
            OgreMaterialAsset *matAsset = dynamic_cast<OgreMaterialAsset*>(framework->Asset()->GetAsset("Ogre Media:EC_WebBrowser.material").get());
            if (!matAsset || !matAsset->IsLoaded())
            {
                // Request our "waiting" asset
                AssetTransferPtr transfer = framework->Asset()->RequestAsset("Ogre Media:EC_WebBrowser.material");
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
    currentSubmesh_ = static_cast<int>(renderSubmeshIndex.Get());
}

void EC_WebBrowser::RemoveMaterial()
{
    // Previous target: when target entity changes.
    EC_Mesh *previousTarget = currentTarget_.lock().get() ? currentTarget_.lock()->Component<EC_Mesh>().get() : 0;
    if (previousTarget)
        RestoreMaterials(previousTarget);
    
    // Current target: when submesh index changes.
    EntityPtr target = RenderTarget();
    EC_Mesh *curretMesh = target.get() ? target->Component<EC_Mesh>().get() : 0;
    if (curretMesh && curretMesh != previousTarget)
        RestoreMaterials(curretMesh);

    // Reset tracking data, everything should be restored.    
    currentTarget_.reset();
    currentSubmesh_ = -1;
}

void EC_WebBrowser::RestoreMaterials(EC_Mesh *mesh)
{
    if (!mesh)
        return;
    
    // Disconnect targets signals. We must not be connected to MaterialChanged when calling ApplyMaterial, result is a infinite loop.
    disconnect(mesh, SIGNAL(MeshChanged()), this, SLOT(OnMeshAssetLoaded()));
    disconnect(mesh, SIGNAL(MaterialChanged(uint, const QString&)), this, SLOT(OnMeshMaterialChanged(uint, const QString&)));
    
    uint targetIndex = currentSubmesh_ > -1 ? static_cast<uint>(currentSubmesh_) : renderSubmeshIndex.Get();
    const AssetReferenceList &materials = mesh->materialRefs.Get();
    
    bool restoreEmptyMaterial = (int)targetIndex >= materials.Size();
    if (!restoreEmptyMaterial)
        if (materials[targetIndex].ref.trimmed().isEmpty())
            restoreEmptyMaterial = true;

    if (!restoreEmptyMaterial)
        mesh->ApplyMaterial();
    else
    {
        if (mesh->OgreEntity() && targetIndex < mesh->OgreEntity()->getNumSubEntities())
        {
            try
            {
                mesh->OgreEntity()->getSubEntity(targetIndex)->setMaterialName(OgreRenderer::Renderer::DefaultMaterialName);
            }
            catch(Ogre::Exception& e)
            {
                LogError(LC + "Failed to set material to target mesh '" + materialName_.c_str() + "': " + e.what());
                return;
            }
        }
    }
}

bool EC_WebBrowser::IsInView()
{
    return inView_;
}

bool EC_WebBrowser::IsInDistance()
{
    return inDistance_;
}

int EC_WebBrowser::DistanceToCamera(Entity *renderTarget)
{
    PROFILE(EC_WebBrowser_DistanceToCamera)

    if (!renderTarget)
        renderTarget = RenderTarget().get();
    if (framework->IsHeadless() || !renderTarget)
    {
        currentDistance_ = -1;
        return -1;
    }
        
    Entity *cameraEnt = framework->Renderer()->MainCamera();
    EC_Placeable *cameraPlaceable = cameraEnt ? cameraEnt->Component<EC_Placeable>().get() : 0;
    EC_Placeable *placeable = renderTarget->Component<EC_Placeable>().get();
    if (!placeable || !cameraPlaceable)
    {
        currentDistance_ = -1;
        return -1;
    }

    currentDistance_ = RoundInt(cameraPlaceable->WorldPosition().Distance(placeable->WorldPosition()));
    return currentDistance_;
}

/// @todo What are the unused width and height for? Remove if they're not used?
void EC_WebBrowser::Invalidate(int /*width*/, int /*height*/)
{
    /**
    if (width <= 0)
        width = size.Get().x();
    if (height <= 0)
        height = size.Get().y();
    */
    if (ipcSocket)
        ipcSocket->write(BrowserProtocol::TypedMessage(BrowserProtocol::Invalidate).Serialize());
}

void EC_WebBrowser::SetRunPermission(bool permitted)
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

float EC_WebBrowser::ActualTextureScale(int width, int height)
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

QSize EC_WebBrowser::ActualTextureSize(int width, int height)
{
    QSize actualSize(width > 0 ? width : size.Get().x(), height > 0 ? height : size.Get().y());
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

void EC_WebBrowser::SetMaterialIllumination(bool illuminating_)
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

void EC_WebBrowser::ResizeTexture(int width, int height, bool updateAfter)
{
    // Not initialized yet: Do nothing as it will be done when the time is right.
    if (textureName_.empty())
        return;

    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(textureName_);
    if (!texture.get())
        return;

    QSize textureSize = ActualTextureSize(Max<int>(1, width), Max<int>(1, height));
    if ((int)texture->getWidth() != textureSize.width() || (int)texture->getHeight() != textureSize.height())
    {
        texture->freeInternalResources();
        texture->setWidth(textureSize.width());
        texture->setHeight(textureSize.height());
        texture->createInternalResources();

        if (updateAfter)
            OnIpcPaint(QList<QRect>() << QRect(0, 0, width, height));
        Invalidate();
    }
}

void EC_WebBrowser::BlitToTexture(const QList<QRect> &dirtyRects, const QList<QByteArray> &buffers)
{
#if !defined(MESHMOON_SERVER_BUILD)
    PROFILE(EC_WebBrowser_BlitToTexture)

    if (textureName_.empty())
        return;
    Ogre::TexturePtr texture = Ogre::TextureManager::getSingleton().getByName(textureName_);
    if (!texture.get())
        return;
    Ogre::HardwarePixelBufferSharedPtr pb = texture->getBuffer();
    if (!pb.get())
        return;
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
                for (int i=0; i<dirtyRects.size(); ++i)
                {
                    const QRect &rect = dirtyRects[i];
                    const QByteArray &data = buffers[i];
                    QByteArray scaledData;
                    u8 *pixelData = (u8*)data.constData();
                    
                    // Texture is different size than the browser rendering. We need to scale.
                    RECT dxRect = { rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height() };
                    if ((int)texture->getWidth() != size.Get().x() || (int)texture->getHeight() != size.Get().y())
                    {
                        PROFILE(EC_WebBrowser_ScaleToTexture)

                        float scale = ActualTextureScale();
                        Ogre::PixelBox srcBox (rect.width(), rect.height(), 1, Ogre::PF_A8R8G8B8, (void*)pixelData);
                        Ogre::PixelBox destBox(FloorInt(rect.width()*scale), FloorInt(rect.height()*scale), 1, Ogre::PF_A8R8G8B8, 0);
                        if (destBox.getWidth() <= 0 || destBox.getHeight() <= 0)
                            continue; // Empty on a axis

                        dxRect.left = FloorInt(rect.x()*scale);
                        dxRect.top = FloorInt(rect.y()*scale);
                        dxRect.right = dxRect.left + (LONG)destBox.getWidth();
                        dxRect.bottom = dxRect.top + (LONG)destBox.getHeight();
                        if (dxRect.right - dxRect.left <= 0 || dxRect.bottom - dxRect.top <= 0)
                            continue; // Empty on a axis
                        
                        scaledData.resize((int)srcBox.getConsecutiveSize()); /// @todo Change to proper size.
                        destBox.data = (void*)scaledData.constData();
                        pixelData = (u8*)scaledData.constData();
                        
                        // Do not change FILTER_NEAREST! This is a lot faster than the other modes and the 
                        // quality does not matter if we are scaling. We are already so far away from the texture.
                        Ogre::Image::scale(srcBox, destBox, Ogre::Image::FILTER_NEAREST);

                        ELIFORP(EC_WebBrowser_ScaleToTexture)
                    }

                    D3DLOCKED_RECT lock;
                    int dxWidth = dxRect.right - dxRect.left;
                    int dxHeight = dxRect.bottom - dxRect.top;
                    HRESULT hr = surface->LockRect(&lock, &dxRect, 0);
                    if (SUCCEEDED(hr))
                    {
                        const int sourceStride = 4 * dxWidth; // bytes per pixel * width
                        if (lock.Pitch != sourceStride) // Almost nevelineBytesr hits: has to be full update and pow^2 width
                        {
                            for(int y=0; y<dxHeight; ++y)
                                memcpy((u8*)lock.pBits + (lock.Pitch * y), pixelData + (sourceStride * y), sourceStride);
                        }
                        else
                            memcpy(lock.pBits, (void*)pixelData, sourceStride * dxHeight);
                        surface->UnlockRect();
                    }
                }
                return;
            }
        }
    }
#elif !defined(MESHMOON_SERVER_BUILD)
    // OpenGL
    Ogre::GLTexture *glTexture = dynamic_cast<Ogre::GLTexture*>(texture.get());
    if (glTexture)
    {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, glTexture->getGLID());

        for (int i=0; i<dirtyRects.size(); ++i)
        {
            const QRect &rect = dirtyRects[i];
            const QByteArray &data = buffers[i];

            if ((int)texture->getWidth() != size.Get().x() || (int)texture->getHeight() != size.Get().y())
            {
                PROFILE(EC_WebBrowser_ScaleToTexture)

                float scale = ActualTextureScale();
                QSize newSize(FloorInt(rect.width() * scale), FloorInt(rect.height() * scale));

                QImage sourceImage((const uchar*)data.constData(), rect.width(), rect.height(), QImage::Format_ARGB32);
                QImage destImage = sourceImage.scaled(newSize);
                QRect scaledRect(destImage.rect());

                glTexSubImage2D(GL_TEXTURE_2D, 0, scaledRect.x(), scaledRect.y(), scaledRect.width(), scaledRect.height(),
                                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (void*)destImage.bits());
            }
            else
            {
                glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x(), rect.y(), rect.width(), rect.height(),
                                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, (void*)data.constData());
            }

        }
        
        glDisable(GL_TEXTURE_2D);
        return;
    }
#endif
}

void EC_WebBrowser::OnWindowResized(int newWidth, int newHeight)
{
    Invalidate();
}

void EC_WebBrowser::OnMouseEventReceived(MouseEvent *mouseEvent)
{
    if (!ipcSocket)
        return;
    if (!mouseInput.Get())
        return;
    // If we are too far away, don't handle input.
    if (!IsInDistance())
    {
        UnfocusBrowser();
        return;
    }
    if (synchronizedClient.Get() != -1 && !controlling_)
        return;
    EntityPtr target = RenderTarget();
    if (!target.get() || !ParentScene() || !ParentEntity())
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

    PROFILE(EC_WebBrowser_OnMouseEventReceived)

    Ray mouseRay = mainCamera->ScreenPointToRay(mouseEvent->x, mouseEvent->y);
    if (!mouseRay.Intersects(mesh->WorldOBB()))
    {
        UnfocusBrowser();
        return;
    }

    RaycastResult *result = world->Raycast(mouseRay, 0xffffffff); /// @todo Get layer from targets placeable?
    if (result->entity != target.get() || result->submesh != renderSubmeshIndex.Get() || !framework->Input()->IsMouseCursorVisible())
    {
        UnfocusBrowser();
        return;
    }

    if (!inputState_.focus)
        FocusBrowser();

    QPoint posBrowser((float)size.Get().x()*result->u, (float)size.Get().y()*result->v);
    switch(mouseEvent->eventType)
    {
        case MouseEvent::MouseMove:
        {
            PROFILE(EC_WebBrowser_OnMouseEventReceived_Move)
            
            BrowserProtocol::MouseMoveMessage msg(posBrowser.x(), posBrowser.y());
            ipcSocket->write(msg.Serialize());
            
            if (controlling_)
            {
                // Synced in OnUpdate at a steady and relatively slow pace.
                syncMoveMsg_.entId = ParentEntity()->Id();
                syncMoveMsg_.compId = Id();
                syncMoveMsg_.x = posBrowser.x();
                syncMoveMsg_.y = posBrowser.y();
                syncMoveMsg_.send = true;
            }
            
            mouseEvent->Suppress();
            break;
        }
        case MouseEvent::MouseScroll:
        {
            PROFILE(EC_WebBrowser_OnMouseEventReceived_Scroll)
            
            BrowserProtocol::MouseButtonMessage msg(posBrowser.x(), posBrowser.y(), mouseEvent->relativeZ, true, true, true);
            ipcSocket->write(msg.Serialize());
            
            if (controlling_)
            {
                TundraLogic::Client *client = TundraClient();
                if (client && client->GetConnection())
                {
                    ECWebBrowserNetwork::MsgMouseButton msgOut;
                    msgOut.entId = ParentEntity()->Id();
                    msgOut.compId = Id();
                    msgOut.x = posBrowser.x();
                    msgOut.y = posBrowser.y();
                    msgOut.button = static_cast<u8>(255);
                    msgOut.pressCount = static_cast<s16>(mouseEvent->relativeZ);
                    syncMoveMsg_.send = false;
                    client->GetConnection()->Send(msgOut);
                }
            }
            
            mouseEvent->Suppress();
            break;
        }
        case MouseEvent::MousePressed:
        case MouseEvent::MouseDoubleClicked:
        {
            PROFILE(EC_WebBrowser_OnMouseEventReceived_Press)
            
            BrowserProtocol::MouseButtonMessage msg(posBrowser.x(), posBrowser.y(), (mouseEvent->eventType != MouseEvent::MouseDoubleClicked ? 1 : 2));
            msg.left = (mouseEvent->button == MouseEvent::LeftButton);
            msg.right = (mouseEvent->button == MouseEvent::RightButton);
            msg.mid = (mouseEvent->button == MouseEvent::MiddleButton);
            if (msg.left) inputState_.clickFocus = true;
            ipcSocket->write(msg.Serialize());
            
            // We don't sync middle button as it will 
            // open popup windows on other clients.
            if (controlling_ && !msg.mid)
            {
                TundraLogic::Client *client = TundraClient();
                if (client && client->GetConnection())
                {
                    ECWebBrowserNetwork::MsgMouseButton msgOut;
                    msgOut.entId = ParentEntity()->Id();
                    msgOut.compId = Id();
                    msgOut.x = posBrowser.x();
                    msgOut.y = posBrowser.y();
                    msgOut.button = static_cast<u8>(mouseEvent->button);
                    msgOut.pressCount = static_cast<s16>(msg.pressCount);
                    syncMoveMsg_.send = false;
                    client->GetConnection()->Send(msgOut);
                }
            }

            mouseEvent->Suppress();
            break;
        }
        case MouseEvent::MouseReleased:
        {
            PROFILE(EC_WebBrowser_OnMouseEventReceived_Release)
            
            BrowserProtocol::MouseButtonMessage msg(posBrowser.x(), posBrowser.y(), 0);
            msg.left = (mouseEvent->button == MouseEvent::LeftButton);
            msg.right = (mouseEvent->button == MouseEvent::RightButton);
            msg.mid = (mouseEvent->button == MouseEvent::MiddleButton);
            ipcSocket->write(msg.Serialize());
            
            // We don't sync middle button as it will 
            // open popup windows on other clients.
            if (controlling_ && !msg.mid)
            {
                TundraLogic::Client *client = TundraClient();
                if (client && client->GetConnection() )
                {
                    ECWebBrowserNetwork::MsgMouseButton msgOut;
                    msgOut.entId = ParentEntity()->Id();
                    msgOut.compId = Id();
                    msgOut.x = posBrowser.x();
                    msgOut.y = posBrowser.y();
                    msgOut.button = static_cast<u8>(mouseEvent->button);
                    msgOut.pressCount = static_cast<s16>(msg.pressCount);
                    syncMoveMsg_.send = false;
                    client->GetConnection()->Send(msgOut);
                }
            }

            mouseEvent->Suppress();
            break;
        }
        default:
            break;
    }
}

void EC_WebBrowser::OnKeyEventReceived(KeyEvent *keyEvent)
{
    if (!ipcSocket)
        return;
    if (!keyboardInput.Get())
        return;
    if (!inputState_.clickFocus)
        return;
    if (!keyEvent || !keyEvent->qtEvent)
        return;

    PROFILE(EC_WebBrowser_OnKeyEventReceived)
    QKeyEvent *e = keyEvent->qtEvent;
 
    quint8 type = 1;    // KT_KEYDOWN
    if (e->type() == QEvent::KeyRelease)
        type = 0;       // KT_KEYUP

#ifdef ROCKET_CEF3_ENABLED
    // See cef/cefclient/cefclient_osr_widget_win.cpp
    QChar key = e->text().at(0);

#ifdef Q_WS_WIN
    BrowserProtocol::KeyboardMessage msg(
        static_cast<int>(e->nativeVirtualKey()), // key
        static_cast<int>(e->nativeVirtualKey()), // nativeKey
        static_cast<int>(key.unicode()), // character
        static_cast<int>(key.toLower().unicode()), // characterNoModifiers
        e->modifiers(), // modifiers
        type
    );
    ipcSocket->write(msg.Serialize());

    if (type == 1)
    {
        BYTE state[256]; memset(&state[0], 0, 256);
        if (GetKeyboardState(&state[0]))
        {
            QChar unicodeBuffer[5];
            int size = ToUnicode(e->nativeVirtualKey(), e->nativeScanCode(), &state[0], reinterpret_cast<LPWSTR>(unicodeBuffer), 5, 0);
            if (size > 0)
                msg.key = unicodeBuffer[0].unicode();
            else
            {
                /// @todo Here be some bugs eg for right arrow and other special keys.
                msg.key = msg.characterNoModifiers;
            }
        }
        msg.type = 2; // KT_CHAR
        ipcSocket->write(msg.Serialize());
    }
#else
#error todo KeyboardMessage
#endif
#else
#ifdef Q_WS_WIN
    BrowserProtocol::KeyboardMessage msg(static_cast<int>(e->nativeVirtualKey()), static_cast<int>(e->nativeModifiers()), type);
    ipcSocket->write(msg.Serialize());
    if (type == 1)      // KT_KEYDOWN
    {
        BYTE state[256]; memset(&state[0], 0, 256);
        if (GetKeyboardState(&state[0]))
        {
            QChar unicodeBuffer[5];
            int size = ToUnicode(e->nativeVirtualKey(), e->nativeScanCode(), &state[0], reinterpret_cast<LPWSTR>(unicodeBuffer), 5, 0);
            if (size > 0)
            {
                BrowserProtocol::KeyboardMessage msgChar(unicodeBuffer[0].unicode(), static_cast<int>(e->nativeModifiers()), 2); // KT_CHAR
                ipcSocket->write(msgChar.Serialize());
            }
        }
    }
#elif (__APPLE__)
    BrowserProtocol::KeyboardMessage msg(static_cast<int>(e->nativeVirtualKey()), 0, 0, static_cast<int>(keyEvent->qtEvent->nativeModifiers()), type);
    ipcSocket->write(msg.Serialize());
    if (type == 1)      // KT_KEYDOWN
    {
        BrowserProtocol::KeyboardMessage msg(static_cast<int>(e->nativeVirtualKey()), static_cast<int>(keyEvent->qtEvent->text().at(0).unicode()), 
            static_cast<int>(keyEvent->qtEvent->text().at(0).toLower().unicode()), static_cast<int>(keyEvent->qtEvent->nativeModifiers()), 2); // KT_CHAR
        ipcSocket->write(msg.Serialize());
    }
#endif
#endif
    keyEvent->Suppress();
}

void EC_WebBrowser::FocusBrowser()
{
    if (inputState_.focus || framework->IsHeadless())
        return;

    if (ipcSocket)
    {
        ipcSocket->write(BrowserProtocol::TypedMessage(BrowserProtocol::FocusIn).Serialize());
        inputState_.focus = true;
        
        if (controlling_)
        {
            TundraLogic::Client *client = TundraClient();
            if (client && client->GetConnection() && ParentEntity())
            {
                ECWebBrowserNetwork::MsgBrowserAction msgOut;
                msgOut.entId = ParentEntity()->Id();
                msgOut.compId = Id();
                msgOut.type = static_cast<u8>(BrowserProtocol::FocusIn);
                client->GetConnection()->Send(msgOut);
            }
        }
    }
}

void EC_WebBrowser::UnfocusBrowser()
{
    if (!inputState_.focus || framework->IsHeadless())
        return;

    if (ipcSocket)
    {
        ipcSocket->write(BrowserProtocol::TypedMessage(BrowserProtocol::FocusOut).Serialize());
        inputState_.focus = false;
        inputState_.clickFocus = false;
        
        if (controlling_)
        {
            TundraLogic::Client *client = TundraClient();
            if (client && client->GetConnection() && ParentEntity())
            {
                ECWebBrowserNetwork::MsgBrowserAction msgOut;
                msgOut.entId = ParentEntity()->Id();
                msgOut.compId = Id();
                msgOut.type = static_cast<u8>(BrowserProtocol::FocusOut);
                client->GetConnection()->Send(msgOut);
            }
        }
    }    
}

void EC_WebBrowser::OnEntityEnterView(Entity *ent)
{
    if (ent == ParentEntity() && !inView_)
    {
        inView_ = true;
        Invalidate();
        OnUpdate(1.0f); // Checks distance
    }
}

void EC_WebBrowser::OnEntityLeaveView(Entity *ent)
{
    if (ent == ParentEntity())
        inView_ = false;
}

void EC_WebBrowser::OnUpdate(float frametime)
{
    PROFILE(EC_WebBrowser_Update)
    if (!inView_)
        return;
        
    if (controlling_)
    {    
        syncDelta_ += frametime;
        if (syncDelta_ >= 0.05f)
        {
            PROFILE(EC_WebBrowser_SyncMouseMove)
            syncDelta_ = 0.0f;
            if (syncMoveMsg_.send)
            {
                TundraLogic::Client *client = TundraClient();
                if (client && client->GetConnection())
                    client->GetConnection()->Send(syncMoveMsg_);
                syncMoveMsg_.send = false;
            }
        }
    }

    updateDelta_ += frametime;
    if (updateDelta_ < 0.5f)
        return;
    updateDelta_ = 0.0f;
    
    // Check distance. If we control this frame we are always in distance.
    // This ensures rendering and mouse event handling from all distances.
    int distance = DistanceToCamera();
    bool inDistanceNow = controlling_ ? true : (distance <= 30 && distance >= 0);
    if (inDistance_ != inDistanceNow)
    {
        inDistance_ = inDistanceNow;
        if (inDistance_)
            Invalidate();
    }
    
    // Resizes texture if there are changes.
    ResizeTexture(size.Get().x(), size.Get().y(), true);
}

void EC_WebBrowser::OnServerMessageReceived(UserConnection *connection, kNet::packet_id_t, kNet::message_id_t id, const char* data, size_t numBytes)
{
    // At this point, do not allow web clients to send these messages (for whatever reason).
    // Remove this is one day there is a sensible web implementation of mouse/key actions!
    if (connection->ConnectionType() != "knet")
        return;

    if (id == ECWebBrowserNetwork::MsgMouseMove::messageID)
    {
        if (!ParentEntity())
            return;

        ECWebBrowserNetwork::MsgMouseMove msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            TundraLogic::Server *server = TundraServer();
            UserConnectionList clients = server ? server->AuthenticatedUsers() : UserConnectionList();
            for(UserConnectionList::const_iterator iter = clients.begin(); iter != clients.end(); ++iter)
            {
                UserConnectionPtr client = (*iter);
                if (client.get() && client.get() != connection && client->ConnectionType() == "knet") // Add sending to web client if one day needed there
                    client->Send(msg);
            }
        }
    }
    else if (id == ECWebBrowserNetwork::MsgMouseButton::messageID)
    {
        if (!ParentEntity())
            return;

        ECWebBrowserNetwork::MsgMouseButton msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            TundraLogic::Server *server = TundraServer();
            UserConnectionList clients = server ? server->AuthenticatedUsers() : UserConnectionList();
            for(UserConnectionList::const_iterator iter = clients.begin(); iter != clients.end(); ++iter)
            {
                UserConnectionPtr client = (*iter);
                if (client.get() && client.get() != connection && client->ConnectionType() == "knet") // Add sending to web client if one day needed there
                    client->Send(msg);
            }
        }
    }
    else if (id == ECWebBrowserNetwork::MsgBrowserAction::messageID)
    {
        if (!ParentEntity())
            return;

        ECWebBrowserNetwork::MsgBrowserAction msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            TundraLogic::Server *server = TundraServer();
            UserConnectionList clients = server ? server->AuthenticatedUsers() : UserConnectionList();
            for(UserConnectionList::const_iterator iter = clients.begin(); iter != clients.end(); ++iter)
            {
                UserConnectionPtr client = (*iter);
                if (client.get() && client.get() != connection && client->ConnectionType() == "knet") // Add sending to web client if one day needed there
                    client->Send(msg);
            }
        }
    }
}

void EC_WebBrowser::OnClientMessageReceived(kNet::packet_id_t, kNet::message_id_t id, const char *data, size_t numBytes)
{
    if (id == ECWebBrowserNetwork::MsgMouseMove::messageID)
    {
        if (!ipcSocket || !ParentEntity())
            return;
            
        PROFILE(EC_WebBrowser_MsgMouseMove)

        ECWebBrowserNetwork::MsgMouseMove msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            BrowserProtocol::MouseMoveMessage msgFwd(msg.x, msg.y);
            ipcSocket->write(msgFwd.Serialize());
        }
    }
    else if (id == ECWebBrowserNetwork::MsgMouseButton::messageID)
    {
        if (!ipcSocket || !ParentEntity())
            return;

        PROFILE(EC_WebBrowser_MsgMouseButton)
        
        ECWebBrowserNetwork::MsgMouseButton msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            if (msg.button != 255)
            {
                Qt::MouseButton button = (Qt::MouseButton)msg.button;
                BrowserProtocol::MouseButtonMessage msgFwd(msg.x, msg.y, msg.pressCount);
                msgFwd.left = (button == Qt::LeftButton);
                msgFwd.right = (button == Qt::RightButton);
                msgFwd.mid = (button == Qt::MiddleButton);
                ipcSocket->write(msgFwd.Serialize());
            }
            // 255 is a wheel event
            else 
            {
                FocusBrowser();
                BrowserProtocol::MouseButtonMessage msgFwd(msg.x, msg.y, msg.pressCount, true, true, true);
                ipcSocket->write(msgFwd.Serialize());
            }
        }
    }
    else if (id == ECWebBrowserNetwork::MsgBrowserAction::messageID)
    {
        if (!ParentEntity())
            return;
            
        PROFILE(EC_WebBrowser_MsgBrowserAction)

        ECWebBrowserNetwork::MsgBrowserAction msg(data, numBytes);
        if (ParentEntity()->Id() == msg.entId && Id() == msg.compId)
        {
            BrowserProtocol::MessageType type = (BrowserProtocol::MessageType)msg.type;

            if (type == BrowserProtocol::FocusIn)
                FocusBrowser();
            else if (type == BrowserProtocol::FocusOut)
                UnfocusBrowser();
            else if (type == BrowserProtocol::Url && msg.payload.size() > 0)
            {
                QString url = QString::fromUtf8((const char*)&msg.payload[0], (int)msg.payload.size()).trimmed();
                if (url.compare(currentUrl_, Qt::CaseInsensitive) == 0)
                    return;
                LoadUrl(url);
            }
        }
    }
}

// InputState

EC_WebBrowser::InputState::InputState()
{
    Reset();
}

void EC_WebBrowser::InputState::Reset()
{
    focus = false;
    clickFocus = false;
    inputContext.reset();
}
