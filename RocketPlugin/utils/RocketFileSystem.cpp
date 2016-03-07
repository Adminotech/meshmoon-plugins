/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketFileSystem.cpp
    @brief  RocketFileSystem class that provides script-friendly and safe file utilities. */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketFileSystem.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "FileUtils.h"

#include "MemoryLeakCheck.h"

// RocketFileDialogSignaler

RocketFileDialogSignaler::RocketFileDialogSignaler(RocketFileSystem::FileDialogType type) :
    type_(type),
    authorized_(false)
{
}

void RocketFileDialogSignaler::HandleFileDialogResult(int result)
{
    QFileDialog *dialog = qobject_cast<QFileDialog *>(sender());
    if (!dialog)
        return;
    if (result == QDialog::Rejected) /// @todo For some reason when dialog is canceled we end up here twice.
    {
        emit Canceled();
        return;
    }
    const QStringList files = dialog->selectedFiles();
    if (files.size() == 1)
    {
        selection_ = files.first();
        lastDir = QFileInfo(selection_).dir().path();
        emit Selected(selection_, this);
    }
}

QByteArray RocketFileDialogSignaler::ReadSelectedFile() const
{
    if (type_ == RocketFileSystem::DirectoryDialog)
    {
        LogError("[RocketFileDialogSignaler]: ReadSelectedFile cannot read file from a directory selection operation.");
        return QByteArray();
    }

    if (authorized_ == true)
    {
        if (!selection_.isEmpty() && QFile::exists(selection_))
        {
            QFile f(selection_);
            if (f.open(QFile::ReadOnly))
            {
                QByteArray data = f.readAll();
                f.close();
                return data;
            }
        }
    }
    else
        LogError("[RocketFileDialogSignaler]: ReadSelectedFile cannot open file, user authorization was not given.");
    return QByteArray();
}

bool RocketFileDialogSignaler::WriteBytesToSelectedFile(const QByteArray &bytes) const
{
    if (type_ != RocketFileSystem::SaveFileDialog)
    {
        LogError("[RocketFileDialogSignaler]: WriteBytesToSelectedFile cannot write file from a directory/existing file selection operation.");
        return false;
    }
    if (bytes.isEmpty() || selection_.isEmpty())
        return false;
        
    QFile f(selection_);
    if (f.open(QFile::WriteOnly))
    {
        f.resize(0);
        f.write(bytes);
        f.close();
        return true;
    }
    return false;
}

bool RocketFileDialogSignaler::WriteStringToSelectedFile(const QString &str) const
{
    return WriteBytesToSelectedFile(str.toUtf8());
}

void RocketFileDialogSignaler::AllowOperation()
{
    authorized_ = true;
    emit OperationAuthorized(true);
}

void RocketFileDialogSignaler::DenyOperation()
{
    emit OperationAuthorized(false);
}

// RocketFileSystem

RocketFileSystem::RocketFileSystem(RocketPlugin *owner) :
    rocket(owner),
    fileDialog(0),
    fileDialogSignaler(0)
{
}

RocketFileSystem::~RocketFileSystem()
{
    SetFileDialogVisible(false);
    SAFE_DELETE(fileDialogSignaler)
}

/// @cond PRIVATE

QString RocketFileSystem::InternalToolpath(InternalTool tool, bool absolutePath)
{
    QString exeName;
    switch(tool)
    {
        case Crunch:
#ifdef Q_WS_WIN
            exeName = "crunch.exe";
#elif defined(Q_WS_MAC)
            exeName = "crunch";
#else
            return exeName;
#endif
            break;
        case SevenZip:
#ifdef Q_WS_WIN
            exeName = "7za.exe";
#elif defined(Q_WS_MAC)
            exeName = "7za";
#else
            return exeName;
#endif
            break;
        default:
            return exeName;
    }
    if (absolutePath)
        return QDir::fromNativeSeparators(Application::InstallationDirectory() + exeName);
    return exeName;
}

bool RocketFileSystem::RemoveDirectory(const QString &path)
{
    return RemoveDirectory(QDir(path));
}

bool RocketFileSystem::RemoveDirectory(QDir dir)
{
    bool ok = true;
    if (!dir.exists())
        return ok;

    // Remove all children recursively
    ok = ClearDirectory(dir);

    // Remove directory itself
    QDir parentDir = QFileInfo(dir.absolutePath()).dir();
    if (!parentDir.rmdir(dir.dirName()))
    {
        LogWarning("[RocketFileSystem]: Failed to remove directory " + dir.dirName() + " from " + parentDir.absolutePath());
        ok = false;
    }
    return ok;
}

bool RocketFileSystem::ClearDirectory(const QString &path)
{
    return ClearDirectory(QDir(path));
}

bool RocketFileSystem::ClearDirectory(QDir dir)
{
    bool ok = true;
    if (!dir.exists())
        return ok;

    // Remove all files
    QFileInfoList entries = dir.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot);
    foreach(QFileInfo entry, entries)
    {
        if (entry.isFile())
        {
            if (!dir.remove(entry.fileName()))
            {
                LogWarning("[RocketFileSystem]: Failed to remove file " + entry.absoluteFilePath());
                ok = false;
            }
        }
    }
    // Remove all directories recursively.
    entries = dir.entryInfoList(QDir::AllDirs|QDir::NoSymLinks|QDir::NoDotAndDotDot);
    foreach(QFileInfo entry, entries)
    {
        if (entry.isDir())
        {
            if (!RemoveDirectory(entry.absoluteFilePath()))
                ok = false;
        }
    }
    return ok;
}

/// @endcond

//void RocketFileSystem::SelectDirectory(const QString &caption, const QString &dir,
//    QObject* initiator, const char* slot, QWidget *parent)
//{
//}

//void RocketFileSystem::SelectFile(const QString &filter, const QString &caption,
//    const QString &dir, QObject* initiator, const char* slot, QWidget *parent)
//{
//}

//void RocketFileSystem::SelectFileName(const QString &filter, const QString &caption,
//    const QString &dir, QObject* initiator, const char* slot, QWidget *parent)
//{
//}

RocketFileDialogSignaler *RocketFileSystem::SelectDirectory(const QString &applicationName,
    const QString &caption, const QString &dir, QWidget *parent)
{
    return CreateFileDialog(DirectoryDialog, applicationName, "", caption, dir, parent);
}

RocketFileDialogSignaler *RocketFileSystem::SelectFile(const QString &applicationName,
    const QString &filter, const QString &caption, const QString &dir, QWidget *parent)
{
    return CreateFileDialog(OpenFileDialog, applicationName, filter, caption, dir, parent);
}

RocketFileDialogSignaler *RocketFileSystem::SelectFileName(const QString &applicationName,
    const QString &filter, const QString &caption, const QString &dir, QWidget *parent)
{
    return CreateFileDialog(SaveFileDialog, applicationName, filter, caption, dir, parent);
}

bool RocketFileSystem::IsFileDialogVisible() const
{
    return fileDialog && fileDialog->isVisible();
}

RocketFileDialogSignaler *RocketFileSystem::CreateFileDialog(FileDialogType type, const QString &applicationName,
        const QString &filter, const QString &caption, const QString &dir, QWidget *parent)
{
    if (IsFileDialogVisible())
        return 0;

    QString directory = (dir.isEmpty() && fileDialogSignaler != 0 ? fileDialogSignaler->lastDir : dir);

    SAFE_DELETE(fileDialogSignaler)
    fileDialogSignaler = new RocketFileDialogSignaler(type);
    connect(fileDialogSignaler, SIGNAL(OperationAuthorized(bool)), SLOT(SetFileDialogVisible(bool)));

    RocketNotificationWidget *notification = rocket->Notifications()->ShowNotification(
        tr("%1 wants to open a file dialog").arg(applicationName), ":/images/icon-copy-32x32.png");
    notification->SetWarningTheme();

    QPushButton *allow = notification->AddButton(true, tr("Allow"));
    QPushButton *deny = notification->AddButton(true, tr("Deny"));
    connect(allow, SIGNAL(clicked()), fileDialogSignaler, SLOT(AllowOperation()));
    connect(deny, SIGNAL(clicked()), fileDialogSignaler, SLOT(DenyOperation()));

    switch(type)
    {
    case OpenFileDialog:
        fileDialog = OpenFileDialogNonModal(filter, caption, directory, parent, fileDialogSignaler,
            SLOT(HandleFileDialogResult(int)), false, false);
        break;
    case SaveFileDialog:
        fileDialog = SaveFileDialogNonModal(filter, caption, directory, parent, fileDialogSignaler,
            SLOT(HandleFileDialogResult(int)), false);
        break;
    case DirectoryDialog:
        fileDialog = DirectoryDialogNonModal(caption, directory, parent, fileDialogSignaler,
            SLOT(HandleFileDialogResult(int)), false);
        break;
    }

    return fileDialogSignaler;
}

void RocketFileSystem::SetFileDialogVisible(bool visible)
{
    if (fileDialog)
        fileDialog->setVisible(visible);
}
