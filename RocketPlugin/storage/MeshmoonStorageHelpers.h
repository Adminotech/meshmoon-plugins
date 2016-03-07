/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  MeshmoonStorageHelpers.h
    @brief Provides structs and other helper objects for Meshmoon Storage  */

#pragma once

#include "RocketFwd.h"
#include "qts3/QS3Fwd.h"

#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

/// Completed bytes to total bytes.
typedef QPair<qint64,qint64> ProgressPair;

/// Monitors a set of operations and provides signals for progress and completion monitoring.
class MeshmoonStorageOperationMonitor : public QObject
{
Q_OBJECT

public:
    enum Type
    {
        UploadFiles = 0,
        DownloadFiles,
        DeleteFiles
    };

    MeshmoonStorageOperationMonitor(Type type_);

    /// Operation type
    Type type;

    /// Currently completed operations.
    quint64 completed;

    /// Total operations.
    quint64 total;

    /// List of asset references that were affected by the operations.
    QStringList changedAssetRefs;

    /// Add download operation.
    void AddOperation(QS3GetObjectResponse *response, qint64 size);

    /// Add upload operation.
    void AddOperation(QS3PutObjectResponse *response, qint64 size);

    /// Add remove operation.
    void AddOperation(QS3RemoveObjectResponse *response);

    /// Combined download operation progress. @see ProgressPair.
    ProgressPair CalculateGetProgress();

    /// Combined upload operation progress. @see ProgressPair.
    ProgressPair CalculatePutProgress();

    /// @cond PRIVATE
private:
    QHash<QS3GetObjectResponse*, ProgressPair > gets;
    QHash<QS3PutObjectResponse*, ProgressPair > puts;

private slots:
    void OnDownloadProgress(QS3GetObjectResponse *response, qint64 completed, qint64 total);
    void OnDownloadFinished(QS3GetObjectResponse *response);
    void OnUploadProgress(QS3PutObjectResponse *response, qint64 completed, qint64 total);
    void OnUploadFinished(QS3PutObjectResponse *response);
    void OnRemoveFinished(QS3RemoveObjectResponse *response);
    /// @endcond

signals:
    /// All monitored operation have finished.
    void Finished(MeshmoonStorageOperationMonitor *operation);

    /// Progress signal for download operations.
    /** @note The first parameter is null until the last progress signal. */
    void Progress(QS3GetObjectResponse *response, qint64 completed, qint64 total);

    /// Progress signal for upload operations.
    /** @note The first parameter is null until the last progress signal. */
    void Progress(QS3PutObjectResponse *response, qint64 completed, qint64 total, MeshmoonStorageOperationMonitor *operation);

    /// Progress signal for remove operations.
    /** @note The first parameter is null until the last progress signal. */
    void Progress(QS3RemoveObjectResponse *response, qint64 completed, qint64 total);
};

/// Provides monitoring signals for the %Meshmoon storage authentication step.
class MeshmoonStorageAuthenticationMonitor : public QObject
{
Q_OBJECT

public:
    MeshmoonStorageAuthenticationMonitor() {}

public slots:
    void EmitCompleted() { emit Completed(); }
    void EmitCanceled() { emit Canceled(); }
    void EmitFailed(const QString &error) { emit Failed(error); }

signals:
    /// Emitted when authentication completes and storage is ready for user interaction.
    void Completed();
    
    /// Emitted authentication was canceled by the user.
    void Canceled();
    
    /// Emitted when authentication failed.
    void Failed(const QString &error);
};
