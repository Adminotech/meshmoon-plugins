/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketFileSystem.h
    @brief  RocketFileSystem class that provides script-friendly and safe file utilities. */

#pragma once

#include "RocketFwd.h"

#include <QObject>
#include <QPointer>
#include <QDir>

class QFileDialog;

/// Utility object that provides script-friendly and safe file utilities.
/** Script example:
    @code
    var dialog = rocket.fileSystem.SelectFileName("Test", ".txml", "SelectFileName", "");
    if (!dialog)
        return; // Another file dialog already open, cannot proceed.
    dialog.OperationAuthorized.connect(function(authorized)
    {
        if (authorized)
        {
            dialog.Selected.connect(function(filename)
            {
                console.LogInfo("User chose the file '" + filename  + "'.");
            });
        }
    });
    @endcode
    @ingroup MeshmoonRocket */
class RocketFileSystem : public QObject
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketFileSystem(RocketPlugin *owner);
    ~RocketFileSystem();
    
    enum FileDialogType
    {
        OpenFileDialog,
        SaveFileDialog,
        DirectoryDialog
    };

    enum InternalTool
    {
        // Crunch texture processing tool
        Crunch = 0,
        // 7zip tool
        SevenZip
    };

    /// Returns internal Rocket tool path for underlying operating system.
    /** @note Tools are usually shipped with the Rocket binary installers. */
    static QString InternalToolpath(InternalTool tool, bool absolutePath = true);
    
    /// Remove directory and all its child files/directories permanently.
    /** @return True if directory and all children was successfully removed. */
    static bool RemoveDirectory(const QString &path);
    static bool RemoveDirectory(QDir dir);
    
    /// Removes all files/directories from a directory permanently.
    /** @return True if directory and all children was successfully removed. */
    static bool ClearDirectory(const QString &path);
    static bool ClearDirectory(QDir dir);
    /// @endcond

    /// @todo C++ versions.
//    void SelectDirectory(const QString &caption, const QString &dir, QObject* initiator,
//        const char* slot, QWidget *parent = 0);
//    void SelectFile(const QString &filter, const QString &caption, const QString &dir,
//        QObject* initiator, const char* slot, QWidget *parent = 0);
//    void SelectFileName(const QString &filter, const QString &caption, const QString &dir,
//        QObject* initiator, const char* slot, QWidget *parent = 0);

public slots:
    /// Opens an open file dialog for picking a single directory.
    /** Before opening the dialog the user is prompted for permission.
        @param caption Dialog's caption.
        @param dir Initial working directory. If empty, the last used directory (if applicable) will be used.
        @param parent Optional parent widget.
        @return Signaler object (use its signals for further actions), or null if we have already ongoing file dialog. */
    RocketFileDialogSignaler *SelectDirectory(const QString &applicationName,
        const QString &caption, const QString &dir, QWidget *parent = 0);

    /// Opens an file selection dialog for picking an existing file.
    /** @param filter The file types to be shown.
        @copydetails SelectDirectory */
    RocketFileDialogSignaler *SelectFile(const QString &applicationName, const QString &filter,
        const QString &caption, const QString &dir, QWidget *parent = 0);

    /// Opens a file selection dialog for picking an existing (overwrite) or a new file (create).
    /** @note This function or the dialog will not create or save anything, it will only return you
        the user selected file path via the returned RocketFileDialogSignaler.
        @param filter The file types to be shown.
        @copydetails SelectDirectory */
    RocketFileDialogSignaler *SelectFileName(const QString &applicationName, const QString &filter,
        const QString &caption, const QString &dir, QWidget *parent = 0);

    /// Returns whether or not we currently have a file dialog invoked via RocketFileSystem visible.
    bool IsFileDialogVisible() const;

private:
    RocketFileDialogSignaler *CreateFileDialog(FileDialogType type, const QString &applicationName,
        const QString &filter, const QString &caption, const QString &dir, QWidget *parent);

    RocketPlugin *rocket;
    QPointer<QFileDialog> fileDialog;
    RocketFileDialogSignaler *fileDialogSignaler;

private slots:
    void SetFileDialogVisible(bool);
};
Q_DECLARE_METATYPE(RocketFileSystem*)

/// Utility signaler object for script usage.
class RocketFileDialogSignaler : public QObject
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketFileDialogSignaler(RocketFileSystem::FileDialogType type);
    
    QString lastDir;
    /// @endcond
    
public slots:
    /** Reads the user selected file.
        @note This function does nothing if the underlying
        operation was a directory selection via RocketFilesSystem::SelectDirectory. */
    QByteArray ReadSelectedFile() const;

    /** Writes bytes to the user selected file.
        @note This function does nothing if the underlying
        operation was not a new file selection via RocketFileSystem::SelectFileName. */
    bool WriteBytesToSelectedFile(const QByteArray &bytes) const;

    /** Writes a string to the user selected file.
        @note Converts the string to UTF8 and calls WriteBytesToSelectedFile.
        This is a convenience method for scrips to pass a JavaScript string without
        doing QByteArray conversions.
        @note This function does nothing if the underlying
        operation was not a new file selection via RocketFileSystem::SelectFileName. */
    bool WriteStringToSelectedFile(const QString &str) const;

signals:
    /// Signaled when user chose whether to authorize file dialog opening operation.
    void OperationAuthorized(bool authorized);

    /// Signals that a file path was chosen from the file dialog.
    /** @param filepath The chosen file or directory path.
        @todo Consider do we want to allow selecting multiple files. */
    void Selected(const QString &filepath, RocketFileDialogSignaler *signaler);

    /// Signals that the file dialog was closed or canceled before selecting a file.
    void Canceled();

private slots:
    void HandleFileDialogResult(int);
    void AllowOperation();
    void DenyOperation();
    
private:
    RocketFileSystem::FileDialogType type_;
    bool authorized_;
    QString selection_;
};
Q_DECLARE_METATYPE(RocketFileDialogSignaler*)
