/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MediaPlayerNetworkMessages.h"

#include <QtGlobal>
#include <QObject>
#include <QSize>
#include <QString>
#include <QSharedMemory>
#include <QMutex>
#include <QTimer>

// Do not change the order of these includes. On windows we need 
// libvlc_structures.h to be included first before libvlc.h due to the proper stdint.h missing.
#include "vlc/libvlc_structures.h"
#include "vlc/libvlc.h"
#include "vlc/libvlc_media.h"
#include "vlc/libvlc_media_player.h"
#include "vlc/libvlc_events.h"

class MediaPlayerNetwork;

class RocketMediaPlayer : public QObject
{
Q_OBJECT

public:
    RocketMediaPlayer(int &argc, char **argv);
    ~RocketMediaPlayer();
    
    QString id;
    QSharedMemory memory;
    MediaPlayerNetwork *network;
    
    QSize size;
   
    MediaPlayerProtocol::PlayerState state;
    bool looping;
    bool debugging;

    static QString UserDataDirectory();
    
    bool IsStarted() const;
    bool Initialized() const;

public slots:
    void Play();
    void Pause();
    void TogglePlay();
    void Stop();
    void Seek(qint64 msec);
    void SetVolume(int volume);
    
    void ReloadMedia(bool startPlayback = false);
    
    libvlc_state_t GetMediaState() const;

    void Cleanup();
    void ConnectionLost();
    void ExitProcess();
    void SendError(QString error = QString());
    
    void OnTypedRequest(MediaPlayerProtocol::MessageType type);
    void OnTypedBooleanRequest(const MediaPlayerProtocol::TypedBooleanMessage &msg);
    void OnSourceRequest(const MediaPlayerProtocol::SourceMessage &msg);

    void OnSendDirty();
    void OnSendStatus();
    void OnCheckChildMedia();
    void OnApplyPendingVolume();

signals:
    void SendDirty();
    void SendStatus();
    void ReloadMediaRequest(bool);
    void SendErrorRequest();
    void CheckChildMedia();
    
protected:
    void timerEvent(QTimerEvent *e);
    
    /// Internal impl for providing memory
    void* InternalLock(void** pixelPlane);

    /// Internal impl for releasing memory
    void InternalUnlock(void* picture, void*const *pixelPlane);

    /// Internal impl for rendering
    void InternalRender(void* picture);
    
    /// Internal impl for events
    void InternalVlcEvent(const libvlc_event_t *event);

    /// Vlc callback for providing memory
    static void* CallBackLock(void* player, void** pixelPlane);

    /// Vlc callback for releasing memory
    static void CallBackUnlock(void* player, void* picture, void*const *pixelPlane);

    /// Vlc callback for rendering
    static void CallBackDisplay(void* player, void* picture);

    /// Vlc callback for events
    static void VlcEventHandler(const libvlc_event_t *event, void *player_);
    
private slots:
    void OnCheckMainProcessAlive();

private:
    libvlc_instance_t *vlcInstance_;
    libvlc_media_player_t *vlcPlayer_;
    libvlc_media_t* vlcMedia_;
    
    int pendingVolume_;
    
    QMutex mutexChildMedia_;
    QList<libvlc_media_t*> pendingChildMedia_;

    QString totalTime_;
    int timerId_;
    qint64 parentProcessId;
    QTimer mainProcessPoller_;
};
