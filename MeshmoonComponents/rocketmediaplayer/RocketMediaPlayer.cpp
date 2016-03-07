/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "RocketMediaPlayer.h"
#include "MediaPlayerNetwork.h"

#ifdef Q_OS_WIN
#include "windows.h"
#include <ShTypes.h>
#include <ShlObj.h>
#endif

#ifdef __APPLE__
#include <sys/types.h>
#include <signal.h>
#endif

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QVarLengthArray>

#include <QTime>
#include <QTimer>
#include <QDir>
#include <QLatin1Literal>
#include <QDebug>

RocketMediaPlayer::RocketMediaPlayer(int &argc, char **argv) :
    network(0),
    vlcInstance_(0),
    vlcPlayer_(0),
    vlcMedia_(0),
    looping(false),
    debugging(false),
    pendingVolume_(-1),
    parentProcessId(-1),
    state(MediaPlayerProtocol::PlayerState())
{
    connect(this, SIGNAL(SendDirty()), this, SLOT(OnSendDirty()), Qt::QueuedConnection);
    connect(this, SIGNAL(SendStatus()), this, SLOT(OnSendStatus()), Qt::QueuedConnection);
    connect(this, SIGNAL(ReloadMediaRequest(bool)), this, SLOT(ReloadMedia(bool)), Qt::QueuedConnection);
    connect(this, SIGNAL(SendErrorRequest()), this, SLOT(SendError()), Qt::QueuedConnection);
    connect(this, SIGNAL(CheckChildMedia()), this, SLOT(OnCheckChildMedia()), Qt::QueuedConnection);

    bool nextIsPort = false;
    bool nextIsPid = false;
    for(int i=1; i<argc; ++i)
    {
        QString param = argv[i];
        
        // Port
        if (param == "--port")
            nextIsPort = true;
        else if (nextIsPort)
        {
            network = new MediaPlayerNetwork(this, param.toUShort());
            if (!network->server->isListening())
                return;
            nextIsPort = false;
        }

        if (param == "--pid")
            nextIsPid = true;
        else if (nextIsPid)
        {
            parentProcessId = param.toLongLong();
            nextIsPid = false;
        }

        // Debugging
        if (param == "--debug")
            debugging = true;
    }
   
    // All params present
    if (IsStarted())
    {
        /// Generate arguments
        QList<QByteArray> args;
        args << QByteArray("--intf=dummy");
        if (debugging)
            args << QByteArray("--verbose=2");

        // --plugin-path lib startup parameter is only supported and needed in VLC 1.x.
        // For >=2.0.0 the plugins are located in <tundra_install_dir>/plugins/vlcplugins.
        // VLC will always look recursively for plugins (5 levels) from /plugins.
        QString vlcLibVersion(libvlc_get_version());
        if (vlcLibVersion.startsWith("1"))
        {
            QString folderToFind = "vlcplugins";
            QDir pluginDir("./");

            if (pluginDir.exists(folderToFind))
                pluginDir.cd(folderToFind);

            // Validate
            if (!pluginDir.absolutePath().endsWith(folderToFind))
                qDebug() << "WARNING - RocketMediaPlayer: Cannot find vlcplugins folder for plugins, starting without specifying plugin path.";

            // Set plugin path to start params
            QString pluginPath = QLatin1Literal("--plugin-path=") % QDir::toNativeSeparators(pluginDir.absolutePath());
            args << QFile::encodeName(pluginPath);
        }

        QVarLengthArray<const char*, 64> vlcArgs(args.size());
        for (int i = 0; i < args.size(); ++i)
            vlcArgs[i] = args.at(i).constData();

        // Create new libvlc instance
        vlcInstance_ = libvlc_new(vlcArgs.size(), vlcArgs.constData());

        // Check if instance is running
        if (!vlcInstance_)
        {
            if (libvlc_errmsg())
                qDebug() << "ERROR - RocketMediaPlayer: Failed to create VLC instance: " << libvlc_errmsg();
            else
                qDebug() << "ERROR - RocketMediaPlayer: Failed to create VLC instance";
            return;
        }
        
        libvlc_set_user_agent(vlcInstance_, "Meshmoon Rocket Media Player 1.0", "Rocket/1.0");
        
        // Create the vlc player and set event callbacks
        vlcPlayer_ = libvlc_media_player_new(vlcInstance_);
        libvlc_event_manager_t *em = libvlc_media_player_event_manager(vlcPlayer_);
        libvlc_event_attach(em, libvlc_MediaPlayerMediaChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerOpening, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerBuffering, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPlaying, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPaused, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerStopped, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerSeekableChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerPausableChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, &VlcEventHandler, this);
        libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, &VlcEventHandler, this);

        // Register callbacks so that we can implement custom drawing of video frames
        libvlc_video_set_callbacks(vlcPlayer_, &CallBackLock, &CallBackUnlock, &CallBackDisplay, this);

        timerId_ = startTimer(20);

        if (pendingVolume_ > -1)
            SetVolume(pendingVolume_);
        
        // Start monitoring parent Rocket process
        if (parentProcessId >= 0)
        {
            connect(&mainProcessPoller_, SIGNAL(timeout()), SLOT(OnCheckMainProcessAlive()));
            mainProcessPoller_.start(5000);
        }
    }
    else
        qDebug() << "ERROR - RocketMediaPlayer: Incomplete input params!";
}

RocketMediaPlayer::~RocketMediaPlayer()
{
    qDebug() << "RocketMediaPlayer::~RocketMediaPlayer";
    Cleanup();
}

void RocketMediaPlayer::Cleanup()
{
    if (vlcMedia_)
    {
        libvlc_media_release(vlcMedia_);
        vlcMedia_ = 0;
    }
    if (vlcPlayer_)
    {
        libvlc_media_player_stop(vlcPlayer_);
        libvlc_media_player_release(vlcPlayer_);
        vlcPlayer_ = 0;
    }
    if (vlcInstance_)
    {
        libvlc_release(vlcInstance_);   
        vlcInstance_ = 0;
    }
    
    if (memory.isAttached())
        memory.detach();
}

void RocketMediaPlayer::ConnectionLost()
{
    qDebug() << "RocketMediaPlayer::ConnectionLost";
    ExitProcess();
}

void RocketMediaPlayer::OnCheckMainProcessAlive()
{
#ifdef __APPLE__
    if (!kill(parentProcessId, 0))
    {
        if (debugging)
            qDebug() << QString("Status report from %1: Rocket (pid: %2) lives").arg(id).arg(parentProcessId);
        return;
    }
    else
    {
        if (debugging)
            qDebug() << "ROCKET APP UNEXPECTEDLY QUIT! I WILL KILL MYSELF GENTLY, FOR I CANNOT LIVE WITHOUT ROCKET";
        ExitProcess();
    }
#endif
}

void RocketMediaPlayer::ExitProcess()
{
    qDebug() << "RocketMediaPlayer::ExitProcess";
    QCoreApplication::quit();
}

void RocketMediaPlayer::SendError(QString error)
{
    if (!network || !network->client)
        return;

    if (error.isEmpty())
    {
        error = "Unknown error";
        if (libvlc_errmsg())
        {
            error = libvlc_errmsg();
            libvlc_clearerr();
        }
    }
    network->client->socket->write(MediaPlayerProtocol::PlayerErrorMessage(error).Serialize());
}

void RocketMediaPlayer::OnTypedRequest(MediaPlayerProtocol::MessageType type)
{
    if (!Initialized())
        return;
    
    switch(type)
    {
        case MediaPlayerProtocol::Play:
        {
            Play();
            break;
        }
        case MediaPlayerProtocol::Pause:
        {
            Pause();
            break;
        }
        case MediaPlayerProtocol::PlayPauseToggle:
        {
            TogglePlay();
            break;
        }
        case MediaPlayerProtocol::Stop:
        {
            Stop();
            break;
        }
    }
}

void RocketMediaPlayer::OnTypedBooleanRequest(const MediaPlayerProtocol::TypedBooleanMessage &msg)
{
    if (msg.type == MediaPlayerProtocol::Looping)
        looping = msg.value;
}

void RocketMediaPlayer::OnSourceRequest(const MediaPlayerProtocol::SourceMessage &msg)
{   
    QByteArray source = msg.source;
    if (QString(source).startsWith("file://", Qt::CaseInsensitive))
    {
        source = source.mid(7);
        vlcMedia_ = libvlc_media_new_path(vlcInstance_, source.constData());
    }
    else
        vlcMedia_ = libvlc_media_new_location(vlcInstance_, source.constData());
    if (vlcMedia_ == 0)
    {
        SendError("Failed to load media from '" + source + "'");
        return;
    }

    size = QSize(0,0);
    state = MediaPlayerProtocol::PlayerState();
    totalTime_ = "";

    libvlc_event_manager_t *em = libvlc_media_event_manager(vlcMedia_);
    libvlc_event_attach(em, libvlc_MediaMetaChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaSubItemAdded, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaDurationChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaParsedChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaFreed, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaStateChanged, &VlcEventHandler, this);

    libvlc_state_t state = libvlc_media_get_state(vlcMedia_);

    if (state != libvlc_Error)
        libvlc_media_player_set_media(vlcPlayer_, vlcMedia_);
    else
        SendError();
}

void RocketMediaPlayer::OnSendDirty()
{
    if (!memory.isAttached())
        return;
    if (state.state == MediaPlayerProtocol::Paused)
        return;
    
    if (network && network->client)
        network->client->socket->write(MediaPlayerProtocol::TypedMessage(MediaPlayerProtocol::Dirty).Serialize());
}

void RocketMediaPlayer::OnSendStatus()
{
    if (network && network->client)
    {
        MediaPlayerProtocol::StateUpdateMessage msg(state);
        network->client->socket->write(msg.Serialize());
    }
}

void RocketMediaPlayer::OnCheckChildMedia()
{
    if (!vlcPlayer_)
        return;

    // As long as the playlist item is playing, we are going to receive 
    // new child media items. Wait for all of them to be parsed before starting playback.
    bool isPlaying = (libvlc_media_player_is_playing(vlcPlayer_) == 1);
    if (isPlaying)
        return;

    mutexChildMedia_.lock();
    qDebug() << "RocketMediaPlayer: Playlist parsed with" << pendingChildMedia_.size() << "items. Starting playback for the first media.";
    foreach(libvlc_media_t *media, pendingChildMedia_)
    {
        if (!isPlaying)
        {
            isPlaying = true;

            // Load media
            libvlc_media_player_set_media(vlcPlayer_, media);
            libvlc_media_player_play(vlcPlayer_);
            
            // Release last media and refresh ptr
            if (vlcMedia_)
            {
                // Unregister events from the "playlist" media
                libvlc_event_manager_t *em = libvlc_media_event_manager(vlcMedia_);
                libvlc_event_detach(em, libvlc_MediaMetaChanged, &VlcEventHandler, this);
                libvlc_event_detach(em, libvlc_MediaSubItemAdded, &VlcEventHandler, this);
                libvlc_event_detach(em, libvlc_MediaDurationChanged, &VlcEventHandler, this);
                libvlc_event_detach(em, libvlc_MediaParsedChanged, &VlcEventHandler, this);
                libvlc_event_detach(em, libvlc_MediaFreed, &VlcEventHandler, this);
                libvlc_event_detach(em, libvlc_MediaStateChanged, &VlcEventHandler, this);
                
                libvlc_media_release(vlcMedia_);
            }
            vlcMedia_ = media;
            
            // Register events to the playlists child media.
            libvlc_event_manager_t *em = libvlc_media_event_manager(vlcMedia_);
            libvlc_event_attach(em, libvlc_MediaMetaChanged, &VlcEventHandler, this);
            libvlc_event_attach(em, libvlc_MediaSubItemAdded, &VlcEventHandler, this);
            libvlc_event_attach(em, libvlc_MediaDurationChanged, &VlcEventHandler, this);
            libvlc_event_attach(em, libvlc_MediaParsedChanged, &VlcEventHandler, this);
            libvlc_event_attach(em, libvlc_MediaFreed, &VlcEventHandler, this);
            libvlc_event_attach(em, libvlc_MediaStateChanged, &VlcEventHandler, this);
        }
    }
    pendingChildMedia_.clear();
    mutexChildMedia_.unlock();
}

void RocketMediaPlayer::Play() 
{
    if (!vlcPlayer_)
        return;
    if (!libvlc_media_player_is_playing(vlcPlayer_))
        libvlc_media_player_play(vlcPlayer_);
}

void RocketMediaPlayer::Pause() 
{
    if (!vlcPlayer_)
        return;
    if (libvlc_media_player_can_pause(vlcPlayer_))
        libvlc_media_player_pause(vlcPlayer_);
}

void RocketMediaPlayer::TogglePlay()
{
    if (!vlcPlayer_)
        return;
    if (!libvlc_media_player_is_playing(vlcPlayer_))
        libvlc_media_player_play(vlcPlayer_);
    else
        libvlc_media_player_pause(vlcPlayer_);
}

void RocketMediaPlayer::Stop() 
{
    if (!vlcPlayer_)
        return;
    libvlc_state_t state = GetMediaState();
    if (state == libvlc_Playing || state == libvlc_Paused || state == libvlc_Ended)
        libvlc_media_player_stop(vlcPlayer_);
}

void RocketMediaPlayer::Seek(qint64 msec)
{
    if (!vlcPlayer_)
        return;

    if (libvlc_media_player_is_seekable(vlcPlayer_))
        libvlc_media_player_set_time(vlcPlayer_, msec);
}

void RocketMediaPlayer::OnApplyPendingVolume()
{
    if (pendingVolume_ > -1)
        SetVolume(pendingVolume_);
}

void RocketMediaPlayer::SetVolume(int volume)
{
    if (vlcPlayer_)
    {
        if (libvlc_audio_set_volume(vlcPlayer_, volume) != -1)
            pendingVolume_ = -1;
        else
        {
            // Try to apply the pending volume as long as its applied correctly
            pendingVolume_ = volume;
            QTimer::singleShot(1, this, SLOT(OnApplyPendingVolume()));
        }
    }
    else
        pendingVolume_ = volume;
}

void RocketMediaPlayer::ReloadMedia(bool startPlayback)
{
    if (!Initialized() || !vlcMedia_)
        return;
    
    // Stop and release player
    Stop();
    libvlc_media_player_release(vlcPlayer_);

    // New player with current media.
    vlcPlayer_ = libvlc_media_player_new_from_media(vlcMedia_);

    libvlc_event_manager_t *em = libvlc_media_player_event_manager(vlcPlayer_);
    libvlc_event_attach(em, libvlc_MediaPlayerMediaChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerOpening, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerBuffering, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPlaying, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPaused, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerStopped, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerTimeChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPositionChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerSeekableChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerPausableChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerTitleChanged, &VlcEventHandler, this);
    libvlc_event_attach(em, libvlc_MediaPlayerLengthChanged, &VlcEventHandler, this);

    // Size is determined for media that has video.
    if (!size.isNull())
    {
        // Setup video callbacks, format and size.
        libvlc_video_set_callbacks(vlcPlayer_, &CallBackLock, &CallBackUnlock, &CallBackDisplay, this);
        libvlc_video_set_format(vlcPlayer_, "RV32", size.width(), size.height(), size.width()*4);
    }
    
    if (pendingVolume_ > -1)
        SetVolume(pendingVolume_);

    // Play or send the stopped state
    if (startPlayback)
        Play();
}

libvlc_state_t RocketMediaPlayer::GetMediaState() const
{
    if (!Initialized() || !vlcMedia_)
        return libvlc_Error;
    return libvlc_media_get_state(vlcMedia_);
}

bool RocketMediaPlayer::Initialized() const
{
    if (!vlcInstance_ || !vlcPlayer_)
        return false;
    return true;
}

bool RocketMediaPlayer::IsStarted() const
{
    if (!network)
        return false;
    return true;
}

QString QStringfromWCharArray(const wchar_t *string, int size)
{
    QString qstr;
    if (sizeof(wchar_t) == sizeof(QChar))
        return qstr.fromUtf16((const ushort *)string, size);
    else
        return qstr.fromUcs4((uint *)string, size);
}

QString WStringToQString(const std::wstring &str)
{
    if (str.length() == 0)
        return "";
    return QStringfromWCharArray(str.data(), str.size());
}

QString RocketMediaPlayer::UserDataDirectory()
{
#ifdef WIN32
    LPITEMIDLIST pidl;

    if (SHGetFolderLocation(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, &pidl) != S_OK)
        return "";

    WCHAR str[MAX_PATH+1] = {};
    SHGetPathFromIDListW(pidl, str);
    CoTaskMemFree(pidl);

    return WStringToQString(str) + "\\" + "Rocket";
#else
    ///\todo Convert to QString instead of std::string.
    char *ppath = 0;
    ppath = getenv("HOME");
    if (ppath == 0)
        return "~";

    return QString(ppath) + "/." + "Rocket";
#endif
}

void RocketMediaPlayer::timerEvent(QTimerEvent * /*e*/)
{
    if (!Initialized() || !vlcMedia_)
        return;
    if (!size.isNull())
        return;

    // This functions sole purpose is to poll VLC about the media dimensions.
    // Only when we know the dimensions we can reserve the shared memory.
    uint w = 0, h = 0;
    if (libvlc_video_get_size(vlcPlayer_, 0, &w, &h) == 0)
    {
        size = QSize(w, h);
  
        if (memory.isAttached())
            memory.detach();
            
        QString id = QString("vlc_ipc_mem_") + QString::number(network->server->serverPort());
        memory.setKey(id);
        
        // For non video sources reserve 1 byte.
        int sizeBytes = w*h*4;
        if (sizeBytes == 0)
            sizeBytes = 1;
            
        bool created = memory.create(sizeBytes);
        if (!created && memory.error() == QSharedMemory::AlreadyExists)
            memory.attach();
        if (memory.isAttached())
        {
            MediaPlayerProtocol::ResizeMessage msg(w, h, memory.key());
            network->client->socket->write(msg.Serialize());

            ReloadMedia(true);
        }
        else
        {
            qDebug() << "ERROR - RocketMediaPlayer: Failed to create required memory block of size " + QString::number(sizeBytes) + " Error: " + memory.errorString();
            Stop();
        }
    }
}

/// VLC callbacks

void* RocketMediaPlayer::CallBackLock(void* player, void** pixelPlane)
{
    RocketMediaPlayer *p = reinterpret_cast<RocketMediaPlayer*>(player);
    return p->InternalLock(pixelPlane);
}

void RocketMediaPlayer::CallBackUnlock(void* player, void* picture, void*const *pixelPlane)
{
    RocketMediaPlayer *p = reinterpret_cast<RocketMediaPlayer*>(player);
    p->InternalUnlock(picture, pixelPlane);
}

void RocketMediaPlayer::CallBackDisplay(void* player, void* picture)
{
    RocketMediaPlayer *p = reinterpret_cast<RocketMediaPlayer*>(player);
    p->InternalRender(picture);
}

void RocketMediaPlayer::VlcEventHandler(const libvlc_event_t *event, void *player)
{
    RocketMediaPlayer *p = reinterpret_cast<RocketMediaPlayer*>(player);
    p->InternalVlcEvent(event);
}

void* RocketMediaPlayer::InternalLock(void** pixelPlane) 
{
    if (!memory.isAttached())
        return 0;

    memory.lock();
    *pixelPlane = memory.data();
    return &memory;
}

void RocketMediaPlayer::InternalUnlock(void* /*picture*/, void*const * /*pixelPlane*/)
{
    if (!memory.isAttached())
        return;

    memory.unlock();

    emit SendDirty();
}

void RocketMediaPlayer::InternalRender(void* /*picture*/)
{
}

void RocketMediaPlayer::InternalVlcEvent(const libvlc_event_t *event)
{
    if (!network || !network->client)
        return;

    // Return on certain events
    switch (event->type)
    {
        case libvlc_MediaPlayerTitleChanged:
        case libvlc_MediaMetaChanged:
        case libvlc_MediaDurationChanged:
        case libvlc_MediaParsedChanged:
        case libvlc_MediaFreed:
            return;
        default:
            break;
    }

    switch (event->type)
    {
        case libvlc_MediaSubItemAdded:
        {
            mutexChildMedia_.lock();
            pendingChildMedia_ << event->u.media_subitem_added.new_child;
            mutexChildMedia_.unlock();
            emit CheckChildMedia();
            break;
        }
        case libvlc_MediaPlayerEncounteredError:
        {
            emit SendErrorRequest();
            break;
        }
        case libvlc_MediaStateChanged:
        {           
            state.state = (MediaPlayerProtocol::PlaybackState)event->u.media_state_changed.new_state;
            emit SendStatus();

            if (event->u.media_state_changed.new_state == libvlc_Ended)
            {
                mutexChildMedia_.lock();
                bool pendingChildren = !pendingChildMedia_.isEmpty();
                mutexChildMedia_.unlock();

                if (!pendingChildren)
                {
                    // Reload/rewind media if at the end
                    emit ReloadMediaRequest(looping);
                }
                else
                {
                    // Checks pending child media and starts playback of first item.
                    // Means the parent media is not a playable media but a playlist file.
                    emit CheckChildMedia();
                }
            }
            break;
        }
        case libvlc_MediaPlayerTimeChanged:
        {   
            state.mediaTimeMsec = event->u.media_player_time_changed.new_time;
            emit SendStatus();
            break;
        }
        case libvlc_MediaPlayerLengthChanged:
        {
            state.mediaLengthMsec = event->u.media_player_length_changed.new_length;
            emit SendStatus();
            break;
        }
        case libvlc_MediaPlayerSeekableChanged:
        {
            state.seekable = (event->u.media_player_seekable_changed.new_seekable > 0 ? true : false);
            emit SendStatus();
            break;
        }
        case libvlc_MediaPlayerPausableChanged:
        {
            state.pausable = (event->u.media_player_pausable_changed.new_pausable > 0 ? true : false);
            emit SendStatus();
            break;
        }
        default:
            break;
    }
}

#ifdef ROCKET_MEDIAPLAYER_EXTERNAL_BUILD
#include "moc_RocketMediaPlayer.cxx"
#endif
