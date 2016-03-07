/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "qts3/QS3Fwd.h"
#include "MeshmoonStorageItem.h"

#include <QDir>
#include <QDialog>
#include <QFileDialog>

#include "ui_RocketStorageAuthWidget.h"
#include "ui_RocketStorageInfoWidget.h"
#include "ui_RocketStorageSelectionDialog.h"

class RocketStorageListWidget;
class QListWidgetItem;

/// @cond PRIVATE

// RocketStorageAuthDialog

class RocketStorageAuthDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RocketStorageAuthDialog(RocketPlugin *plugin, const QString &storageAclCheckPath, const QString &storageBucket);
    ~RocketStorageAuthDialog();

public slots:
    void Show();
    void TryAuthenticate(const QString &accessKey, const QString &secretKey);
    
private slots:
    void TryAuthenticate();
    void Cancel();
    
    void OnAclReply(QS3GetAclResponse *response);
    
private:
    void Authenticated();
    void AuthenticationFailed(const QString &error = "");
    
    RocketPlugin *plugin_;
    Ui::RocketStorageAuthWidget ui_;
    
    QMovie *loaderAnimation_;
    QS3Client *s3_;
    QString storageAclCheckPath_;
    QString storageBucket_;
    
    bool restoreForeground_;

signals:
    void Authenticated(QString accessKey, QString secretKey);
};

// RocketStorageFileDialog

class RocketStorageFileDialog : public QFileDialog
{
    Q_OBJECT

public:
    explicit RocketStorageFileDialog(QWidget *parent, RocketPlugin *plugin, const QString &title, const QString &startFolder, QDir::Filters filters, bool confirmOverwrite = true);
    ~RocketStorageFileDialog();
    
    MeshmoonStorageItem executionFolder;
    MeshmoonStorageItemWidgetList files;
    
public slots:
    void Open();
    void Close();
    
signals:
    void FilesSelected(const QStringList &files, bool confirmOverwrite);
    
    void FileSelected(const QString &filePath, MeshmoonStorageItemWidgetList items);
    void FolderSelected(const QString &folderPath, MeshmoonStorageItemWidgetList items);
    
private slots:
    void OnFilesSelected(const QStringList &filenames);
    
private:
    bool confirmOverwrite_;
    RocketPlugin *plugin_;
};

// RocketStorageInfoDialog

class RocketStorageInfoDialog : public QDialog
{
Q_OBJECT

public:
    explicit RocketStorageInfoDialog(RocketPlugin *plugin, const QString &title, MeshmoonStorageOperationMonitor *_monitor = 0);
    ~RocketStorageInfoDialog();
    
    void EnableInputMode(const QString &labelInputTitle, const QString &text = "");
    void EnableTextMode(const QString &text);
    
    Ui::RocketStorageInfoWidget ui;
    
    QDir dir;
    MeshmoonStorageItemWidgetList items;
    QStringList names;
    
    /// Storage monitor ptr that will be used if this dialog is accepted. For internal use.
    MeshmoonStorageOperationMonitor *monitor;

public slots:
    void Open();
    void OpenDelayed(uint msec);
    void Close();   
    
signals:
    void AcceptClicked();
    void Rejected();

private:
    RocketPlugin *plugin_;
};

/// @endcond

/// Dialog for selecting an asset from the %Meshmoon storage.
class RocketStorageSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RocketStorageSelectionDialog(RocketPlugin *plugin, MeshmoonStorage *storage, const QStringList &suffixFilters,
                                          bool allowChangingFolder, MeshmoonStorageItem &startDirectory, QWidget *parent = 0);

public slots:
    /// Returns the selected storage item.
    /** @return Selected storage item check isNull() for validity. */
    MeshmoonStorageItem Selected() const;

    /// Returns the currently navigated folder.
    /** Can be used to remember last open directory when opening many dialogs in a row
        by calling this function in the signal handlers.
        @return Storage item for the folder check isNull() for validity. */
    MeshmoonStorageItem CurrentDirectory() const;

    /// Open a folder to this view. Will be rarely needed as the user can navigate from the UI.
    /** @note This function is called automatically with the ctor passed startDirectory.
        @note If this directory is not valid for the current worlds storage, the dialog
        will show that storage could not be listed and it will be unusable. */
    void OpenFolder(const MeshmoonStorageItem &directory);

signals:
    /// Emitted when a storage item is selected.
    void StorageItemSelected(const MeshmoonStorageItem &item);

    /// Emitted when selection was canceled by the user.
    void Canceled();
    
private slots:
    void OnSelect();
    void OnCancel();

    void OnStorageAuthCompleted();
    void OnItemClicked(QListWidgetItem *item);
    void OnItemDoubleClicked(QListWidgetItem *item);
    void OnFilterChanged(const QString &filter);

private:
    RocketPlugin *plugin_;
    MeshmoonStorage *storage_;
    MeshmoonStorageItem item_;
    MeshmoonStorageItem startDirectory_;
    MeshmoonStorageItemWidget *currentFolder_;

    QStringList suffixFilters_;
    bool allowChangingFolder_;

    RocketStorageListWidget *view_;
    Ui::RocketStorageSelectionDialog ui_;
};
