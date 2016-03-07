/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include <QObject>
#include <QThread>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QDir>
#include <QProcess>
#include <QMutex>

/// @cond PRIVATE

/// Worker thread that unpacks zip file contents.
class RocketZipWorker : public QThread
{
Q_OBJECT

public:
    enum Compression
    {
        NoCompression = 0,
        Fastest = 1,
        Fast = 3,
        Normal = 5,
        Maximum = 7,
        Ultra = 9
    };

    RocketZipWorker(const QString &destinationFilePath, const QDir inputDir, Compression compression = Normal);
    virtual ~RocketZipWorker();

protected:
    /// QThread override.
    void run();
    
public slots:
    QString DestinationFilePath() const;
    QString DestinationFileName() const;
    QDir InputDirectory() const;
    
    bool Succeeded();
    bool Completed();
    
private slots:
    void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
signals:
    /// Emitted when zip packaging has been completed.
    /** @note Connect your slot with Qt::QueuedConnection so
        you will receive the callback in your thread. */
    void AsynchPackageCompleted(bool successfull);
 
private:
    void SetCompleted();

    bool succeeded_;
    bool completed_;
    QDir inputDir_;
    QString destinationFilePath_;
    Compression compression_;
    static const QString LC_;
    
    QProcess *process_;
    QMutex stateMutex_;
};

/// @endcond
