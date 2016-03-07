/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "MediaPlayerNetwork.h"
#include "MediaPlayerNetworkMessages.h"
#include "RocketMediaPlayer.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>

// MediaPlayerNetwork

MediaPlayerNetwork::MediaPlayerNetwork(RocketMediaPlayer *player_, quint16 port) :
    player(player_),
    client(0)
{
    server = new QTcpServer(this);
    if (server->listen(QHostAddress::LocalHost, port))
        connect(server, SIGNAL(newConnection()), SLOT(OnNewConnection()));
    else
        qDebug() << "ERROR - RocketMediaPlayer: Network server startup failed!";
}

MediaPlayerNetwork::~MediaPlayerNetwork()
{
    client = 0;
    foreach(MediaPlayerClient *client, clients)
        delete client;
    clients.clear();
}

void MediaPlayerNetwork::OnNewConnection()
{
    while (server->hasPendingConnections())
    {
        QTcpSocket *socket = server->nextPendingConnection();
        if (!socket)
            break;
        
        MediaPlayerClient *c = new MediaPlayerClient(player, socket);
        if (!client)
            client = c;
        clients << c;
    }
}

// MediaPlayerClient

MediaPlayerClient::MediaPlayerClient(RocketMediaPlayer *listener, QTcpSocket *socket_) :
    socket(socket_)
{
    connect(socket, SIGNAL(readyRead()), SLOT(OnData()), Qt::UniqueConnection);    
    connect(socket, SIGNAL(disconnected()), listener, SLOT(ConnectionLost()), Qt::QueuedConnection);
    
    connect(this, SIGNAL(TypedRequest(MediaPlayerProtocol::MessageType)), 
        listener, SLOT(OnTypedRequest(MediaPlayerProtocol::MessageType)));
    connect(this, SIGNAL(TypedBooleanRequest(const MediaPlayerProtocol::TypedBooleanMessage&)), 
        listener, SLOT(OnTypedBooleanRequest(const MediaPlayerProtocol::TypedBooleanMessage&)));
    connect(this, SIGNAL(SourceRequest(const MediaPlayerProtocol::SourceMessage&)), 
        listener, SLOT(OnSourceRequest(const MediaPlayerProtocol::SourceMessage&)));
    connect(this, SIGNAL(SeekRequest(qint64)), listener, SLOT(Seek(qint64)));
    connect(this, SIGNAL(VolumeRequest(int)), listener, SLOT(SetVolume(int)));
}

void MediaPlayerClient::OnData()
{
    if (socket->bytesAvailable() == 0)
        return;
        
    QByteArray data = socket->readAll();
    QDataStream s(&data, QIODevice::ReadOnly);
    
    while (!s.atEnd())
    {
        quint8 typeInt = 0; s >> typeInt;
        MediaPlayerProtocol::MessageType type = (MediaPlayerProtocol::MessageType)typeInt;

        switch(type)
        {           
            case MediaPlayerProtocol::Source:
            {
                MediaPlayerProtocol::SourceMessage msg(s);
                emit SourceRequest(msg);
                break;
            }
            case MediaPlayerProtocol::Seek:
            {
                MediaPlayerProtocol::SeekMessage msg(s);
                emit SeekRequest(msg.msec);
                break;
            }
            case MediaPlayerProtocol::Volume:
            {
                MediaPlayerProtocol::VolumeMessage msg(s);
                emit VolumeRequest(msg.VolumeToInt());
                break;
            }
            case MediaPlayerProtocol::Play:
            case MediaPlayerProtocol::Pause:
            case MediaPlayerProtocol::PlayPauseToggle:
            case MediaPlayerProtocol::Stop:
            case MediaPlayerProtocol::StateUpdate:
            {
                emit TypedRequest(type);
                break;
            }
            case MediaPlayerProtocol::Looping:
            {
                MediaPlayerProtocol::TypedBooleanMessage msg(type, s);
                emit TypedBooleanRequest(msg);
                break;
            }
            case MediaPlayerProtocol::Invalid:
                return;
            default:
                return;
        }
    }
}

#ifdef ROCKET_MEDIAPLAYER_EXTERNAL_BUILD
#include "moc_MediaPlayerNetwork.cxx"
#endif
