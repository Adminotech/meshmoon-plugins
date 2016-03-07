/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "BrowserNetwork.h"
#include "BrowserNetworkMessages.h"
#include "RocketWebBrowser.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>

// BrowserNetwork

BrowserNetwork::BrowserNetwork(RocketWebBrowser *browser_, quint16 port_) :
    browser(browser_),
    client(0),
    port(port_)
{
    server = new QTcpServer(this);
    if (server->listen(QHostAddress::LocalHost, port))
        connect(server, SIGNAL(newConnection()), SLOT(OnNewConnection()));
    else
        qDebug() << "ERROR - RocketWebBrowser: Network server startup failed!";
}

BrowserNetwork::~BrowserNetwork()
{
    if (server)
        server->close();
    server = 0;

    client = 0;
    foreach(BrowserClient *client, clients)
        delete client;
    clients.clear();
}

void BrowserNetwork::OnNewConnection()
{
    while (server->hasPendingConnections())
    {
        QTcpSocket *socket = server->nextPendingConnection();
        if (!socket)
            break;

        BrowserClient *b = new BrowserClient(browser, socket);
        if (!client)
            client = b;
        clients << b;
    }
}

// BrowserClient

BrowserClient::BrowserClient(RocketWebBrowser *listener, QTcpSocket *socket_) :
    socket(socket_)
{
    connect(socket, SIGNAL(readyRead()), SLOT(OnData()), Qt::UniqueConnection);
    connect(socket, SIGNAL(disconnected()), listener, SLOT(ConnectionLost()), Qt::QueuedConnection);

    connect(this, SIGNAL(UrlRequest(const BrowserProtocol::UrlMessage&)), 
        listener, SLOT(OnUrlRequest(const BrowserProtocol::UrlMessage&)));
    connect(this, SIGNAL(ResizeRequest(const BrowserProtocol::ResizeMessage&)), 
        listener, SLOT(OnResizeRequest(const BrowserProtocol::ResizeMessage&)));
    connect(this, SIGNAL(MouseMoveRequest(const BrowserProtocol::MouseMoveMessage&)), 
        listener, SLOT(OnMouseMoveRequest(const BrowserProtocol::MouseMoveMessage&)));
    connect(this, SIGNAL(MouseButtonRequest(const BrowserProtocol::MouseButtonMessage&)), 
        listener, SLOT(OnMouseButtonRequest(const BrowserProtocol::MouseButtonMessage&)));
    connect(this, SIGNAL(KeyboardRequest(const BrowserProtocol::KeyboardMessage&)), 
        listener, SLOT(OnKeyboardRequest(const BrowserProtocol::KeyboardMessage&)));
    connect(this, SIGNAL(TypedRequest(const BrowserProtocol::TypedMessage&)), 
        listener, SLOT(OnTypedRequest(const BrowserProtocol::TypedMessage&)));
}

void BrowserClient::OnData()
{
    if (socket->bytesAvailable() == 0)
        return;
        
    QByteArray data = socket->readAll();
    QDataStream s(&data, QIODevice::ReadOnly);
    
    while (!s.atEnd())
    {
        quint8 typeInt = 0; s >> typeInt;
        BrowserProtocol::MessageType type = (BrowserProtocol::MessageType)typeInt;

        switch(type)
        {
            case BrowserProtocol::Url:
            {
                emit UrlRequest(BrowserProtocol::UrlMessage(s));
                break;
            }
            case BrowserProtocol::Resize:
            {
                emit ResizeRequest(BrowserProtocol::ResizeMessage(s));
                break;
            }
            case BrowserProtocol::MouseMoveEvent:
            {
                emit MouseMoveRequest(BrowserProtocol::MouseMoveMessage(s));
                break;
            }
            case BrowserProtocol::MouseButtonEvent:
            {
                emit MouseButtonRequest(BrowserProtocol::MouseButtonMessage(s));
                break;
            }
            case BrowserProtocol::KeyboardEvent:
            {
                emit KeyboardRequest(BrowserProtocol::KeyboardMessage(s));
                break;
            }
            case BrowserProtocol::FocusIn:
            case BrowserProtocol::FocusOut:
            case BrowserProtocol::Invalidate:
            case BrowserProtocol::Close:
            {
                emit TypedRequest(BrowserProtocol::TypedMessage(type));
                break;
            }
            
            case BrowserProtocol::Invalid:
                return;
            default:
                return;
        }
    }
}

#ifdef ROCKET_WEBBROWSER_EXTERNAL_BUILD
#include "moc_BrowserNetwork.cxx"
#endif
