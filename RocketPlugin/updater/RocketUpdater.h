/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"

#include <QObject>
#include <QString>

class AutoUpdater;
class QAction;

class RocketUpdater : public QObject
{
Q_OBJECT

public:
    RocketUpdater(RocketPlugin *plugin);
    ~RocketUpdater();

public slots:
    void CheckUpdates(QString parameter = "/checknow");

private:
    Framework *framework_;
    QAction *actUpdate_;
    
#if defined(Q_WS_MAC) && defined (MAC_ENABLE_UPDATER) 
    AutoUpdater* updater_;
#endif
};
