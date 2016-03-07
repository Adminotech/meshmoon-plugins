
#pragma once

#include "BrowserNetworkMessages.h"

#include <QtGlobal>
#include <QObject>
#include <QList>

class RocketWebBrowser;
class QTcpServer;
class QTcpSocket;

// BrowserClient

class BrowserClient : public QObject
{
    Q_OBJECT

public:
    BrowserClient(RocketWebBrowser *listener, QTcpSocket *socket_);

    QTcpSocket *socket;

private slots:
    void OnData();

signals:
    void UrlRequest(const BrowserProtocol::UrlMessage &msg);
    void ResizeRequest(const BrowserProtocol::ResizeMessage &msg);
    void MouseMoveRequest(const BrowserProtocol::MouseMoveMessage &msg);
    void MouseButtonRequest(const BrowserProtocol::MouseButtonMessage &msg);
    void KeyboardRequest(const BrowserProtocol::KeyboardMessage &msg);
    void TypedRequest(const BrowserProtocol::TypedMessage &msg);    
};
typedef QList<BrowserClient*> BrowserClients;

// BrowserNetwork

class BrowserNetwork : public QObject
{
Q_OBJECT

public:
    BrowserNetwork(RocketWebBrowser *browser_, quint16 port_);
    ~BrowserNetwork(); 
    
    quint16 port;
    
    QTcpServer *server;
    BrowserClient *client;
    BrowserClients clients;
    RocketWebBrowser *browser;
    
private slots:
    void OnNewConnection();
};
