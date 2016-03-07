/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketAssetMonitor.h"
#include "RocketTaskbar.h"
#include "RocketPlugin.h"
#include "RocketLobbyWidget.h"
#include "RocketLobbyWidgets.h"
#include "RocketNotifications.h"
#include "MeshmoonBackend.h"

#include "Framework.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "FrameAPI.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAsset.h"
#include "IAssetTransfer.h"

RocketAssetMonitor::RocketAssetMonitor(RocketPlugin *plugin) :
    plugin_(plugin),
    lastKnownAssetTransferCount_(0),
    transferCountPeak_(0)
{
    connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
}

RocketAssetMonitor::~RocketAssetMonitor()
{
}

uint RocketAssetMonitor::NumCurrentTransfers() const
{
    return static_cast<int>(plugin_->GetFramework()->Asset()->NumCurrentTransfers());
}

bool RocketAssetMonitor::AllTransfersCompleted() const
{
    return (NumCurrentTransfers() == 0);
}

void RocketAssetMonitor::StartMonitoringForTaskbar(RocketTaskbar *taskbar)
{
    taskbar_ = taskbar;    
    if (taskbar_)
        connect(&taskbarMonitoringDoneTimer_, SIGNAL(timeout()), taskbar_, SLOT(ClearMessageAndStopLoadAnimation()), Qt::UniqueConnection);
    UpdateTaskbar(NumCurrentTransfers());
}

void RocketAssetMonitor::StopMonitoringForTaskbar()
{
    if (taskbar_)
    {
        taskbar_->ClearMessageAndStopLoadAnimation();
        disconnect(&taskbarMonitoringDoneTimer_, SIGNAL(timeout()), taskbar_, SLOT(ClearMessageAndStopLoadAnimation()));
    }
    taskbar_ = 0;
}

void RocketAssetMonitor::OnUpdate(float frametime)
{
    const uint transfers = NumCurrentTransfers();
    if (lastKnownAssetTransferCount_ != transfers)
    {
        // Update tracking state
        lastKnownAssetTransferCount_ = transfers;
        if (transfers > transferCountPeak_)
            transferCountPeak_ = transfers;
    
        // Update loading screen completion percent
        if (plugin_->Lobby() && plugin_->Lobby()->IsLoaderVisible() && plugin_->IsConnectedToServer())
        {
            qreal progress = 100.0 - ((static_cast<qreal>(transfers) / static_cast<qreal>(transferCountPeak_)) * 100.0);
            plugin_->Lobby()->GetLoader()->SetCompletion(progress);
        }

        // Update taskbar
        UpdateTaskbar(lastKnownAssetTransferCount_);

        // Emit signals
        emit AssetTransfersRemainingChanged(lastKnownAssetTransferCount_);
        if (lastKnownAssetTransferCount_ == 0)
        {
            transferCountPeak_ = 0;
            emit TransfersCompleted();
        }
    }
}

void RocketAssetMonitor::UpdateTaskbar(uint transfers)
{
    if (!taskbar_)
        return;

    // There are worlds that do polling via BinaryAsset to REST APIs etc.
    // If there are less than 5 transfers, check if all are binary,
    // meaning they have very little to do with the 3D world and can be ignored.
    if (transfers < 5)
    {
        bool allBinary = true;
        std::vector<AssetTransferPtr> pending = plugin_->GetFramework()->Asset()->PendingTransfers();
        for (std::vector<AssetTransferPtr>::const_iterator iter = pending.begin(), end = pending.end(); iter != end; ++iter)
        {
            if ((*iter)->AssetType() != "Binary")
            {
                allBinary = false;
                break;
            }
        }
        if (allBinary)
            transfers = 0;
    }

    if (transfers == 0)
    {
        if (!taskbarMonitoringDoneTimer_.isActive())
        {
            taskbar_->SetMessage("1");
            taskbarMonitoringDoneTimer_.start(2500);

            // Time to hide the main Rocket loading screen if still visible.
            if (plugin_->Lobby() && plugin_->Lobby()->isVisible())
                plugin_->Hide();
        }
        return;
    }

    // If we are back in business, stop taskbar done timer.
    if (taskbarMonitoringDoneTimer_.isActive())
        taskbarMonitoringDoneTimer_.stop();
    // ... and restart taskbar animation.
    if (!taskbar_->IsLoadAnimationRunning())
        taskbar_->StartLoadAnimation();

    // Update taskbar UI.
    taskbar_->SetMessage(QString::number(transfers));
}

void RocketAssetMonitor::OnClearAssetCache()
{
    if (!plugin_->GetFramework())
        return;

    int count = 0;
    qint64 size = 0;

    // AssetAPI cache
    QString cachePath = plugin_->GetFramework()->Asset()->GetAssetCache()->CacheDirectory();
    if (!cachePath.isEmpty())
    {
        QDir cacheDir(cachePath);
        QFileInfoList cacheFiles = cacheDir.entryInfoList();  // Using flags here would be nice but does not seem to work
        foreach(QFileInfo fileInfo, cacheFiles)
        {
            if (fileInfo.isFile() && fileInfo.exists())
            {
                count++;
                size += fileInfo.size();
            }
        }

        // Clear the cache. If we happen to throw here we wont show false log/ui information.
        if (count > 0)
            plugin_->GetFramework()->Asset()->GetAssetCache()->ClearAssetCache();
    }

    // Meshmoon backend cache
    cachePath = plugin_->Backend()->CacheDirectory();
    if (!cachePath.isEmpty())
    {
        QDir cacheDir(cachePath);
        QFileInfoList cacheFiles = cacheDir.entryInfoList();  // Using flags here would be nice but does not seem to work
        foreach(QFileInfo fileInfo, cacheFiles)
        {
            if (fileInfo.isFile() && fileInfo.exists())
            {
                count++;
                size += fileInfo.size();
                QFile::remove(fileInfo.absoluteFilePath());
            }
        }
    }

    QString msg;
    if (count > 0)
    {
        qreal sizeMb = ((static_cast<qreal>(size) / 1024.0) / 1024.0);
        msg = QString("Cleared %1 files with total size of %2 mb from asset cache.").arg(count).arg(sizeMb, 0, 'f', 2);
    }
    else
        msg = "Asset cache already empty.";
        
    // Show information box for user. Note: this will block so not the most convenient.
    plugin_->Notifications()->ShowSplashDialog(msg, ":/images/icon-recycle.png");
}
