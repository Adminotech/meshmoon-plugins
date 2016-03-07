/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "AssetFwd.h"
#include "InputFwd.h"
#include "SceneFwd.h"
#include "Math/float3.h"

#include "RocketSceneWidget.h"
#include "MeshmoonStorageItem.h"

#include <QTimer>
#include <QListWidget>
#include <QPointer>
#include <QDrag>

#include "ui_RocketStorageWidget.h"

class EC_Mesh;
class RaycastResult;

class RocketStorageListWidget;
class RocketStorageSearchWidget;

class QListWidgetItem;

/// @cond PRIVATE
class StorageItemDragAndDropState : public QObject
{
    Q_OBJECT

public:
    enum ItemType
    {
        IT_Invalid,
        IT_Mesh,
        IT_Material,
        IT_Script
    };
    
    StorageItemDragAndDropState(RocketPlugin *plugin);

    bool IsSupported(const QString &suffix_) const;
    bool HasSupportedAsset() const;
    
    // Current states type.
    ItemType Type() const;
    
    // Suffix needs to be lower case and cannot include prefix '.'
    ItemType TypeFor(const QString &suffix_) const;

    // State init
    void Init(const QString &relativeRef_, const QString &suffix_);
    
    // Filter our stuff that we don't want raycasts to hit.
    bool ShouldIgnoreRaycastResult(RaycastResult *result);

    // Updated on the fly during drop mouse move.
    void UpdateFromRaycastResult(ScenePtr scene_, RaycastResult *result);
    void SetTarget(EntityPtr ent, IComponent *comp);
    void SetSubmeshIndex(int submeshIndex_);
    void SetPositionAndNormal(float3 position, float3 normal);
    
    /** Apply changes as replicated to current target.
        @note This function will call Reset automatically once changes have been applied. */
    void Apply();

    // Reset whole state. Resets scene modifications and refs. 
    void Reset();

    // Clear scene modifications and reset local weak refs to scene.
    void ResetSceneModifications();

    /** Clear current scene preview modifications, eg. restore materials to mesh or
        remove temp/local only ents for mesh drop preview. */
    void ClearScenePreview();

    // Returns tooltip for the current state.
    QString TooltipMessage() const;

private slots:
    void OnMeshDropMeshChanged();

private:
    // All supported drop types
    QString relativeRef;
    QString filename;
    QString filebasename;
    QString suffix;
    
    QStringList supportedMeshSuffixes_;

    SceneWeakPtr scene;

    // Current target entity (under drag and drop mouse)
    EntityWeakPtr entity;
    
    // Returns a unique component name for @c base. If no components with this type, base is returned as is.
    QString UniqueComponentName(u32 typeId, const QString &base);

    // Material specific 
    weak_ptr<EC_Mesh> meshComponent;
    int submeshIndex;
    AssetReference originalMaterialRef;
    
    // Template entity specific (eg. mesh drop)
    void CreateTemplateEntity(const QString &name, const QStringList &components);
    void DestroyTemplateEntity();
    EntityPtr templateEntity;
    
    RocketPlugin *plugin_;
};
/// @endcond

class RocketStorageWidget : public RocketSceneWidget
{
Q_OBJECT

public:
    explicit RocketStorageWidget(RocketPlugin *plugin, MeshmoonStorage *storage, const QString &storageUrl);
    ~RocketStorageWidget();

    RocketStorageListWidget *ListView();
   
    QString storageUrl_;
    Ui::RocketStorageWidget ui;
    
    void ShowTooltip(QString message = "", const QStringList &files = QStringList());
    void HideTooltip();
    void UpdateTooltip();

    void DeleteSelected() { OnDeleteClicked(); }

    QString SearchTerm() const;
    void ApplySearchTerm(const QString &term);
    
    QString RelativeReference(const MeshmoonStorageItemWidget *item) const;
    QString RelativeReference(const QString &key) const;

public slots:
    /// Set progress bar progress. Usually used with bytes.
    void SetProgress(qint64 completed, qint64 total);

    /// Set progress message.
    void SetProgressMessage(const QString &message);

    /// Hide all progress related widgets.
    void HideProgress();

    /// Hide progress bar. Leaves the label visible.
    void HideProgressBar();
    
    /** Returns if the widget is pinned and should not 
        be resized/positioned by external logic. */
    bool IsPinned() const;
    
    /// Returns if the storage view is currently in a protected folder.
    bool InProtectedFolder() const;

signals:
    void FileClicked(MeshmoonStorageItemWidget *item);
    void FolderClicked(MeshmoonStorageItemWidget *item);
    
    void DownloadRequest(MeshmoonStorageItemWidgetList items, bool zip);
    void DeleteRequest(MeshmoonStorageItemWidgetList items);
    
    void UploadSceneRequest(const QString &filepath);
    void UploadRequest(bool confirmOverwrite);
    void UploadRequest(const QStringList &files, bool confirmOverwrite);
    
    void CreateFolderRequest();
    void UploadFilesRequest();
    void CreateNewFileRequest(const QString &suffix);
    
    void RefreshRequest();
    void CloseRequest();
    void LogoutAndCloseRequest();

private slots:
    void OnMouseRightPressed(MouseEvent *e);
    void OnItemClicked(QListWidgetItem *item);
    void OnItemDoubleClicked(QListWidgetItem *item);
    
    void OnDownloadClicked();
    void OnDownloadAsZipClicked();
    void OnDeleteClicked();

    void OnShowMenu();
    void OnHandleDragEnterEvent(QDragEnterEvent *e, QGraphicsItem *widget);
    void OnHandleDragLeaveEvent(QDragLeaveEvent *e);
    void OnHandleDragMoveEvent(QDragMoveEvent *e, QGraphicsItem *widget);
    void OnHandleDropEvent(QDropEvent *e, QGraphicsItem *widget);
    void OnSearchLineEdited(const QString & newText);
    
    bool StorageDropInside3DScene(QDropEvent *e);
    
    void SetPinned(bool pinned);
    void SetPreviewFileOnMouse(bool preview);

protected:
    MeshmoonStorageItemWidget *StorageListItemAtGlobal(const QPoint &globalPos);
    MeshmoonStorageItemWidget *StorageListItemAt(const QPoint &scenePos);

private:
    friend class RocketStorageListWidget;
    
    StorageItemDragAndDropState storageDragAndDropState_;

    RocketPlugin *plugin_;
    MeshmoonStorage *storage_;
    RocketStorageListWidget *listWidget_;
    RocketStorageSearchWidget * searchWidget_;
    QTimer progressTimer_;
    
    QWidget *toolTipWidget_;
    QMenu *contextMenu_;
};

// RocketStorageListWidget

class RocketStorageListWidget : public QListWidget
{
    Q_OBJECT

public:
    /// Constructor. If storageWidget is null key events and context menus are disabled.
    RocketStorageListWidget(QWidget *parent, RocketPlugin *plugin, RocketStorageWidget *storageWidget = 0);
    ~RocketStorageListWidget();

    void SetPreviewFileOnMouse(bool enabled);
    bool ShowContextMenu(const QPoint &pos);

    /// QAbstractItemView override.
    void keyPressEvent(QKeyEvent *event);

signals:
    void CreateFolderRequest();
    void UploadFilesRequest();
    void DownloadRequest();
    void DownloadAsZipRequest();

private slots:
    void CreateNewFileSubMenu(QMenu *menu);
    void OnCreateNewItem();
    
    void ShowPreview(AssetPtr asset);
    void ShowPreview(const QString &diskSource);
    void ResetPreview();
    void OnPreviewAssetLoaded(AssetPtr asset);
    
    void OnMouseEntered(const QModelIndex &index);

    void DragStart(QDrag *drag);
    void DragUpdate(float frametime);
    void DragStop();
    void DragTempFileCleanup();
    
    void OnDragDownloadOperationProgress(QS3GetObjectResponse *response, qint64 complete, qint64 total);
    void OnDragOperationFinished(MeshmoonStorageOperationMonitor *monitor);

protected:
    /// QListWidget override.
    QMimeData *mimeData(const QList<QListWidgetItem*> items) const;
    
    /// QAbstractItemView override.
    void startDrag(Qt::DropActions supportedActions);

    /// QAbstractItemView override.
    bool viewportEvent(QEvent *e);

    /// QAbstractItemView override.
    void mouseMoveEvent(QMouseEvent *e);

private:
    QMenu *contextMenu_;
    QLabel *preview_;
    
    RocketPlugin *plugin_;
    Framework *framework_;
    RocketStorageWidget *storageWidget_;
    
    AssetTransferWeakPtr previewTransfer_;
    
    QPointer<QDrag> drag_;
    QPointer<MeshmoonStorageOperationMonitor> dragMonitor_;
    QMap<QString, QString> pendingDragDownloads_;
};

// RocketStorageSearchWidget

class RocketStorageSearchWidget : public QWidget
{
    Q_OBJECT

public:
    RocketStorageSearchWidget(QWidget * parent, RocketStorageWidget *storageWidget);
    ~RocketStorageSearchWidget();

public slots:
    void Clear();
    
    QString SearchTerm() const;
    
    void ApplySearchTerm(const QString &term);
    
protected:
    bool eventFilter(QObject *obj, QEvent *e);

signals:
    void SearchLineEdited(const QString &text);

private:
    QLineEdit *lineSearch_;
    RocketStorageWidget * storageWidget_;
};
