/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"

#include <QObject>
#include <QString>
#include <QPointer>
#include <QTimer>

/// Provides asset monitoring.
/** @ingroup MeshmoonRocket */
class RocketAssetMonitor : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketAssetMonitor(RocketPlugin *plugin);
    ~RocketAssetMonitor();

    friend class RocketPlugin;
    /// @endcond
    
public slots:
    /// Returns the current ongoing transfer count from AssetAPI.
    uint NumCurrentTransfers() const;

    /// Returns if all AssetAPI transfers are completed.
    bool AllTransfersCompleted() const;

signals:
    /// Emitted when number of ongoing asset transfers change.
    /** @note This includes when assets are loaded during runtime,
        not just on initial world loading. Expect multiple signals,
        number of remaining transfer can go to zero multiple times. */
    void AssetTransfersRemainingChanged(uint num);
    
    /// Emitted when all asset transfers have completed.
    /** @note This includes when assets are loaded during runtime,
        not just on initial world loading. Expect multiple signals. */
    void TransfersCompleted();

private slots:   
    /// Invoked by RocketPlugin.
    void StartMonitoringForTaskbar(RocketTaskbar *taskbar);

    /// Invoked by RocketPlugin.
    void StopMonitoringForTaskbar();

    /// Invoked by lobby UI.
    /** @note Don't make this a public slot, we don't
        want scripts to clear users cache. */
    void OnClearAssetCache();

    /// Main update for transfer count monitoring.
    void OnUpdate(float frametime);

private:   
    /// Invoked internally.
    void UpdateTaskbar(uint transfers);

    RocketPlugin *plugin_;
    QPointer<RocketTaskbar> taskbar_;

    QTimer taskbarMonitoringDoneTimer_;

    uint lastKnownAssetTransferCount_;
    uint transferCountPeak_;
};
Q_DECLARE_METATYPE(RocketAssetMonitor*)
