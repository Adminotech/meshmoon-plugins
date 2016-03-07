/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketZipWorker.h"
#include "RocketFileSystem.h"

#include "Application.h"
#include "LoggingFunctions.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QDebug>

#include "MemoryLeakCheck.h"

const QString RocketZipWorker::LC_ = "[RocketZipWorker]: ";

RocketZipWorker::RocketZipWorker(const QString &destinationFilePath, const QDir inputDir, Compression compression) :
    destinationFilePath_(destinationFilePath),
    inputDir_(inputDir),
    compression_(compression),
    succeeded_(false),
    completed_(false),
    process_(0)
{
}

RocketZipWorker::~RocketZipWorker()
{
    SAFE_DELETE(process_);
}

QString RocketZipWorker::DestinationFilePath() const
{
    return destinationFilePath_;
}

QString RocketZipWorker::DestinationFileName() const
{
    return QFileInfo(destinationFilePath_).fileName();
}

QDir RocketZipWorker::InputDirectory() const
{
    return inputDir_;
}

bool RocketZipWorker::Succeeded()
{
    QMutexLocker stateLock(&stateMutex_);
    bool result = succeeded_;
    return result;
}

bool RocketZipWorker::Completed()
{
    QMutexLocker stateLock(&stateMutex_);
    bool result = completed_;
    return result;
}

void RocketZipWorker::SetCompleted()
{
    QMutexLocker stateLock(&stateMutex_);
    completed_ = true;
}

void RocketZipWorker::run()
{
    if (!inputDir_.exists())
    {
        LogError(LC_ + "Input directory does not exist: " + inputDir_.absolutePath());
        SetCompleted();
        emit AsynchPackageCompleted(false);
        return;
    }
    if (!destinationFilePath_.toLower().endsWith(".zip"))
    {
        LogError(LC_ + "Destination file does not end with .zip, aborting.");
        SetCompleted();
        emit AsynchPackageCompleted(false);
        return;
    }
    if (QFile::exists(destinationFilePath_) && !QFile::remove(destinationFilePath_))
    {
        LogError(LC_ + "Destination file already exists and failed to remove it, aborting.");
        SetCompleted();
        emit AsynchPackageCompleted(false);
        return;
    }

    QString executableName = RocketFileSystem::InternalToolpath(RocketFileSystem::SevenZip);
    if (executableName.isEmpty())
    {
        LogError(LC_ + "RocketZipWorker cannot be used on your operating system.");
        SetCompleted();
        emit AsynchPackageCompleted(false);
        return;
    }

    QStringList inputParameters;
    inputParameters << "a" << "-tzip" << destinationFilePath_ << inputDir_.absolutePath() + + "/*" <<
        "-mx" + QString::number(compression_) << "-mmt" << "-y" << "-r";

    SAFE_DELETE(process_);
    process_ = new QProcess();
    connect(process_, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(OnProcessFinished(int, QProcess::ExitStatus)));
    process_->start(executableName, inputParameters);
    
    exec();
}

void RocketZipWorker::OnProcessFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/)
{
    QMutexLocker stateLock(&stateMutex_);
    succeeded_ = (exitCode == 0);
    stateLock.unlock();

    emit AsynchPackageCompleted(true);
    exit();
}
