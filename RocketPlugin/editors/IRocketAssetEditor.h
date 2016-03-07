/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "Win.h"

#include "RocketFwd.h"
#include "AssetFwd.h"
#include "qts3/QS3Fwd.h"

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QPair>
#include <QPointer>

class QMenuBar;

#include "ui_RocketResourceEditor.h"

/// Interface that is implemented by all of the Rocket asset editors.
class IRocketAssetEditor : public QWidget
{
    Q_OBJECT

public:
    /// Create resource editor from a existing and valid MeshmoonStorageItemWidget.
    IRocketAssetEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource, const QString &windowTitle, const QString &resourceDescription = "");

    /// Create resource editor from an MeshmoonAsset. 
    /** @note Any editor that can edit and save should not implement loading assets with this constructor.
        This overload exists for view-only editors to be able to load resource from any URL. */
    IRocketAssetEditor(RocketPlugin *plugin, MeshmoonAsset *resource, const QString &windowTitle, const QString &resourceDescription = "");

    /// Dtor.
    ~IRocketAssetEditor();

    /// Call this to start the editor.
    /** Will trigger the resource fetch operation from storage or from AssetAPI. */
    void Start(QS3Client *s3 = 0);
    
    /// File information.
    QString filename;
    QString suffix;
    QString completeSuffix;

    /// UI.
    Ui::RocketResourceEditorWidget ui;
    
    /// First = if serialization was successful, Second = data.
    typedef QPair<bool, QByteArray> SerializationResult;

    /// Dependency state enum.
    enum DependencyState
    {
        DS_OK = 0,
        DS_Error,
        DS_Verifying
    };

public slots:
    /// Show editor.
    void Show();

    /// Hide editor.
    void Hide();
    
    /// Set a message to the UI.
    void SetMessage(const QString &msg);

    /// Set a error to the UI.
    void SetError(const QString &msg);

    /// Clear any message or error from the UI.
    void ClearMessage();

    /// Updates last saved time stamp to UI.
    void UpdateTimestamp();
    
    /// Returns if this editor has unsaved changes.
    /** @return True if saving is enabled and there are unsaved changes, otherwise false. */
    bool HasUnsavedChanges() const;
    
    /// Returns if the editor has done changes to the resource.
    /** @note This will ignore if saving is enabled, changes are tracked regardless.
        Use HasUnsavedChanges if you want that taken into account. */
    bool IsTouched() const;
    
    /// Meshmoon storage item key. May return empty if resource is not from storage.
    QString StorageKey() const;
    
    /// Returns asset ref for the resource.
    /** Absolute URL for storage items, passed in asset ref for anything else. */
    QString AssetRef() const;

signals:
    /// Emitted when a editor wants to save a resource.
    void SaveRequest(IRocketAssetEditor *editor, const QByteArray &data);

    /// Emitted when a editor wants to be closed.
    void CloseRequest(IRocketAssetEditor *editor);

protected:
    /// Implementations must handle the initial resource data download.
    /** In most cases this is when you update/create the user interface
        to edit the resource. 
        @param Resource raw data. */
    virtual void ResourceDownloaded(QByteArray data) = 0;

    /// Implementations must return the current raw data for the resource.
    /** The data will be used to upload a new version of the resource to storage. 
        @return Current raw data of the edited resource. */
    virtual SerializationResult ResourceData() = 0;
    
    /// Called when editor is about to be closed.
    /** This opportunity should be taken to close any dialogs to clear the view, 
        as the about to close logic might pop its own 'unsaved changes' dialog.
        You can also check if there are changes and call Touched to prompt the 'unsaved changes' dialog,
        but it is recommended to call Touch when the change happens. */
    virtual void AboutToClose() {};

    /// Set resource description, this will show in the bottom right corner, can also be passed to ctor.
    void SetResourceDescription(const QString &description);

    /// Top menu bar. Ensured to return valid ptr.
    QMenuBar *MenuBar();

    /// Top file menu. Ensured to return valid ptr.
    QMenu *FileMenu();

    /// Top tools menu. Ensured to return valid ptr.
    QMenu *ToolsMenu();
    
    /// Top settings menu. Ensured to return valid ptr.
    QMenu *SettingsMenu();

    /// Restore editor geometry from config.
    /** This is disabled by default, as not all editors will want this behavior.
        Easy use is to call RestoreGeometryFromConfig in your impl ctor with true as the second parameter.
        @param Unique editor name.
        @param Set to true if you want the base editor automatically storing 
        its geometry to the config when it closes.
        @return True if geometry was set from config, if returns false a default size should be set.
        Additionally if this function sets the geometry, the show event triggered center-to-main-window step is skipped. */
    bool RestoreGeometryFromConfig(const QString &editorName, bool autoSaveGeometryOnClose);

    /// Restore size from config if exists.
    void StoreGeometryToConfig(const QString &editorName);

    /// Sets save the menu item and Ctrl+S shortcut enabled/disabled.
    /** Use to disable saving if the editor is read/view only. */
    void SetSaveEnabled(bool enabled);
    
    /// Returns if this editor is capable of saving.
    /** Saving can be disabled by the editor implementation or (view only editors)
        if the resource does not originate from the storage. */
    bool CanSave() const;

    // Call this function when editor does changes. It will notify user if he is closing the editor
    // when he has unsaved changes. If the user decides to close anyway we let the editor reload 
    // it's resource via AssetAPI if necessary (this should be done if the editor is doing live edits).
    void Touched();
    
    /// Shows a modal dialog with your error. Use with fatal errors to get the users attention.
    void ShowError(const QString &title, const QString &message);

    /// QWidget override.
    void showEvent(QShowEvent *e);

    /// QWidget override.
    void closeEvent(QCloseEvent *e);

    /// Rocket plugin ptr.
    RocketPlugin *plugin_;

private slots:
    /// Editor save handler.
    void OnSavePressed();

    /// Unsaved changed dialog action handler.
    void OnUnsavedDialogButtonPressed(QAbstractButton *button);

    /// Returns true if close was canceled.
    bool TryClose();

    /// Storage resource data fetch completion handler.
    void OnResourceFetched(QS3GetObjectResponse *response);
    
    /// AssetAPI resource data fetch completion handler.
    void OnResourceFetched(AssetPtr asset);

    /// Storage resource data fetch progress handler.
    void OnResourceFetchProgress(QS3GetObjectResponse *response, qint64 received, qint64 total);

    /// This gets invoked when the storage upload has completed for a SaveRequest.
    /** @note Connection done by MeshmoonStorage on SaveRequest */
    void OnResourceSaveFinished(MeshmoonStorageOperationMonitor *monitor);

    /// This gets invoked on the storage upload progress for a SaveRequest.
    /** @note Connection done by MeshmoonStorage on SaveRequest */
    void OnResourceSaveProgress(QS3PutObjectResponse *response, qint64 completed, qint64 total, MeshmoonStorageOperationMonitor *operation);

private:
    void _InitUi(const QString &resourceDescription = "");

    QString storageKey_;
    QString assetRef_;

    bool closeEmitted_;
    bool closeWithoutSaving_;
    bool closeAfterSaving_;
    
    bool saving_;
    bool touched_;

    QMenuBar *menuBar_;
    QMenu *menuFile_;
    QMenu *menuTools_;
    QMenu *menuSettings_;
    
    QAction *saveAction_;
    QAction *closeAction_;
    
    QString windowTitle_;
    QString cachedMessage_;
    
    QString geometryConfigEditorName_;
    bool geometryAutoSaveOnClose_;
    bool geometrySetFromConfig_;
    bool geometryMaximisedFromConfig_;
    
    QPointer<QMessageBox> unsavedChangesDialog_;
};
