/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonComponents.h
    @brief  MeshmoonComponents registers Meshmoon ECs and handles logic related to them. */

#pragma once

#include "MeshmoonComponentsApi.h"

#include "IModule.h"
#include "SceneFwd.h"
#include "AssetFwd.h"
#include "OgreModuleFwd.h"
#include "Math/float3.h"

#include <QHash>
#include <QAbstractSocket>

class OgreMeshAsset;

/// Registers Meshmoon Entity-Components and handles logic related to them.
/// @cond PRIVATE
class MESHMOON_COMPONENTS_API MeshmoonComponents : public IModule
{
    Q_OBJECT

public:
    MeshmoonComponents();
    virtual ~MeshmoonComponents();

    /// Emits teleport request.
    void EmitTeleportRequest(const QString &sceneId, const QString &pos, const QString &rot);

signals:
    void TeleportRequest(const QString &sceneId, const QString &pos, const QString &rot);

private slots:
    void OnClientConnected();
    void OnClientDisconnected();

    void ConnectToScene(Scene *);
    void DisconnectFromScene(Scene *);
    void OnComponentAdded(Entity *, IComponent *);
    void OnComponentRemoved(Entity *, IComponent *);

private:
    void Load(); ///< IModule override.
    void Initialize(); ///< IModule override.
    void Update(f64 frametime); ///< IModule override.

    float processMonitorDelta_;
    int numMaxWebBrowserProcesses_;
    int numMaxMediaPlayerProcesses_;
    typedef std::list<EntityWeakPtr> WeakEntityList;
    WeakEntityList webBrowsers;
    WeakEntityList mediaBrowsers;
};

inline QString QAbstractSocketErrorToString(QAbstractSocket::SocketError error)
{
    switch(error)
    {
    case QAbstractSocket::ConnectionRefusedError: return "ConnectionRefusedError";
    case QAbstractSocket::RemoteHostClosedError: return "RemoteHostClosedError";
    case QAbstractSocket::HostNotFoundError: return "HostNotFoundError";
    case QAbstractSocket::SocketAccessError: return "SocketAccessError";
    case QAbstractSocket::SocketResourceError: return "SocketResourceError";
    case QAbstractSocket::SocketTimeoutError: return "SocketTimeoutError";
    case QAbstractSocket::DatagramTooLargeError: return "DatagramTooLargeError";
    case QAbstractSocket::NetworkError: return "NetworkError";
    case QAbstractSocket::AddressInUseError: return "AddressInUseError";
    case QAbstractSocket::SocketAddressNotAvailableError: return "SocketAddressNotAvailableError";
    case QAbstractSocket::UnsupportedSocketOperationError: return "UnsupportedSocketOperationError";
    case QAbstractSocket::UnfinishedSocketOperationError: return "UnfinishedSocketOperationError";
    case QAbstractSocket::ProxyAuthenticationRequiredError: return "ProxyAuthenticationRequiredError";
    case QAbstractSocket::SslHandshakeFailedError: return "SslHandshakeFailedError";
    case QAbstractSocket::ProxyConnectionRefusedError: return "ProxyConnectionRefusedError";
    case QAbstractSocket::ProxyConnectionClosedError: return "ProxyConnectionClosedError";
    case QAbstractSocket::ProxyConnectionTimeoutError: return "ProxyConnectionTimeoutError";
    case QAbstractSocket::ProxyNotFoundError: return "ProxyNotFoundError";
    case QAbstractSocket::ProxyProtocolError: return "ProxyProtocolError";
    case QAbstractSocket::UnknownSocketError: default: return "UnknownSocketError";
    }
}
/// @endcond
