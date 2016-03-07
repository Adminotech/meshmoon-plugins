/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */
    
#pragma once

#include "RocketFwd.h"
#include "SceneDesc.h"

#include <QDialog>

#include "ui_RocketStorageSceneImportWidget.h"

/// @cond PRIVATE

struct RocketStorageImportEntityItem
{
    int row;
    bool existsScene;

    EntityDesc desc;
};
typedef std::vector<RocketStorageImportEntityItem> ImportEntityItemList;

struct RocketStorageImportAssetItem
{
    int row;
    QString pathAbsolute;
    QString pathRelative;
    bool existsStorage;
    bool existsDisk;

    AssetDesc desc;
};
typedef std::vector<RocketStorageImportAssetItem> ImportAssetItemList;

// RocketStorageSceneImporter

class RocketStorageSceneImporter : public QDialog
{
    Q_OBJECT

public:
    RocketStorageSceneImporter(QWidget *parent, RocketPlugin *plugin, const QString &destinationPrefix,
        const SceneDesc &sceneDesc_, const ImportEntityItemList &entities_, 
        const ImportAssetItemList &assets_);
    ~RocketStorageSceneImporter();

    SceneDesc sceneDesc;
    ImportEntityItemList entities;
    ImportAssetItemList assets;
    QString destinationPrefix_;

    Ui::RocketStorageSceneImportWidget ui;

signals:
    void ImportScene(const QStringList &uploadFiles, const SceneDesc &sceneDesc);

private slots:
    void OnUploadClicked();
    void OnAssetWidgetClicked(int row, int column);

    void OnToggleSize(bool checked);
    void OnToggleFileTypes();
    void OnToggleOverwriteFiles();
    void OnToggleEntSelection();
    void OnToggleAssetSelection();

    void UpdateActionButton();

private:    
    RocketPlugin *plugin_;
    QMenu *menu_;
    QRect originalRect_;
};

/// @endcond
