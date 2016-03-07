/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

#pragma once

#include "RocketFwd.h"
#include "AssetFwd.h"
#include "MeshmoonAsset.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QPair>
#include <QVariant>
#include <QDialog>
#include <QSize>
#include <QPushButton>

#include "ui_RocketAssetSelectionDialog.h"

/// Represents a collection of assets from a set of sources.
/** @ingroup MeshmoonRocket */
class MeshmoonAssetLibrary : public QObject
{
    Q_OBJECT

public:
    explicit MeshmoonAssetLibrary(RocketPlugin *plugin);
    ~MeshmoonAssetLibrary();
    
    /// Dumps asset information to native console.
    void DumpAssetInformation() const;

public slots:
    /// Updates library state with all known sources.
    /** @see Signal LibraryUpdated. */
    void UpdateLibrary();

    /// Clears library state.
    void ClearLibrary();

    /// Returns if library has known assets.
    bool HasAssets() const;
    
    /// Returns if a source with url exists.
    bool HasSource(const QString &sourceUrl) const;

    /// Add custom asset library source.
    /** @param Source name.
        @param JSON source url.
        @note Adding a source will auto trigger a UpdateLibrary call. */
    bool AddSource(const QString &sourceName, const QString &sourceUrl);
    bool AddSource(const MeshmoonLibrarySource &source); /**< @overload @param source (Source name, JSON source URL) pair */

    /// Returns the available asset tags for a optional source or if not defined for all sources.
    QStringList AvailableAssetTags(const MeshmoonLibrarySource &source = MeshmoonLibrarySource()) const;

    /// Returns all known sources.
    MeshmoonLibrarySourceList Sources() const;

    /// Returns a source with url if known.
    MeshmoonLibrarySource Source(const QString &sourceUrl) const;

    /// Returns a source with name if known.
    MeshmoonLibrarySource SourceByName(const QString &sourceName) const;

    /// Returns all knows assets.
    MeshmoonAssetList Assets() const;

    /// Lists available assets with optional type and source information.
    MeshmoonAssetList Assets(MeshmoonAsset::Type type, const MeshmoonLibrarySource &source = MeshmoonLibrarySource()) const;

    /// Searches for an asset in the library. Returns null if asset not found.
    MeshmoonAsset *FindAsset(const QString &ref) const;

    /// Show selection dialog for certain type of assets from certain source.
    /** @note Can return null ptr if a dialog is already open, there can only be one application modal dialog at a time!
        @param Type of the assets the dialog should present to the user.
        @param Optional source, do not define to allow all sources.
        @param Forced tag will mean only the assets with this tag will be shown, this cannot be changed by the user from the UI.
        @param Parent widget for the dialog. If null the dialog will be application modal to the main window with Qt::SplashScreen, if a valid widget it will be window modal to it with Qt::Tool.
        @return Asset dialog, look at its signals for callbacks. */
    MeshmoonAssetSelectionDialog *ShowSelectionDialog(MeshmoonAsset::Type type = MeshmoonAsset::All, const MeshmoonLibrarySource &source = MeshmoonLibrarySource(), const QString &forcedTag = "", QWidget *parent = 0);
    
    /// @overload
    /** @param Assets list to present to the user. 
        @param Forced tag will mean only the assets with this tag will be shown, this cannot be changed by the user from the UI.
        @param Parent widget for the dialog. If null the dialog will be application modal to the main window with Qt::SplashScreen, if a valid widget it will be window modal to it with Qt::Tool.
        @return Asset dialog, look at its signals for callbacks. */
    MeshmoonAssetSelectionDialog *ShowSelectionDialog(const MeshmoonAssetList &assets, const QString &forcedTag = "", QWidget *parent = 0);

    /// Creates a button widget you can embed to any UI.
    /** @note It is the callers responsibility to delete the widget when no longer needed.
        @param Source asset
        @param Size of the widget 
        @param Should the name widget be created.
        @param Should the description widget be created.
        @param If given overrides the input asset name. Not used if createName is false.
        @param If given overrides the background color if a preview image cannot be resolved from @c asset.
        @param If given overrides the input asset description. Not used if createDescription is false.
        @return Asset selection button widget. */
    QPushButton *CreateAssetWidget(MeshmoonAsset *asset = 0, const QSize &size = QSize(200,200), bool createName = true, bool createDescription = true, 
                                   const QString &tooltip = "", const QString &nameOverride = "", const QString &descriptionOverride = "", 
                                   QColor backgroundColor = QColor(), int fixedNameContainerHeight = -1);

signals:
    void LibraryUpdated(const MeshmoonAssetList &assets);

private slots:
    void DeserializeAssetLibrary(const QString &sourceUrl, const QByteArray &data);
    MeshmoonAsset *DeserializeAsset(MeshmoonAsset::Type type, const MeshmoonLibrarySource &source, const QString &baseUrl, const QVariant &data);
    void SourceCompleted(const QString &sourceUrl);
    
    void OnSourceCompleted(AssetPtr asset);
    void OnSourceFailed(IAssetTransfer *transfer, QString reason);
    
    void EmitUpdated();

private:
    QString LC;
    
    RocketPlugin *plugin_;
    QList<MeshmoonAsset*> assets_; ///< The authorative list of assets in the library.
    QHash<QString, QPointer<MeshmoonAsset> > assetsByRef_; ///< The hash map of assets for fast lookup.

    MeshmoonLibrarySourceList sources_;
    MeshmoonLibrarySourceList pendingSources_;
    
    QPointer<MeshmoonAssetSelectionDialog> dialog_;
};

/// Dialog for selecting an asset from an asset library.
/** @ingroup MeshmoonRocket */
class MeshmoonAssetSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    /// Open a selection dialog for type
    explicit MeshmoonAssetSelectionDialog(RocketPlugin *plugin, const MeshmoonAssetList &_assets, const QString &forcedTag = "", QWidget *parent = 0);
    
    /// All assets that were available for the user in this dialog.
    MeshmoonAssetList assets;

public slots:   
    /// Get the selected asset.
    /** @return MeshmoonAsset ptr or null if nothing was selected. */
    MeshmoonAsset *Selected() const;

private slots:
    // Creates widgets.
    void CreateAssetWidgets();

    // Updates geometry to the current dynamic content.
    void UpdateGeometry();
    
    // Updates the view with the current user selected filtering options.
    void UpdateFiltering();
    
    // Updates button style.
    void UpdateButtonStyle(QPushButton *button, const QString &imagePath = ":/images/asset-libary-no-preview.png");

    void OnSelect();
    void OnCancel();
    
    void OnFilterChanged(const QString &term);
    void OnCategoryChanged(const QString &category);
    void OnSelectionChanged();
    
    void OnPreviewImageCompleted(AssetPtr asset);

signals:
    /// Emitted when a asset is selected.
    void AssetSelected(MeshmoonAsset *asset);

    /// Emitted when selection was canceled by the user.
    void Canceled();

protected:
    bool eventFilter(QObject *obj, QEvent *event);
    
private:
    RocketPlugin *plugin_;
    Framework *framework_;
    MeshmoonAsset *selected_;
    
    Ui::RocketAssetSelectionDialog ui_;
    QGridLayout *grid_;
    QList<QPushButton*> buttons_;
    
    int itemsPerRow_;
    
    QString currentTag_;
    QString currentTerm_;
};
Q_DECLARE_METATYPE(MeshmoonAssetLibrary*)
