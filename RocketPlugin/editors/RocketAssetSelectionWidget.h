/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "FrameworkFwd.h"
#include "AssetFwd.h"

#include "IRocketAssetEditor.h"
#include "storage/MeshmoonStorageItem.h"
#include "ConfigAPI.h"

#include "ui_RocketAssetSelectionWidget.h"

/// Helper widget to select and validate asset references from any web source.
class RocketAssetSelectionWidget : public QWidget, public Ui::RocketAssetSelectionWidget
{
    Q_OBJECT

public:
    RocketAssetSelectionWidget(RocketPlugin *plugin, const QString &assetType, const QString &assetContext = "", QWidget *parent = 0);
    ~RocketAssetSelectionWidget();

public slots:
    /// Set and validate a new asset reference.
    void SetAssetRef(const QString &assetRef);
    
    /// Set the full config information for this widget with valid file, section and key.
    /** This data is used to read initial value and save latest validated value. */
    void SetConfigData(const ConfigData &configData);
    
    /// Returns the currently set asset ref.
    /** @note This is potentially a relative string to either world storage or to
        the current context (give to ctor).
        @see CurrentFullAssetRef to get the absolute ref. */
    QString CurrentAssetRef();
    
    /// Returns the currently set absolute asset ref.
    /** @see CurrentAssetRef */
    QString CurrentFullAssetRef();
    
    /// Forcefully emits the currently set asset ref via AssetRefChanged() signal.
    void EmitCurrentAssetRef();

    /// Clears any currently set ref.
    void ClearAssetRef();
    
    /// Pick asset from storage.
    void PickFromStorage();
    
    /// Open asset with a editor.
    void OpenCurrentInEditor();
    
    /// Current state. Useful to check if in error state before a save operation.
    IRocketAssetEditor::DependencyState State() const { return state_; }

signals:
    /// Emitted when a new asset ref has been set and validated.
    /** The assetRef will always, if possible, be resolved to a relative ref against
        the current context (given in ctor) or the current worlds storage.
        If cant be resolved to relative a full absolute URL ref is used.
        @note Also emitted when ref is set to empty string. */
    void AssetRefChanged(const QString &assetRef, AssetPtr asset);
    
private slots:
    void SetState(IRocketAssetEditor::DependencyState state);
    
    QString ResolveRelativeOrAbsoluteRef(QString ref);
    
    void OnEditingFinished();
    void OnAssetRefChanged(const QString &assetRef, bool emitChange = true);
    void OnAssetTransferCompleted(AssetPtr asset);
    void OnAssetTransferFailed(IAssetTransfer *transfer, QString reason);
    
    void OnAssetSelectedFromStorage(const MeshmoonStorageItem &item);
    void OnStorageSelectionClosed();

private:
    RocketPlugin *plugin_;
    QString assetType_;
    QString assetRef_;
    QString assetContext_;
    
    ConfigData configData_;
    
    IRocketAssetEditor::DependencyState state_;
    MeshmoonStorageItem lastStorageDir_;
};