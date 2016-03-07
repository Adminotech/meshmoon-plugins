/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "qts3/QS3Fwd.h"
#include "qts3/QS3Defines.h"

#include <QListWidgetItem>

/// Higher level representation of a file or a folder in %Meshmoon storage.
struct MeshmoonStorageItem
{
    MeshmoonStorageItem();
    MeshmoonStorageItem(const MeshmoonStorageItemWidget *storageItem, const MeshmoonStorage *storage);
    MeshmoonStorageItem(const QString storageKey, const MeshmoonStorage *storage);

    /// Generates a child storage item.
    /** Meant to help creating valid non-yet-existing MeshmoonStorageItem objects to a storage folder.
        The returned object can be used eg. in MeshmoonStorage::UploadData.
        @note Only works for directory items (isDirectory == true), otherwise a null item is returned. */
    MeshmoonStorageItem GenerateChildFileItem(const QString &filename) const;
    
    /// Is this a null item (empty path).
    bool IsNull() const;

    /// Is directory.
    bool isDirectory;

    /// Is file.
    bool isFile;

    /// Relative storage path from the root.
    QString path;

    /// File or folder name.
    QString filename;

    /// Full asset ref (url).
    QString fullAssetRef;

    /// Relative asset ref.
    /** @note Use this whenever you can when adding refs to scene, its much cleaner. */
    QString relativeAssetRef;
};

/// Lower level representation of a storage item with richer information and can be used directly as a UI item.
class MeshmoonStorageItemWidget : public QObject, public QListWidgetItem
{
    Q_OBJECT

public:
    MeshmoonStorageItemWidget(RocketPlugin *plugin, const QS3Object &data_);
    MeshmoonStorageItemWidget(RocketPlugin *plugin, const QString key);
    MeshmoonStorageItemWidget(RocketPlugin *plugin, const MeshmoonStorageItemWidget *other, MeshmoonStorageItemWidget *parent);
    ~MeshmoonStorageItemWidget();

    QS3Object data;
    QString filename;
    QString suffix;
    QString completeSuffix;
    QString visualEditorName;

    bool hasTextEditor;
    bool hasVisualEditor;
    bool canCreateInstances;

    MeshmoonStorageItemWidget *parent;
    MeshmoonStorageItemWidgetList children;

    void AddChild(MeshmoonStorageItemWidget *item);
    void DeleteChildren();
    
    /// Returns if this item is a protected file or is inside a protected folder.
    bool IsProtected() const;
    
    QString RelativeAssetReference();
    QString AbsoluteAssetReference();

    /// Returns all folders.
    MeshmoonStorageItemWidgetList Folders();
    
    /** Returns all files. If recursive is true, will return all files recursively
        also from child folders. */
    MeshmoonStorageItemWidgetList Files(bool recursive = false);
    MeshmoonStorageItemWidgetList FoldersAndFiles();
    
    static QString IconImagePath(QString suffix, QString completeSuffix = "");
    
    static bool FindParent(MeshmoonStorageItemWidget *child, MeshmoonStorageItemWidget *parentCandidate);

public slots:
    bool OpenVisualEditor();
    bool OpenTextEditor();

    void CopyAssetReference() { OnCopyAssetReference(); }
    void CopyUrl()            { OnCopyUrl(); }
    
    /// Size as a formatted string.
    QString FormattedSize() const;
    
    /// Storage item for this item widget.
    MeshmoonStorageItem Item() const;

signals:
    void DownloadRequest(MeshmoonStorageItemWidget *item);
    void DeleteRequest(MeshmoonStorageItemWidget *item);
    void CopyAssetReferenceRequest(MeshmoonStorageItemWidget *item);
    void CopyUrlRequest(MeshmoonStorageItemWidget *item);
    void CreateCopyRequest(MeshmoonStorageItemWidget *item);
    void InstantiateRequest(MeshmoonStorageItemWidget *item);
    void RestoreBackupRequest(MeshmoonStorageItemWidget *item);
    void EditorOpened(IRocketAssetEditor *editor);
    
private slots:
    void OnDownload();
    void OnDelete();
    void OnCopyAssetReference();
    void OnCopyUrl();
    void OnCreateCopy();
    void OnInstantiate();
    void OnRestoreBackup(); 

private:
    void SetFormat(const QString &suffix_ = "", const QString &completeSuffix_ = "");

    RocketPlugin *plugin_;
};
