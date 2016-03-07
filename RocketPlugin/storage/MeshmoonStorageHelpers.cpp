/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "MeshmoonStorageHelpers.h"

#include "qts3/QS3Defines.h"

// MeshmoonStorageOperationMonitor

MeshmoonStorageOperationMonitor::MeshmoonStorageOperationMonitor(Type type_) :
    total(0),
    completed(0),
    type(type_)
{
}

ProgressPair MeshmoonStorageOperationMonitor::CalculateGetProgress()
{
    ProgressPair result;
    foreach(const ProgressPair &iter, gets.values())
    {
        result.first += iter.first;
        result.second += iter.second;
    }
    return result;    
}

ProgressPair MeshmoonStorageOperationMonitor::CalculatePutProgress()
{
    ProgressPair result;
    foreach(const ProgressPair &iter, puts.values())
    {
        result.first += iter.first;
        result.second += iter.second;
    }
    return result;    
}

void MeshmoonStorageOperationMonitor::AddOperation(QS3GetObjectResponse *response, qint64 size)
{
    if (response && connect(response, SIGNAL(finished(QS3GetObjectResponse*)), SLOT(OnDownloadFinished(QS3GetObjectResponse*))))
    {
        total++;
        gets[response] = QPair<qint64,qint64>(0, size);
        connect(response, SIGNAL(downloadProgress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDownloadProgress(QS3GetObjectResponse*, qint64, qint64)));
    }
}

void MeshmoonStorageOperationMonitor::OnDownloadProgress(QS3GetObjectResponse *response, qint64 completed, qint64 total)
{
    if (!gets.contains(response))
        return;
        
    ProgressPair &updated = gets[response];
    if (updated.first < completed)
        updated.first = completed;
    if (total >= 1 && updated.second != total)
        updated.second = total;

    ProgressPair totals = CalculateGetProgress();

    QS3GetObjectResponse *dummy = 0;
    emit Progress(dummy, totals.first, totals.second);
}

void MeshmoonStorageOperationMonitor::OnDownloadFinished(QS3GetObjectResponse *response)
{
    completed++;
    emit Progress(response, -1, -1);
    if (completed >= total)
        emit Finished(this);
}

void MeshmoonStorageOperationMonitor::AddOperation(QS3PutObjectResponse *response, qint64 size)
{
    if (response && connect(response, SIGNAL(finished(QS3PutObjectResponse*)), SLOT(OnUploadFinished(QS3PutObjectResponse*))))
    {
        total++;
        puts[response] = QPair<qint64,qint64>(0, size);
        connect(response, SIGNAL(uploadProgress(QS3PutObjectResponse*, qint64, qint64)), SLOT(OnUploadProgress(QS3PutObjectResponse*, qint64, qint64)));
    }
}

void MeshmoonStorageOperationMonitor::OnUploadProgress(QS3PutObjectResponse *response, qint64 completed, qint64 total)
{
    if (!puts.contains(response))
        return;

    ProgressPair &updated = puts[response];
    if (updated.first < completed)
        updated.first = completed;
    if (total >= 1 && updated.second != total)
        updated.second = total;

    ProgressPair totals = CalculatePutProgress();
    
    QS3PutObjectResponse *dummy = 0;
    emit Progress(dummy, totals.first, totals.second, this);
}

void MeshmoonStorageOperationMonitor::OnUploadFinished(QS3PutObjectResponse *response)
{
    completed++;
    emit Progress(response, -1, -1, this);
    if (completed >= total)
        emit Finished(this);
}

void MeshmoonStorageOperationMonitor::AddOperation(QS3RemoveObjectResponse *response)
{
    if (response && connect(response, SIGNAL(finished(QS3RemoveObjectResponse*)), SLOT(OnRemoveFinished(QS3RemoveObjectResponse*))))
        total++;
}

void MeshmoonStorageOperationMonitor::OnRemoveFinished(QS3RemoveObjectResponse *response)
{
    completed++;
    emit Progress(response, completed, total);
    if (completed >= total)
        emit Finished(this);        
}