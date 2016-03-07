/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "OgreModuleFwd.h"
#include "MeshmoonData.h"
#include "HighPerfClock.h"

#include <set>
#include <OgreResourceManager.h>

#include <QString>
#include <QByteArray>
#include <QTextStream>

namespace Ogre
{
    class Material;
    class Texture;
}

/// Provides reporting functionality.
/** Can open the Meshmoon Rocket support request dialog for the user.
    Serializes scene information into a string that can be used in applications. 
    @ingroup MeshmoonRocket */
class RocketReporter : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketReporter(RocketPlugin *plugin);
    ~RocketReporter();

    void SetServerInfo(const QUrl &loginUrl, const Meshmoon::Server &serverInfo);
    
    /// Info dumpers. @note These are non-slots for a reason, this info can be sensitive so don't expose to scripts.
    QByteArray DumpEverything();
    void DumpSystemInfo(QTextStream &stream);
    void DumpUserInfo(QTextStream &stream);
    void DumpLoginInfo(QTextStream &stream);
    void DumpServerInfo(QTextStream &stream);
    void DumpNetworkInfo(QTextStream &stream);
    void DumpOgreSceneOverview(QTextStream &stream);
    void DumpOgreSceneComplexity(QTextStream &stream);
    void DumpTundraSceneComplexity(QTextStream &stream);
    QString CensorIP(const QString &ip);
    /// @endcond

public slots:
    /// Shows the Rocket dialog for sending a support request to Meshmoon.
    /** User can input own description for the report. Additional data will 
        also be sent that user can preview in the dialog.
        @param Message that should be initially shown in the report dialog.
        The user can modify the text once the dialog is open. */
    void ShowSupportRequestDialog(const QString &message = "");

    /// Returns current Ogre scene overview information as a formatted string.
    QString DumpOgreSceneOverview();

    /// Returns current Ogre scene complexity information as a formatted string.
    QString DumpOgreSceneComplexity();

    /// Returns current Tundra scene complexity information as a formatted string.
    QString DumpTundraSceneComplexity();

private slots:
    void Reset();
    void OnSettingsApplied(const ClientSettings *settings);

    void OnUpdate(float frametime);
    void OnConnectionStateChanged(bool connected);

    void OnShowRenderingPerformanceInfo();
    void OnShowNetworkPerformanceInfo();
    
    void OnShowReportData();
    void OnSendReport();

private:
    uint GetNumResources(Ogre::ResourceManager& manager);
    void GetVerticesAndTrianglesFromMesh(Ogre::Mesh* mesh, size_t& vertices, size_t& triangles);
    void GetMaterialsFromEntity(Ogre::Entity* entity, std::set<Ogre::Material*>& dest);
    void GetTexturesFromMaterials(const std::set<Ogre::Material*>& materials, std::set<Ogre::Texture*>& dest);
    
    QString LC_;
    RocketPlugin *plugin_;
    
    QPointer<QWidget> supportDialog_;

    QUrl loginUrl_;
    Meshmoon::Server currentServer_;

    bool detectRenderingPerf_;
    bool detectNetworkPerf_;
    bool renderingPerformanceWarningShowed_;
    bool networkPerformanceWarningShowed_;

    float tProfile_;
    uint numFpsLow_;
    uint numRttHigh_;
    tick_t timePrev_;
};
