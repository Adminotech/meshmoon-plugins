/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "TundraProtocolModuleFwd.h"

#include <QObject>
#include <QString>

/// @cond PRIVATE

class RocketNetworking : public QObject
{
Q_OBJECT

public:
    RocketNetworking(RocketPlugin *plugin);
    ~RocketNetworking();
    
public slots:
    /// Send list of asset reference to server and from there to all client. 
    /// Server and clients will force re-request them from the original source.
    void SendAssetReloadMsg(const QString &assetReference);
    void SendAssetReloadMsg(const QStringList &assetReferences);
    
    /// Send a layer visibility request to the server.
    void SendLayerUpdateMsg(u32 layerId, bool visible);

private:
    RocketPlugin *plugin_;
    QString LC;
};

/// @endcond
