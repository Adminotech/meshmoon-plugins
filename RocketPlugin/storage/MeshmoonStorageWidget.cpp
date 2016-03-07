/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "utils/RocketFileSystem.h"

#include "MeshmoonStorage.h"
#include "MeshmoonStorageWidget.h"
#include "MeshmoonStorageHelpers.h"
#include "qts3/QS3Client.h"

#include "Framework.h"
#include "LoggingFunctions.h"

#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "IAsset.h"

#include "InputAPI.h"
#include "ConfigAPI.h"
#include "FrameAPI.h"

#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"

#include "IRenderer.h"
#include "OgreWorld.h"

#include "Scene/Scene.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Script.h"

#include "MemoryLeakCheck.h"

namespace
{
    const ConfigData cStoragePinned("rocket-storage", "storage", "pinned", false);
    const ConfigData cStoragePreviewOnHover("rocket-storage", "storage", "preview-on-hover", true);
}

RocketStorageWidget::RocketStorageWidget(RocketPlugin *plugin, MeshmoonStorage *storage, const QString & storageUrl) :
    RocketSceneWidget(plugin->GetFramework()),
    plugin_(plugin),
    storage_(storage),
    listWidget_(0),
    searchWidget_(0),
    storageUrl_(storageUrl),
    toolTipWidget_(0),
    contextMenu_(0),
    storageDragAndDropState_(plugin)
{
    ui.setupUi(widget_);
    
    listWidget_ = new RocketStorageListWidget(ui.frameStorage, plugin_, this);
    searchWidget_ = new RocketStorageSearchWidget(ui.frameStorage, this);
    ui.verticalLayoutStorage->insertWidget(1, searchWidget_);
    ui.verticalLayoutStorage->insertWidget(2, listWidget_);
    
    ui.progress->hide();
    ui.labelProgressTitle->hide();
    
    progressTimer_.setSingleShot(true);
    connect(&progressTimer_, SIGNAL(timeout()), SLOT(HideProgress()));
    
    connect(listWidget_, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(OnItemClicked(QListWidgetItem*)));
    connect(listWidget_, SIGNAL(itemDoubleClicked(QListWidgetItem*)), SLOT(OnItemDoubleClicked(QListWidgetItem*)));
    connect(listWidget_, SIGNAL(DownloadRequest()), SLOT(OnDownloadClicked()));
    connect(listWidget_, SIGNAL(DownloadAsZipRequest()), SLOT(OnDownloadAsZipClicked()));
    connect(listWidget_, SIGNAL(CreateFolderRequest()), SIGNAL(CreateFolderRequest()));
    connect(listWidget_, SIGNAL(UploadFilesRequest()), SIGNAL(UploadFilesRequest()));
    
    connect(ui.buttonMenu, SIGNAL(clicked()), SLOT(OnShowMenu()), Qt::QueuedConnection);
    
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(MouseRightPressed(MouseEvent*)), SLOT(OnMouseRightPressed(MouseEvent*)), Qt::UniqueConnection);
    
    UiGraphicsView *gv = framework_->Ui()->GraphicsView();
    connect(gv, SIGNAL(DragEnterEvent(QDragEnterEvent*, QGraphicsItem*)), SLOT(OnHandleDragEnterEvent(QDragEnterEvent*, QGraphicsItem*)));
    connect(gv, SIGNAL(DragLeaveEvent(QDragLeaveEvent*)), SLOT(OnHandleDragLeaveEvent(QDragLeaveEvent*)));
    connect(gv, SIGNAL(DragMoveEvent(QDragMoveEvent*, QGraphicsItem*)), SLOT(OnHandleDragMoveEvent(QDragMoveEvent*, QGraphicsItem*)));
    connect(gv, SIGNAL(DropEvent(QDropEvent*, QGraphicsItem*)), SLOT(OnHandleDropEvent(QDropEvent*, QGraphicsItem*)));
    connect(searchWidget_, SIGNAL(SearchLineEdited(const QString &)), SLOT(OnSearchLineEdited(const QString &)));
    
    SetPinned(framework_->Config()->DeclareSetting(cStoragePinned).toBool());    
    connect(ui.pinStorage, SIGNAL(toggled(bool)), SLOT(SetPinned(bool)));
    
    SetPreviewFileOnMouse(framework_->Config()->DeclareSetting(cStoragePreviewOnHover).toBool());
}

RocketStorageWidget::~RocketStorageWidget()
{
    storage_ = 0;

    if (progressTimer_.isActive())
        progressTimer_.stop();
    listWidget_ = 0;
    searchWidget_ = 0;

    SAFE_DELETE(toolTipWidget_);
}

void RocketStorageWidget::OnMouseRightPressed(MouseEvent *e)
{
    if (!isVisible() || !listWidget_)
        return;

    if (e->button == MouseEvent::RightButton && scene())
    {
        // Unfortunately the input contexts that take events over Qt
        // will screw up right mouse clicks to they wont arrive every time.
        // This method here guarantees us right clicks where ever the focus
        // or other handling logic might be!
        QPointF scenePos(e->x, e->y);
        QGraphicsItem *item = scene()->itemAt(scenePos);
        if (item == this)
        {
            QPointF itemPos = item->mapFromScene(scenePos);
            QPoint pos = listWidget_->mapFrom(widget_, itemPos.toPoint());
            if (listWidget_->ShowContextMenu(pos))
                e->Suppress();
        }
    }
}

MeshmoonStorageItemWidget *RocketStorageWidget::StorageListItemAtGlobal(const QPoint &globalPos)
{
    return StorageListItemAt(framework_->Ui()->GraphicsView()->mapFromGlobal(globalPos));
}

MeshmoonStorageItemWidget *RocketStorageWidget::StorageListItemAt(const QPoint &scenePos)
{
    MeshmoonStorageItemWidget *result = 0;
    QGraphicsItem *item = scene()->itemAt(scenePos);
    if (item == this)
    {
        QPointF itemPos = mapFromScene(scenePos);
        QPoint pos = listWidget_->mapFrom(widget_, itemPos.toPoint());
        QListWidgetItem *baseItem = listWidget_->itemAt(pos);
        if (baseItem)
            result = dynamic_cast<MeshmoonStorageItemWidget*>(baseItem);
    }
    return result;
}

void RocketStorageWidget::ShowTooltip(QString message, const QStringList &files)
{   
    if (message.isEmpty() && !files.isEmpty())
    {
        // A single txml takes priority and other files are dropped later.
        foreach(const QString &file, files)
        {
            QFileInfo info(file);
            if (info.suffix() == "txml" || info.suffix() == "scene")
            {
                message = "<b>Import Scene</b> <span style=\"color: rgb(240,240,240);\">" + info.fileName() + "</span>";
                break;
            }
        }
        if (message.isEmpty())
        {
            // Single file
            if (files.size() == 1)
                message = "<b>Upload File</b> <span style=\"color: rgb(240,240,240);\">" + QFileInfo(files.first()).fileName() + "</span>";
            // Multiple files
            else
            {
                QHash<QString, int> suffixes;
                foreach(const QString &file, files)
                    suffixes[QFileInfo(file).suffix()] += 1;
                QStringList messageParts;                
                foreach(const QString &suffix, suffixes.keys())
                    messageParts << QString("%1x %2").arg(suffixes[suffix]).arg(suffix);
                message = "<b>Upload Files</b> <span style=\"color: rgb(240,240,240);\">" + messageParts.join(" ") + "</span>";
            }
        }
    }
    if (message.isEmpty())
        return;

    if (!toolTipWidget_)
    {
        toolTipWidget_ = new QWidget(0, Qt::ToolTip);
        toolTipWidget_->setLayout(new QHBoxLayout());
        toolTipWidget_->layout()->setMargin(0);
        toolTipWidget_->layout()->setSpacing(0);
        toolTipWidget_->setContentsMargins(0,0,0,0);
        toolTipWidget_->setStyleSheet(QString("QWidget { background-color: transparent; } QLabel { padding: 3px; background-color: %1; \
                                              border: 0px; font-family: \"Arial\"; font-size: 14px; color: white; }").arg(Meshmoon::Theme::BlueStr));
        
        QLabel *tip = new QLabel(toolTipWidget_);
        tip->setObjectName("tip");
        tip->setTextFormat(Qt::RichText);
        toolTipWidget_->layout()->addWidget(tip);
    }
    
    QLabel *tip = toolTipWidget_->findChild<QLabel*>("tip");
    if (tip)
        tip->setText(message);

    toolTipWidget_->show();
    toolTipWidget_->update();
    toolTipWidget_->updateGeometry();

    UpdateTooltip();
}

void RocketStorageWidget::HideTooltip()
{
    if (toolTipWidget_)
        toolTipWidget_->hide();
    SAFE_DELETE(toolTipWidget_);
}

void RocketStorageWidget::UpdateTooltip()
{
    if (toolTipWidget_)
    {
        QPoint cursorPos = QCursor::pos();
        toolTipWidget_->move(QPoint(cursorPos.x()+25, cursorPos.y()+15));
    }
}

bool RocketStorageWidget::InProtectedFolder() const
{
    MeshmoonStorageItem dir = storage_->CurrentDirectory();
    QString relativeRef = storage_->RelativeRefForKey(dir.path);
    return relativeRef.startsWith("backup", Qt::CaseSensitive);
}

void RocketStorageWidget::OnHandleDragEnterEvent(QDragEnterEvent *e, QGraphicsItem* /*widget*/)
{
    if (!e || !e->mimeData())
        return;

    storageDragAndDropState_.Reset();

    // No drops allowed to protected folders
    if (InProtectedFolder())
    {
        e->ignore();
        return;
    }

    if (storage_)
        storage_->UpdateStorageWidgetAnimationsState(framework_->Input()->MousePos().x());

    if (!e->source() || !dynamic_cast<RocketStorageListWidget*>(e->source()))
    {
        QStringList files = MeshmoonStorage::ParseFileReferences(e->mimeData());
        if (!files.isEmpty())
        {
            ShowTooltip("", files);
            e->accept();
        }
    }
    else
    {
        QStringList refs = MeshmoonStorage::ParseStorageSchemaReferences(e->mimeData());
        if (!refs.isEmpty())
        {
            QFileInfo fi(refs.first());
            QString ref = fi.filePath();
            if (ref.startsWith(MeshmoonStorage::Schema, Qt::CaseInsensitive))
                ref = ref.mid(18);

            QString relativeRef = storage_->RelativeRefForKey(ref);
            storageDragAndDropState_.Init(relativeRef, fi.suffix().toLower());
            
            if (storageDragAndDropState_.HasSupportedAsset())
            {
                ShowTooltip(storageDragAndDropState_.TooltipMessage());
                e->accept();
            }
        }
    }
}

void RocketStorageWidget::OnHandleDragLeaveEvent(QDragLeaveEvent* e)
{
    storageDragAndDropState_.Reset();
    HideTooltip();
    
    if (storage_)
        storage_->UpdateStorageWidgetAnimationsState(framework_->Input()->MousePos().x());
    
    if (e) 
        e->accept();
}

void RocketStorageWidget::OnHandleDragMoveEvent(QDragMoveEvent *e, QGraphicsItem *widget)
{
    if (!e)
        return;
    if ((widget && widget != this))
    {
        storageDragAndDropState_.ResetSceneModifications();
        if (toolTipWidget_)
            toolTipWidget_->hide();
        e->ignore();
        return;
    }
    
    if (storage_)
        storage_->UpdateStorageWidgetAnimationsState(framework_->Input()->MousePos().x());

    // If this is a drop from storage to the 3D scene, we wont accept it if mouse is on top of storage. 
    // This would break the illusion when raycasting through the widget where the user cannot actually see.
    if (storageDragAndDropState_.HasSupportedAsset() && !StorageDropInside3DScene(e))
        return;

    if (toolTipWidget_ && !toolTipWidget_->isVisible())
        toolTipWidget_->show();
    UpdateTooltip();
    e->acceptProposedAction();
}

void RocketStorageWidget::OnHandleDropEvent(QDropEvent *e, QGraphicsItem *widget)
{
    storageDragAndDropState_.Reset();
    HideTooltip();
    
    if (!e || !e->mimeData())
        return;
    if ((widget && widget != this))
        return;

    // Check if drop contained a txml file.
    if (!e->source() || !dynamic_cast<RocketStorageListWidget*>(e->source()))
    {
        QString txmlPath = storage_->ParseFirstSceneFile(e->mimeData());
        if (!txmlPath.isEmpty())
            emit UploadSceneRequest(txmlPath);
        // Parse all files from drop.
        else
        {
            QStringList acceptedFiles = MeshmoonStorage::ParseFileReferences(e->mimeData());
            if (!acceptedFiles.isEmpty())
                emit UploadRequest(acceptedFiles, true);
        }
        e->acceptProposedAction();
    }
    else
    {
        QStringList refs = MeshmoonStorage::ParseStorageSchemaReferences(e->mimeData());
        if (!refs.isEmpty())
        {
            QFileInfo fi(refs.first());
            QString ref = fi.filePath();
            if (ref.startsWith(MeshmoonStorage::Schema, Qt::CaseInsensitive))
                ref = ref.mid(18);

            QString relativeRef = storage_->RelativeRefForKey(ref);
            storageDragAndDropState_.Init(relativeRef, fi.suffix().toLower());

            if (!storageDragAndDropState_.HasSupportedAsset())
            {
                e->ignore();
                return;
            }
        }
        else
        {
            e->ignore();
            return;
        }

        if (storageDragAndDropState_.HasSupportedAsset() && !StorageDropInside3DScene(e))
        {
            e->ignore();
            return;
        }

        storageDragAndDropState_.Apply();
        e->acceptProposedAction();
    }

    // Clear any tooltips made by the storage -> 3D scene or other logic above drop above!
    SAFE_DELETE(toolTipWidget_);
}

bool RocketStorageWidget::StorageDropInside3DScene(QDropEvent *e)
{
    if (!e)
        return false;

    // If this is a drop from storage to the 3D scene, we wont accept it if mouse is on top of storage. 
    // This would break the illusion when raycasting through the widget where the user cannot actually see.
    if (storageDragAndDropState_.HasSupportedAsset())
    {
        Scene *scene = framework_->Renderer()->MainCameraScene();
        OgreWorldPtr world = (scene != 0 ? scene->Subsystem<OgreWorld>() : OgreWorldPtr());

        if (!world.get() || geometry().contains(e->pos()))
        {
            storageDragAndDropState_.ResetSceneModifications();
            if (toolTipWidget_)
                toolTipWidget_->hide();
            e->ignore();
            return false;
        }

        // Execute raycast and do drop preview. We want to ignore temp entity that the drop state creates so we get all and ignore those.
        RaycastResult *result = 0;
        QList<RaycastResult*> results = world->RaycastAll(e->pos());
        foreach(RaycastResult *r, results)
        {
            if (r && !storageDragAndDropState_.ShouldIgnoreRaycastResult(r))
            {
                result = r;
                break;
            }
        }
        if (!result)
        {
            storageDragAndDropState_.ResetSceneModifications();
            if (toolTipWidget_)
                toolTipWidget_->hide();
            e->ignore();
            return false;
        }
        // Ignore entities that have certain components
        if (result->entity)
        {
            if (result->entity->Component("EC_Avatar") || result->entity->Component("EC_TransformGizmo"))
            {
                storageDragAndDropState_.ResetSceneModifications();
                if (toolTipWidget_)
                    toolTipWidget_->hide();
                e->ignore();
                return false;
            }
        }
        
        storageDragAndDropState_.UpdateFromRaycastResult(scene->shared_from_this(), result);
        ShowTooltip(storageDragAndDropState_.TooltipMessage());
        return true;
    }

    return false;
}

void RocketStorageWidget::OnSearchLineEdited(const QString &newText)
{
    for (int i = 0; i < listWidget_->count(); ++i)
    {
        QListWidgetItem * item = listWidget_->item(i);
        if (item)
            item->setHidden(!newText.isEmpty() ? !item->text().contains(newText, Qt::CaseInsensitive) : false);
    }
}

QString RocketStorageWidget::SearchTerm() const
{
    return (searchWidget_ ? searchWidget_->SearchTerm() : "");        
}

void RocketStorageWidget::ApplySearchTerm(const QString &term)
{
    if (searchWidget_)
        searchWidget_->ApplySearchTerm(term);
}

QString RocketStorageWidget::RelativeReference(const MeshmoonStorageItemWidget *item) const
{
    if (item)
        return RelativeReference(item->data.key);
    return "";
}

QString RocketStorageWidget::RelativeReference(const QString &key) const
{
    return storage_->RelativeRefForKey(key);
}

void RocketStorageWidget::OnItemClicked(QListWidgetItem *item)
{
    MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(item);
    if (storageItem)
        emit FileClicked(storageItem);
}

void RocketStorageWidget::OnItemDoubleClicked(QListWidgetItem *item)
{
    MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(item);
    if (!storageItem)
        return;

    // Folder double click (open9)
    if (storageItem->data.isDir)
    {
        searchWidget_->Clear();
        emit FolderClicked(storageItem);
    }
    // Open visual editor..
    else if (storageItem->hasVisualEditor)
        storageItem->OpenVisualEditor();
    // .. or text editor
    else if (storageItem->hasTextEditor)
        storageItem->OpenTextEditor();
}

void RocketStorageWidget::OnShowMenu()
{
    SAFE_DELETE(contextMenu_);
    contextMenu_ = new QMenu();

    QAction *act = contextMenu_->addAction("Preview on Hover");
    act->setCheckable(true);
    act->setChecked(framework_->Config()->DeclareSetting(cStoragePreviewOnHover).toBool());
    connect(act, SIGNAL(toggled(bool)), this, SLOT(SetPreviewFileOnMouse(bool)));
    
    contextMenu_->addSeparator();
    
    act = contextMenu_->addAction(QIcon(":/images/icon-refresh.png"), "Refresh");
    connect(act, SIGNAL(triggered()), this, SIGNAL(RefreshRequest()), Qt::QueuedConnection);

    act = contextMenu_->addAction(QIcon(":/images/icon-x-24x24.png"), "Close Storage");
    connect(act, SIGNAL(triggered()), this, SIGNAL(CloseRequest()), Qt::QueuedConnection);

    act = contextMenu_->addAction(QIcon(":/images/icon-minus-32x32.png"), "Logout Storage");
    connect(act, SIGNAL(triggered()), this, SIGNAL(LogoutAndCloseRequest()), Qt::QueuedConnection);
    
    contextMenu_->popup(QCursor::pos());
}

void RocketStorageWidget::OnDownloadClicked()
{
    MeshmoonStorageItemWidgetList items;
    foreach(QListWidgetItem *selection, listWidget_->selectedItems())
    {
        MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(selection);
        if (storageItem && !storageItem->data.isDir)
            items << storageItem;
    }
    if (!items.isEmpty())
        emit DownloadRequest(items, false);
}

void RocketStorageWidget::OnDownloadAsZipClicked()
{
    MeshmoonStorageItemWidgetList items;
    foreach(QListWidgetItem *selection, listWidget_->selectedItems())
    {
        MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(selection);
        if (storageItem && !storageItem->data.isDir)
            items << storageItem;
        else if (storageItem && storageItem->data.isDir)
        {
            if (!storageItem->IsProtected())
                items << storageItem->Files(true);
        }
    }
    if (!items.isEmpty())
        emit DownloadRequest(items, true);
}

void RocketStorageWidget::OnDeleteClicked()
{
    if (!ListView())
        return;

    MeshmoonStorageItemWidgetList items;
    foreach(QListWidgetItem *selection, ListView()->selectedItems())
    {
        MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(selection);
        if (storageItem)
            items << storageItem;
    }
    if (!items.isEmpty())
        emit DeleteRequest(items);
}

RocketStorageListWidget *RocketStorageWidget::ListView()
{
    return listWidget_;
}

void RocketStorageWidget::SetProgressMessage(const QString &message)
{
    if (!ui.labelProgressTitle->isVisible())
        ui.labelProgressTitle->show();
    ui.labelProgressTitle->setText(message);
}

void RocketStorageWidget::SetProgress(qint64 completed, qint64 total)
{
    if (completed < 0 || total <= 0)
        return;
        
    if (completed >= total)
    {
        ui.labelProgressTitle->setText("Done");
        progressTimer_.start(3000);
    }
    else if (progressTimer_.isActive())
        progressTimer_.stop();
        
    if (!ui.progress->isVisible())
        ui.progress->show();
    if (!ui.labelProgressTitle->isVisible() && !ui.labelProgressTitle->text().isEmpty())
        ui.labelProgressTitle->show();
    
    int progress = int(((qreal)completed / (qreal)total) * 100.0);
    if (progress < 0)
        progress = 0;
    else if (progress > 100)
        progress = 100;
    ui.progress->setValue(progress);
}

void RocketStorageWidget::HideProgress()
{
    HideProgressBar();
    ui.labelProgressTitle->hide();
    ui.labelProgressTitle->setText("");
}

void RocketStorageWidget::HideProgressBar()
{
    ui.progress->hide();
    ui.progress->setValue(0);
}

void RocketStorageWidget::SetPinned(bool pinned)
{
    if (ui.pinStorage->isChecked() != pinned)
    {
        ui.pinStorage->blockSignals(true);
        ui.pinStorage->setChecked(pinned);
        ui.pinStorage->blockSignals(false);
    }
    ui.pinStorage->setToolTip(pinned ? "Uncheck to enable automatic hiding " : "Check to disable automatic hiding ");
    Fw()->Config()->Write(cStoragePinned, ui.pinStorage->isChecked());
}

bool RocketStorageWidget::IsPinned() const
{
    return ui.pinStorage->isChecked();
}

void RocketStorageWidget::SetPreviewFileOnMouse(bool preview)
{
    if (listWidget_)
        listWidget_->SetPreviewFileOnMouse(preview);
    Fw()->Config()->Write(cStoragePreviewOnHover, preview);
}

// RocketStorageSearchWidget

RocketStorageSearchWidget::RocketStorageSearchWidget(QWidget * parent, RocketStorageWidget * storageWidget) :
    QWidget(parent),
    storageWidget_(storageWidget),
    lineSearch_(0)
{
    setObjectName(QString::fromUtf8("storageSearchWidget"));
    setStyleSheet(QString("QWidget#storageSearchWidget { background-color: transparent; } ") +
        "QLineEdit#storageSearchLine { background-color: transparent; border: 0px; font-family: 'Arial'; font-weight: bold; font-size: 13px; padding: 3px; padding-left: 0px; }" +
        "QLineEdit#storageSearchLine:focus { background-color: rgba(255,255,255,200); }");

    QHBoxLayout *layout = new QHBoxLayout();
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);
    setLayout(layout);

    lineSearch_ = new QLineEdit(this);
    lineSearch_->setObjectName(QString::fromUtf8("storageSearchLine"));
    lineSearch_->setPlaceholderText("Filter...");
    lineSearch_->setFocusPolicy(Qt::ClickFocus);
    layout->addWidget(lineSearch_);
    lineSearch_->clearFocus();
    
    lineSearch_->installEventFilter(this);

    connect(lineSearch_, SIGNAL(textEdited(const QString&)), this, SIGNAL(SearchLineEdited(const QString&)));
}

RocketStorageSearchWidget::~RocketStorageSearchWidget()
{
}

bool RocketStorageSearchWidget::eventFilter(QObject *obj, QEvent *e)
{
    if (!obj || !e || obj != lineSearch_)
        return false;
    if (e->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent*>(e);
        if (ke->modifiers() == Qt::NoModifier && ke->key() == Qt::Key_Escape)
        {
            Clear();
            return true;
        }
    }
    return false;
}

void RocketStorageSearchWidget::Clear()
{
    if (lineSearch_)
    {
        ApplySearchTerm("");
        lineSearch_->clearFocus();
    }
}

QString RocketStorageSearchWidget::SearchTerm() const
{
    return (lineSearch_ ? lineSearch_->text() : "");
}

void RocketStorageSearchWidget::ApplySearchTerm(const QString &term)
{
    if (!lineSearch_)
        return;

    // setText() does not emit textEdited signal
    lineSearch_->setText(term);
    emit SearchLineEdited(term);
}

// RocketStorageListWidget

RocketStorageListWidget::RocketStorageListWidget(QWidget *parent, RocketPlugin *plugin, RocketStorageWidget *storageWidget) :
    QListWidget(parent),
    plugin_(plugin),
    framework_(plugin->Fw()),
    storageWidget_(storageWidget),
    contextMenu_(0),
    preview_(0)
{
    setObjectName(QString::fromUtf8("listWidget"));
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setLineWidth(0);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setIconSize(QSize(24, 24));
    setWordWrap(true);
    setSortingEnabled(false);
    setFocusPolicy(Qt::StrongFocus);
    setDragEnabled(true);
    setDropIndicatorShown(false);
    setMouseTracking(true);

    SetPreviewFileOnMouse(false);
    
    DragTempFileCleanup();
}

RocketStorageListWidget::~RocketStorageListWidget()
{
    SAFE_DELETE(contextMenu_);
    ResetPreview();
    DragTempFileCleanup();
}

void RocketStorageListWidget::keyPressEvent(QKeyEvent *e)
{
    if (!storageWidget_ || !e)
        return;

    // Delete
    if (e->key() == Qt::Key_Delete && storageWidget_)
    {
        if (selectedItems().size() > 0)
            storageWidget_->DeleteSelected();
        e->accept();
        return;
    }
    // Ctrl + <key> with one item selected
    else if (e->modifiers() == Qt::ControlModifier && selectedItems().size() == 1)
    {
        MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(selectedItems().first()); 
        if (storageItem)
        {
            if (e->key() == Qt::Key_C)
            {
                storageItem->CopyAssetReference();
                e->accept();
                return;
            }
            else if (e->key() == Qt::Key_X)
            {
                storageItem->CopyUrl();
                e->accept();
                return;
            }
        }
    }
    
    QListWidget::keyPressEvent(e);
}

QMimeData *RocketStorageListWidget::mimeData(const QList<QListWidgetItem*> items) const
{
    // Make a temp dir where we can properly name the files and download items that are not yet in the cache.    
    QDir destDir = QDir::fromNativeSeparators(Application::UserDataDirectory() + "assetcache/meshmoon/storage");
    if (!destDir.exists())
        destDir.mkpath(destDir.absolutePath());

    QVariantMap pendingDownloads;

    // Multi selection
    if (items.size() > 1)
    {
        // Set file:// refs for dragging into application/disk.
        QList<QUrl> fileUrls;
        foreach(QListWidgetItem *baseItem, items)
        {
            MeshmoonStorageItemWidget *item = dynamic_cast<MeshmoonStorageItemWidget*>(baseItem);
            if (!item || item->data.isDir)
                continue;

            QString destFile = destDir.absoluteFilePath(item->data.key);
            if (QFile::exists(destFile))
                QFile::remove(destFile);
            pendingDownloads[item->data.key] = destFile;
            fileUrls << QUrl::fromLocalFile(destFile);
        }
        if (fileUrls.isEmpty())
            return 0;

        QMimeData *mime = new QMimeData();
        mime->setProperty("pendingDownloads", pendingDownloads);
        mime->setUrls(fileUrls);
        return mime;
    }

    // Single selection
    MeshmoonStorageItemWidget *item = dynamic_cast<MeshmoonStorageItemWidget*>(items.first());
    if (!item || item->data.isDir)
        return 0;

    QMimeData *mime = new QMimeData();
    const QString relativeRef = item->RelativeAssetReference();
    const QString absoluteRef = item->AbsoluteAssetReference();
    const QString diskSource = framework_->Asset()->Cache()->FindInCache(framework_->Asset()->ResolveAssetRef("", relativeRef));

    // Our internal drag and drop functionality expects a 
    // meshmoonstorage:// ref to be the first URL. 
    QVariantList storageRefs;
    storageRefs << QUrl(MeshmoonStorage::Schema + item->data.key);

    /** Mark pending downloads if mouse is moved outside of the main window.
        The destFile is fake at this point, it does not exist, but the OS should not care
        during the drag. We will inform with a tooltip the user not to release 
        mouse before downloads are done. This is utterly a shit way of doing this
        but Qt does not seemingly allow for a better way (cant delay drag execution
        while downloading files, without the user thinking the drag did not start and
        hence will let go of the mouse button). */
    QList<QUrl> fileUrls;
    QString destFile = destDir.absoluteFilePath(item->data.key);
    if (QFile::exists(destFile))
        QFile::remove(destFile);
    pendingDownloads[item->data.key] = destFile;
    fileUrls << QUrl::fromLocalFile(destFile);

    mime->setUrls(fileUrls);
    mime->setProperty("storageRefs", storageRefs);
    mime->setProperty("relativeRef", relativeRef);
    mime->setProperty("absoluteRef", absoluteRef);
    mime->setProperty("diskSource", diskSource);
    mime->setProperty("pendingDownloads", pendingDownloads);

    // Update OS clipboard
    if (QApplication::clipboard())
        QApplication::clipboard()->setMimeData(mime);    

    return mime;
}

void RocketStorageListWidget::startDrag(Qt::DropActions supportedActions)
{
    if (drag_ || dragMonitor_ || !pendingDragDownloads_.isEmpty())
    {
        LogError("Drag and drop operation already ongoing");
        return;
    }

    QModelIndexList indexes = selectedIndexes();
    if (indexes.count() > 0)
    {
        ResetPreview();

        QMimeData *data = model()->mimeData(indexes);
        if (data)
        {
            QDrag *drag = new QDrag(this);
            drag->setMimeData(data);
            
            DragStart(drag);
            drag->exec(supportedActions | Qt::LinkAction, Qt::CopyAction);
            drag->deleteLater();
            DragStop();
        }
    }
}

void RocketStorageListWidget::DragTempFileCleanup()
{
    RocketFileSystem::ClearDirectory(QDir::fromNativeSeparators(Application::UserDataDirectory() + "assetcache/meshmoon/storage"));
}

void RocketStorageListWidget::DragStart(QDrag *drag)
{
    DragStop();

    drag_ = drag;

    if (drag_ && drag_->mimeData())
    {
        QVariantMap downloads = drag_->mimeData()->property("pendingDownloads").toMap();
        foreach(const QString &storagePath, downloads.keys())
            pendingDragDownloads_[storagePath] = downloads[storagePath].toString();

        connect(framework_->Frame(), SIGNAL(Updated(float)), SLOT(DragUpdate(float)), Qt::UniqueConnection);
    }
}

void RocketStorageListWidget::DragUpdate(float /*frametime*/)
{
    if (!plugin_->Storage()->StorageWidget())
        return;

    if (drag_)
    {
        QPoint mousePos = QCursor::pos();
        if (!framework_->Ui()->MainWindow()->geometry().contains(mousePos))
        {
            if (!dragMonitor_ && !pendingDragDownloads_.isEmpty())
            {
                dragMonitor_ = plugin_->Storage()->DownloadFiles(pendingDragDownloads_);
                connect(dragMonitor_, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDragDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
                connect(dragMonitor_, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnDragOperationFinished(MeshmoonStorageOperationMonitor*)));

                plugin_->Storage()->StorageWidget()->ShowTooltip("Downloading drop targets, please wait...");
                pendingDragDownloads_.clear();
            }
            plugin_->Storage()->StorageWidget()->UpdateTooltip();
        }
        else if (!dragMonitor_ && !plugin_->Storage()->StorageWidget()->storageDragAndDropState_.HasSupportedAsset())
            plugin_->Storage()->StorageWidget()->HideTooltip();
    }
}

void RocketStorageListWidget::OnDragDownloadOperationProgress(QS3GetObjectResponse *response, qint64 complete, qint64 total)
{
    if (!drag_)
        return;

    MeshmoonStorageOperationMonitor *monitor = dynamic_cast<MeshmoonStorageOperationMonitor*>(sender());
    if (!plugin_->Storage()->StorageWidget() || !monitor || complete < 0 || total <= 0)
        return;
    
    int progress = int(((qreal)complete / (qreal)total) * 100.0);
    if (progress < 0)
        progress = 0;
    else if (progress > 100)
        progress = 100;

    plugin_->Storage()->StorageWidget()->ShowTooltip(QString("Downloading drop targets, please wait... %1/%2 %3%").arg(monitor->completed+1).arg(monitor->total).arg(progress));
}

void RocketStorageListWidget::OnDragOperationFinished(MeshmoonStorageOperationMonitor *monitor)
{
    if (!drag_)
        return;

    if (plugin_->Storage()->StorageWidget())
        plugin_->Storage()->StorageWidget()->ShowTooltip(QString("Ready to drop %1 %2").arg(monitor->total).arg(monitor->total > 1 ? "files" : "file"));

    dragMonitor_ = 0;
}

void RocketStorageListWidget::DragStop()
{
    drag_ = 0;

    // Abort any unfinished download operations.
    if (dragMonitor_)
    {
        plugin_->Storage()->Abort(dragMonitor_);
        dragMonitor_ = 0;
    }
    
    pendingDragDownloads_.clear();
    
    if (plugin_->Storage()->StorageWidget())
        plugin_->Storage()->StorageWidget()->HideTooltip();
}

void RocketStorageListWidget::SetPreviewFileOnMouse(bool enabled)
{
    ResetPreview();
    setProperty("previewOnHover", enabled);

    if (enabled)
        connect(this, SIGNAL(entered(const QModelIndex&)), SLOT(OnMouseEntered(const QModelIndex&)), Qt::UniqueConnection);
}

void RocketStorageListWidget::OnPreviewAssetLoaded(AssetPtr asset)
{
    IAssetTransfer *current = previewTransfer_.lock().get();
    IAssetTransfer *origin = dynamic_cast<IAssetTransfer*>(sender());

    // This handler will get all hover initiated hover transfer completions.
    // Only show the actual currently hovered transfer asset.
    if (current && current == origin)
    {
        ShowPreview(asset);
        previewTransfer_.reset();
    }

    // Binary indicates this asset was not loaded to AssetAPI pre preview.
    // Make sure by our internal flag, we don't want to accidentally unload scene assets.
    if (asset->Type() == "Binary" && origin && !origin->property("wasLoaded").toBool())
        framework_->Asset()->ForgetAsset(asset, false);
}

void RocketStorageListWidget::ShowPreview(AssetPtr asset)
{
    ShowPreview(asset->DiskSource());
}

void RocketStorageListWidget::ShowPreview(const QString &diskSource)
{
    QFileInfo fi(diskSource);
    QString suffix = fi.suffix().toLower();

    // Texture
    if (QImageReader::supportedImageFormats().contains(suffix.toAscii()))
    {
        QImage img(diskSource);
        if (!img.isNull())
        {
            QImage scaled = (img.width() > 256 || img.height() > 256 ? img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation) : img);
            if (!preview_)
            {
                preview_ = new QLabel(0, Qt::ToolTip);
                preview_->setStyleSheet("border: 1px outset #1f1f1f");
                preview_->setAttribute(Qt::WA_DeleteOnClose, true);
                preview_->show();
            }
            
            preview_->setFixedSize(scaled.size());
            preview_->move(QCursor::pos() + QPoint(25, 25));
            preview_->setPixmap(QPixmap::fromImage(scaled));
        }
        else
            ResetPreview();
    }
    else
        ResetPreview();
}

void RocketStorageListWidget::ResetPreview()
{
    previewTransfer_.reset();
    if (preview_)
    {
        preview_->close();
        preview_ = 0;
    }
}

void RocketStorageListWidget::OnMouseEntered(const QModelIndex &index)
{
    if (!property("previewOnHover").toBool())
        return;

    MeshmoonStorageItemWidget *storageItem = dynamic_cast<MeshmoonStorageItemWidget*>(item(index.row()));
    if (!storageItem || storageItem->data.isDir)
    {
        ResetPreview();
        return;
    }

    QFileInfo fi(storageItem->data.key);
    QString suffix = fi.suffix().toLower();
    
    // Texture
    if (QImageReader::supportedImageFormats().contains(suffix.toAscii()))
    {
        QString ref = storageItem->RelativeAssetReference();
        AssetPtr asset = framework_->Asset()->FindAsset(ref);
        if (!asset.get() || !asset->IsLoaded() || asset->DiskSource().isEmpty())
        {
            previewTransfer_ = framework_->Asset()->RequestAsset(ref, (asset.get() ? "" : "Binary"));
            if (!previewTransfer_.expired())
            {
                previewTransfer_.lock()->setProperty("wasLoaded", asset.get() != 0);
                connect(previewTransfer_.lock().get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnPreviewAssetLoaded(AssetPtr)), Qt::UniqueConnection);
            }
        }
        else
            ShowPreview(asset);
    }
    else
        ResetPreview();
}

bool RocketStorageListWidget::viewportEvent(QEvent *e)
{
    if (e->type() == QEvent::HoverLeave)
        ResetPreview();
    return QAbstractItemView::viewportEvent(e);
}

void RocketStorageListWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (property("previewOnHover").toBool())
    {
        if (preview_)
            preview_->move(QCursor::pos() + QPoint(25, 25));

        QListWidgetItem *item = itemAt(e->pos());
        if (!item)
            ResetPreview();
    }
    return QAbstractItemView::mouseMoveEvent(e);
}

bool RocketStorageListWidget::ShowContextMenu(const QPoint &pos)
{
    if (!storageWidget_)
        return false;

    QListWidgetItem *baseItem = itemAt(pos);
    MeshmoonStorageItemWidget *storageItem = (baseItem != 0 ? dynamic_cast<MeshmoonStorageItemWidget*>(baseItem) : 0);
    if (!storageItem)
    {
        SAFE_DELETE(contextMenu_);
        if (!storageWidget_->InProtectedFolder())
        {
            contextMenu_ = new QMenu();
            CreateNewFileSubMenu(contextMenu_);
            if (!contextMenu_->actions().isEmpty())
                contextMenu_->popup(QCursor::pos());
        }
        return true;
    }
    if (storageItem->data.isDir && storageItem->filename.isEmpty())
        return false;
    
    QList<QListWidgetItem*> selected = selectedItems();
    
    // If clicked item is selected. Take into account the whole group of items.
    // If not, deselect everything else and do operations on the one item.
    if (!storageItem->isSelected())
    {
        clearSelection();
        selected.clear();
        storageItem->setSelected(true);
    }
    if (selected.size() == 1 && selected.first() == baseItem)
        selected.clear();

    // Create
    SAFE_DELETE(contextMenu_);
    contextMenu_ = new QMenu();

    // Fill single selection file
    QAction *act = 0;
    if (selected.isEmpty())
    {        
        bool inProtectedFolder = storageItem->IsProtected();

        if (!storageItem->data.isDir)
        {
            // Edit
            if (storageItem->hasTextEditor || storageItem->hasVisualEditor)
            {
                // Visual editor
                if (storageItem->hasVisualEditor)
                {
                    act = contextMenu_->addAction(QIcon(":/images/icon-visual-editor-32x32.png"), QString("Open in %1...")
                        .arg(!storageItem->visualEditorName.isEmpty() ? storageItem->visualEditorName : "Visual Editor"));
                    connect(act, SIGNAL(triggered()), storageItem, SLOT(OpenVisualEditor()));
                }
                // Text editor
                if (storageItem->hasTextEditor)
                {
                    act = contextMenu_->addAction(QIcon(":/images/icon-text-editor-32x32.png"), "Open in Text Editor...");
                    connect(act, SIGNAL(triggered()), storageItem, SLOT(OpenTextEditor()));
                }
            }
            contextMenu_->addSeparator();

            // Instancing
            if (storageItem->canCreateInstances)
            {
                act = 0;
                if (storageItem->suffix == "txml")
                {
                    if (inProtectedFolder)
                    {
                        act = contextMenu_->addAction(QIcon(":/images/icon-box.png"), "Restore World to this Backup");
                        connect(act, SIGNAL(triggered()), storageItem, SLOT(OnRestoreBackup()));
                        act = 0;
                    }
                    //else 
                    //    act = contextMenu_->addAction("Instantiate scene");
                }
                else if (storageItem->suffix == "mesh")
                    act = contextMenu_->addAction(QIcon(":/images/icon-box.png"), "Create Mesh Entity");
                else if (storageItem->suffix == "js")
                    act = contextMenu_->addAction(QIcon(":/images/icon-box.png"), "Create Script Entity");

                if (act)
                    connect(act, SIGNAL(triggered()), storageItem, SLOT(OnInstantiate()));
            }
            contextMenu_->addSeparator();

            act = contextMenu_->addAction(QIcon(":/images/icon-copy-32x32.png"), "Copy Asset Reference");
            act->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_C));
            connect(act, SIGNAL(triggered()), storageItem, SLOT(OnCopyAssetReference()));

            act = contextMenu_->addAction(QIcon(":/images/icon-copy-32x32.png"), "Copy URL");
            act->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_X));
            connect(act, SIGNAL(triggered()), storageItem, SLOT(OnCopyUrl()));

            if (!inProtectedFolder)
            {
                act = contextMenu_->addAction(QIcon(":/images/icon-clone-32x32.png"), "Create Copy...");
                connect(act, SIGNAL(triggered()), storageItem, SLOT(OnCreateCopy()));
            }

            act = contextMenu_->addAction(QIcon(":/images/icon-download-64x64.png"), "Download...");
            connect(act, SIGNAL(triggered()), storageItem, SLOT(OnDownload()));
            
            if (!inProtectedFolder)
            {
                contextMenu_->addSeparator();
                CreateNewFileSubMenu(contextMenu_);
            }
            contextMenu_->addSeparator();
        }
        else if (!inProtectedFolder)
        {
            act = contextMenu_->addAction(QIcon(":/images/icon-download-64x64.png"), "Download as zip...");
            connect(act, SIGNAL(triggered()), this, SIGNAL(DownloadAsZipRequest()));
        }

        // Delete item
        if (!inProtectedFolder)
        {
            act = contextMenu_->addAction("Delete");
            act->setShortcut(QKeySequence(Qt::Key_Delete));
            connect(act, SIGNAL(triggered()), storageItem, SLOT(OnDelete()));
        }

        // Size if applicable
        QString sizeStr = storageItem->FormattedSize();
        if (!sizeStr.isEmpty())
        {
            act = contextMenu_->addAction(sizeStr);
            QFont sizeFont("Courier New");
            sizeFont.setPixelSize(11);
            act->setFont(sizeFont);
            act->setDisabled(true);
        }
    }
    else
    {
        bool selectionContainsFolders = false;
        foreach(QListWidgetItem *sItem, selected)
        {
            MeshmoonStorageItemWidget * it = dynamic_cast<MeshmoonStorageItemWidget*>(sItem);
            if (it->data.isDir)
            {
                selectionContainsFolders = true;
                break;
            }
        }

        if (!selectionContainsFolders)
        {
            act = contextMenu_->addAction(QIcon(":/images/icon-download-64x64.png"), "Download Selected...");
            connect(act, SIGNAL(triggered()), this, SIGNAL(DownloadRequest()));
        }

        act = contextMenu_->addAction(QIcon(":/images/icon-download-64x64.png"), "Download Selected as zip...");
        connect(act, SIGNAL(triggered()), this, SIGNAL(DownloadAsZipRequest()));

        act = contextMenu_->addAction("Delete Selected");
        connect(act, SIGNAL(triggered()), storageWidget_, SLOT(OnDeleteClicked()));
    }

    // Show
    if (!contextMenu_->actions().isEmpty())
        contextMenu_->popup(QCursor::pos());
    return true;
}

void RocketStorageListWidget::CreateNewFileSubMenu(QMenu *menu)
{
    if (!menu)
        return;

    QMenu *newFileMenu = menu->addMenu(QIcon(":/images/icon-create-32x32.png"), "New");
    if (!newFileMenu)
        return;

    QAction *act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("folder")), "Folder");
    act->setProperty("createNewFileSuffix", "folder");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));
    
    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("file")), "Upload files...");
    act->setProperty("createNewFileSuffix", "uploadfiles");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));

    newFileMenu->addSeparator();
    
    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("material")), "Ogre Material (.material)");
    act->setProperty("createNewFileSuffix", ".material");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));
    
    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("particle")), "Ogre Particle (.particle)");
    act->setProperty("createNewFileSuffix", ".particle");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));

    newFileMenu->addSeparator();

    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("js")), "Meshmoon Script (.js)");
    act->setProperty("createNewFileSuffix", ".js");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));

    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("js")), "WebRocket Script (.webrocketjs)");
    act->setProperty("createNewFileSuffix", ".webrocketjs");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));

    newFileMenu->addSeparator();

    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("json")), "JSON file (.json)");
    act->setProperty("createNewFileSuffix", ".json");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));

    act = newFileMenu->addAction(QIcon(MeshmoonStorageItemWidget::IconImagePath("file")), "Text file (.txt)");
    act->setProperty("createNewFileSuffix", ".txt");
    connect(act, SIGNAL(triggered()), this, SLOT(OnCreateNewItem()));
}

void RocketStorageListWidget::OnCreateNewItem()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (!act || !storageWidget_)
        return;
        
    QString suffix = act->property("createNewFileSuffix").toString().trimmed();
    if (suffix.isEmpty())
        return;

    if (suffix != "folder" && suffix != "uploadfiles")
        emit storageWidget_->CreateNewFileRequest(suffix);
    else if (suffix == "folder")
        emit CreateFolderRequest();
    else if (suffix == "uploadfiles")
        emit UploadFilesRequest();
}

/// @cond PRIVATE

StorageItemDragAndDropState::StorageItemDragAndDropState(RocketPlugin *plugin) :
    plugin_(plugin),
    submeshIndex(-1)
{
    Reset();
    
    supportedMeshSuffixes_ << "mesh" << "3d" << "b3d" << "blend" << "dae" << "bvh" << "3ds" << "ase" << "obj" << "ply" << "dxf" <<
        "nff" << "smd" << "vta" << "mdl" << "md2" << "md3" << "mdc" << "md5mesh" << "x" << "q3o" << "q3s" << "raw" << "ac" <<
        "stl" << "irrmesh" << "irr" << "off" << "ter" << "mdl" << "hmp" << "ms3d" << "lwo" << "lws" << "lxo" << "csm" <<
        "ply" << "cob" << "scn" << "fbx";
}

bool StorageItemDragAndDropState::HasSupportedAsset() const
{
    return relativeRef != "" && suffix != "";
}

bool StorageItemDragAndDropState::IsSupported(const QString &suffix_) const
{
    if (suffix_.startsWith("."))
        return TypeFor(suffix_.mid(1).toLower()) != IT_Invalid;
    return TypeFor(suffix_.toLower()) != IT_Invalid;
}

StorageItemDragAndDropState::ItemType StorageItemDragAndDropState::Type() const
{
    return TypeFor(suffix);
}

StorageItemDragAndDropState::ItemType StorageItemDragAndDropState::TypeFor(const QString &suffix_) const
{
    if (suffix_ == "material")
        return IT_Material;
    else if (suffix_ == "js" || suffix_ == "webrocketjs")
        return IT_Script;
    else if (supportedMeshSuffixes_.contains(suffix_, Qt::CaseInsensitive))
        return IT_Mesh;
    return IT_Invalid;
}

QString StorageItemDragAndDropState::TooltipMessage() const
{
    QString message;
    if (!HasSupportedAsset())
        return message;

    ItemType type = Type();
    if (type == IT_Material)
    {
        message = "<b>Apply Material</b> <span style=\"color: rgb(240,240,240);\">" + filename + "</span>";
        if (!entity.expired() && submeshIndex != -1)
        {
            message += QString(" <b>to</b> <span style=\"color: rgb(240,240,240);\">%1 index %2</span>")
                .arg(entity.lock()->Name() != "" ? entity.lock()->Name() : "Entity " + QString::number(entity.lock()->Id()))
                .arg(QString::number(submeshIndex));
        }
    }
    else if (type == IT_Mesh)
    {
        message = "<b>Create Mesh</b> <span style=\"color: rgb(240,240,240);\">" + filename + "</span>";
        if (templateEntity.get())
        {
            EC_Placeable *placeable = templateEntity->Component<EC_Placeable>().get();
            if (placeable)
            {
                float3 pos = placeable->WorldPosition();
                message += QString(" <b>to</b> <span style=\"color: rgb(240,240,240);\">(%1, %2, %3)</span>").arg(pos.x, 0, 'f', 2).arg(pos.y, 0, 'f', 2).arg(pos.z, 0, 'f', 2);
            }
        }
    }
    else if (type == IT_Script)
    {
        if (!entity.expired())
        {
            message = QString("<b>Attach Script</b> <span style=\"color: rgb(240,240,240);\">%1</span> <b>to</b> <span style=\"color: rgb(240,240,240);\">%2</span>").arg(filename)
                .arg(entity.lock()->Name() != "" ? entity.lock()->Name() : "Entity " + QString::number(entity.lock()->Id()));
        }
    }
    return message;
}

void StorageItemDragAndDropState::Init(const QString &relativeRef_, const QString &suffix_)
{
    Reset();
    
    // This is where we decide if this ref/suffix is valid. If not we return without setting the internal state!
    // This logic will be used to advance the drag and drop and contentiously updating this state.
    if (!IsSupported(suffix_))
        return;

    relativeRef = relativeRef_;
    suffix = suffix_;
    
    QFileInfo fi(relativeRef_);
    filename = fi.fileName();
    filebasename = fi.baseName();
}

void StorageItemDragAndDropState::Reset()
{
    ResetSceneModifications();
    
    // Hack for mesh drops so it wont flicker (constantly creating/removing the template),
    // only really destroy the entity when drop is canceled.
    // This should be done in ResetSceneModifications but cant.
    DestroyTemplateEntity();

    relativeRef.clear();
    suffix.clear();
    filename.clear();
    filebasename.clear();
}

void StorageItemDragAndDropState::ResetSceneModifications()
{
    ClearScenePreview();

    entity.reset();

    meshComponent.reset();
    submeshIndex = -1;
}

void StorageItemDragAndDropState::ClearScenePreview()
{
    ItemType type = Type();
    if (type == IT_Material)
    {
        if (submeshIndex != -1 && !meshComponent.expired())
        {
            AssetReferenceList materials = meshComponent.lock()->materialRefs.Get();
            if (submeshIndex < materials.Size())
            {
                materials.Set(submeshIndex, originalMaterialRef);
                meshComponent.lock()->materialRefs.Set(materials, AttributeChange::LocalOnly);
            }
        }
        
        submeshIndex = -1;
        originalMaterialRef.ref = "";
    }
    else if (type == IT_Mesh)
    {
        if (templateEntity.get())
        {
            EC_Placeable *placeable = templateEntity->Component<EC_Placeable>().get();
            if (placeable)
                placeable->visible.Set(false, AttributeChange::LocalOnly);
        }
    }
}

bool StorageItemDragAndDropState::ShouldIgnoreRaycastResult(RaycastResult *result)
{
    if (result->entity)
    {
        if (templateEntity.get() && templateEntity.get() == result->entity)
            return true;
    }
    return false;
}

void StorageItemDragAndDropState::UpdateFromRaycastResult(ScenePtr scene_, RaycastResult *result)
{
    scene = SceneWeakPtr(scene_);

    SetTarget(result->entity ? result->entity->shared_from_this() : EntityPtr(), result->component);
    SetSubmeshIndex(result->submesh);
    SetPositionAndNormal(result->pos, result->normal);
}

void StorageItemDragAndDropState::SetTarget(EntityPtr ent, IComponent *comp)
{
    // Do nothing if this entity is already set.
    if (!entity.expired() && ent.get() && entity.lock().get() == ent.get())
        return;

    ResetSceneModifications();

    if (ent.get())
    {
        entity = EntityWeakPtr(ent);
        meshComponent = weak_ptr<EC_Mesh>(ent->Component<EC_Mesh>());
    }
}

void StorageItemDragAndDropState::SetSubmeshIndex(int submeshIndex_)
{
    ItemType type = Type();
    if (type == IT_Material)
    {
        if (meshComponent.expired())
            return;
        if (submeshIndex_ < 0 || submeshIndex_ >= static_cast<int>(meshComponent.lock()->NumSubMeshes()))
        {
            ClearScenePreview();
            return;
        }
        
        // Don't do anything if this submesh is already applied
        if (submeshIndex == submeshIndex_)
            return;
        // Then if index is set, this is a sub index change in the current mesh
        else if (submeshIndex != -1)
            ClearScenePreview();

        // Set preview material in local only mode
        AssetReferenceList materials = meshComponent.lock()->materialRefs.Get();
        
        // Store the original for later restoring
        if (submeshIndex_ < materials.Size())
            originalMaterialRef = materials[submeshIndex_];
        else
            originalMaterialRef.ref = "";

        while (materials.Size() <= submeshIndex_)
            materials.Append(AssetReference());
        materials.Set(submeshIndex_, AssetReference(relativeRef, "OgreMaterial"));
        meshComponent.lock()->materialRefs.Set(materials, AttributeChange::LocalOnly);
        
        submeshIndex = submeshIndex_;
    }
}

void StorageItemDragAndDropState::SetPositionAndNormal(float3 position, float3 normal)
{
    ItemType type = Type();
    if (type == IT_Mesh)
    {
        /// @todo If we did not hit a entity with ray result, the mesh should be created in front of the camera!
        if (scene.expired() || entity.expired())
        {
            DestroyTemplateEntity();
            return;
        }

        if (!templateEntity.get())
            CreateTemplateEntity(filebasename, QStringList() << "EC_Name" << "EC_Placeable" << "EC_Mesh");
        if (!templateEntity.get())
            return;            

        shared_ptr<EC_Mesh> mesh = templateEntity->Component<EC_Mesh>();
        if (mesh)
        {
            connect(mesh.get(), SIGNAL(MeshChanged()), this, SLOT(OnMeshDropMeshChanged()), Qt::UniqueConnection);

            if (mesh->meshRef.Get().ref != relativeRef)
                mesh->meshRef.Set(AssetReference(relativeRef, "OgreMesh"), AttributeChange::LocalOnly);
        }
        shared_ptr<EC_Placeable> placeable = templateEntity->Component<EC_Placeable>();
        if (placeable)
        {
            if (!placeable->visible.Get())
                placeable->visible.Set(true, AttributeChange::LocalOnly);

            Transform t = placeable->transform.Get();
            t.pos = position;
            placeable->transform.Set(t, AttributeChange::LocalOnly);
        }
    }
}

void StorageItemDragAndDropState::CreateTemplateEntity(const QString &name, const QStringList &components)
{
    if (scene.expired())
        return;

    if (!templateEntity.get())
        templateEntity = scene.lock()->CreateLocalEntity(components, AttributeChange::LocalOnly, true, true);

    if (templateEntity.get() && templateEntity->Name() != name)
        templateEntity->SetName(name);
}

void StorageItemDragAndDropState::DestroyTemplateEntity()
{
    if (!scene.expired() && templateEntity.get())
    {
        entity_id_t entId = templateEntity->Id();
        templateEntity.reset();
        scene.lock()->RemoveEntity(entId);
    }
}

void StorageItemDragAndDropState::OnMeshDropMeshChanged()
{
    EC_Mesh *mesh = (templateEntity.get() ? templateEntity->Component<EC_Mesh>().get() : 0);
    if (mesh)
    {
        const AssetReference cBlockPlacerMaterialRef("Ogre Media:BlockPlacer.material", "OgreMaterial");

        AssetReferenceList materials;
        for (int mi=0, len=mesh->NumSubMeshes(); mi<len; mi++)
            materials.Append(cBlockPlacerMaterialRef);
        mesh->materialRefs.Set(materials, AttributeChange::LocalOnly);
    }
}

void StorageItemDragAndDropState::Apply()
{
    ItemType type = Type();
    if (type == IT_Material)
    {
        // Material has already been applied by SetSubmeshIndex. Just make it a replicated change now.
        if (submeshIndex != -1 && !meshComponent.expired())
        {
            AssetReferenceList materials = meshComponent.lock()->materialRefs.Get();
            meshComponent.lock()->materialRefs.Set(materials, AttributeChange::Replicate);
        }

        submeshIndex = -1;
        originalMaterialRef.ref = "";
    }
    else if (type == IT_Mesh)
    {
        if (templateEntity.get())
        {
            // Clear temp materials for a clean clone
            EC_Mesh *mesh = templateEntity->Component<EC_Mesh>().get();
            if (mesh)
                mesh->materialRefs.Set(AssetReferenceList("OgreMaterial"), AttributeChange::LocalOnly);

            // Clone as replicated, this will add the clone to the parent scene.
            EntityPtr cloned = templateEntity->Clone(false, false, templateEntity->Name(), AttributeChange::Replicate);
            if (!cloned)
                LogError("Meshmoon Storage Drag and Drop: Failed to create mesh entity from template!");
            DestroyTemplateEntity();
        }
    }
    else if (type == IT_Script && !entity.expired())
    {
        EC_Script::RunMode mode = EC_Script::RunOnBoth;

        // Native script: Ask execution mode        
        if (suffix == "js")
        {
            QMessageBox *dialog = plugin_->Notifications()->ShowSplashDialog("Select script execution mode for <b>" + filename + "</b>", 
                "", QMessageBox::NoButton, QMessageBox::NoButton, 0, false);
            if (dialog)
            {
                dialog->setAttribute(Qt::WA_DeleteOnClose, false);
                dialog->setWindowTitle("Script Execution Mode");

                foreach(QAbstractButton *b, dialog->buttons())
                    dialog->removeButton(b);

                QPushButton *b = dialog->addButton("Server and Clients", QMessageBox::YesRole);
                b->setAutoDefault(true);
                b->setDefault(true);
                b->setMinimumWidth(140);

                b = dialog->addButton("Only Clients", QMessageBox::ActionRole);
                b->setMinimumWidth(120);
                b = dialog->addButton("Only Server", QMessageBox::AcceptRole);
                b->setMinimumWidth(120);

                b = dialog->addButton("Cancel", QMessageBox::RejectRole);
                b->setMinimumWidth(120);

                dialog->show();
                dialog->setFocus(Qt::ActiveWindowFocusReason);
                dialog->activateWindow();

                plugin_->Notifications()->CenterToMainWindow(dialog);
                
                // Hack to hide the drag&drop tooltip before blocking dialog exec
                /// @todo Does not seem to work :(
                QApplication::processEvents();
                
                dialog->exec();
                dialog->deleteLater();

                QString text = dialog->clickedButton() ? dialog->clickedButton()->text() : "";
                if (text.isEmpty() || text == "Cancel")
                    return;

                if (text == "Only Clients")
                    mode = EC_Script::RunOnClient;
                else if (text == "Only Server")
                    mode = EC_Script::RunOnServer;
            }
        }
        // WebRocket script: Client side only execution.
        else
            mode = EC_Script::RunOnClient;

        EC_Script *script = dynamic_cast<EC_Script*>(entity.lock()->CreateComponent(
            EC_Script::TypeIdStatic(), UniqueComponentName(EC_Script::TypeIdStatic(), filebasename), AttributeChange::Replicate).get());
        if (script)
        {
            script->runOnLoad.Set(true, AttributeChange::Replicate);

            // Run mode
            script->runMode.Set(static_cast<int>(mode), AttributeChange::Replicate);
            
            // Ref
            AssetReferenceList refs("Script");
            refs.Append(AssetReference(relativeRef, "Script"));
            script->scriptRef.Set(refs, AttributeChange::Replicate);
        }
    }
    
    // Reset state, this should NOT revert the change, above logic should have replicated them and clear
    // any state that might revert the change (eg. reset submesh index). Do not reset ent/mesh ptrs, 
    // they will be done in Reset.
    Reset();
}

QString StorageItemDragAndDropState::UniqueComponentName(u32 typeId, const QString &base)
{
    QString name = base;
    if (entity.lock()->Component(typeId, name).get())
    {
        for (int i=1; i<10000; ++i)
        {
            name = QString("%1 %2").arg(base).arg(i);
            if (!entity.lock()->Component(typeId, name).get())
                break;
        }
    }
    return name;
}

/// @endcond
