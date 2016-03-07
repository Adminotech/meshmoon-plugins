/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MeshmoonStorage.h"
#include "MeshmoonStorageHelpers.h"
#include "MeshmoonStorageWidget.h"
#include "MeshmoonStorageDialogs.h"
#include "RocketStorageSceneImporter.h"
#include "MeshmoonUser.h"

#include "qts3/QS3Client.h"

#include "RocketPlugin.h"
#include "RocketNetworking.h"
#include "RocketTaskbar.h"
#include "RocketNotifications.h"
#include "editors/IRocketAssetEditor.h"
#include "utils/RocketFileSystem.h"
#include "utils/RocketAnimations.h"
#include "utils/RocketZipWorker.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"
#include "IRenderer.h"
#include "ConfigAPI.h"
#include "ConsoleAPI.h"

#include "UiAPI.h"
#include "UiMainWindow.h"

#include "AssetAPI.h"
#include "IAsset.h"

#include "InputAPI.h"
#include "InputContext.h"

#include "SceneStructureModule.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "SceneDesc.h"
#include "Entity.h"

#include "EC_Mesh.h"
#include "EC_Placeable.h"
#include "EC_Name.h"
#include "EC_Script.h"
#include "EC_Material.h"

#include "OgreSceneImporter.h"
#include "OgreMaterialUtils.h"
#include "OgreMaterialAsset.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QClipboard>
#include <QDomDocument>
#include <QTextStream>
#include <QRegExp>

#include "MemoryLeakCheck.h"

const QString MeshmoonStorage::Schema = "meshmoonstorage://";

MeshmoonStorage::MeshmoonStorage(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    LC("[MeshmoonStorage]: "),
    s3_(0),
    authenticator_(0),
    storageWidget_(0),
    toggleAction_(0)
{
    framework_->Console()->RegisterCommand("setMeshmoonStorage",
        "Use custom Meshmoon storage. Parameter must be a URL to the scene txml.",
        this, SLOT(SetDebugStorageInformation(const QStringList&)));
        
    // Use high priority in order to suppress possible interfering application logic scripts (f.ex. avatar scripts).
    inputCtx_ = framework_->Input()->RegisterInputContext("RocketStorage", 2000);
    connect(inputCtx_.get(), SIGNAL(KeyEventReceived(KeyEvent*)), SLOT(OnKeyEvent(KeyEvent*)));
    
    // Use hight priority over Qt to always do auto hide logic from mouse move. This does not suppress the events.
    inputCtxMove_ = framework_->Input()->RegisterInputContext("RocketBuildEditorHighPriorityOverQt", 1);
    inputCtxMove_->SetTakeMouseEventsOverQt(true);
    connect(inputCtxMove_.get(), SIGNAL(MouseMove(MouseEvent *)), SLOT(OnMouseMove(MouseEvent*)));
}

MeshmoonStorage::State::State() :
    storageRoot(0),
    cdUp(0),
    currentFolder(0),
    infoDialog(0),
    ui(None),
    folderOperationOngoing(false),
    closeOnEditorsClosed(false),
    headless(true)
{
    protectedExtensions << ".txml" << ".txml.server.log";
}

MeshmoonStorage::~MeshmoonStorage()
{
    SAFE_DELETE(authenticator_);
    SAFE_DELETE(toggleAction_);
    ResetUi();
    Reset();
}

void MeshmoonStorage::OnKeyEvent(KeyEvent *e)
{
    if (toggleAction_ && plugin_->IsConnectedToServer())
    {
        const QKeySequence toggleStorage = framework_->Input()->KeyBinding("Rocket.ToggleStorage", QKeySequence(Qt::ControlModifier + Qt::Key_S));
        if (e->sequence == toggleStorage)
        {
            toggleAction_->toggle();
            e->Suppress();
            return;
        }
    }
}

void MeshmoonStorage::OnMouseMove(MouseEvent *e)
{
    if (!e->IsLeftButtonDown() && !e->IsRightButtonDown() && !e->IsMiddleButtonDown())
        UpdateStorageWidgetAnimationsState(e->x);
}

void MeshmoonStorage::OnStorageWidgetAnimationsCompleted()
{
    UpdateStorageWidgetAnimationsState(framework_->Input()->MousePos().x());
}

void MeshmoonStorage::UpdateStorageWidgetAnimationsState(int mouseX, bool forceShow)
{
    if (!storageWidget_ || storageWidget_->IsPinned() || storageWidget_->IsHiddenOrHiding() || storageWidget_->IsClosing())
        return;

    RocketAnimationEngine *engine = storageWidget_->AnimationEngine();
    if (!engine->IsInitialized() || (engine->IsRunning() && !forceShow))
        return;

    /// @todo Should not hide if there is ongoing upload or something like that?
    /*if (!forceShow && contextWidget->IsVisible())
        forceShow = true;*/

    // We assume the scene is always full screen, as it currently is in Tundra,
    // and that the build widget is always on the right side of the screen.
    // If these things change the below code needs to be adjusted.
    QPointF scenePos = storageWidget_->scenePos();

    const int padding = 70;
    const int animationState = engine->Value<int>("state");

    int triggerWidth = scenePos.x() - padding;
    if (!forceShow && mouseX < triggerWidth)
    {
        if (animationState != 1)
        {
            QRectF sceneRect = storageWidget_->scene()->sceneRect();
            QRectF rect = storageWidget_->geometry();

            engine->StoreValue("state", 1);
            engine->StoreValue("pos", scenePos);
            engine->StoreValue("opacity", storageWidget_->opacity());
            scenePos.setX(sceneRect.width() - 150);
            storageWidget_->Animate(rect.size(), scenePos, 0.5);
        }
    }
    else
    {
        if (animationState != 0)
        {
            engine->StoreValue("state", 0);
            QRectF rect = storageWidget_->geometry();
            storageWidget_->Animate(rect.size(), engine->Value<QPointF>("pos"), engine->Value<qreal>("opacity"));
        }
    }
}

void MeshmoonStorage::SetDebugStorageInformation(const QStringList &params)
{
    QString storageSpecUrl = !params.isEmpty() ? params.first().trimmed() : "";
    if (!storageSpecUrl.isEmpty())
    {
        LogInfo("Meshmoon storage debug spec url: " + storageSpecUrl);
        SetStorageInformation(storageSpecUrl, storageSpecUrl);      
    }
}

void MeshmoonStorage::SetStorageInformation(const QString &storageUrl, const QString &txmlUrl)
{
    /// @todo Should we parse the host also from this. Does EU vs. US region url matter when making REST API calls?!    
    state_.storageHostUrl = QUrl(txmlUrl).toString(QUrl::RemovePath);
    
    // Bucket
    QString bucket = storageUrl;
    if (bucket.startsWith("https://"))
        bucket = bucket.mid(8);
    else if (bucket.startsWith("http://"))
        bucket = bucket.mid(7);
    // .s3.amazonaws.com
    if (bucket.contains(".s3.amazonaws.com", Qt::CaseInsensitive))
    {
        bucket = bucket.left(bucket.indexOf(".s3.amazonaws.com", 0, Qt::CaseInsensitive));
    }
    // .s3-eu-west etc.
    else if (bucket.contains(".s3-", Qt::CaseInsensitive))
    {
        bucket = bucket.left(bucket.indexOf(".s3-", 0, Qt::CaseInsensitive));
    }
    else
    {
        LogError(LC + "Cannot determine storage information from: " + storageUrl);
        return;    
    }
    state_.storageBucket = bucket;
    
    QString path = QUrl(txmlUrl).path();

    // Txml acl test path.
    state_.storageTxmlPath = path;
    if (state_.storageTxmlPath.startsWith("/"))
        state_.storageTxmlPath = state_.storageTxmlPath.mid(1);

    // Storage base path where list objects is executed.
    state_.storageBasePath = path.left(path.lastIndexOf("/"));
    if (state_.storageBasePath.startsWith("/"))
        state_.storageBasePath = state_.storageBasePath.mid(1);
    if (!state_.storageBasePath.endsWith("/"))
        state_.storageBasePath += "/";

    LogDebug("Storage information received");
    LogDebug("* Host   : " + state_.storageHostUrl);
    LogDebug("* Bucket : " + state_.storageBucket);
    LogDebug("* Base   : " + state_.storageBasePath);
    LogDebug("* Txml   : " + state_.storageTxmlPath);
}

void MeshmoonStorage::ClearStorageInformation()
{
    state_.storageHostUrl = "";
    state_.storageBucket = "";
    state_.storageBasePath = "";
    state_.storageTxmlPath = "";
}

void MeshmoonStorage::Unauthenticate()
{
    // User logged out. Reset storage config.
    if (state_.folderOperationOngoing)
        return;

    framework_->Config()->Write("adminotech", "ast", "seventeen", "");
    framework_->Config()->Write("adminotech", "ast", "five", "");
}

void MeshmoonStorage::UpdateTaskbarEntry(RocketTaskbar *taskbar)
{
    if (framework_->IsHeadless())
        return;

    if (!taskbar)
    {
        SAFE_DELETE(toggleAction_);
        return;
    }
    if (!toggleAction_)
    {
        const QKeySequence toggleStorage = framework_->Input()->KeyBinding("Rocket.ToggleStorage", QKeySequence(Qt::ControlModifier + Qt::Key_S));

        toggleAction_ = new QAction(QIcon(":/images/icon-update.png"), tr("Meshmoon Storage"), 0);
        toggleAction_->setToolTip(QString("%1 (%2)").arg(tr("Meshmoon Storage")).arg(toggleStorage.toString()));
        toggleAction_->setShortcut(toggleStorage);
        toggleAction_->setShortcutContext(Qt::WidgetShortcut);
        toggleAction_->setVisible(false);
        toggleAction_->setEnabled(false);
        toggleAction_->setCheckable(true);
        
        taskbar->AddAction(toggleAction_);
        connect(toggleAction_, SIGNAL(toggled(bool)), this, SLOT(ToggleVisibility()));
        
        RocketUserAuthenticator *authenticator = plugin_->User()->RequestAuthentication();
        if (authenticator)
            connect(authenticator, SIGNAL(Completed(int, const QString&)), this, SLOT(UpdateTaskbarEntryContinue(int)), Qt::UniqueConnection);
        else
            LogError(LC + "Cannot add scene storage shortcut to taskbar, failed to request authenticatorInternal.");
    }
}

MeshmoonStorage::State MeshmoonStorage::CurrentState() const
{
    return state_;
}

void MeshmoonStorage::UpdateTaskbarEntryContinue(int level)
{
    if (framework_->IsHeadless() || !toggleAction_)
        return;

    Meshmoon::PermissionLevel levelEnum = (Meshmoon::PermissionLevel)level;
    bool showToggleAction = (levelEnum < Meshmoon::Elevated ? false : true);
    toggleAction_->setVisible(showToggleAction);
    toggleAction_->setEnabled(showToggleAction);
}

void MeshmoonStorage::Reset()
{
    state_.folderOperationOngoing = false;
    SAFE_DELETE(s3_);
}

void MeshmoonStorage::ResetUi()
{
    if (framework_->IsHeadless())
        return;
    if (!framework_->Ui() || !framework_->Ui()->GraphicsScene())
        return;
    
    for (int i=0; i<state_.editors.size(); ++i)
    {
        IRocketAssetEditor *openEditor = state_.editors[i];
        if (!openEditor)
            continue;
        
        // Disconnect before close or it will trigger OnEditorCloseRequest where we also close and delete the widget!
        disconnect(openEditor, SIGNAL(CloseRequest(IRocketAssetEditor*)), this, SLOT(OnEditorCloseRequest(IRocketAssetEditor*))); 
        if (!openEditor->testAttribute(Qt::WA_DeleteOnClose))
            openEditor->deleteLater();
        openEditor->close();
        state_.editors[i] = 0;
    }
    state_.editors.clear();
    state_.closeOnEditorsClosed = false;

    disconnect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnWindowResized(const QRectF&)));
        
    // Remove items from list without deleting then
    // delete root that will handle deleting the whole child chain.
    if (storageWidget_ && storageWidget_->ListView())
    {
        RocketStorageListWidget *list = storageWidget_->ListView();
        while (list->count() > 0)
            list->takeItem(0);
    }
    SAFE_DELETE(state_.storageRoot);
    SAFE_DELETE(state_.cdUp);
    if (state_.infoDialog)
        state_.infoDialog->Close();
    SAFE_DELETE(state_.infoDialog);
    
    // Remove storage widget from scene.
    if (storageWidget_ && storageWidget_->scene())
        framework_->Ui()->GraphicsScene()->removeItem(storageWidget_);
    SAFE_DELETE(storageWidget_);
    
    state_.ui = None;
}

void MeshmoonStorage::ToggleVisibility()
{
    if (framework_->IsHeadless())
        return;
        
    if (!storageWidget_ || (storageWidget_ && s3_ && !storageWidget_->isVisible()))
        Open();
    else
        Close();
}

void MeshmoonStorage::Open()
{
    if (framework_->IsHeadless())
        return;

    // Already open. Do nothing.
    if (storageWidget_)
    {
        if (!storageWidget_->isVisible())
        {
            state_.headless = false;
            UpdateInterface();
        }
        return;
    }

    Reset();
    ResetUi();

    if (!plugin_->IsConnectedToServer())
    {
        plugin_->Notifications()->ShowSplashDialog("Cannot open scene storage, not connected to a server.", ":/images/icon-update.png");
        LogError(LC + "Cannot open scene storage, not connected to a server.");
        return;
    }

    if (state_.storageBasePath.isEmpty() || state_.storageBucket.isEmpty() || state_.storageTxmlPath.isEmpty())
    {
        plugin_->Notifications()->ShowSplashDialog("Cannot open scene storage, you don't have the needed information to proceed.", ":/images/icon-update.png");
        LogError(LC + "Cannot open scene storage, you don't have the needed information to proceed.");
        return;
    }
    
    state_.headless = false;

    Authenticate();
}

bool MeshmoonStorage::Close()
{
    /// If an active authenticator is still ongoing and we get closed,
    /// destroy it. Otherwise the old ptr will be returned in Open() again.
    if (authenticator_)
        authenticator_->EmitCanceled();
    SAFE_DELETE(authenticator_);
    
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot close storage view</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return false;
    }
    
    // First force close all editors that don't have unsaved changes.
    // This is done because the unsaved changes dialog blocks
    for (int i=0; i<state_.editors.size(); ++i)
    {
        IRocketAssetEditor *openEditor = state_.editors[i];
        if (!openEditor || openEditor->HasUnsavedChanges())
            continue;

        // Closing a non-modified editor so that it wont trigger OnEditorCloseRequest and the ptr will be reseted.
        disconnect(openEditor, SIGNAL(CloseRequest(IRocketAssetEditor*)), this, SLOT(OnEditorCloseRequest(IRocketAssetEditor*))); 
        if (!openEditor->testAttribute(Qt::WA_DeleteOnClose))
            openEditor->deleteLater();
        openEditor->close();
        state_.editors[i] = 0;
    }
 
    bool unsavedEditors = false;
    for (int i=0; i<state_.editors.size(); ++i)
    {
        IRocketAssetEditor *openEditor = state_.editors[i];
        if (!openEditor || !openEditor->HasUnsavedChanges())
            continue;

        // If this editor has unsaved changes, call its close to popup the editor specific warning. 
        // Once all editors are closed by the user the storage can be closed. Delay close as the dialog is blocking.        
        unsavedEditors = true;
        QTimer::singleShot(i+1, openEditor, SLOT(close()));
    }
    if (unsavedEditors)
    {
        state_.closeOnEditorsClosed = true;
        return false;
    }

    Reset();
    ResetUi();
    
    state_.headless = true;
    
    // Storage is now closed. Enable drag and drop from core.
    SceneStructureModule *sceneStructModule = framework_->GetModule<SceneStructureModule>();
    if (sceneStructModule)
        sceneStructModule->SetAssetDragAndDropEnabled(true);
    return true;
}

bool MeshmoonStorage::IsAuthenticated()
{
    if (s3_)
        return true;
    return false;
}

bool MeshmoonStorage::IsReady()
{
    if (IsAuthenticated() && state_.storageRoot && state_.currentFolder)
        return true;
    return false;
}

MeshmoonStorageAuthenticationMonitor *MeshmoonStorage::Authenticate()
{
    if (framework_->IsHeadless())
        return 0;

    /// Storage is already ready. Return a dummy object that will emit completed signal.
    if (IsReady())
    {
        MeshmoonStorageAuthenticationMonitor *authenticator = new MeshmoonStorageAuthenticationMonitor();
        QTimer::singleShot(1, authenticator, SLOT(EmitCompleted()));
        QTimer::singleShot(500, authenticator, SLOT(deleteLater()));
        return authenticator;
    }

    // Already authenticating.
    if (authenticator_)
        return authenticator_;
        
    // At this point we might be authenticated, but still fetching the root file listing (IsReady() returned false above)
    if (IsAuthenticated())
    {
        // In this case we can just make a new auth object and return it. Once storage is ready it will find this and emit its completed signal.
        authenticator_ = new MeshmoonStorageAuthenticationMonitor();
        return authenticator_;
    }

    RocketUserAuthenticator *authenticatorInternal = plugin_->User()->RequestAuthentication();
    if (authenticatorInternal)
    {
        connect(authenticatorInternal, SIGNAL(Completed(int, const QString&)), this, SLOT(AuthenticateContinue(int)), Qt::UniqueConnection);
        authenticator_ = new MeshmoonStorageAuthenticationMonitor();
        return authenticator_;
    }

    plugin_->Notifications()->ShowSplashDialog("Cannot open scene storage, failed to request users permission level.", ":/images/icon-update.png");
    LogError(LC + "Cannot open scene storage, failed to request users permission level.");
    return 0;
}

void MeshmoonStorage::AuthenticateContinue(int level)
{
    if (framework_->IsHeadless())
        return;

    Meshmoon::PermissionLevel levelEnum = (Meshmoon::PermissionLevel)level;
    if (levelEnum < Meshmoon::Elevated)
    {
        QString message = "Cannot open scene storage, authentication level \"" + PermissionLevelToString(levelEnum) + "\" is not high enough.";
        plugin_->Notifications()->ShowSplashDialog(message, ":/images/icon-update.png");
        LogError(LC + message);
        
        /// No clearance for storage.
        if (authenticator_)
            authenticator_->EmitFailed(message);
        SAFE_DELETE(authenticator_);
        return;
    }

    // Init UI
    connect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnWindowResized(const QRectF&)), Qt::UniqueConnection);

    storageWidget_ = new RocketStorageWidget(plugin_, this, state_.storageHostUrl);

    // UI events
    connect(storageWidget_, SIGNAL(FileClicked(MeshmoonStorageItemWidget*)), SLOT(OnFileClicked(MeshmoonStorageItemWidget*)));
    connect(storageWidget_, SIGNAL(FolderClicked(MeshmoonStorageItemWidget*)), SLOT(OnFolderClicked(MeshmoonStorageItemWidget*)));
    
    connect(storageWidget_, SIGNAL(LogoutAndCloseRequest()), SLOT(Unauthenticate()));
    connect(storageWidget_, SIGNAL(LogoutAndCloseRequest()), SLOT(Close()), Qt::QueuedConnection);
    connect(storageWidget_, SIGNAL(CloseRequest()), SLOT(Close()), Qt::QueuedConnection);
    connect(storageWidget_, SIGNAL(RefreshRequest()), SLOT(RefreshCurrentFolderContent()));

    // Requests
    connect(storageWidget_, SIGNAL(DownloadRequest(MeshmoonStorageItemWidgetList, bool)), SLOT(OnDownloadRequest(MeshmoonStorageItemWidgetList, bool)));

    connect(storageWidget_, SIGNAL(UploadSceneRequest(const QString&)), SLOT(OnUploadSceneRequest(const QString&)));
    connect(storageWidget_, SIGNAL(UploadRequest(bool)), SLOT(OnUploadRequest(bool)));
    connect(storageWidget_, SIGNAL(UploadRequest(const QStringList&, bool)), SLOT(UploadFiles(const QStringList&, bool)));
    connect(storageWidget_, SIGNAL(DeleteRequest(MeshmoonStorageItemWidgetList)), SLOT(OnDeleteRequest(MeshmoonStorageItemWidgetList)));
    connect(storageWidget_, SIGNAL(CreateFolderRequest()), SLOT(OnCreateFolderRequest()));
    connect(storageWidget_, SIGNAL(UploadFilesRequest()), SLOT(OnUploadFilesRequest()));
    
    connect(storageWidget_, SIGNAL(CreateNewFileRequest(const QString&)), SLOT(OnCreateNewFileRequest(const QString&)));

    // Add to scene
    framework_->Ui()->GraphicsScene()->addItem(storageWidget_);
    
    storageWidget_->Hide();
    storageWidget_->AnimationEngine()->HookCompleted(this, SLOT(OnStorageWidgetAnimationsCompleted()));

    state_.ui = Storage;

    // Store authentication locally for this machine.
    QString accessKey = framework_->Config()->Read("adminotech", "ast", "seventeen", QVariant("")).toString().trimmed();
    QString secretKey = framework_->Config()->Read("adminotech", "ast", "five", QVariant("")).toString().trimmed();

    RocketStorageAuthDialog *authDialog = new RocketStorageAuthDialog(plugin_, state_.storageTxmlPath, state_.storageBucket);
    connect(authDialog, SIGNAL(Authenticated(QString, QString)), SLOT(OnStorageAuthenticated(QString, QString)));
    authDialog->TryAuthenticate(accessKey, secretKey);
    
    emit StorageWidgetCreated(storageWidget_);
}

bool MeshmoonStorage::OpenDirectory(const MeshmoonStorageItem &directory)
{   
    if (!directory.isDirectory)
    {
        LogError(LC + "OpenDirectory called with non-directory MeshmoonStorageItem.");
        return false;
    }
        
    MeshmoonStorageItemWidget *folder = GetFolder(directory.path);
    if (!folder)
        return false;
    
    ShowFolder(folder);
    return true;
}

QString MeshmoonStorage::BaseUrl() const
{
    if (state_.storageHostUrl.isEmpty() || state_.storageBasePath.isEmpty())
        return "";
    if (!state_.storageHostUrl.endsWith("/") && !state_.storageBasePath.startsWith("/"))
        return state_.storageHostUrl + "/" + state_.storageBasePath;
    return state_.storageHostUrl + state_.storageBasePath;
}

MeshmoonStorageItem MeshmoonStorage::RootDirectory() const
{
    return MeshmoonStorageItem(state_.storageRoot, this);
}

MeshmoonStorageItem MeshmoonStorage::CurrentDirectory() const
{
    return MeshmoonStorageItem(state_.currentFolder, this);
}

MeshmoonStorageItemList MeshmoonStorage::List(const MeshmoonStorageItem &directory, bool listFiles, bool listFolders) const
{
    MeshmoonStorageItemList items;
    if (!directory.isDirectory)
    {
        LogError(LC + "ListDirectories called with non-directory MeshmoonStorageItem.");
        return items;
    }

    MeshmoonStorageItemWidget *folder = GetFolder(directory.path);
    if (folder)
    {
        if (listFolders)
        {
            MeshmoonStorageItemWidgetList storageFolders = folder->Folders();
            foreach(MeshmoonStorageItemWidget *storageFolder, storageFolders)
            {
                if (storageFolder && storageFolder != state_.cdUp)
                    items << MeshmoonStorageItem(storageFolder, this);
            }
        }
        if (listFiles)
        {
            MeshmoonStorageItemWidgetList storageFiles = folder->Files();
            foreach(MeshmoonStorageItemWidget *storageFile, storageFiles)
                items << MeshmoonStorageItem(storageFile, this);
        }
    }
    return items;
}

MeshmoonStorageItemWidget *MeshmoonStorage::Fill(QListWidget *listWidget, const MeshmoonStorageItem &directory, bool listFiles, bool listFolders, const QStringList &fileSuffixFilters) const
{
    if (!listWidget)
        return 0;

    if (!directory.isDirectory)
    {
        LogError(LC + "ListDirectories called with non-directory MeshmoonStorageItem.");
        return 0;
    }

    MeshmoonStorageItemWidget *folder = GetFolder(directory.path);
    if (folder)
    {
        if (listFolders)
        {
            MeshmoonStorageItemWidgetList storageFolders = folder->Folders();
            foreach(MeshmoonStorageItemWidget *storageFolder, storageFolders)
            {
                if (storageFolder && storageFolder != state_.cdUp)
                    listWidget->addItem(new MeshmoonStorageItemWidget(plugin_, storageFolder, folder)); // Protect our ptr from being deleted
            }
        }
        if (listFiles)
        {
            MeshmoonStorageItemWidgetList storageFiles = folder->Files();
            foreach(MeshmoonStorageItemWidget *storageFile, storageFiles)
            {
                // Let's accept both 'suffix' and '.suffix' for ease of use. The MeshmoonStorageItemWidget internally has without the prefix '.'.
                bool shouldAdd = fileSuffixFilters.isEmpty();
                if (!shouldAdd)
                {
                    if (fileSuffixFilters.contains(storageFile->suffix) || fileSuffixFilters.contains(storageFile->completeSuffix))
                        shouldAdd = true;
                    else if (fileSuffixFilters.contains("." + storageFile->suffix) || fileSuffixFilters.contains("." + storageFile->completeSuffix))
                        shouldAdd = true;
                }
                if (shouldAdd)
                    listWidget->addItem(new MeshmoonStorageItemWidget(plugin_, storageFile, folder)); // Protect our ptr from being deleted
            }
        }
    }
    return folder;
}

RocketStorageSelectionDialog *MeshmoonStorage::ShowSelectionDialog(const QStringList &suffixFilters, bool allowChangingFolder, MeshmoonStorageItem startDirectory, QWidget *parent)
{
    return new RocketStorageSelectionDialog(plugin_, this, suffixFilters, allowChangingFolder, startDirectory, parent);
}

void MeshmoonStorage::OnWindowResized(const QRectF &sceneRect)
{
    UpdateInterface(sceneRect);
}

void MeshmoonStorage::UpdateInterface(QRectF sceneRect)
{
    if (state_.ui == None)
        return;

    // Headless mode, don't show ui or update position!
    if (state_.headless)
        return;

    if (sceneRect.isNull())
        sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
     
    bool barTop = plugin_->Taskbar() ? plugin_->Taskbar()->IsTopAnchored() : false;
    qreal barHeight = plugin_->Taskbar() ? plugin_->Taskbar()->Height() : 0;
       
    if (state_.ui == Storage)
    {
        static const qreal minWidth = 545.0;
        static const qreal maxWidth = 600.0;
        qreal targetWidth = Min<qreal>((sceneRect.width() / 5.0) * 2.0, maxWidth);
        targetWidth = Max<qreal>(targetWidth, minWidth);

        QRectF spec(sceneRect.width() - targetWidth, barTop ? barHeight : 0, targetWidth, sceneRect.height() - barHeight);
        
        // If the hide state is active, reset back to show state and trigger a mouse event
        // so the hide calculations are executed again.
        RocketAnimationEngine *engine = storageWidget_->AnimationEngine();
        int animationState = engine->Value<int>("state");
        /*if (!storageWidget_->IsPinned())
        {
            if (animationState != 0)
            {
                engine->StoreValue("state", 0);
                storageWidget_->setPos(spec.topLeft());
                storageWidget_->setOpacity(storageWidget_->showOpacity);
            }
        }*/

        if (animationState == 0 || storageWidget_->IsPinned())
            storageWidget_->Animate(spec.size(), spec.topLeft(), -1.0, sceneRect, (!storageWidget_->isVisible() ? RocketAnimations::AnimateLeft : RocketAnimations::NoAnimation));
        else
        {
            QSizeF origSize = spec.size();
            engine->StoreValue("pos", spec.topLeft());
            engine->StoreValue("opacity", storageWidget_->showOpacity);
            spec.setLeft(sceneRect.width() - 150);
            storageWidget_->Animate(origSize, spec.topLeft(), 0.5, sceneRect, RocketAnimations::AnimateLeft);
        }
    }
}

void MeshmoonStorage::OnStorageAuthenticated(QString accessKey, QString secretKey)
{
    if (accessKey.isEmpty() || secretKey.isEmpty())
    {
        Close();

        // Auth was canceled by the user.
        if (authenticator_)
            authenticator_->EmitCanceled();
        SAFE_DELETE(authenticator_);
        return;
    }
    framework_->Config()->Write("adminotech", "ast", "seventeen", accessKey);
    framework_->Config()->Write("adminotech", "ast", "five", secretKey);

    UpdateInterface();

    SAFE_DELETE(s3_);
    s3_ = new QS3Client(QS3Config(accessKey, secretKey, state_.storageBucket, QS3Config::EU_WEST_1));

    RefreshFolderContent(state_.storageBasePath);
    
    // Storage is now open and authenticated. Disable drag and drop from core.
    SceneStructureModule *sceneStructModule = framework_->GetModule<SceneStructureModule>();
    if (sceneStructModule)
        sceneStructModule->SetAssetDragAndDropEnabled(false);
}

void MeshmoonStorage::RefreshCurrentFolderContent()
{
    if (!s3_)
        return;
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot refresh current folder content, no folder selected.");
        return;
    }
    
    RefreshFolderContent(state_.currentFolder->data.key);
}

void MeshmoonStorage::RefreshFolderContent(const QString &prefix)
{
    if (!s3_)
        return;

    QS3ListObjectsResponse *response = s3_->listObjects(prefix.endsWith("/") ? prefix : prefix + "/");
    connect(response, SIGNAL(finished(QS3ListObjectsResponse*)), SLOT(OnListSceneFolderObjectsResponse(QS3ListObjectsResponse*)));
}

void MeshmoonStorage::OnListSceneFolderObjectsResponse(QS3ListObjectsResponse *response)
{
    if (!response)
        return;
    if (!response->succeeded)
    {
        LogError(LC + response->error.toString());
        if (response->httpStatusCode == 403)
        {
            plugin_->Notifications()->ShowSplashDialog("<b>Could not update storage view</b><br><br>Please contact Meshmoon with your information to resolve this issue.", ":/images/icon-update.png");
            Close();
        }

        if (authenticator_)
            authenticator_->EmitFailed("Count not update storage view");
        SAFE_DELETE(authenticator_);
        return;
    }
    
    // Create cd up item once.
    if (!state_.cdUp)
        state_.cdUp = new MeshmoonStorageItemWidget(plugin_, "..");

    // Create root if does not yet exist.
    bool wasReady = IsReady();
    if (!state_.storageRoot)
    {
        wasReady = false;

        QString rootKey = state_.storageBasePath;
        if (!rootKey.endsWith("/")) 
            rootKey.append("/");
        for(int i=0; i<response->objects.size(); ++i)
        {
            QS3Object &rootCandidate = response->objects[i];
            if (rootCandidate.key == rootKey)
            {
                state_.storageRoot = new MeshmoonStorageItemWidget(plugin_, rootCandidate);
                response->objects.removeAt(i);
                break;
            }
        }
        // Root not found in response objects, emulate it.
        if (!state_.storageRoot)
            state_.storageRoot = new MeshmoonStorageItemWidget(plugin_, rootKey);
    }
    
    UpdateFolder(response);
    
    // An authenticator is waiting for completion, emit now that root has been fetched.
    if (authenticator_)
        authenticator_->EmitCompleted();
    SAFE_DELETE(authenticator_);

    // Emit ready signal. This happens once per storage open.
    if (!wasReady)
        emit Ready();
}

void MeshmoonStorage::UpdateFolder(QS3ListObjectsResponse *response)
{
    if (response->objects.isEmpty())
    {
        LogWarning(LC + "List objects response has no objects, not updating view.");
        return;
    }

    ClearFolderView();

    MeshmoonStorageItemWidget *folder = GetFolder(response->prefix);
    if (!folder)
    {
        LogError(LC + "Could not find folder item for " + response->prefix);
        return;
    }
    folder->DeleteChildren();

    MeshmoonStorageItemWidgetList items;
    items << folder;

    // Create items
    while(!response->objects.isEmpty())
    {
        QS3Object obj = response->objects.takeFirst();
        
        // Skip root and folder itself. They are already created
        if (obj.key == state_.storageRoot->data.key || obj.key == folder->data.key)
            continue;
        if (!obj.key.startsWith(folder->data.key))
        {
            LogError(LC + "Skipping item " + obj.key + " that is not part of the refreshed folder " + folder->data.key);
            continue;
        }
        MeshmoonStorageItemWidget *item = new MeshmoonStorageItemWidget(plugin_, obj);
        
        connect(item, SIGNAL(DownloadRequest(MeshmoonStorageItemWidget*)), SLOT(OnDownloadRequest(MeshmoonStorageItemWidget*)));
        connect(item, SIGNAL(DeleteRequest(MeshmoonStorageItemWidget*)), SLOT(OnDeleteRequest(MeshmoonStorageItemWidget*)));
        connect(item, SIGNAL(CopyAssetReferenceRequest(MeshmoonStorageItemWidget*)), SLOT(OnCopyAssetReferenceRequest(MeshmoonStorageItemWidget*)));
        connect(item, SIGNAL(CopyUrlRequest(MeshmoonStorageItemWidget*)), SLOT(OnCopyUrlRequest(MeshmoonStorageItemWidget*)));
        connect(item, SIGNAL(CreateCopyRequest(MeshmoonStorageItemWidget*)), SLOT(OnCreateCopyRequest(MeshmoonStorageItemWidget*)));
        connect(item, SIGNAL(EditorOpened(IRocketAssetEditor*)), SLOT(OnEditorOpened(IRocketAssetEditor*)));

        if (item->canCreateInstances)
            connect(item, SIGNAL(InstantiateRequest(MeshmoonStorageItemWidget*)), SLOT(OnInstantiateRequest(MeshmoonStorageItemWidget*)));
        if (item->suffix == "txml")
            connect(item, SIGNAL(RestoreBackupRequest(MeshmoonStorageItemWidget*)), SLOT(OnRestoreBackupRequest(MeshmoonStorageItemWidget*)));

        items <<  item;
    }

    // Create missing folders
    CreateMissingFolders(items);
    
    // Parent items
    foreach(MeshmoonStorageItemWidget *item, items)
    {
        if (item->parent || item == folder)
            continue;
        foreach(MeshmoonStorageItemWidget *parentCandidate, items)
        {
            if (!parentCandidate->data.isDir)
                continue;
            if (MeshmoonStorageItemWidget::FindParent(item, parentCandidate))
                break;
        }
        if (!item->parent)
        {
            LogDebug(LC + "Deleting orphan storage item without a parent: " + item->data.key);
            SAFE_DELETE(item);
        }
    }

    ShowFolder(folder);
}

void MeshmoonStorage::ClearFolderView()
{
    if (!storageWidget_ || !storageWidget_->ListView() || storageWidget_->ListView()->count() == 0)
        return;

    // Remove items from the list in a manner that they wont be automatically deleted.
    // We handle removing the items when its appropriate.
    RocketStorageListWidget *list = storageWidget_->ListView();
    while (list->count() > 0)
        list->takeItem(0);
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFolder(const MeshmoonStorageItem &item) const
{
    if (item.IsNull() || !item.isDirectory)
        return 0;
    return GetFolder(item.path);
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFolder(QString key) const
{
    if (!state_.storageRoot)
        return 0;
    if (!key.endsWith("/"))
        key.append("/");
    if (state_.storageRoot->data.key == key)
        return state_.storageRoot;
    return GetFolder(state_.storageRoot, key);
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFolder(MeshmoonStorageItemWidget *lookupFolder, const QString &key) const
{
    if (!lookupFolder)
        return 0;

    MeshmoonStorageItemWidget *found = 0;
    foreach(MeshmoonStorageItemWidget *folder, lookupFolder->Folders())
    {
        if (!folder)
            continue;
        if (folder->data.key == key)
            return folder;
        found = GetFolder(folder, key);
        if (found)
            break;
    }
    return found;
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFile(const MeshmoonStorageItem &item) const
{
    if (item.IsNull() || !item.isFile)
        return 0;
    return GetFile(item.path);
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFile(const QString &key) const
{
    if (!state_.storageRoot)
        return 0;
    return GetFile(state_.storageRoot, key);
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFile(MeshmoonStorageItemWidget *lookupFolder, const QString &key) const
{
    if (!lookupFolder)
        return 0;
        
    MeshmoonStorageItemWidget *found = 0;
    foreach(MeshmoonStorageItemWidget *file, lookupFolder->Files())
    {
        if (file->data.key == key)
            return file;
    }
    foreach(MeshmoonStorageItemWidget *folder, lookupFolder->Folders())
    {
        if (!folder)
            continue;
        found = GetFile(folder, key);
        if (found)
            break;
    }
    return found;
}

MeshmoonStorageItemWidget *MeshmoonStorage::GetFileByName(MeshmoonStorageItemWidget *lookupFolder, const QString &filename) const
{
    if (!lookupFolder)
        return 0;
    foreach(MeshmoonStorageItemWidget *file, lookupFolder->Files())
    {
        if (file->filename == filename)
            return file;
    }
    return 0;
}

QString MeshmoonStorage::UrlForRelativeRef(const QString &relativeRef) const
{
    return UrlForItem(BaseUrl(), relativeRef);
}

QString MeshmoonStorage::RelativeRefForUrl(const QString &absoluteRef) const
{
    QString baseUrl = BaseUrl();
    if (baseUrl.isEmpty())
        return "";
    if (!baseUrl.endsWith("/"))
        baseUrl += "/";
    if (absoluteRef.startsWith(baseUrl, Qt::CaseInsensitive))
        return absoluteRef.mid(baseUrl.length());
    return "";
}

MeshmoonStorageItem MeshmoonStorage::ItemForRelativeRef(const QString &relativeRef) const
{
    return ItemForUrl(UrlForRelativeRef(relativeRef));
}

MeshmoonStorageItem MeshmoonStorage::ItemForUrl(const QString &absoluteRef) const
{
    if (state_.storageHostUrl.isEmpty())
        return MeshmoonStorageItem();

    QString storageHost(state_.storageHostUrl);
    if (!storageHost.endsWith("/"))
        storageHost += "/";
    if (absoluteRef.startsWith(storageHost, Qt::CaseInsensitive))
        return MeshmoonStorageItem(absoluteRef.mid(storageHost.length()), this);
    return MeshmoonStorageItem();
}

QString MeshmoonStorage::UrlForItem(const MeshmoonStorageItem &item) const
{
    if (item.IsNull())
        return "";
    return UrlForItem(item.path);
}

QString MeshmoonStorage::UrlForItem(const QString &storagePath) const
{
    return UrlForItem(state_.storageHostUrl, storagePath);
}

QString MeshmoonStorage::UrlForItem(const QString &storageUrl, const QString &storagePath)
{   
    QString rootUrl(storageUrl);
    if (!rootUrl.endsWith("/"))
        rootUrl += "/";
    QString relativeRef(storagePath);
    if (relativeRef.startsWith("/"))
        relativeRef = relativeRef.mid(1);
    relativeRef.prepend(rootUrl);
    return relativeRef;
}

QString MeshmoonStorage::RelativeRefForKey(QString storagePath) const
{
    QString rootKey = state_.storageRoot ? state_.storageRoot->data.key : state_.storageBasePath;
    if (rootKey.startsWith("/") && !storagePath.startsWith("/"))
        storagePath = "/" + storagePath;
    if (!rootKey.startsWith("/") && storagePath.startsWith("/"))
        rootKey = "/" + rootKey;

    QString assetRef = storagePath.mid(rootKey.length());
    if (assetRef.startsWith("/"))
        assetRef = assetRef.mid(1);
    return assetRef;
}

QString MeshmoonStorage::ContentMimeType(const QString &suffix, const QString & /*completeSuffix*/) const
{
    if (suffix == "xml" || suffix == "txml")
        return "text/xml";
    else if (suffix == "txt" || suffix == "material" || suffix == "particle")
        return "text/plain";
    else if (suffix == "png")
        return "image/png";
    else if (suffix == "bmp")
        return "image/bmp";
    else if (suffix == "gif")
        return "image/gif";
    else if (suffix == "jpg" || suffix == "jpeg")
        return "image/jpeg";
    else if (suffix == "js")
        return "application/javascript";
    else if (suffix == "json")
        return "application/json";
    else if (suffix == "zip")
        return "application/x-zip-compressed";
        
    // Default to S3 default
    return "binary/octet-stream";
}

void MeshmoonStorage::OnFileClicked(MeshmoonStorageItemWidget *file)
{
    if (!file)
        return;
}

void MeshmoonStorage::OnFolderClicked(MeshmoonStorageItemWidget *folder)
{
    if (!folder)
        return;
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot change folder</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }

    if (folder != state_.cdUp)
        ShowFolder(folder);
    else
        ShowFolder(state_.cdUp->parent);
}

void MeshmoonStorage::OnOperationFinished(MeshmoonStorageOperationMonitor *operation)
{
    operation->deleteLater();
    
    state_.folderOperationOngoing = false;

    switch (operation->type)
    {
        case MeshmoonStorageOperationMonitor::DownloadFiles:
        {
            QString destZip = operation->property("destZip").toString();
            QString sourceDir = operation->property("sourceDir").toString();
            if (!destZip.isEmpty() && !sourceDir.isEmpty())
            {               
                RocketZipWorker *packager = new RocketZipWorker(destZip, sourceDir, RocketZipWorker::Maximum);
                connect(packager, SIGNAL(AsynchPackageCompleted(bool)), SLOT(OnZipPackagerCompleted(bool)), Qt::QueuedConnection);
                packager->start(QThread::HighPriority);
                
                storageWidget_->HideProgressBar();
                storageWidget_->SetProgressMessage("Compressing zip file, please wait...");
            }
            break;
        }
        case MeshmoonStorageOperationMonitor::UploadFiles:
        {
            if (plugin_->Networking() && !operation->changedAssetRefs.isEmpty())
                plugin_->Networking()->SendAssetReloadMsg(operation->changedAssetRefs);

            RefreshCurrentFolderContent();
            
            if (!state_.pendingSceneCreation.IsEmpty())
            {
                Scene *activeScene = framework_->Renderer()->MainCameraScene();
                if (activeScene)
                {
                    LogInfo(LC + QString("Scene assets uploaded, creating %1 entities.").arg(state_.pendingSceneCreation.entities.size()));
                    activeScene->CreateContentFromSceneDesc(state_.pendingSceneCreation, false, AttributeChange::Replicate);
                }
                
                state_.pendingSceneCreation.entities.clear();
                state_.pendingSceneCreation.assets.clear();
            }
            break;
        }
        case MeshmoonStorageOperationMonitor::DeleteFiles:
        {
            RefreshCurrentFolderContent();
            break;
        }
        default:
            break;
    }
}

void MeshmoonStorage::OnZipPackagerCompleted(bool successfull)
{
    storageWidget_->HideProgress();
    
    RocketZipWorker *packager = dynamic_cast<RocketZipWorker*>(sender());
    if (packager)
    {
        packager->deleteLater();
        RocketFileSystem::RemoveDirectory(packager->InputDirectory());
    }
}

void MeshmoonStorage::OnCopyAssetReferenceRequest(MeshmoonStorageItemWidget *item)
{
    if (item && !item->data.isDir && QApplication::clipboard())
        QApplication::clipboard()->setText(RelativeRefForKey(item->data.key));
}

void MeshmoonStorage::OnCopyUrlRequest(MeshmoonStorageItemWidget *item)
{
    if (item && !item->data.isDir && QApplication::clipboard())
        QApplication::clipboard()->setText(UrlForItem(item->data.key));
}

void MeshmoonStorage::OnCreateCopyRequest(MeshmoonStorageItemWidget *item)
{
    if (!s3_ || !item || item->data.isDir)
        return;

    // Suggest a unique name
    QString suggestion;
    if (item->filename.contains("."))
    {
        for(int i=0; i<=100; ++i)
        {
            // Suggest a name automatically that does not exist yet with <baseName>-copy<index>.<completeSuffix>
            suggestion = QString("%1-copy%2.%3").arg(QFileInfo(item->filename).baseName()).arg((i == 0 ? "" : QString::number(i))).arg(item->completeSuffix);
            if (!GetFileByName(state_.currentFolder, suggestion))
                break;
            // Give up and let the user give filename
            if (i==100)
            {
                suggestion = item->filename;
                break;
            }
        }
    }
    else
        suggestion = item->filename;

    QString newFilename;
    QRegExp validator("^[\\.a-z0-9-_]+$", Qt::CaseInsensitive);

    while(true)
    {
        QPixmap emptyIcon(16,16);
        emptyIcon.fill(Qt::transparent);

        QInputDialog inputDialog;
        inputDialog.setWindowTitle(" ");
        inputDialog.setWindowIcon(QIcon(emptyIcon));
        inputDialog.setInputMode(QInputDialog::TextInput);
        inputDialog.setLabelText("Creating Copy of " + item->filename);
        inputDialog.setTextValue((newFilename.isEmpty() ? suggestion : newFilename));
        inputDialog.setOkButtonText("Create");
        inputDialog.setCancelButtonText("Cancel");
        inputDialog.setModal(true);

        plugin_->Notifications()->DimForeground();
        int ret = inputDialog.exec();
        plugin_->Notifications()->ClearForeground();
        
        // Bail out early.
        if (ret)
        {
            newFilename = inputDialog.textValue().trimmed();
            if (newFilename.isEmpty())
                return;
        }
        else
            return;

        // Validate.
        if (!validator.exactMatch(newFilename))
        {
            if (validator.matchedLength() != -1)
                plugin_->Notifications()->ExecSplashDialog(QString("The filename has an invalid character %1 at index %2")
                    .arg(newFilename.mid(validator.matchedLength(), 1)).arg(validator.matchedLength()), ":/images/icon-exit.png");
            else
                plugin_->Notifications()->ExecSplashDialog("The filename has invalid characters for a URL, use [A-Z] [a-z] [0-9] -_.", ":/images/icon-exit.png");
            continue;
        }
        
        // Check that file suffix has not changed. Dont use complete suffix so "." can be used in the name.
        QString newSuffix = QFileInfo(newFilename).suffix();
        if (newSuffix != item->suffix)
        {
            plugin_->Notifications()->ExecSplashDialog(QString("You can't change the file suffix from .%1 to .%2 when creating a copy").arg(item->suffix).arg(newSuffix), ":/images/icon-exit.png");
            continue;
        }

        // Check that the file does not already exist.
        if (GetFileByName(state_.currentFolder, newFilename))
            plugin_->Notifications()->ExecSplashDialog(QString("File with name '%1' already exists").arg(newFilename), ":/images/icon-exit.png");
        else
            break;
    }
    
    if (newFilename.isEmpty())
        return;
    
    // Generate full destination key
    QString destinationKey = item->data.key;
    if (destinationKey.startsWith("/"))
        destinationKey = destinationKey.mid(1);
    if (destinationKey.contains("/"))
        destinationKey = destinationKey.mid(0, destinationKey.lastIndexOf("/")+1);
    else
        destinationKey = "";
    destinationKey += newFilename;

    CreateFileCopy(item->data.key, destinationKey, true, QS3::PublicRead);
}

QS3CopyObjectResponse *MeshmoonStorage::CreateFileCopy(const QString &sourceKey, const QString &destinationKey, bool refreshCurrentFolderWhenDone, QS3::CannedAcl acl)
{
    if (!GetFile(sourceKey))
    {
        LogError(LC + QString("Cannot perform copy operation. Source file %1 does not exist in storage.").arg(sourceKey));
        return 0;
    }
    if (GetFile(destinationKey))
    {
        LogError(LC + QString("Cannot perform copy operation. Destination file %1 already exists in storage.").arg(destinationKey));
        return 0;
    }

    QS3CopyObjectResponse *copyOperation = s3_->copy(sourceKey, destinationKey, acl);
    copyOperation->setProperty("refreshCurrentFolder", refreshCurrentFolderWhenDone);
    connect(copyOperation, SIGNAL(finished(QS3CopyObjectResponse*)), SLOT(OnCopyOperationCompleted(QS3CopyObjectResponse*)));
    return copyOperation;
}

void MeshmoonStorage::OnCopyOperationCompleted(QS3CopyObjectResponse *response)
{
    if (!response)
        return;       
    if (!response->succeeded)
    {
        plugin_->Notifications()->ShowSplashDialog("Storage copy operation failed", ":/images/icon-exit.png");
        return;
    }

    QVariant refreshVariant = response->property("refreshCurrentFolder");
    if (refreshVariant.isValid() && !refreshVariant.isNull() && refreshVariant.type() == QVariant::Bool && refreshVariant.toBool())
        RefreshCurrentFolderContent();
}

void MeshmoonStorage::OnDownloadRequest(MeshmoonStorageItemWidget *item)
{
    MeshmoonStorageItemWidgetList files;
    files << item;
    OnDownloadRequest(files, false);
}

void MeshmoonStorage::OnDownloadRequest(MeshmoonStorageItemWidgetList files, bool zip)
{
    if (!s3_)
    {
        LogError(LC + "Cannot start download. Storage client is null.");
        return;
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot download files right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }
    if (state_.infoDialog)
        return;
    if (files.isEmpty())
        return;
  
    QVariant dirVariant = framework_->Config()->Read("adminotech", "ast", "download-folder");
    QString lastDir = (!dirVariant.isNull() && dirVariant.isValid() ? dirVariant.toString().trimmed() : "");

    RocketStorageFileDialog *dialog = new RocketStorageFileDialog(framework_->Ui()->MainWindow(), plugin_, !zip ? "Select download folder" : "Select zip save path", 
        lastDir, QDir::NoDotAndDotDot|QDir::Files|QDir::AllDirs|QDir::NoSymLinks);
    if (!zip)
        connect(dialog, SIGNAL(FolderSelected(const QString&, MeshmoonStorageItemWidgetList)), SLOT(OnDownloadRequest(const QString&, MeshmoonStorageItemWidgetList)));
    else
        connect(dialog, SIGNAL(FileSelected(const QString&, MeshmoonStorageItemWidgetList)), SLOT(OnZipDownloadRequest(const QString&, MeshmoonStorageItemWidgetList)));

    dialog->executionFolder = CurrentDirectory();
    dialog->files = files;
    dialog->setFileMode(!zip ? QFileDialog::Directory : QFileDialog::AnyFile);
    dialog->setViewMode(QFileDialog::Detail);
    dialog->setOption(QFileDialog::ShowDirsOnly, !zip);
    if (zip)
    {
        dialog->setNameFilters(QStringList() << "ZIP arhive (*.zip)");
        dialog->setAcceptMode(QFileDialog::AcceptSave);
    }

    dialog->Open();
    dialog->setFocus(Qt::ActiveWindowFocusReason);
    dialog->activateWindow();
}

void MeshmoonStorage::OnZipDownloadRequest(const QString &zipFilePath, MeshmoonStorageItemWidgetList files)
{
    RocketStorageFileDialog *dialog = dynamic_cast<RocketStorageFileDialog*>(sender());
    if (!dialog)
        return;
        
    QString destZip = zipFilePath;
    if (!destZip.endsWith(".zip", Qt::CaseInsensitive))
        destZip += ".zip";
    
    // Temporary directory for files pre zipping
    QDir tempDir = QDir::fromNativeSeparators(Application::UserDataDirectory() + "assetcache/meshmoon/storage-temp-zip-" + QUuid::createUuid().toString().replace("{","").replace("}",""));
    if (!tempDir.exists())
    {
        if (!tempDir.mkpath(tempDir.absolutePath()))
        {
            LogError("Failed to create temp download directory " + QDir::toNativeSeparators(tempDir.absolutePath()));
            return;
        }
    }

    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::DownloadFiles);
    connect(monitor, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));
    
    monitor->setProperty("destZip", destZip);
    monitor->setProperty("sourceDir", tempDir.absolutePath());

    foreach(MeshmoonStorageItemWidget *file, files)
    {
        if (!file)
            continue;
        QS3GetObjectResponse *response = s3_->get(file->data.key);
        if (response)
        {
            QString relativeKey = file->data.key;
            if (relativeKey.startsWith(dialog->executionFolder.path))
                relativeKey = relativeKey.mid(dialog->executionFolder.path.length());
            if (relativeKey.startsWith("/"))
                relativeKey = relativeKey.mid(1);

            QString dest = tempDir.absoluteFilePath(relativeKey);
            response->setProperty("destpath", dest);
            monitor->AddOperation(response, file->data.size);
        }
    }

    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
}

void MeshmoonStorage::OnDownloadRequest(const QString &folderPath, MeshmoonStorageItemWidgetList files)
{
    RocketStorageFileDialog *dialog = dynamic_cast<RocketStorageFileDialog*>(sender());
    if (!dialog)
        return;

    if (folderPath.isEmpty())
        return;
    if (files.isEmpty())
        return;
    if (state_.infoDialog)
        return;

    QDir dir(folderPath);
    if (!dir.exists())
    {
        LogError(LC + "Selected download folder does not exist: " + folderPath);
        return;
    }
    
    framework_->Config()->Write("adminotech", "ast", "download-folder", folderPath);
    
    uint replacing = 0;
    uint replacingNotMentioned = 0;
    QStringList replaceFilenames;

    // Check files we are about to overwrite and confirm with UI.
    /// @todo Make global settings that has option to always skip confirmations!
    foreach(MeshmoonStorageItemWidget *file, files)
    {
        QString filepath = dir.absoluteFilePath(file->filename);
        if (!QFile::exists(filepath))
            continue;

        QFileInfo fileInfo(filepath);
        QString filename = fileInfo.fileName();
        if (replacing < 10)
        {
            if (!replaceFilenames.contains(filename))
                replaceFilenames << filename;
        }
        else
            replacingNotMentioned++;
        replacing++;
    }

    // Nothing will be replaced, continue right away.
    if (replacing == 0)
    {
        MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::DownloadFiles);
        connect(monitor, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
        connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));
        
        foreach(MeshmoonStorageItemWidget *file, files)
        {
            if (!file)
                continue;
            QS3GetObjectResponse *response = s3_->get(file->data.key);
            if (response)
            {
                response->setProperty("destpath", dir.absoluteFilePath(file->filename));
                monitor->AddOperation(response, file->data.size);
            }
        }
        
        if (monitor->total == 0)
            SAFE_DELETE(monitor)
        else
            state_.folderOperationOngoing = true;
    }
    // Replacing files. Do a confirmation.
    else
    {
        QString confirmMessage = "<b>" + QDir::toNativeSeparators(dir.path()) + "</b>";
        confirmMessage += "<pre>  ";
        confirmMessage += replaceFilenames.join("<br>  ");
        if (replacingNotMentioned > 0)
            confirmMessage += "<br>  " + QString("... and %1 ").arg(replacingNotMentioned) + (replacingNotMentioned == 1 ? "other" : "others");
        confirmMessage += "</pre>";

        state_.infoDialog = new RocketStorageInfoDialog(plugin_, QString("Overwrite %1 existing %2?").arg(replacing).arg((replacing == 1 ? "file" : "files")));
        connect(state_.infoDialog, SIGNAL(AcceptClicked()), SLOT(OnDownloadRequestContinue()));
        connect(state_.infoDialog, SIGNAL(Rejected()), SLOT(DestroyInfoDialog()));

        state_.infoDialog->dir = dir;
        state_.infoDialog->items = files;
        state_.infoDialog->ui.buttonOk->setText("Overwrite");
        state_.infoDialog->ui.labelText->setStyleSheet("padding-left: 15px;");
        state_.infoDialog->EnableTextMode(confirmMessage);
        state_.infoDialog->OpenDelayed(100);
    }
}

void MeshmoonStorage::OnDownloadRequestContinue()
{
    if (!state_.infoDialog)
        return;

    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::DownloadFiles);
    connect(monitor, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));

    foreach(MeshmoonStorageItemWidget *file, state_.infoDialog->items)
    {
        if (!file)
            continue;
        QS3GetObjectResponse *response = s3_->get(file->data.key);
        if (response)
        {
            response->setProperty("destpath", state_.infoDialog->dir.absoluteFilePath(file->filename));
            monitor->AddOperation(response, file->data.size);
        }
    }

    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
        
    state_.infoDialog->Close();
    DestroyInfoDialog();
}

void MeshmoonStorage::OnDownloadOperationProgress(QS3GetObjectResponse *response, qint64 complete, qint64 total)
{
    if (storageWidget_->ui.labelProgressTitle->text().isEmpty())
        storageWidget_->ui.labelProgressTitle->setText("Downloading");
    storageWidget_->SetProgress(complete, total);

    // Save file to disk
    if (response && response->succeeded)
    {           
        QString destpath = response->property("destpath").toString();
        if (destpath.isEmpty())
        {
            LogWarning(LC + "Failed to determine destination from 'destpath' property for downloaded file " + response->key);
            return;
        }
        
        // Check that parent dir exists
        QDir parentDir = QFileInfo(destpath).dir();
        if (!parentDir.exists())
            parentDir.mkpath(parentDir.absolutePath());

        QFile file(destpath);
        if (file.open(QIODevice::WriteOnly))
        {
            file.resize(0);
            file.write(response->data);
            file.close();
        }
        else
            LogError(LC + "Failed to save downloaded data of " + response->key + " to file " + destpath);
    }
    else if (response && !response->succeeded)
        LogError(LC + "Failed to download file: " + response->key);
}

void MeshmoonStorage::OnUploadSceneRequest(const QString &filepath)
{
    if (!s3_)
    {
        LogError(LC + "Cannot upload scene file content. Storage client is null.");
        return;
    }
    if (!state_.storageRoot)
    {
        LogError(LC + "Cannot upload scene file content. Root folder is null.");
        return;
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot upload scene file content right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }
    if (!state_.pendingSceneCreation.IsEmpty())
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot upload scene file content right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }
    if (state_.infoDialog)
        return;

    if (!QFile::exists(filepath))
    {
        LogError(LC + "Cannot process scene, file does not exist: " + filepath);
        return;
    }
    
    QString txmlFolder = QFileInfo(filepath).dir().path();
    if (!txmlFolder.endsWith("/"))
        txmlFolder += "/";
    
    Scene *scene = framework_->Renderer()->MainCameraScene();
    if (!scene)
    {
        LogError(LC + "Cannot process scene file, no active scene.");
        return;
    }

    SceneDesc sceneDesc = scene->CreateSceneDescFromXml(filepath);
    
    ImportEntityItemList entities;
    foreach(const EntityDesc &entDesc, sceneDesc.entities)
    {
        RocketStorageImportEntityItem item;
        item.desc = entDesc;
        item.existsScene = !entDesc.name.trimmed().isEmpty() ? (scene->EntityByName(entDesc.name) ? true : false) : false;
        
        entities.push_back(item);
    }
    
    QString storageRootPath = state_.storageRoot->data.key;
    QString currentFolderPath = state_.currentFolder->data.key;
    QString destinationPrefix = (storageRootPath == currentFolderPath ? "" : currentFolderPath.remove(0, storageRootPath.length()));

    ImportAssetItemList assets;
    foreach (AssetDesc assetDesc, sceneDesc.assets.values())
    {
        if (assetDesc.source.trimmed().isEmpty() || assetDesc.destinationName.trimmed().isEmpty())
            continue;
        QString lower = assetDesc.source.toLower();
        if (lower.startsWith("http://") || lower.startsWith("https://") || lower.startsWith("local://") || lower.startsWith("generated://"))
            continue;
        QFileInfo info(assetDesc.source);
        if (info.isDir())
            continue;
        
        QString fullRelPath = destinationPrefix + assetDesc.destinationName;
        assetDesc.destinationName = fullRelPath;

        // Any asset
        RocketStorageImportAssetItem item;
        item.desc = assetDesc;
        item.pathAbsolute = info.absoluteFilePath();
        item.existsDisk = QFile::exists(item.pathAbsolute);
        item.existsStorage = GetFile(state_.storageRoot->data.key + assetDesc.destinationName) != 0 ? true : false;
        item.pathRelative = info.fileName();

        if (item.pathRelative.contains(" "))
        {
            plugin_->Notifications()->ShowSplashDialog("Spaces are not allowed in filenames. Found in filename: \"" + item.pathRelative + "\", full path: \"" + item.pathAbsolute + "\"");
            return;
        }
        if (item.pathAbsolute.startsWith(txmlFolder))
            item.pathRelative = item.pathAbsolute.mid(txmlFolder.length());

        assets.push_back(item);
        
        // Material?
        if (assetDesc.typeName == "Mesh materials" || assetDesc.typeName == "Material")
        {
            // Find referenced textures
            QStringList textureFiles = ParseTextures(info);
            foreach(const QString textureFile, textureFiles)
            {
                QFileInfo texInfo(textureFile);

                AssetDesc texDesc;
                texDesc.source = texInfo.absoluteFilePath();
                texDesc.destinationName = destinationPrefix + texInfo.fileName();
                if (texDesc.destinationName.contains(" "))
                {
                    plugin_->Notifications()->ShowSplashDialog("Spaces are not allowed in filenames. Found in filename: \"" + texDesc.destinationName + "\", full path: \"" + texDesc.source + "\"");
                    return;
                }

                // Texture
                RocketStorageImportAssetItem texItem;
                texItem.desc = texDesc;
                texItem.pathAbsolute = texInfo.absoluteFilePath();
                texItem.existsDisk = QFile::exists(texItem.pathAbsolute);
                texItem.existsStorage = GetFile(state_.storageRoot->data.key + texDesc.destinationName) != 0 ? true : false;
                texItem.pathRelative = texItem.pathAbsolute;

                if (texItem.pathRelative.startsWith(txmlFolder))
                    texItem.pathRelative = texItem.pathRelative.mid(txmlFolder.length());
                    
                assets.push_back(texItem);
            }
        }
    }

    RocketStorageSceneImporter *importer = new RocketStorageSceneImporter(framework_->Ui()->MainWindow(), plugin_, destinationPrefix, sceneDesc, entities, assets);
    connect(importer, SIGNAL(ImportScene(const QStringList&, const SceneDesc &)), SLOT(OnUploadSceneRequestContinue(const QStringList&, SceneDesc)));
}

void MeshmoonStorage::OnUploadSceneRequestContinue(const QStringList &uploadFiles, const SceneDesc &sceneDesc)
{
    if (!state_.storageRoot || !state_.currentFolder)
        return;
    
    if (!uploadFiles.isEmpty())
    {
        state_.pendingSceneCreation = sceneDesc;
        UploadFiles(uploadFiles, false);
    }
    else if (uploadFiles.isEmpty() && !sceneDesc.entities.isEmpty())
    {
        Scene *activeScene = framework_->Renderer()->MainCameraScene();
        if (activeScene)
            activeScene->CreateContentFromSceneDesc(sceneDesc, false, AttributeChange::Replicate);
        else
            LogError(LC + "Failed to get current scene to create entities from scene file import.");
    }
}

void MeshmoonStorage::OnUploadRequest(bool confirmOverwrite)
{
    if (!s3_)
    {
        LogError(LC + "Cannot start upload. Storage client is null.");
        return;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start upload. No destination folder selected.");
        return; 
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot start new file uploads right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }
    if (state_.infoDialog)
        return;
    
    QVariant dirVariant = framework_->Config()->Read("adminotech", "ast", "upload-folder");
    QString lastDir = (!dirVariant.isNull() && dirVariant.isValid() ? dirVariant.toString().trimmed() : "");

    RocketStorageFileDialog *dialog = new RocketStorageFileDialog(framework_->Ui()->MainWindow(), plugin_, "Select files for upload", 
        lastDir, QDir::NoDotAndDotDot|QDir::Files|QDir::AllDirs|QDir::NoSymLinks, confirmOverwrite);
    connect(dialog, SIGNAL(FilesSelected(const QStringList&, bool)), SLOT(UploadFiles(const QStringList&, bool)));
    
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setViewMode(QFileDialog::Detail);
    dialog->Open();
    dialog->setFocus(Qt::ActiveWindowFocusReason);
    dialog->activateWindow();
}

QS3CopyObjectResponse *MeshmoonStorage::BackupTxml(bool openBackupFolderWhenDone, QString postfix)
{
    if (!s3_)
    {
        LogError(LC + "Cannot start txml backup. Storage client not opened.");
        return 0;
    }

    postfix = postfix.trimmed();
    if (postfix.isEmpty())
        postfix = "rocket-storage-backup";
    postfix = postfix.replace(" ", "-");

    QString txmlName = state_.storageTxmlPath.split("/", QString::SkipEmptyParts).last().trimmed();
    txmlName = txmlName.replace(".txml", "", Qt::CaseInsensitive);
    QString timestamp = QDateTime::currentDateTimeUtc().toString("dd-MM-yyyy-hh-mm-ss");
    QString destinationKey = state_.storageBasePath + "backup/" + timestamp + "_" + txmlName + (postfix.isEmpty() ? "" : "_" + postfix) + ".txml";

    QS3CopyObjectResponse *backupOperation = s3_->copy(state_.storageTxmlPath, destinationKey, QS3::BucketOwnerFullControl);
    if (openBackupFolderWhenDone)
        connect(backupOperation, SIGNAL(finished(QS3CopyObjectResponse*)), this, SLOT(OnBackupTxmlCompleted(QS3CopyObjectResponse*)));
    return backupOperation; 
}

void MeshmoonStorage::OnBackupTxmlCompleted(QS3CopyObjectResponse *response)
{
    if (response->succeeded)
        RefreshFolderContent(state_.storageBasePath + "backup");
}

bool MeshmoonStorage::HasVisualEditor(const MeshmoonStorageItem &item)
{
    MeshmoonStorageItemWidget *storageItem = GetFile(item);
    if (storageItem)
        return storageItem->hasVisualEditor;
    return false;
}

bool MeshmoonStorage::HasTextEditor(const MeshmoonStorageItem &item)
{
    MeshmoonStorageItemWidget *storageItem = GetFile(item);
    if (storageItem)
        return storageItem->hasTextEditor;
    return false;
}

bool MeshmoonStorage::OpenVisualEditor(const MeshmoonStorageItem &item)
{
    MeshmoonStorageItemWidget *storageItem = GetFile(item);
    if (storageItem)
        return storageItem->OpenVisualEditor();
    return false;
}

bool MeshmoonStorage::OpenTextEditor(const MeshmoonStorageItem &item)
{
    MeshmoonStorageItemWidget *storageItem = GetFile(item);
    if (storageItem)
        return storageItem->OpenTextEditor();
    return false;
}

void MeshmoonStorage::Abort(MeshmoonStorageOperationMonitor *operation)
{
    state_.folderOperationOngoing = false;
    if (storageWidget_)
        storageWidget_->HideProgress();

    if (!operation)
        return;
        
    // Disconnect triggering any future signals from this operation
    disconnect(operation, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)),
        this, SLOT(OnDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
    disconnect(operation, SIGNAL(Progress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)),
        this, SLOT(OnUploadOperationProgress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)));
    disconnect(operation, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), 
        this, SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));

    SAFE_DELETE_LATER(operation);
}

MeshmoonStorageOperationMonitor *MeshmoonStorage::DownloadFile(const MeshmoonStorageItem &file, const QDir &destination)
{
    MeshmoonStorageItemList files;
    files << file;
    return DownloadFiles(files, destination);
}

MeshmoonStorageOperationMonitor *MeshmoonStorage::DownloadFiles(const MeshmoonStorageItemList &files, const QDir &destination)
{
    if (destination.exists())
        destination.mkpath(destination.absolutePath());

    QMap<QString, QString> filesToDestinationPaths;
    foreach(const MeshmoonStorageItem &file, files)
    {
        if (!file.IsNull() && file.isFile)
            filesToDestinationPaths[file.path] = destination.absoluteFilePath(file.filename);
    }
    return DownloadFiles(filesToDestinationPaths);
}

MeshmoonStorageOperationMonitor *MeshmoonStorage::DownloadFiles(const QMap<QString, QString> &filesToDestinationPaths)
{
    if (!s3_)
    {
        LogError(LC + "Cannot start download. Storage client is null.");
        return 0;
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot download files right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return 0;
    }
    if (state_.infoDialog)
        return 0;
    if (filesToDestinationPaths.isEmpty())
        return 0;

    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::DownloadFiles);
    connect(monitor, SIGNAL(Progress(QS3GetObjectResponse*, qint64, qint64)), SLOT(OnDownloadOperationProgress(QS3GetObjectResponse*, qint64, qint64)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));

    foreach(const QString &storageKey, filesToDestinationPaths.keys())
    {       
        QS3GetObjectResponse *response = s3_->get(storageKey);
        if (response)
        {       
            response->setProperty("destpath", filesToDestinationPaths[storageKey]);
            
            // We cant reliably know the size before hand if in headless mode.
            MeshmoonStorageItemWidget *file = GetFile(storageKey);
            monitor->AddOperation(response, (file ? file->data.size : 0));
        }
    }

    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
        
    return monitor;
}

MeshmoonStorageOperationMonitor *MeshmoonStorage::UploadFiles(const QStringList &files, bool confirmOverwrite, MeshmoonStorageItem directory)
{
    // Swap our current directory if one was defined.
    if (!directory.IsNull())
    {
        if (!directory.isDirectory)
        {
            LogError(LC + "UploadFiles called with non-directory MeshmoonStorageItem.");
            return 0;
        }
        if (!OpenDirectory(directory))
            return 0;
    }

    if (!s3_)
    {
        LogError(LC + "Cannot start uploads. Storage client not opened.");
        return 0;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start upload. No destination folder selected.");
        return 0; 
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot start new file uploads right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return 0;
    }
    if (state_.infoDialog)
        return 0;
    if (files.isEmpty())
    {
        LogError(LC + "UploadFiles input parameter file list is empty.");
        return 0;
    }

    uint replacing = 0;
    uint replacingNotMentioned = 0;
    QStringList replaceFilenames;

    // Check files we are about to overwrite and confirm with UI.
    /// @todo Make global settings that has option to always skip confirmations!
    if (confirmOverwrite)
    {
        foreach(QString filepath, files)
        {
            if (!QFile::exists(filepath))
                continue;
            QFileInfo fileInfo(filepath);
            QString filename = fileInfo.fileName();
            if (filename.contains(" "))
            {
                plugin_->Notifications()->ShowSplashDialog("Spaces are not allowed in filenames. Found in filename: \"" + filename + "\", full path: \"" + fileInfo.absoluteFilePath() + "\"");
                return 0;
            }
            foreach(MeshmoonStorageItemWidget *iter, state_.currentFolder->Files())
            {
                if (iter->filename == filename)
                {
                    if (replacing < 10)
                    {
                        if (!replaceFilenames.contains(filename))
                            replaceFilenames << filename;
                    }
                    else
                        replacingNotMentioned++;
                    replacing++;
                }
            }
        }
    }
    
    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::UploadFiles);
    connect(monitor, SIGNAL(Progress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)), SLOT(OnUploadOperationProgress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));
    
    // Nothing will be replaced, continue right away.
    if (replacing == 0)
    {
        state_.highlightKeys.clear();
        
        QString lastDir = "";
        foreach(QString filepath, files)
        {
            QFile file(filepath);
            if (!file.exists())
                continue;
            QFileInfo fileInfo(filepath);

            if (fileInfo.fileName().contains(" "))
            {
                plugin_->Notifications()->ShowSplashDialog("Spaces are not allowed in filenames. Found in filename: \"" + fileInfo.fileName() + "\", full path: \"" + fileInfo.absoluteFilePath() + "\"");
                SAFE_DELETE(monitor);
                return monitor;
            }
            // Store the used folder path to config.
            if (lastDir.isEmpty())
            {
                lastDir = fileInfo.dir().path();
                framework_->Config()->Write("adminotech", "ast", "upload-folder", lastDir);
            }

            QS3FileMetadata metadata;
            metadata.contentType = ContentMimeType(fileInfo.suffix(), fileInfo.completeSuffix());

            QString key = state_.currentFolder->data.key;
            if (!key.endsWith("/"))
                key.append("/");
            key += fileInfo.fileName();
            
            state_.highlightKeys << key;
            monitor->AddOperation(s3_->put(key, &file, metadata, QS3::PublicRead), fileInfo.size());
        }

        if (monitor->total == 0)
            SAFE_DELETE(monitor)
        else
            state_.folderOperationOngoing = true;
    }
    // Replacing files. Do a confirmation.
    else
    {
        QString confirmMessage = "<pre>";
        confirmMessage += replaceFilenames.join("<br>");
        if (replacingNotMentioned > 0)
            confirmMessage += "<br>" + QString("... and %1 ").arg(replacingNotMentioned) + (replacingNotMentioned == 1 ? "other" : "others");
        confirmMessage += "</pre>";

        state_.infoDialog = new RocketStorageInfoDialog(plugin_, QString("Overwrite %1 existing %2?").arg(replacing).arg((replacing == 1 ? "file" : "files")), monitor);
        connect(state_.infoDialog, SIGNAL(AcceptClicked()), SLOT(OnUploadRequestContinue()));
        connect(state_.infoDialog, SIGNAL(Rejected()), SLOT(DestroyInfoDialog()));

        state_.infoDialog->names = files;
        state_.infoDialog->ui.buttonOk->setText("Overwrite");
        state_.infoDialog->ui.labelText->setStyleSheet("padding-left: 15px;");
        state_.infoDialog->EnableTextMode(confirmMessage);
        state_.infoDialog->OpenDelayed(100);
    }
    
    return monitor;
}

MeshmoonStorageOperationMonitor *MeshmoonStorage::UploadData(const MeshmoonStorageItem &file, const QByteArray &data)
{
    if (!file.isFile)
    {
        LogError(LC + "Passed in MeshmoonStorageItem is not a file.");
        return 0;
    }
    if (!s3_)
    {
        LogError(LC + "Cannot start uploads. Storage client not opened.");
        return 0;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start upload. No destination folder selected.");
        return 0; 
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot start new file uploads right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return 0;
    }
    if (state_.infoDialog)
        return 0;
        
    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::UploadFiles);
    connect(monitor, SIGNAL(Progress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)), SLOT(OnUploadOperationProgress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));
    
    state_.highlightKeys.clear();
    state_.highlightKeys << file.path;

    QFileInfo fileInfo(file.path);
    QS3FileMetadata metadata;
    metadata.contentType = ContentMimeType(fileInfo.suffix(), fileInfo.completeSuffix());

    monitor->AddOperation(s3_->put(file.path, data, metadata, QS3::PublicRead), data.size());

    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
        
    return monitor;
}

void MeshmoonStorage::OnUploadRequestContinue()
{
    if (!state_.infoDialog)
        return;

    MeshmoonStorageOperationMonitor *monitor = state_.infoDialog->monitor;
    if (!monitor)
    {
        LogError(LC + "OnUploadRequestContinue: Cannot continue, internal operation monitor is null!");
        state_.infoDialog->Close();
        DestroyInfoDialog();
        return;
    }

    state_.highlightKeys.clear();
    
    QString lastDir = "";
    foreach(QString filepath, state_.infoDialog->names)
    {
        QFile file(filepath);
        if (!file.exists())
            continue;
        QFileInfo fileInfo(filepath);

        // Store the used folder path to config.
        if (lastDir.isEmpty())
        {
            lastDir = fileInfo.dir().path();
            framework_->Config()->Write("adminotech", "ast", "upload-folder", lastDir);
        }

        QS3FileMetadata metadata;
        metadata.contentType = ContentMimeType(fileInfo.suffix(), fileInfo.completeSuffix());

        QString key = state_.currentFolder->data.key;
        if (!key.endsWith("/"))
            key.append("/");
        key += fileInfo.fileName();

        state_.highlightKeys << key;
        monitor->AddOperation(s3_->put(key, &file, metadata, QS3::PublicRead), fileInfo.size());
    }

    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
        
    state_.infoDialog->Close();
    DestroyInfoDialog();
}

void MeshmoonStorage::OnUploadOperationProgress(QS3PutObjectResponse *response, qint64 complete, qint64 total, MeshmoonStorageOperationMonitor *operation)
{
    if (response && !response->succeeded)
        LogError(LC + "File upload failed: " + response->key + " Error description: " + response->error.toString());
        
    if (storageWidget_->ui.labelProgressTitle->text().isEmpty())
        storageWidget_->ui.labelProgressTitle->setText("Uploading");
    storageWidget_->SetProgress(complete, total);
    
    // Mark down assets that we want to auto reload on server and all clients.
    if (response && operation && state_.storageRoot)
    {
        QString relativeRef = RelativeRefForKey(response->key);

        // Try with relative ref.
        bool clientNeeds = false;
        AssetPtr loadedAsset = framework_->Asset()->GetAsset(relativeRef);        
        if (loadedAsset.get() && loadedAsset->IsLoaded())
        {
            if (!operation->changedAssetRefs.contains(loadedAsset->Name()))
            {
                operation->changedAssetRefs << loadedAsset->Name();
                clientNeeds = true;
            }
        }
        else if (!loadedAsset.get())
        {
            // Try full url based on the default storage.
            // This will hit if user is using assets from his storage but
            // not using relative refs but full urls.
            AssetStoragePtr storage = framework_->Asset()->GetDefaultAssetStorage();
            if (storage.get())
            {
                QString storageFullRef = storage->GetFullAssetURL(relativeRef);
                if (storageFullRef.startsWith("http"))
                {
                    loadedAsset = framework_->Asset()->GetAsset(storageFullRef); 
                    if (loadedAsset.get() && loadedAsset->IsLoaded())
                    {
                        if (!operation->changedAssetRefs.contains(loadedAsset->Name()))
                        {
                            operation->changedAssetRefs << loadedAsset->Name();
                            clientNeeds = true;
                        }
                    }
                }
            }
        }

        // If this is a certain type of asset eg. script that the server might be using
        // without it being loaded to the client asset system. Inform the server about the upload completion.
        // This is nice when developing scripts with Server only execution, to make server reload it once you hit save
        // in the rocket editor - Do we have other situations where this might be needed?
        if (!clientNeeds && (relativeRef.endsWith(".js", Qt::CaseInsensitive) || relativeRef.endsWith(".webrocketjs", Qt::CaseInsensitive)))
            operation->changedAssetRefs << framework_->Asset()->ResolveAssetRef("", relativeRef);
    }
}

void MeshmoonStorage::OnEditorOpened(IRocketAssetEditor *editor)
{
    if (!s3_ || !editor)
        return;

    foreach(IRocketAssetEditor *existing, state_.editors)
    {
        if (existing && existing->StorageKey() == editor->StorageKey())
        {
            // Activate the existing editor
            if (existing->isMinimized())
                existing->showNormal();

            existing->setFocus(Qt::ActiveWindowFocusReason);
            existing->activateWindow();
            
            // Delete the created editor.
            if (editor != existing)
            {
                editor->close();
                SAFE_DELETE(editor);
            }
            return;
        }
    }

    // Hook editor signals.
    connect(editor, SIGNAL(SaveRequest(IRocketAssetEditor*, const QByteArray&)), this, SLOT(OnEditorSaveRequest(IRocketAssetEditor*, const QByteArray&)));
    connect(editor, SIGNAL(CloseRequest(IRocketAssetEditor*)), this, SLOT(OnEditorCloseRequest(IRocketAssetEditor*)), Qt::QueuedConnection); 

    // Start and show.
    editor->Start(s3_);
    editor->Show();

    state_.editors << editor;
}

void MeshmoonStorage::OnEditorSaveRequest(IRocketAssetEditor *editor, const QByteArray &data)
{
    if (!editor)
        return;

    if (!s3_)
    {
        editor->SetError("Cannot upload editor resource. Storage client is not valid anymore.");
        return;
    }
    if (!state_.currentFolder)
    {
        editor->SetError("Cannot upload editor resource. No destination folder selected.");
        return; 
    }
    if (state_.folderOperationOngoing)
    {
        editor->SetError("Cannot upload editor resource right now. Try again once ongoing operations are finished.");
        return;
    }
    if (state_.infoDialog)
    {
        editor->SetError("Cannot upload editor resource right now. Handle the blocking dialogs in the storage view first.");
        return;
    }
    if (editor->StorageKey().trimmed().isEmpty())
    {
        editor->SetError("Storage information missing, cannot save.");
        return;
    }

    editor->ClearMessage();

    MeshmoonStorageOperationMonitor *upload = UploadData(MeshmoonStorageItem(editor->StorageKey(), this), data);
    if (upload)
    {
        connect(upload, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), 
            editor, SLOT(OnResourceSaveFinished(MeshmoonStorageOperationMonitor*)));
        connect(upload, SIGNAL(Progress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)), 
            editor, SLOT(OnResourceSaveProgress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)));
    }
    else
        editor->SetError("Failed to startup upload");
}

void MeshmoonStorage::OnEditorCloseRequest(IRocketAssetEditor *editor)
{
    if (!editor)
        return;

    for (int i=0; i<state_.editors.size(); ++i)
    {
        if (state_.editors[i] && state_.editors[i] == editor)
        {
            state_.editors.removeAt(i);
            break;
        }
    }

    if (!editor->testAttribute(Qt::WA_DeleteOnClose))
        editor->deleteLater();
    editor->close();

    // If we are waiting for all editors to close and this was the last one, close storage.
    // Note that forcefully closed dialogs may be still in the list but are null.
    if (state_.closeOnEditorsClosed)
    {
        int numOpen = 0;
        for (int i=0; i<state_.editors.size(); ++i)
            if (state_.editors[i]) numOpen++;
        if (numOpen == 0)
            Close();
    }
    
}

void MeshmoonStorage::OnInstantiateRequest(MeshmoonStorageItemWidget *item)
{
    if (!item)
        return;
        
    Scene *scene = framework_->Renderer()->MainCameraScene();
    if (!scene)
        return;
    Entity *cameraEnt = framework_->Renderer()->MainCamera();
    if (!cameraEnt)
        return;
        
    QString relativeRef = RelativeRefForKey(item->data.key);
    if (relativeRef.isEmpty())
        return;
        
    EntityPtr ent;
        
    QStringList components;
    if (item->suffix == "mesh" || item->suffix == "obj" || item->suffix == "dae" || item->suffix == "ply")
    {
        components << "EC_Name" << "EC_Placeable" << "EC_Mesh";
        ent = scene->CreateEntity(scene->NextFreeId(), components, AttributeChange::Replicate, true, true);
        if (ent)
        {
            EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(ent->GetComponent("EC_Mesh").get());
            if (mesh)
                mesh->meshRef.Set(AssetReference(relativeRef), AttributeChange::Replicate);
            else
                LogError(LC + "Failed to create instance of " + relativeRef + ". Could not create mesh component.");
        }
        else
            LogError(LC + "Failed to create instance of " + relativeRef + ". Could not create target entity.");
    }
    else if (item->suffix == "txml")
    {
        LogWarning(LC + "Instantiating from txml not implemented!");
    }
    else if (item->suffix == "js")
    {
        components << "EC_Name" << "EC_Script";
        ent = scene->CreateEntity(scene->NextFreeId(), components, AttributeChange::Replicate, true, true);
        if (ent)
        {
            EC_Script *script = dynamic_cast<EC_Script*>(ent->GetComponent("EC_Script").get());
            if (script)
            {
                script->runOnLoad.Set(true, AttributeChange::Replicate);
                AssetReferenceList refs = script->scriptRef.Get();
                refs.Append(AssetReference(relativeRef));
                script->scriptRef.Set(refs, AttributeChange::Replicate);
            }
            else
                LogError(LC + "Failed to create instance of " + relativeRef + ". Could not create script component.");
        }
        else
            LogError(LC + "Failed to create instance of " + relativeRef + ". Could not create target entity.");
    }

    if (ent)
    {
        // Set name
        EC_Name *entName = dynamic_cast<EC_Name*>(ent->GetComponent("EC_Name").get());
        if (entName)
        {
            QString objName = item->filename;
            if (objName.contains("."))
                objName = objName.left(objName.lastIndexOf("."));
            entName->name.Set(objName, AttributeChange::Replicate);
        }

        // Position entity
        EC_Placeable *placeable = dynamic_cast<EC_Placeable*>(ent->GetComponent("EC_Placeable").get());
        EC_Placeable *cameraPlaceable = dynamic_cast<EC_Placeable*>(cameraEnt->GetComponent("EC_Placeable").get());
        if (placeable && cameraPlaceable)
        {
            float3 dir = cameraPlaceable->WorldOrientation() * scene->ForwardVector();
            float3 worldPos = cameraPlaceable->WorldPosition() + dir * 10;
            placeable->SetPosition(worldPos);
        }
    }
}

void MeshmoonStorage::OnRestoreBackupRequest(MeshmoonStorageItemWidget *txmlItem)
{
    if (!txmlItem)
        return;
        
    QString txmlKey = txmlItem->data.key;
    if (txmlKey.trimmed().isEmpty() || !txmlKey.toLower().endsWith(".txml"))
    {
        LogError(LC + "OnRestoreBackupRequest called with a non .txml MeshmoonStorageItemWidget");
        return;
    }

    /// @todo Don't backup the current storage txml but instead upload the current state from the server, how to do this!?
    /// Note that we cannot upload the scene state from client as server might have localonly entities (eg. scripts might use this!)
    
    /*QMessageBox::StandardButton backupResponse = QMessageBox::question(framework_->Ui()->MainWindow(), "Backup world state", 
        "Would you like to backup the current world state before restoring world?", QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);
    if (backupResponse == QMessageBox::Yes)
    {
        QString backupName = QInputDialog::getText(framework_->Ui()->MainWindow(), "Backup name", QString("<b>Before the backup is restored the current world state will be backed up</b><br>") +
            "The backup filename will automatically contain a date and timestamp. Please provide a name for the backup:", 
            QLineEdit::Normal, "pre backup restore").trimmed();
        if (backupName.isEmpty())
            backupName = "pre backup restore";
            
        QS3CopyObjectResponse *response = BackupTxml(true, backupName);
        response->setProperty("txmlkey", txmlKey);
        connect(response, SIGNAL(finished(QS3CopyObjectResponse*)), SLOT(OnPreRestoreBackupCompleted(QS3CopyObjectResponse*)));
    }
    else*/
        OnContinueRestoreBackupRequest(txmlKey);
}

void MeshmoonStorage::OnPreRestoreBackupCompleted(QS3CopyObjectResponse *response)
{
    if (response && response->succeeded)
        OnContinueRestoreBackupRequest(response->property("txmlkey").toString());
    else
        plugin_->Notifications()->ShowSplashDialog("Failed to backup current world state before restoring world backup. Aborting restore operation.", ":/images/icon-update.png");
}

void MeshmoonStorage::OnContinueRestoreBackupRequest(const QString &txmlKey)
{
    if (txmlKey.trimmed().isEmpty() || !txmlKey.toLower().endsWith(".txml"))
    {
        LogError(LC + "OnContinueRestoreBackupRequest called with a non .txml url");
        return;
    }

    if (!s3_)
    {
        LogError(LC + "Cannot start backup restore. Storage client is null.");
        return;
    }

    QMessageBox::StandardButton confirmationResponse = QMessageBox::warning(framework_->Ui()->MainWindow(), "Restore confirmation", 
        QString("Are you sure you want to continue restoring the <b>%1</b> backup?").arg(txmlKey.split("/", QString::SkipEmptyParts).last()), QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    if (confirmationResponse != QMessageBox::Yes)
        return;

    QS3GetObjectResponse *response = s3_->get(txmlKey);
    connect(response, SIGNAL(finished(QS3GetObjectResponse*)), SLOT(OnRestoreBackupDownloaded(QS3GetObjectResponse*)));
}

void MeshmoonStorage::OnRestoreBackupDownloaded(QS3GetObjectResponse *response)
{
    if (!response || !response->succeeded || response->data.isEmpty())
    {
        plugin_->Notifications()->ShowSplashDialog("Failed to download backup world state. Aborting restore operation.", ":/images/icon-update.png");
        return;
    }

    Scene *activeScene = framework_->Scene()->MainCameraScene();
    if (!activeScene)
        return;

    // Load the backup data
    QTextStream txmlStream(response->data);
    txmlStream.setCodec("UTF-8");
    txmlStream.setAutoDetectUnicode(true);

    QDomDocument sceneDocument("Scene");
    QString errorMsg; int errorLine, errorColumn;
    if (!sceneDocument.setContent(txmlStream.readAll(), &errorMsg, &errorLine, &errorColumn))
    {
        plugin_->Notifications()->ShowSplashDialog(QString("Backup world state XML parsing error: %1 at line %2 column %3").arg(errorMsg).arg(errorLine).arg(errorColumn), ":/images/icon-update.png");
        return;
    }

    // Remove replicated non-temporary entities from the scene
    uint preservedEnts = 0;
    QList<entity_id_t> removeIds;
    QSet<QString> oldAssetRefs;
    
    Scene::EntityMap entities = activeScene->Entities();
    for (Scene::EntityMap::const_iterator iter = entities.begin(); iter != entities.end(); ++iter)
    {
        EntityPtr ent = iter->second;
        if (!ent.get())
            continue;
        if (ent->IsReplicated() && !ent->IsTemporary())
        {
            /** Track all assets from entities we are going to remove. We can unload all those assets before loading the new content.
                If we don't do this, the client will crash in big scene if it loads "duplicate" assets. This happens if you load a non-optimized 
                scene when you currently have a optimized scene. 
                
                @note This is not a complete list of components that load assets. We are trying to find and unload the big spenders! 
            */
            EC_Mesh *meshComp = ent->Component<EC_Mesh>().get();
            if (meshComp)
            {
                // Unload meshes
                const QString &meshRef = meshComp->meshRef.Get().ref;
                if (!meshRef.trimmed().isEmpty())
                    oldAssetRefs << meshRef;
                    
                // Unload materials
                const AssetReferenceList &materials = meshComp->materialRefs.Get();
                for (int iMat=0; iMat<materials.Size(); ++iMat)
                {
                    QString matRef = materials[iMat].ref;
                    if (!matRef.trimmed().isEmpty())
                    {
                        OgreMaterialAsset *materialAsset = dynamic_cast<OgreMaterialAsset*>(framework_->Asset()->GetAsset(matRef).get());
                        if (materialAsset)
                        {
                            oldAssetRefs << matRef;
                        
                            // Unload material textures    
                            for (std::vector<AssetReference>::const_iterator iter = materialAsset->references_.begin(); iter != materialAsset->references_.end(); ++iter)
                            {
                                QString texRef = (*iter).ref;
                                if (!texRef.trimmed().isEmpty())
                                    oldAssetRefs << texRef;
                            }
                        }
                    }
                }
            }
            EC_Material *materialComp = ent->Component<EC_Material>().get();
            if (materialComp)
            {
                // Input mat
                QString inputMatRef = materialComp->inputMat.Get();
                if (!inputMatRef.trimmed().isEmpty())
                {
                    OgreMaterialAsset *materialAsset = dynamic_cast<OgreMaterialAsset*>(framework_->Asset()->GetAsset(inputMatRef).get());
                    if (materialAsset)
                    {
                        oldAssetRefs << inputMatRef;

                        // Unload material textures    
                        for (std::vector<AssetReference>::const_iterator iter = materialAsset->references_.begin(); iter != materialAsset->references_.end(); ++iter)
                        {
                            QString texRef = (*iter).ref;
                            if (!texRef.trimmed().isEmpty())
                                oldAssetRefs << texRef;
                        }
                    }
                }

                // Output mat
                QString outputMatRef = materialComp->outputMat.Get();
                if (!outputMatRef.trimmed().isEmpty())
                {
                    OgreMaterialAsset *materialAsset = dynamic_cast<OgreMaterialAsset*>(framework_->Asset()->GetAsset(outputMatRef).get());
                    if (materialAsset)
                    {
                        oldAssetRefs << outputMatRef;

                        // Unload material textures    
                        for (std::vector<AssetReference>::const_iterator iter = materialAsset->references_.begin(); iter != materialAsset->references_.end(); ++iter)
                        {
                            QString texRef = (*iter).ref;
                            if (!texRef.trimmed().isEmpty())
                                oldAssetRefs << texRef;
                        }
                    }
                }
            }

            // Mark this entity for removal
            removeIds << ent->Id();
        }
        else
            preservedEnts++;
    }
    entities.clear();
    foreach(entity_id_t removeId, removeIds)
        activeScene->RemoveEntity(removeId, AttributeChange::Replicate);
        
    // Unload assets that were used by the removed entities
    foreach(const QString &oldAssetRef, oldAssetRefs)
        framework_->Asset()->ForgetAsset(oldAssetRef, false);

    // Create entities from backup
    QList<Entity*> createdEnts = activeScene->CreateContentFromXml(sceneDocument, false, AttributeChange::Replicate);
    
    QString message = "<b>World restore from backup completed</b><pre>";
    if (!removeIds.isEmpty())
        message += "Removed " + QString::number(removeIds.size()) + " Entities from current state<br>";
    message += "Created " + QString::number(createdEnts.size()) + " new Entities from the backup";
    if (preservedEnts > 0)
        message += "<br>Preserved " + QString::number(preservedEnts) + " replicated temporary Entities";
    if (oldAssetRefs.size() > 0)
        message += "<br>Unloaded " + QString::number(oldAssetRefs.size()) + " old assets";
    message += "</pre>";
    
    plugin_->Notifications()->ShowSplashDialog(message, ":/images/icon-update.png");
}

void MeshmoonStorage::OnDeleteRequest(MeshmoonStorageItemWidget *item)
{
    MeshmoonStorageItemWidgetList files;
    files << item;
    OnDeleteRequest(files);
}

MeshmoonStorageItemWidgetList MeshmoonStorage::GetAllSubfiles(MeshmoonStorageItemWidget *item)
{
    MeshmoonStorageItemWidgetList list;
    foreach(MeshmoonStorageItemWidget * item, item->children)
    {
        if (item->data.isDir)
        {
            list.append(GetAllSubfiles(item));
            list << item;
        }
        else
            list << item;
    }

    return list;
}

MeshmoonStorageItemWidgetList MeshmoonStorage::GetAllParents(MeshmoonStorageItemWidget * item)
{
    MeshmoonStorageItemWidgetList parents;
    MeshmoonStorageItemWidget * parent = item->parent;
    while(parent)
    {
        parents << parent;
        parent = parent->parent;
    }

    return parents;
}

void MeshmoonStorage::OnDeleteRequest(MeshmoonStorageItemWidgetList files)
{
    if (!s3_)
    {
        LogError(LC + "Cannot start file removal. Storage client is null.");
        return;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start file removal. No folder selected.");
        return; 
    }
    if (state_.folderOperationOngoing)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot delete files right now</b><br>Wait for current operation to finish", ":/images/icon-update.png");
        return;
    }
    if (state_.infoDialog)
        return;
    if (files.isEmpty())
        return;
    if (state_.currentFolder->data.key.contains("/backup/"))
    {
        plugin_->Notifications()->ShowSplashDialog("<b>Cannot remove protected backup files</b>", ":/images/icon-update.png");
        return;
    }

    int acceptedFiles = 0;
    int acceptedButNotMentioned = 0;
    int acceptedFolders = 0;
    QStringList rejectedFiles;
    MeshmoonStorageItemWidgetList rejectedFolders;
    QStringList rejectedFolderNames;
    QStringList rejectedExtensions;

    // Spin it once to get all the subfiles
    MeshmoonStorageItemWidgetList subfiles;
    MeshmoonStorageItemWidgetList candidateFolders;
    foreach(MeshmoonStorageItemWidget *file, files)
    {
        if (!file || !file->data.isDir || file->filename.isEmpty())
            continue;
        if (file->data.key.contains("/backup/"))
        {
            rejectedFolders << file;
            rejectedFolderNames << file->filename;
            continue;
        }

        candidateFolders << file;
        subfiles.append(GetAllSubfiles(file));
        subfiles << file;
    }

    /* Remove candidate folders from the final list, since files must be first removed and
        all subfolders and their first before the candidate folder is removed */
    foreach(MeshmoonStorageItemWidget * file, candidateFolders)
        files.removeOne(file);
    foreach(MeshmoonStorageItemWidget * file, rejectedFolders)
        files.removeOne(file);

    QString confirmMessage = "<pre>";
    foreach(MeshmoonStorageItemWidget *file, files)
    {
        if (file->data.isDir)
            continue;

        bool acceptedFile = true;
        foreach(const QString &protectedExt, state_.protectedExtensions)
        {
            if (file->data.key.endsWith(protectedExt, Qt::CaseInsensitive))
            {
                if (!rejectedExtensions.contains(protectedExt))
                    rejectedExtensions << protectedExt;

                acceptedFile = false;
                rejectedFiles << file->filename;
                break;
            }
        }
        if (acceptedFile)
        {
            if (acceptedFiles < 10)
                confirmMessage += file->filename + "<br>";
            else
                acceptedButNotMentioned++;
            acceptedFiles++;
        }
    }

    /* Also check if some of the subfiles are protected
       If not, remove all parent folders which are candidates for deletion */
    foreach(MeshmoonStorageItemWidget *item, subfiles)
    {
        if (item->data.isDir)
            continue;

        foreach(const QString &protectedExt, state_.protectedExtensions)
        {
            if (item->data.key.endsWith(protectedExt, Qt::CaseInsensitive))
            {
                if (!rejectedExtensions.contains(protectedExt))
                    rejectedExtensions << protectedExt;

                rejectedFiles << item->filename;
                rejectedFolders.append(GetAllParents(item));
            }
        }
    }

    foreach(MeshmoonStorageItemWidget * item, rejectedFolders.toSet())
    {
        if (candidateFolders.removeOne(item))
            rejectedFolderNames << item->filename;
        subfiles.removeOne(item);
    }

    /* If there is only one folder selected that contains both protected and unprotected types,
       put that folder into consideration so we get a proper dialog */
    if (!subfiles.isEmpty() && candidateFolders.isEmpty() && rejectedFolderNames.size() == 1)
        acceptedFolders = 1;
    else
        acceptedFolders = candidateFolders.size();


    if (acceptedButNotMentioned > 0)
        confirmMessage += QString("... and %1 ").arg(acceptedButNotMentioned) + (acceptedButNotMentioned == 1 ? "other" : "others") + "<br>";

    if (confirmMessage.endsWith("<br>"))
        confirmMessage.chop(4);
    confirmMessage += "</pre>";

    if (candidateFolders.size() > 0)
    {
        acceptedButNotMentioned = candidateFolders.size() - 10;
        confirmMessage += QString("<b>The following folder(s) and all of its subitems will be removed</b><span><pre>");
        for (int i = 0; i < candidateFolders.size(); ++i)
        {
            if (i == 10) break;
            confirmMessage += candidateFolders.at(i)->filename + "<br>";
        }

        if (acceptedButNotMentioned > 0)
            confirmMessage += QString("... and %1 ").arg(acceptedButNotMentioned) + (acceptedButNotMentioned == 1 ? "other" : "others") + "<br>";
    }

    if (confirmMessage.endsWith("<br>"))
        confirmMessage.chop(4);
    confirmMessage += "</pre></span>";

    files.append(subfiles);

    if (acceptedFiles == 0 && acceptedFolders == 0)
    {
        plugin_->Notifications()->ShowSplashDialog("<b>You selection contains only protected file types</b><span style='color:red;'><pre>  " + 
            rejectedExtensions.join("<br>  ") + "</pre></span>", ":/images/icon-update.png");
        return;
    }
    if (!rejectedFiles.isEmpty())
        confirmMessage += QString("<b>Not removing %1 %2 with protected file types</b><span style='color:red;'><pre>  ").arg(rejectedFiles.size()).arg((rejectedFiles.size() == 1 ? "file" : "files")) + 
            rejectedExtensions.join("<br>  ") + "</pre></span>";

    if (!rejectedFolderNames.isEmpty())
        confirmMessage += QString("<b>The following %1 %2 contain protected file types and will NOT be completely removed</b><span style='color:red;'><pre> ").arg(rejectedFolderNames.size()).arg((rejectedFolderNames.size() == 1 ? "folder" : "folders")) +
            rejectedFolderNames.join("<br> ") + "</pre></span>";

    QString message;
    message += QString("Delete confirmation for");
    message += (acceptedFiles > 0 ? QString(" %1 ").arg(acceptedFiles) : QString());
    message += (acceptedFiles != 0 ? (acceptedFiles == 1 ? "file " : "files ") : QString());
    message += (acceptedFiles != 0 && acceptedFolders != 0 ? QString("and") : QString());
    message += (acceptedFolders > 0 ? QString(" %1 ").arg(acceptedFolders) : QString());
    message += (acceptedFolders != 0 ? (acceptedFolders == 1 ? "folder" : "folders") : QString());

    state_.infoDialog = new RocketStorageInfoDialog(plugin_, message);
    connect(state_.infoDialog, SIGNAL(AcceptClicked()), SLOT(OnDeleteRequestContinue()));
    connect(state_.infoDialog, SIGNAL(Rejected()), SLOT(DestroyInfoDialog()));
 
    state_.infoDialog->items = files;
    state_.infoDialog->ui.buttonOk->setText("Delete");
    state_.infoDialog->ui.labelText->setStyleSheet("padding-left: 15px;");
    state_.infoDialog->EnableTextMode(confirmMessage);
    state_.infoDialog->OpenDelayed(100);
}
    
void MeshmoonStorage::OnDeleteRequestContinue()
{
    if (!state_.infoDialog)
        return;

    MeshmoonStorageOperationMonitor *monitor = new MeshmoonStorageOperationMonitor(MeshmoonStorageOperationMonitor::DeleteFiles);
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnOperationFinished(MeshmoonStorageOperationMonitor*)));
    
    foreach(MeshmoonStorageItemWidget *file, state_.infoDialog->items)
    {
        bool acceptedFile = true;
        foreach(const QString &protectedExt, state_.protectedExtensions)
        {
            if (file->data.key.endsWith(protectedExt, Qt::CaseInsensitive))
            {
                acceptedFile = false;
                break;
            }
        }
        if (acceptedFile)
            monitor->AddOperation(s3_->remove(file->data.key));
    }
    
    if (monitor->total == 0)
        SAFE_DELETE(monitor)
    else
        state_.folderOperationOngoing = true;
    
    state_.infoDialog->Close();
    DestroyInfoDialog();
}

void MeshmoonStorage::OnCreateFolderRequest()
{
    if (!s3_)
    {
        LogError(LC + "Cannot start creating folder. Storage client is null.");
        return;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start creating folder. No folder selected.");
        return; 
    }
    if (state_.infoDialog)
        return;
    
    state_.infoDialog = new RocketStorageInfoDialog(plugin_, "Create new folder");
    connect(state_.infoDialog, SIGNAL(AcceptClicked()), SLOT(OnCreateFolderRequestContinue()));
    connect(state_.infoDialog, SIGNAL(Rejected()), SLOT(DestroyInfoDialog()));
    
    state_.infoDialog->EnableInputMode("", "");
    state_.infoDialog->ui.buttonOk->setText("Create");
    state_.infoDialog->Open();
}

void MeshmoonStorage::OnCreateFolderRequestContinue()
{
    QString folderName = ValidateInfoDialogFilename(true);
    if (folderName.isEmpty())
        return;
    
    // Construct new key and create
    QString folderFullKey = state_.currentFolder->data.key;
    if (!folderFullKey.endsWith("/"))
    {
        state_.infoDialog->ui.labelError->setText("Unkown error");
        return;
    }
    folderFullKey += folderName + "/";

    QS3PutObjectResponse *response = s3_->createFolder(folderFullKey, QS3::PublicRead);
    if (response)
        connect(response, SIGNAL(finished(QS3PutObjectResponse*)), SLOT(RefreshCurrentFolderContent()));

    state_.infoDialog->Close();
    DestroyInfoDialog();
}

void MeshmoonStorage::OnUploadFilesRequest()
{
    OnUploadRequest(true);
}

bool MeshmoonStorage::FileExistsInCurrentFolder(const QString &filename, Qt::CaseSensitivity sensitivity)
{
    // If no current folder, lets say it exists to trigger error situations if not already detected!
    if (!state_.currentFolder)
    {
        LogError(LC + "FileExistsInCurrentFolder no current folder selected!");
        return true;
    }

    foreach(MeshmoonStorageItemWidget *existing, state_.currentFolder->FoldersAndFiles())
        if (existing && existing->filename.compare(filename, sensitivity) == 0)
            return true;
    return false;
}

void MeshmoonStorage::OnCreateNewFileRequest(const QString &suffix)
{
    if (!suffix.startsWith("."))
    {
        LogError(LC + "Cannot start creating new file. Input suffix does not start with a dot: " + suffix);
        return;
    }
    if (!s3_)
    {
        LogError(LC + "Cannot start creating new file. Storage client is null.");
        return;
    }
    if (!state_.currentFolder)
    {
        LogError(LC + "Cannot start creating new file. No folder selected.");
        return; 
    }
    if (state_.infoDialog)
        return;

    state_.infoDialog = new RocketStorageInfoDialog(plugin_, "Create new file");
    connect(state_.infoDialog, SIGNAL(AcceptClicked()), SLOT(OnCreateNewFileRequestContinue()));
    connect(state_.infoDialog, SIGNAL(Rejected()), SLOT(DestroyInfoDialog()));

    state_.infoDialog->EnableInputMode("", "newfile");
    state_.infoDialog->ui.buttonOk->setText("Create");
    state_.infoDialog->setProperty("createNewFileSuffix", suffix);
    state_.infoDialog->Open();
}

void MeshmoonStorage::OnCreateNewFileRequestContinue()
{
    if (!state_.infoDialog || !state_.currentFolder)
        return;

    QString requestSuffix = state_.infoDialog->property("createNewFileSuffix").toString();
    if (requestSuffix.isEmpty())
        return;

    QString filename = ValidateInfoDialogFilename(true, requestSuffix);
    if (filename.isEmpty())
        return;

    MeshmoonStorageItem destinationItem = CurrentDirectory().GenerateChildFileItem(filename);
    if (destinationItem.IsNull())
    {
        LogError(LC + "Failed to create MeshmoonStorageItem from current folder with " + filename);
        return;
    }

    // @note Our S3 client wont allow to upload 0 byte files, needs to have something!
    QByteArray data;
    {
        QTextStream stream(&data);
        if (requestSuffix.compare(".material", Qt::CaseInsensitive) == 0)
        {
            stream << "material " << AssetAPI::SanitateAssetRef(destinationItem.fullAssetRef) << endl
                   << "{" << endl
                   << "    technique" << endl
                   << "    {" << endl
                   << "        pass" << endl
                   << "        {" << endl
                   << "            vertex_program_ref meshmoon/DiffuseVP" << endl
                   << "            {" << endl
                   << "            }" << endl
                   << "            fragment_program_ref meshmoon/DiffuseFP" << endl
                   << "            {" << endl
                   << "            }" << endl << endl
                   << "            texture_unit diffuseMap" << endl
                   << "            {" << endl
                   << "            }" << endl
                   << "        }" << endl
                   << "    }" << endl
                   << "}" << endl;
        }
        else if (requestSuffix.compare(".particle", Qt::CaseInsensitive) == 0)
        {
            stream << "particle_system " << AssetAPI::SanitateAssetRef(destinationItem.fullAssetRef) << endl
                   << "{" << endl
                   << "}" << endl;
        }
        else if (requestSuffix.compare(".js", Qt::CaseInsensitive) == 0 || requestSuffix.compare(".webrocketjs", Qt::CaseInsensitive) == 0)
        {
            stream << "// Script " << destinationItem.filename << endl;
        }
        else if (requestSuffix.compare(".json", Qt::CaseInsensitive) == 0)
        {
            stream << "{" << endl
                   << "}" << endl;
        }
        else if (requestSuffix.compare(".txt", Qt::CaseInsensitive) == 0)
        {
            stream << " " << endl;
        }
        stream.flush();
    }

    if (!data.isEmpty())
    {
        state_.infoDialog->Close();
        DestroyInfoDialog();

        UploadData(destinationItem, data);
    }
    else
    {
        state_.infoDialog->ui.labelError->setText("Failed to create default file content for file type " + requestSuffix);
        LogError(LC + QString("Cannot create file %1. No default file content could be created for file type %2").arg(destinationItem.filename).arg(requestSuffix));    
    }
}

void MeshmoonStorage::DestroyInfoDialog()
{
    /// @todo Potential memory leak if a monitor has been set to this dialog and cancel was pressed!
    SAFE_DELETE(state_.infoDialog);
}

QString MeshmoonStorage::ValidateInfoDialogFilename(bool errorIfFileExists, const QString &mustHaveSuffix)
{
    if (!mustHaveSuffix.isEmpty() && mustHaveSuffix.endsWith(".", Qt::CaseInsensitive))
    {
        LogError(LC + "ValidateInfoDialogFilename mustHaveSuffix does not start with a dot: " + mustHaveSuffix);
        return "";
    }
    if (!state_.infoDialog)
        return "";

    QString inputName = state_.infoDialog->ui.lineEditInput->text().trimmed();
    if (inputName.isEmpty())
    {
        state_.infoDialog->ui.labelError->setText("No name entered");
        return "";
    }
    if (inputName.contains(" "))
    {
        state_.infoDialog->ui.labelError->setText("Spaces not allowed");
        return "";
    }
    // Check bad characters. This is not a complete list, should prolly do a regexp for aA-zZ 0-9
    if (inputName.contains("/") || inputName.contains("\\") || inputName.contains("%") ||
        inputName.contains("?") || inputName.contains("&") || inputName.contains("@") ||
        inputName.contains("$") || inputName.contains("#") || inputName.contains("(") ||
        inputName.contains(")") || inputName.contains("{") || inputName.contains("}"))
    {
        state_.infoDialog->ui.labelError->setText("/ \\ % ? & @ $ # ( ) { } not allowed");
        return "";
    }

    if (!mustHaveSuffix.isEmpty() && !inputName.endsWith(mustHaveSuffix, Qt::CaseInsensitive))
    {
        // If there is no suffix in the name, append it automatically without an error.
        if (!inputName.endsWith(".") && QFileInfo(inputName).suffix().isEmpty())
            inputName = inputName + mustHaveSuffix;
        // If there is a suffix it must match, report error.
        else
        {
            state_.infoDialog->ui.labelError->setText("Name must end with " + mustHaveSuffix);
            return "";
        }
    }

    if (errorIfFileExists && FileExistsInCurrentFolder(inputName))
    {
        state_.infoDialog->ui.labelError->setText(inputName + " already exists");
        return "";
    }

    return inputName;
}

QStringList MeshmoonStorage::CreateMissingFolders(MeshmoonStorageItemWidgetList &items)
{
    QStringList createdKeys;

    // Create non openEditor folders. Virtual folders 
    // seem to be missing from returned the object list.
    foreach(MeshmoonStorageItemWidget *item, items)
    {
        // We only want to look for file keys that have non-openEditor folders in the middle.
        if (item->data.isDir)
            continue;

        QStringList folderNames = item->data.key.split("/", QString::SkipEmptyParts);
        if (folderNames.size() > 1)
        {
            // Remove last, its the file part
            folderNames.removeLast();
            for(int i=0; i<folderNames.size(); ++i)
            {
                QString folderFullKey = QStringList(folderNames.mid(0, i+1)).join("/") + "/";
                if (folderFullKey == "/")
                    continue;
                
                bool found = false;
                foreach(MeshmoonStorageItemWidget *iter, items)
                {
                    if (iter && iter->data.key == folderFullKey)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    found = GetFolder(folderFullKey) ? true : false;
                if (!found)
                {
                    // Note that eTag and lastModified can't be filled on emulated folders.
                    QS3Object folderObj;
                    folderObj.key = folderFullKey;
                    folderObj.isDir = true;

                    items << new MeshmoonStorageItemWidget(plugin_, folderObj);
                    createdKeys << folderFullKey;
                }
            }
        }
    }
    return createdKeys;
}

void MeshmoonStorage::ShowFolder(MeshmoonStorageItemWidget *folder)
{
    if (!storageWidget_)
        return;

    if (!folder)
    {
        LogWarning(LC + "Trying to show no openEditor folder. Returning to storage root.");
        folder = state_.storageRoot;
        if (!folder)
        {
            LogWarning(LC + "Root does not exist. Cannot show folder.");
            return;
        }
    }

    // Set current path.
    if (state_.storageRoot)
    {
        if (folder == state_.storageRoot)
        {
            storageWidget_->ui.labelPath->setText("Meshmoon Storage");
        }
        else
        {
            QString rootKey = state_.storageRoot->data.key;
            if (rootKey.endsWith("/") && rootKey.lastIndexOf("/") > 0)
                storageWidget_->ui.labelPath->setText(folder->data.key.mid(rootKey.lastIndexOf("/")));
            else
                storageWidget_->ui.labelPath->setText(folder->data.key);
        }
    }
    else
        storageWidget_->ui.labelPath->setText(folder->data.key);
        
    // Clean out list without deleting items.
    RocketStorageListWidget *list = storageWidget_->ListView();
    if (!list)
    {
        LogError(LC + "Cannot show folder " + folder->data.key + ". List widget is not valid.");
        return;
    }
    if (list->count() > 0)
        ClearFolderView();

    // Add "go up" item if not in root.
    if (folder != state_.storageRoot)
    {
        state_.cdUp->parent = folder->parent;
        list->addItem(state_.cdUp);
    }
    
    QString filterTerm = storageWidget_->SearchTerm();
    
    foreach(MeshmoonStorageItemWidget *folder, folder->Folders())
    {
        list->addItem(folder);
        if (state_.highlightKeys.contains(folder->data.key))
            folder->setSelected(true);
        else
            folder->setHidden(!filterTerm.isEmpty() ? !folder->text().contains(filterTerm, Qt::CaseInsensitive) : false);
    }
    foreach(MeshmoonStorageItemWidget *file, folder->Files())
    {
        list->addItem(file);
        if (state_.highlightKeys.contains(file->data.key))
            file->setSelected(true);
        else
            file->setHidden(!filterTerm.isEmpty() ? !file->text().contains(filterTerm, Qt::CaseInsensitive) : false);
    }
    
    state_.highlightKeys.clear();
    state_.currentFolder = folder;
}

QStringList MeshmoonStorage::ParseTextures(const QFileInfo &materialFile)
{
    QFile file(materialFile.absoluteFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return QStringList();

    QByteArray data = file.readAll();
    file.close();
    QSet<QString> textures = OgreRenderer::ProcessMaterialForTextures(QString(data));

    QStringList textureFiles;
    foreach(QString texture, textures)
    {
        texture = texture.trimmed();
        if (texture.startsWith("http://") || texture.startsWith("https://"))
        {
            LogDebug("Found web texture ref, not modifying: " + texture);
            continue;
        }
        if (texture.startsWith("Ogre Media:", Qt::CaseInsensitive))
        {
            LogDebug("Found 'Ogre Media:' texture ref, not modifying: " + texture);
            continue;
        }
        /// @todo Resolve local:// from local storages and what?
        QString path = materialFile.dir().absoluteFilePath(texture);
        textureFiles << path;
    }
    return textureFiles;
}

QStringList MeshmoonStorage::ParseStorageSchemaReferences(const QMimeData *mime)
{
    QStringList refs;
    if (!mime)
        return refs;

    // Internally storage puts dragged item refs to a property in the QMimeData
    QVariantList storageRefs = mime->property("storageRefs").toList();
    foreach (const QVariant &url, storageRefs)
    {
        QString urlStr;
        if (url.type() == QVariant::Url)
            urlStr = url.toUrl().toString();
        else if (url.type() == QVariant::String)
            urlStr = url.toString();
        if (urlStr.startsWith(Schema) && !refs.contains(urlStr))
            refs << urlStr;
    }
    // Also check URLs. These should be primarily disk refs and be ignored.
    foreach (const QUrl &url, mime->urls())
    {
        QString urlStr = url.toString();
        if (urlStr.startsWith(Schema) && !refs.contains(urlStr))
            refs << urlStr;
    }
    return refs;
}

QStringList MeshmoonStorage::ParseFileReferences(const QMimeData *mime)
{
    QStringList files;
    if (!mime)
        return files;

    const QList<QUrl> urls = mime->urls();
    foreach (const QUrl &url, urls)
    {
        QString filepath = url.toString();
        if (filepath.startsWith("http://") || filepath.startsWith("https://") || filepath.startsWith(Schema))
            continue;

#ifdef Q_WS_WIN
        if (filepath.startsWith("file:///"))
            filepath = filepath.mid(8);
#else
        if (filepath.startsWith("file://"))
            filepath = filepath.mid(7);
#endif

        QFileInfo fileinfo(filepath);
        if (fileinfo.isFile())
            files << fileinfo.absoluteFilePath();
    }
    return files;
}

QString MeshmoonStorage::ParseFirstSceneFile(const QMimeData *mime)
{
    const QStringList fileRefs = ParseFileReferences(mime);
    foreach (const QString &filepath, fileRefs)
    {
        QFileInfo fileinfo(filepath);
        
        // .txml
        if (fileinfo.suffix().compare("txml", Qt::CaseInsensitive) == 0)
            return filepath;
        // .scene (in place conversion to .txml)
        else if (fileinfo.suffix().compare("scene", Qt::CaseInsensitive) == 0)
        {
            /// @todo Would there be a better place for this code?
            ScenePtr dummyScene = MAKE_SHARED(Scene, "Dummy", framework_, false, true);

            OgreSceneImporter importer(dummyScene);
            SceneDesc desc = importer.CreateSceneDescFromScene(filepath);
            foreach (const AssetDesc &adesc, desc.assets)
            {
                if (!adesc.dataInMemory || adesc.data.size() <= 0 || adesc.typeName != "OgreMaterial")
                    continue;
                if (adesc.source == adesc.destinationName)
                    continue;

                QString destPath = fileinfo.dir().absoluteFilePath(adesc.destinationName);
                if (QFile::exists(destPath))
                    QFile::remove(destPath);

                QFile file(destPath);
                if (file.open(QFile::WriteOnly))
                {
                    file.write(adesc.data);
                    file.close();

                    LogInfo(QString("[MeshmoonStorage]: Saved extracted material %1 to disk from source material %2")
                        .arg(adesc.destinationName).arg(adesc.source.split("/", QString::SkipEmptyParts).last()));
                }
            }

            dummyScene->CreateContentFromSceneDesc(desc, false, AttributeChange::Disconnected);         

            QString tempTxmlFile = fileinfo.absoluteFilePath().replace(".scene", ".txml", Qt::CaseInsensitive);
            bool success = dummyScene->SaveSceneXML(tempTxmlFile, false, false);
            if (success)
            {
                RocketNotificationWidget *w = plugin_->Notifications()->ShowNotification(tr("%1 has been successfully converted to %2").arg(fileinfo.fileName()).arg(tempTxmlFile));
                w->SetAutoHide(15000);
                return tempTxmlFile;
            }
            else
            {
                RocketNotificationWidget *w = plugin_->Notifications()->ShowNotification(tr("Failed to convert %1 to TXML").arg(fileinfo.fileName()));
                w->SetErrorTheme();
                return "";
            }
        }
    }
    return "";
}