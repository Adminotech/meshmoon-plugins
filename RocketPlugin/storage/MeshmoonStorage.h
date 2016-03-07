/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "InputFwd.h"
#include "qts3/QS3Fwd.h"

#include "MeshmoonStorageItem.h"
#include "SceneDesc.h"

#include <QObject>
#include <QAction>
#include <QString>
#include <QStringList>
#include <QList>
#include <QRectF>
#include <QFileInfo>
#include <QListWidget>
#include <QMap>

/// Provides access to the %Meshmoon Storage.
/** @ingroup MeshmoonRocket */
class MeshmoonStorage : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE
    MeshmoonStorage(RocketPlugin *plugin);
    ~MeshmoonStorage();
    
    /// "meshmoonstorage://" schema used when referencing to files inside the storage.
    /** Used for example for drag and dropping. */
    static const QString Schema;

    /// Returns all Storage schema (meshmoonstorage://) references from the mime data.
    static QStringList ParseStorageSchemaReferences(const QMimeData *mime);
    
    /// Returns all "file://" references from the mime data as absolute file paths (without file:// prefix).
    static QStringList ParseFileReferences(const QMimeData *mime);
    
    /// Returns the first scene description file from the mime data.
    /** @note If Ogre .scene file is encountered, it will be converted in place
        to a .txml file into the parent directory and the path to that .txml is returned. */
    QString ParseFirstSceneFile(const QMimeData *mime);

    enum UiState
    {
        None = 0,
        Storage,
        Import    
    };

    struct State
    {
        State();

        // Path info
        QString storageHostUrl;
        QString storageBasePath;
        QString storageTxmlPath;
        QString storageBucket;
        
        // If true user cannot execute any actions
        // or leave the current folder.
        bool folderOperationOngoing;
        
        // If storage is operating in headless mode.
        bool headless;
        
        bool closeOnEditorsClosed;

        // Storage items.
        MeshmoonStorageItemWidget *storageRoot;
        MeshmoonStorageItemWidget *cdUp;
        MeshmoonStorageItemWidget *currentFolder;
        
        // Ongoing info dialog ptr.
        RocketStorageInfoDialog *infoDialog;
        
        // Editors.
        QList<IRocketAssetEditor*> editors;
        
        // Extensions in this list cannot be remove from storage.
        QStringList protectedExtensions;
        
        // Files that should be highlighted/selected on refresh.
        QStringList highlightKeys;
        
        // Pending scene desc creation after upload.
        SceneDesc pendingSceneCreation;
        
        // Ui state
        UiState ui;
    };
    void UpdateStorageWidgetAnimationsState(int mouseX, bool forceShow = false);
    /// @endcond

    RocketStorageWidget *StorageWidget() const { return storageWidget_; }

public slots:
    /// Open the tool.
    /** @note Authentication level is check before opening. Will only succeed 
        if you have a high enough permission level to the current scene. */
    void Open();

    /// Close the tool.
    /** @return Returns if close was successful. We may not allow closing 
        if there are pending storage operations or the user has editors open. */
    bool Close();

    /// Toggle visibility.
    void ToggleVisibility();

    /// Shows the authentication dialog for the user if necessary. If already authenticated, will fire Completed signal in the next main loop cycle.
    /** Before authentication is done you cannot do any file operations (ListDirectories, UploadFiles etc.). Use IsAuthenticated to check current state.
        @note Authentication level is check before opening. Will only succeed if you have a high enough permission level to the current scene.
        @return Authenticator monitor. The Completed signal will only fire once the root file listing is fetched and storage is ready for interaction. */
    MeshmoonStorageAuthenticationMonitor *Authenticate();

    /// Returns if storage has been authenticated and is ready for interaction.
    /** If this returns false use Authenticate to start the process. */
    bool IsAuthenticated();
    
    /// Returns if storage has fetched the root file listing and is ready for interaction.
    /** If this return false connect to the Ready signal or use Authenticate and its monitors Succeeded signal. */
    bool IsReady();
 
    /// Sets the current storage directory.
    bool OpenDirectory(const MeshmoonStorageItem &directory);

    /// Storage base url, empty string if unknown aka. client does not have privileges to receive storage information.
    /** @note If you are using the base url for QString::startsWith kind of logic, always check if the returned string is empty, othewise it will always match! */
    QString BaseUrl() const;

    /// Returns the storage root directory.
    MeshmoonStorageItem RootDirectory() const;

    /// Returns the current storage directory.
    MeshmoonStorageItem CurrentDirectory() const;

    /// Lists directories and/or folders for a storage directory.
    MeshmoonStorageItemList List(const MeshmoonStorageItem &directory, bool listFiles = true, bool listFolders = true) const;

    /// Fills a list widget with requested data and returns the listed directory as a MeshmoonStorageItemWidget.
    /** @param List widget to fill.
        @param Directory to list.
        @param If files should be filled.
        @param If folders should be filled.
        @param If not empty, only files with these suffixes will be filled. Folders will be shown in any case if listFolders is true. Both 'jpg' and '.jpg' forms of suffixes will work. 
        @return Listed directory ptr or null if not found. */
    MeshmoonStorageItemWidget *Fill(QListWidget *listWidget, const MeshmoonStorageItem &directory, bool listFiles = true, bool listFolders = true, const QStringList &fileSuffixFilters = QStringList()) const;

    /// Shows a storage item selection dialog with given options.
    /** @param If not empty, only files with these suffixes will be filled. Folders will be shown in any case if listFolders is true. Both 'jpg' and '.jpg' forms of suffixes will work.
        @param If changing the folder from startDirectory is allowed.
        @param Directory to show when dialog opens. If not defined storage root directory is used.
        @param Parent widget for the dialog. If null the dialog will be application modal to the main window with Qt::SplashScreen, if a valid widget it will be window modal to it with Qt::Tool.
        @return Storage selection dialog, or null if a this type of modal dialog is already shown. The dialog will be automatically destroyed when closed, you should only call functions or connect to the signals. */
    RocketStorageSelectionDialog *ShowSelectionDialog(const QStringList &suffixFilters = QStringList(), bool allowChangingFolder = true, MeshmoonStorageItem startDirectory = MeshmoonStorageItem(), QWidget *parent = 0);

    /// Aborts a operation.
    /** Aborts and destroys @c operation. Do not use @c operation after calling this function.
        @note Does not necessarily abort ongoing network requests. Does abort saving files to disk from finished download operations.
        @param Operation to abort. */
    void Abort(MeshmoonStorageOperationMonitor *operation);

    /// Download files from storage.
    /** @overload
        @note This function ignores the folder path of the storage source file. If you want to keep the path, use the QMap<QString, QString> overload.
        @param Storage file to download.
        @param Destination folder where to save file.
        @return Operation monitor or null ptr if the download request cannot be started. */
    MeshmoonStorageOperationMonitor *DownloadFile(const MeshmoonStorageItem &file, const QDir &destination);

    /// Download files from storage.
    /** @overload
        @note This function ignores the folder path of the storage source file. If you want to keep the path, use the QMap<QString, QString> overload.
        @note This function does no checking if multiple files from different storage sub folders have the same destination filename when saved to the destination folder.
        @param Storage files to download.
        @param Destination folder where to save the files.
        @return Operation monitor or null ptr if the download request cannot be started. */
    MeshmoonStorageOperationMonitor *DownloadFiles(const MeshmoonStorageItemList &files, const QDir &destination);
    
    /// Download files from storage.
    /** This overload gives you the most flexibility, allowing you to map any storage path to any destination disk path.
        @note None of the DownloadFile(s) functions support downloading full directories.
        @param Maps storage file path keys to destination file on hard drive.
        @code
        var files = {
            "meshes/my.mesh"]  : "C:\Meshmoon\Storage\my.mesh",
            "textures/my.png"] : "C:\Meshmoon\Storage\img\my.mesh"
        };
        var monitor = rocket.storage.DownloadFiles(files);
        monitor.Finished.connect(function() {
            console.LogInfo("Downloads done for " + Object.keys(files);
        });
        @endcode */
    MeshmoonStorageOperationMonitor *DownloadFiles(const QMap<QString, QString> &filesToDestinationPaths);

    /// Upload files to storage. 
    /** Prefer using confirmOverwrite true so user has the final say if something should be overwritten. 
        @param List of absolute file paths to the hard drive.
        @param If we should show a confirmation dialog if you are about to overwrite a file
        @param Directory to upload to, this will default to CurrentDirectory. 
        @return Operation monitor or null ptr if the upload request cannot be started. */
    MeshmoonStorageOperationMonitor *UploadFiles(const QStringList &files, bool confirmOverwrite, MeshmoonStorageItem directory = MeshmoonStorageItem());

    /// Uploads data to storage key.
    /** This function does not ask the user if he wants to overwrite. The operation is performed immediately.
        @param Target file item for the upload.
        @param Upload data.
        @return Operation monitor or null ptr if the upload request cannot be started. */
    MeshmoonStorageOperationMonitor *UploadData(const MeshmoonStorageItem &file, const QByteArray &data);

    /// Copies a file from source key to destination key.
    /** @param Source file storage key.
        @param Destination file storage key.
        @param If the currently viewed folder content should be updated when the copy operation finishes.
        @return The copy operation ptr, null if source file does not exist or destination already exists. */
    QS3CopyObjectResponse *CreateFileCopy(const QString &sourceKey, const QString &destinationKey, bool refreshCurrentFolderWhenDone = true, QS3::CannedAcl acl = QS3::PublicRead);

    /// Makes a copy of the main txml to the default Meshmoon /backup folder with optional file postfix.
    QS3CopyObjectResponse *BackupTxml(bool openBackupFolderWhenDone = false, QString postfix = "");
    
    /// Returns if @c filename exists in currently open storage folder.
    bool FileExistsInCurrentFolder(const QString &filename, Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive);

    /// Returns if this item has a visual editor.
    bool HasVisualEditor(const MeshmoonStorageItem &item);

    /// Returns if this item can be opened in a text editor.
    bool HasTextEditor(const MeshmoonStorageItem &item);

    /// Opens a visual editor for the storage item.
    /** @param Storage item to open visual editor on.
        @return True if storage item is valid and has a visual editor, false otherwise. */
    bool OpenVisualEditor(const MeshmoonStorageItem &item);

    /// Opens a text editor for the storage item.
    /** @param Storage item to open text editor on.
        @return True if storage item is valid and can be opened in a text editor, false otherwise. */
    bool OpenTextEditor(const MeshmoonStorageItem &item);

    /// Get URL for a storage item.
    QString UrlForItem(const MeshmoonStorageItem &item) const;

    /// Get absolute storage URL for a relative asset reference.
    /** This is different from a storage key as keys have the path up to the worlds storage included in them.
        With this function you can get a absolute URL for any relative ref you encounter in the scene.
        @return An absolute storage URL that points to @c relativeRef. It does not matter if this item actually exists in the storage. */
    QString UrlForRelativeRef(const QString &relativeRef) const;

    /// Get the relative ref for a absolute storage URL.
    /** This function converts an absolute storage URL to a working relative ref.
        @return A working relative ref if absolute ref points to storage, otherwise empty string. */
    QString RelativeRefForUrl(const QString &absoluteRef) const;

    /// Get a storage item for a relative ref.
    /** @param Relative asset reference.
        @return Storage item for the relative ref. Returned item will be valid even if it does not exists in the storage. */
    MeshmoonStorageItem ItemForRelativeRef(const QString &relativeRef) const;

    /// Get a storage item for an absolute storage URL.
    /** @param Absolute storage URL.
        @return Storage item for the URL. Returned item will be invalid if @c absoluteRef 
        does not point to the storage, check MeshmoonStorageItem::IsNull .*/
    MeshmoonStorageItem ItemForUrl(const QString &absoluteRef) const;

    /// Get content mime type.
    /// @todo completeSuffix unused!
    QString ContentMimeType(const QString &suffix, const QString &completeSuffix) const;

signals:
    /// Emitted once
    void Ready();
    
    /// @cond PRIVATE

    /// Emitted when storage widget is created.
    /** For internal use, do not document to the public API.
        @note This does not mean it is shown, listen for those from the widget. */
    void StorageWidgetCreated(RocketStorageWidget *storageWidget);

    /// @endcond
    
public:
    /// @cond PRIVATE

    /// Get URL for a storage key.
    QString UrlForItem(const QString &storagePath) const;

    /// Get URL for a storage key.
    /** @note The base url of the returned ref can be different 
        than the AssetAPI default storage base url. The returned url can
        be used for setting refs but not recommended to use with
        eg. getting asset ptrs from AssetAPI, for that use AssetAPI::ResolveAssetRef("", RelativeRefForKey()). */
    static QString UrlForItem(const QString &storageUrl, const QString &storagePath);

    /// Get relative asset ref for a storage key.
    QString RelativeRefForKey(QString storagePath) const;

    /// Updates taskbar with storage item.
    void UpdateTaskbarEntry(RocketTaskbar *taskbar);

    /// Returns current state.
    MeshmoonStorage::State CurrentState() const;

    /// Sets storage information that is used to determine the scene storage.
    void SetStorageInformation(const QString &storageUrl, const QString &txmlUrl);
    
    /// Clears the storage information.
    void ClearStorageInformation();

    /// @endcond

private slots:
    void SetDebugStorageInformation(const QStringList &params);

    void UpdateTaskbarEntryContinue(int level);
    void AuthenticateContinue(int level);

    void OnWindowResized(const QRectF &sceneRect);
    void UpdateInterface(QRectF sceneRect = QRectF());

    void OnStorageAuthenticated(QString accessKey, QString secretKey);
    void Unauthenticate();

    void RefreshCurrentFolderContent();
    void RefreshFolderContent(const QString &prefix);

    // Ui signal handlers
    void OnFileClicked(MeshmoonStorageItemWidget *file);
    void OnFolderClicked(MeshmoonStorageItemWidget *folder);

    void OnCopyAssetReferenceRequest(MeshmoonStorageItemWidget *item);
    void OnCopyUrlRequest(MeshmoonStorageItemWidget *item);
    void OnCreateCopyRequest(MeshmoonStorageItemWidget *item);

    void OnDownloadRequest(MeshmoonStorageItemWidget *item);
    void OnDownloadRequest(MeshmoonStorageItemWidgetList files, bool zip);
    void OnDownloadRequest(const QString &folderPath, MeshmoonStorageItemWidgetList files);
    void OnZipDownloadRequest(const QString &zipFilePath, MeshmoonStorageItemWidgetList files);
    void OnDownloadRequestContinue();

    void OnDeleteRequest(MeshmoonStorageItemWidget *item);
    void OnDeleteRequest(MeshmoonStorageItemWidgetList files);
    void OnDeleteRequestContinue();

    void OnUploadSceneRequest(const QString &filepath);
    void OnUploadSceneRequestContinue(const QStringList &uploadFiles, const SceneDesc &sceneDesc);

    void OnUploadRequest(bool confirmOverwrite);
    void OnUploadRequestContinue();

    /// Handles signal from MeshmoonStorageItemWidget to open its editor.
    void OnEditorOpened(IRocketAssetEditor *editor);
    
    /// Handles signal from IRocketAssetEditor to save content.
    void OnEditorSaveRequest(IRocketAssetEditor *editor, const QByteArray &data);
    
    /// Handles signal from IRocketAssetEditor to close.
    void OnEditorCloseRequest(IRocketAssetEditor *editor);

    void OnInstantiateRequest(MeshmoonStorageItemWidget *item);
    void OnRestoreBackupRequest(MeshmoonStorageItemWidget *txmlItem);
    
    void OnPreRestoreBackupCompleted(QS3CopyObjectResponse *response);
    void OnContinueRestoreBackupRequest(const QString &txmlKey);
    void OnRestoreBackupDownloaded(QS3GetObjectResponse *response);

    void OnCreateFolderRequest();
    void OnCreateFolderRequestContinue();
    void OnUploadFilesRequest();
    void OnCreateNewFileRequest(const QString &suffix);
    void OnCreateNewFileRequestContinue();
    
    QString ValidateInfoDialogFilename(bool errorIfFileExists = true, const QString &mustHaveSuffix = "");
    
    void DestroyInfoDialog();

    // Progress handlers
    void OnBackupTxmlCompleted(QS3CopyObjectResponse *response);
    void OnListSceneFolderObjectsResponse(QS3ListObjectsResponse *response);
    void OnDownloadOperationProgress(QS3GetObjectResponse *response, qint64 complete, qint64 total);
    void OnUploadOperationProgress(QS3PutObjectResponse *response, qint64 complete, qint64 total, MeshmoonStorageOperationMonitor *operation);

    // Main finished handler
    void OnOperationFinished(MeshmoonStorageOperationMonitor *operation);
    void OnCopyOperationCompleted(QS3CopyObjectResponse *response);
    
    // Zip packager finished handler
    void OnZipPackagerCompleted(bool successfull);

    // Folder operations
    void UpdateFolder(QS3ListObjectsResponse *response);
    void ShowFolder(MeshmoonStorageItemWidget *folder);
    void ClearFolderView();
    QStringList CreateMissingFolders(MeshmoonStorageItemWidgetList &items);

    // Get folder with storage item. Start recursive search from root folder.
    MeshmoonStorageItemWidget *GetFolder(const MeshmoonStorageItem &item) const;

    // Get folder by key. Start recursive search from root folder.
    MeshmoonStorageItemWidget *GetFolder(QString key) const;

    // Get folder by key. Start recursive search from given lookupFolder.
    MeshmoonStorageItemWidget *GetFolder(MeshmoonStorageItemWidget *lookupFolder, const QString &key) const;

    // Get file with storage item. Start recursive search from root folder.
    MeshmoonStorageItemWidget *GetFile(const MeshmoonStorageItem &item) const;

    // Get file by key. Start recursive search from root folder.
    MeshmoonStorageItemWidget *GetFile(const QString &key) const;

    // Get file by key. Start recursive search from given lookupFolder.
    MeshmoonStorageItemWidget *GetFile(MeshmoonStorageItemWidget *lookupFolder, const QString &key) const;

    // Get file by filename. Non-recursive search from given lookupFolder.
    MeshmoonStorageItemWidget *GetFileByName(MeshmoonStorageItemWidget *lookupFolder, const QString &filename) const;

    MeshmoonStorageItemWidgetList GetAllSubfiles(MeshmoonStorageItemWidget * item);
    MeshmoonStorageItemWidgetList GetAllParents(MeshmoonStorageItemWidget * item);

    QStringList ParseTextures(const QFileInfo &materialFile);

    /// @todo Remove this and unify toggling tool key sequences to RocketPlugin (build mode Ctrl+B, storage Ctrl+S).
    /// @see inputCtx
    void OnKeyEvent(KeyEvent *e);
    void OnMouseMove(MouseEvent *e);
    
    void OnStorageWidgetAnimationsCompleted();
   
private:  
    void Reset();
    void ResetUi();

    RocketPlugin *plugin_;
    Framework *framework_;
    MeshmoonStorageAuthenticationMonitor *authenticator_;
    
    QString LC;
    QS3Client *s3_;
    QAction *toggleAction_;
    State state_;
    
    RocketStorageWidget *storageWidget_;
    
    /// @todo Remove this and unify toggling tool key sequences to RocketPlugin (build mode Ctrl+B, storage Ctrl+S).
    /// @see OnKeyEvent
    InputContextPtr inputCtx_;
    InputContextPtr inputCtxMove_;
};
Q_DECLARE_METATYPE(MeshmoonStorage*)
