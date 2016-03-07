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
#include <QList>

class RocketMediaPlayer;
class QTcpServer;
class QTcpSocket;

// BrowserClient

class MediaPlayerClient : public QObject
{
    Q_OBJECT

public:
    MediaPlayerClient(RocketMediaPlayer *listener, QTcpSocket *socket_);

    QTcpSocket *socket;

private slots:
    void OnData();

signals:
    void TypedRequest(MediaPlayerProtocol::MessageType type);
    void TypedBooleanRequest(const MediaPlayerProtocol::TypedBooleanMessage &msg);
    void SourceRequest(const MediaPlayerProtocol::SourceMessage &msg);
    void SeekRequest(qint64 msec);    
    void VolumeRequest(int volume);
};
typedef QList<MediaPlayerClient*> MediaPlayerClients;

// BrowserNetwork

class MediaPlayerNetwork : public QObject
{
Q_OBJECT

public:
    MediaPlayerNetwork(RocketMediaPlayer *player_, quint16 port);
    ~MediaPlayerNetwork(); 
    
    QTcpServer *server;
    MediaPlayerClient *client;
    MediaPlayerClients clients;
    RocketMediaPlayer *player;
    
private slots:
    void OnNewConnection();
};
