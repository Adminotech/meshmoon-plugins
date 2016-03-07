/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonAssimpPlugin.h
    @brief  Implements loading of Assimp supported assets to OgreMeshAsset. */

#pragma once

#include "MeshmoonAssimpPluginApi.h"

#include "IModule.h"
#include "OgreModuleFwd.h"
#include "AssetFwd.h"
#include "MeshmoonAssimpPluginFwd.h"

/// Implements loading of Assimp supported assets to OgreMeshAsset.
/** Parts of this plugins code is based on OgreAssimp http://code.google.com/p/ogreassimp/  
    Copyright (c) 2011 Jacob 'jacmoe' Moen. Licensed under the MIT license. */
class MESHMOON_ASSIMP_API MeshmoonAssimpPlugin : public IModule
{
    Q_OBJECT

public:
    MeshmoonAssimpPlugin();
    virtual ~MeshmoonAssimpPlugin();

public slots:
    /// Returns if we have asset import info for @c assetRef.
    bool HasImportInformation(const QString &assetRef) const;

    /// Returns asset import info for @c assetRef.
    ImportInfo ImportInformation(const QString &assetRef) const;

    /// Adds new asset import info for @c assetRef.
    void AddImportInformation(const QString &assetRef, const ImportInfo &importInformation);

private slots:
    void OnAssetCreated(AssetPtr asset);
    void OnAssimpMeshConversionRequest(OgreMeshAsset *asset, const u8 *data, size_t len);
    void OnAssimpImportCompleted(MeshmoonOpenAssetImporter *importer, bool success, const ImportInfo &info);

private:
    /// IModule override.
    void Load();

    /// IModule override.
    void Initialize();
    
    /// This is used for keeping state of imports that are done outside of the AssetAPI system.
    /// We must keep state so we wont do import the same source ref multiple times.
    QHash<QString, ImportInfo> importInfoCache_;

    // Log channel name.
    QString LC;
};
