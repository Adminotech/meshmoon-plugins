/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "IRocketAssetEditor.h"

#include "MeshmoonAsset.h"
#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "Framework.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "ConfigAPI.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"

#include "qts3/QS3Client.h"

IRocketAssetEditor::IRocketAssetEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource, const QString &windowTitle, const QString &resourceDescription) :
    QWidget(plugin->Fw()->Ui()->MainWindow(), Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    plugin_(plugin),
    filename(resource->filename),
    suffix(resource->suffix),
    completeSuffix(resource->completeSuffix),
    storageKey_(resource->data.key),
    windowTitle_(windowTitle)
{
    _InitUi(resourceDescription);
}

IRocketAssetEditor::IRocketAssetEditor(RocketPlugin *plugin, MeshmoonAsset *resource, const QString &windowTitle, const QString &resourceDescription) :
    QWidget(plugin->Fw()->Ui()->MainWindow(), Qt::Dialog | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
    plugin_(plugin),
    filename(resource->filename),
    suffix(QFileInfo(resource->filename).suffix()),
    completeSuffix(QFileInfo(resource->filename).completeSuffix()),
    assetRef_(resource->assetRef),
    windowTitle_(windowTitle)
{
    _InitUi(resourceDescription);
    
    // This is not a storage resource, disable saving.
    SetSaveEnabled(false);
}

void IRocketAssetEditor::_InitUi(const QString &resourceDescription)
{
    menuBar_ = 0;
    menuFile_ = 0;
    menuTools_ = 0;
    menuSettings_ = 0;

    closeEmitted_ = false;
    closeWithoutSaving_ = false;
    closeAfterSaving_ = false;
    
    saving_ = false;
    touched_ = false;

    geometryAutoSaveOnClose_ = false;
    geometrySetFromConfig_ = false;
    geometryMaximisedFromConfig_ = false;

    // Init ui
    ui.setupUi(this);

    // Window title
    if (windowTitle_.isEmpty())
        windowTitle_ = windowTitle();
    setWindowTitle(filename + " - " + windowTitle_);

    // Top menu
    saveAction_ = FileMenu()->addAction(QIcon(":/images/icon-check.png"), "Save Changes");
    saveAction_->setShortcut(QKeySequence(Qt::ControlModifier + Qt::Key_S));
    saveAction_->setShortcutContext(Qt::WindowShortcut);
    saveAction_->setEnabled(false);
    saveAction_->setProperty("saveEnabled", true);

    closeAction_ = FileMenu()->addAction(QIcon(":/images/icon-cross.png"), "Close");

    connect(saveAction_, SIGNAL(triggered()), SLOT(OnSavePressed()), Qt::QueuedConnection);
    connect(closeAction_, SIGNAL(triggered()), SLOT(close()), Qt::QueuedConnection);

    // File description
    SetResourceDescription(resourceDescription);

    // Init message if s3 client is not passed to use.
    SetMessage("No resource received for editing");

    hide();
}

IRocketAssetEditor::~IRocketAssetEditor()
{
    if (unsavedChangesDialog_)
        unsavedChangesDialog_->close();
}

void IRocketAssetEditor::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (plugin_ && !geometrySetFromConfig_)
        plugin_->Notifications()->CenterToMainWindow(this);
        
    // This boolean is cleared on the first show.
    if (geometryMaximisedFromConfig_)
    {
        geometryMaximisedFromConfig_ = false;
        showMaximized();
    }
}

void IRocketAssetEditor::closeEvent(QCloseEvent *e)
{
    if (TryClose())
        e->ignore();
    else if (geometryAutoSaveOnClose_ && !geometryConfigEditorName_.isEmpty())
        StoreGeometryToConfig(geometryConfigEditorName_);
}

void IRocketAssetEditor::Start(QS3Client *s3)
{
    // Request the resource
    // 1) From storage with key
    // 2) From AssetAPI with asset ref
    QString storageKey = StorageKey();
    if (!storageKey.isEmpty())
    {
        QS3GetObjectResponse *response = (s3 ? s3->get(storageKey) : 0);
        if (response)
        {
            SetMessage("Downloading resource...");
            connect(response, SIGNAL(finished(QS3GetObjectResponse*)), SLOT(OnResourceFetched(QS3GetObjectResponse*)));
            connect(response, SIGNAL(downloadProgress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnResourceFetchProgress(QS3GetObjectResponse*, qint64, qint64)));
        }
        else
            SetError("Failed to request resource data from storage with " + storageKey);        
    }
    else if (!AssetRef().isEmpty())
    {
        // Get from loaded asset
        AssetPtr asset = plugin_->GetFramework()->Asset()->GetAsset(AssetRef());
        if (asset)
            OnResourceFetched(asset);
        else
        {
            // Request from source
            AssetTransferPtr response = plugin_->GetFramework()->Asset()->RequestAsset(AssetRef());
            if (response)
            {
                SetMessage("Downloading resource...");
                connect(response.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnResourceFetched(AssetPtr)));
            }
        }
    }
    else
        SetError("Failed to request resource data");
}

void IRocketAssetEditor::Show()
{
    show();
}

void IRocketAssetEditor::Hide()
{
    hide();
}

void IRocketAssetEditor::SetMessage(const QString &msg)
{
    ui.labelMessage->setText("<span style='color: white;'>" + msg + "</span>");
}

void IRocketAssetEditor::SetError(const QString &msg)
{
    ui.labelMessage->setText("<span style='color: rgb(255,61,61);'>" + msg + "</span>");
}

void IRocketAssetEditor::ClearMessage()
{
    ui.labelMessage->clear();
}

void IRocketAssetEditor::UpdateTimestamp()
{
    ui.labelTimestamp->setText("Saved at " + QDateTime::currentDateTime().toString("hh:mm:ss"));
}

bool IRocketAssetEditor::HasUnsavedChanges() const
{
    if (IsTouched() && CanSave())
        return true;
    return false;
}

bool IRocketAssetEditor::IsTouched() const
{
    return touched_;
}

QString IRocketAssetEditor::StorageKey() const
{
    return storageKey_;
}

QString IRocketAssetEditor::AssetRef() const
{
    if (plugin_ && !StorageKey().isEmpty())
        return plugin_->Storage()->UrlForItem(StorageKey());
    return assetRef_;
}

void IRocketAssetEditor::OnResourceFetched(QS3GetObjectResponse *response)
{
    if (!plugin_)
    {
        ShowError("Operational Error", "Rocket plugin not set before editor usage, cannot continue!");
        return;
    }
    if (!response || !response->succeeded)
    {
        ShowError("Loading Failed", "Failed to request resource for editing");
        return;
    }

    ClearMessage();

    // Only enable saving if SetSaveEnabled has not been called to disable it.
    if (saveAction_->property("saveEnabled").toBool())
        saveAction_->setEnabled(true);

    // Call the implementation to handle data.
    ResourceDownloaded(response->data);
}

void IRocketAssetEditor::OnResourceFetched(AssetPtr asset)
{
    if (!plugin_)
    {
        ShowError("Operational Error", "Rocket plugin not set before editor usage, cannot continue!");
        return;
    }
    if (!asset.get())
    {
        ShowError("Loading Failed", "Failed to request resource for editing");
        return;
    }
    
    // Try first from disk source, fallback to RawData() which uses the in-mem serialization.
    QByteArray data;
    if (!asset->DiskSource().isEmpty())
    {
        QFile diskSource(asset->DiskSource());
        if (diskSource.open(QFile::ReadOnly))
        {
            data = diskSource.readAll();
            diskSource.close();
        }
    }
    if (data.isEmpty())
        data = asset->RawData();
    if (data.isEmpty())
    {
        ShowError("Loading Failed", "Failed to open resource for editing");
        return;
    }

    ClearMessage();

    // Only enable saving if SetSaveEnabled has not been called to disable it.
    if (CanSave())
        SetSaveEnabled(true);

    // Call the implementation to handle data.
    ResourceDownloaded(data);
}

void IRocketAssetEditor::OnResourceFetchProgress(QS3GetObjectResponse* /*response*/, qint64 received, qint64 total)
{
    if (received < 0 || total <= 0)
        return;

    int progress = int(((qreal)received / (qreal)total) * 100.0);
    if (progress < 0)
        progress = 0;
    else if (progress > 100)
        progress = 100;

    SetMessage(QString("Downloading resource... %1%").arg(progress));
}

void IRocketAssetEditor::OnResourceSaveFinished(MeshmoonStorageOperationMonitor* /*monitor*/)
{
    saving_ = false;
    
    /** @todo This is not good for all cases eg. long upload and stuff is happening in the editor.
        Make a virtual function that gets called here so the editor impl can set the message again! */
    SetMessage(cachedMessage_);
    UpdateTimestamp();
    
    if (closeAfterSaving_)
        close();
}

void IRocketAssetEditor::OnResourceSaveProgress(QS3PutObjectResponse* /*response*/, qint64 completed, qint64 total, MeshmoonStorageOperationMonitor* /*operation*/)
{
    if (completed < 0 || total <= 0)
        return;

    int progress = int(((qreal)completed / (qreal)total) * 100.0);
    if (progress < 0)
        progress = 0;
    else if (progress > 100)
        progress = 100;
    SetMessage(QString("Uploading resource... %1%").arg(progress));
}

void IRocketAssetEditor::OnSavePressed()
{
    if (saving_)
    {
        plugin_->Notifications()->ShowSplashDialog("Already saving, please wait for the current operation to complete.",
            ":/images/icon-update.png", QMessageBox::Ok, QMessageBox::Ok, this);
        return;
    }

    SerializationResult result = ResourceData();
    if (result.first)
    {
        saving_ = true;
        touched_ = false;
        cachedMessage_ = ui.labelMessage->text();
        setWindowTitle(filename + " - " + windowTitle_);
        emit SaveRequest(this, result.second);
    }
}

void IRocketAssetEditor::OnUnsavedDialogButtonPressed(QAbstractButton *button)
{
    QMessageBox *dialog = dynamic_cast<QMessageBox*>(sender());
    if (!dialog)
        return;

    QMessageBox::ButtonRole role = dialog->buttonRole(button);
    if (role == QMessageBox::AcceptRole)
    {
        closeAfterSaving_ = true;
        OnSavePressed();
    }
    else if (role == QMessageBox::ActionRole)
    {
        closeWithoutSaving_ = true;
        close();
    }
}

bool IRocketAssetEditor::TryClose()
{
    AboutToClose();

    if (!closeEmitted_)
    {
        // This editor has changes. Ask the user if he really wants to lose changes.
        if (HasUnsavedChanges())
        {
            if (plugin_ && !closeWithoutSaving_)    
            {
                // Activate existing dialog
                if (unsavedChangesDialog_)
                {
                    unsavedChangesDialog_->setFocus(Qt::ActiveWindowFocusReason);
                    unsavedChangesDialog_->activateWindow();
                    return true;
                }

                unsavedChangesDialog_ = plugin_->Notifications()->ShowSplashDialog("There are unsaved changes for <b>" + filename + "</b>",
                    ":/images/icon-update.png", QMessageBox::NoButton, QMessageBox::NoButton, this, false);
                if (unsavedChangesDialog_)
                {
                    unsavedChangesDialog_->setWindowTitle(filename + " - " + windowTitle_);

                    foreach(QAbstractButton *b, unsavedChangesDialog_->buttons())
                        unsavedChangesDialog_->removeButton(b);

                    QPushButton *b = unsavedChangesDialog_->addButton("Cancel", QMessageBox::RejectRole);
                    b->setAutoDefault(true);
                    b->setDefault(true);
                    b->setIcon(QIcon(":/images/icon-cross.png"));
                    b->setIconSize(QSize(16,16));

                    b = unsavedChangesDialog_->addButton("Close without saving", QMessageBox::ActionRole);
                    b->setIcon(QIcon(":/images/icon-cross.png"));
                    b->setIconSize(QSize(16,16));
                    b->setMinimumWidth(175);

                    b = unsavedChangesDialog_->addButton("Save and close", QMessageBox::AcceptRole);
                    b->setIcon(QIcon(":/images/icon-check.png"));
                    b->setIconSize(QSize(16,16));
                    b->setMinimumWidth(140);

                    connect(unsavedChangesDialog_, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(OnUnsavedDialogButtonPressed(QAbstractButton*)));

                    unsavedChangesDialog_->show();
                    unsavedChangesDialog_->setFocus(Qt::ActiveWindowFocusReason);
                    unsavedChangesDialog_->activateWindow();

                    RocketNotifications::CenterToWindow(this, unsavedChangesDialog_);
                    return true;
                }
            }

            // If this asset is loaded we want to do a reload from the source.
            // This will restore the last state from source to any live edited assets.
            // 1) Absolute storage URL
            // 2) Absolute storage S3 variation URL
            // 3) Absolute URL resolved from relative ref and the default storage
            QStringList refs;

            QString storageKey = StorageKey();
            if (!storageKey.isEmpty())
            {
                refs << plugin_->Storage()->UrlForItem(storageKey)
                     << MeshmoonAsset::S3RegionVariationURL(plugin_->Storage()->UrlForItem(storageKey))
                     << plugin_->GetFramework()->Asset()->ResolveAssetRef("", plugin_->Storage()->RelativeRefForKey(storageKey));
            }
            else
                refs << AssetRef();

            foreach(const QString &ref, refs)
            {
                AssetPtr existing = plugin_->GetFramework()->Asset()->GetAsset(ref);
                if (existing.get())
                {
                    plugin_->GetFramework()->Asset()->RequestAsset(existing->Name(), existing->Type(), true);
                    existing.reset();
                    break;
                }
            }
        }

        closeEmitted_ = true;
        emit CloseRequest(this);

        // This is a hack to close the editor down on the first press, if it was 
        // not originated from MeshmoonAsset::OpenVisualEditor or MeshmoonStorageItemWidget::OpenVisualEditor.
        return (!CanSave() && StorageKey().isEmpty() ? false : true);
    }
    return false;
}

void IRocketAssetEditor::Touched()
{
    if (!touched_)
    {
        touched_ = true;
        if (CanSave())
            setWindowTitle(filename + "* - " + windowTitle_);
    }
}

void IRocketAssetEditor::ShowError(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void IRocketAssetEditor::SetResourceDescription(const QString &description)
{
    if (ui.labelFileType)
        ui.labelFileType->setText(description.trimmed());
}

QMenuBar *IRocketAssetEditor::MenuBar()
{
    if (!menuBar_)
    {
        menuBar_ = new QMenuBar(this);
        menuBar_->setFixedHeight(21);
        ui.verticalLayout->insertWidget(0, menuBar_);
    }
    return menuBar_;
}

QMenu *IRocketAssetEditor::FileMenu()
{
    if (!menuFile_)
        menuFile_ = MenuBar()->addMenu("File");
    return menuFile_;
}

QMenu *IRocketAssetEditor::ToolsMenu()
{
    if (!menuTools_)
        menuTools_ = MenuBar()->addMenu("Tools");
    return menuTools_;
}

QMenu *IRocketAssetEditor::SettingsMenu()
{
    if (!menuSettings_)
        menuSettings_ = MenuBar()->addMenu("Settings");
    return menuSettings_;
}

bool IRocketAssetEditor::RestoreGeometryFromConfig(const QString &editorName, bool autoSaveGeometryOnClose)
{
    if (editorName.isEmpty())
        return false;

    geometryConfigEditorName_ = editorName;
    geometryAutoSaveOnClose_ = autoSaveGeometryOnClose;

    ConfigData data("rocket-asset-editors", "geometry-cache", editorName, QVariant(), QRect());
    if (plugin_->Fw()->Config()->HasKey(data))
    {
        QRect rect = plugin_->Fw()->Config()->Read(data).toRect();
        if (!rect.isNull() && rect.isValid())
        {
            RocketNotifications::EnsureGeometryWithinDesktop(rect);
            setGeometry(rect);
            geometrySetFromConfig_ = true;
            
            data = ConfigData("rocket-asset-editors", "geometry-cache", editorName + "-maximized", QVariant(), false);
            if (plugin_->Fw()->Config()->HasKey(data))
                geometryMaximisedFromConfig_ = plugin_->Fw()->Config()->Read(data).toBool();
            return true;
        }
    }
    return false;
}

void IRocketAssetEditor::StoreGeometryToConfig(const QString &editorName)
{
    if (editorName.isEmpty())
        return;

    // If maximized restore the normal geometry so it goes to config.        
    bool maximized = isMaximized();
    if (maximized)
        showNormal();

    plugin_->Fw()->Config()->Write("rocket-asset-editors", "geometry-cache", editorName, geometry());
    plugin_->Fw()->Config()->Write("rocket-asset-editors", "geometry-cache", editorName + "-maximized", maximized);
}

void IRocketAssetEditor::SetSaveEnabled(bool enabled)
{
    if (saveAction_)
    {
        // Don't allow enabling saving if storage info is missing.
        if (enabled && StorageKey().trimmed().isEmpty())
        {
            enabled = false;
            LogWarning("IRocketAssetEditor: Cannot enable saving, storage information is missing.");
        }

        saveAction_->setEnabled(enabled);
        saveAction_->setProperty("saveEnabled", enabled);

        setWindowTitle(filename + (enabled && touched_ ? "*" : "") + " - " + windowTitle_);
    }
}

bool IRocketAssetEditor::CanSave() const
{
    return (saveAction_ ? saveAction_->property("saveEnabled").toBool() : false);
}
