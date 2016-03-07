/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
//#include "DebugOperatorNew.h"

#include "RocketUpdater.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "Application.h"

#ifdef Q_WS_WIN
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QStringList>
#elif defined(Q_WS_MAC) && defined(MAC_ENABLE_UPDATER)
#include "CoreDefines.h"
#include "CocoaInitializer.h"
#include "SparkleAutoUpdater.h"
#endif

//#include "MemoryLeakCheck.h"

RocketUpdater::RocketUpdater(RocketPlugin *plugin) :
    framework_(plugin->GetFramework())
#if defined(Q_WS_MAC) && defined(MAC_ENABLE_UPDATER)
    , updater_(0)
#endif
{
    if (framework_->HasCommandLineParameter("--rocket-no-update"))
        return;
#if defined(Q_WS_MAC) && defined(MAC_ENABLE_UPDATER)
    CocoaInitializer initializer;
    //updater_ = new SparkleAutoUpdater(framework_, "");
#endif
    if (!framework_->IsHeadless())
        CheckUpdates("/silent");
}

RocketUpdater::~RocketUpdater()
{
#if defined(Q_WS_MAC) && defined(MAC_ENABLE_UPDATER)
    SAFE_DELETE(updater_);
#endif
    framework_ = 0;
}

void RocketUpdater::CheckUpdates(QString parameter)
{
    if (framework_->HasCommandLineParameter("--rocket-no-update"))
        return;
#ifdef Q_WS_WIN
    QDir installDir(QDir::fromNativeSeparators(Application::InstallationDirectory()));
    QFile executable(installDir.absoluteFilePath("RocketUpdater.exe"));
    QFile config(installDir.absoluteFilePath("RocketUpdater.ini"));
    if (executable.exists() && config.exists())
        QProcess::startDetached(executable.fileName(), QStringList() << parameter);
#elif defined(Q_WS_MAC) && defined(MAC_ENABLE_UPDATER)
    if (updater_)
        updater_->checkForUpdates(parameter);
#endif
}
