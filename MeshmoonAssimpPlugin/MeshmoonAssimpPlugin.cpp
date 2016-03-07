/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MeshmoonAssimpPlugin.h"
#include "MeshmoonOpenAssetImporter.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "IAsset.h"

#include "OgreMeshAsset.h"
#include "OgreMaterialAsset.h"

#include <OgreSubMesh.h>

#include "MemoryLeakCheck.h"

MeshmoonAssimpPlugin::MeshmoonAssimpPlugin() :
    IModule("MeshmoonAssimpPlugin"),
    LC("[MeshmoonAssimpPlugin]: ")
{
}

MeshmoonAssimpPlugin::~MeshmoonAssimpPlugin()
{
}

void MeshmoonAssimpPlugin::Load()
{
    // Connect our Assimp implementation.
    connect(Fw()->Asset(), SIGNAL(AssetCreated(AssetPtr)), SLOT(OnAssetCreated(AssetPtr)), Qt::UniqueConnection);
}

void MeshmoonAssimpPlugin::Initialize()
{
    // Disconnect/disable 'OpenAssetImport' module if loaded
    // so that our implementation from Load() is used.
    IModule *assimpModule = Fw()->ModuleByName("OpenAssetImport");
    if (assimpModule)
        disconnect(Fw()->Asset(), SIGNAL(AssetCreated(AssetPtr)), assimpModule, SLOT(OnAssetCreated(AssetPtr)));
}

void MeshmoonAssimpPlugin::OnAssetCreated(AssetPtr asset)
{
    if (!asset.get() || asset->Type() != "OgreMesh")
        return;

    OgreMeshAsset *meshAsset = dynamic_cast<OgreMeshAsset*>(asset.get());
    if (meshAsset && meshAsset->IsAssimpFileType())
    {
        connect(meshAsset, SIGNAL(ExternalConversionRequested(OgreMeshAsset*, const u8*, size_t)), 
            SLOT(OnAssimpMeshConversionRequest(OgreMeshAsset*, const u8*, size_t)), Qt::UniqueConnection);
    }
}

void MeshmoonAssimpPlugin::OnAssimpMeshConversionRequest(OgreMeshAsset *asset, const u8 *data, size_t len)
{
    if (!asset)
        return;

    /// @todo Allocating the whole importer for every single import is probably a tad slow.
    MeshmoonOpenAssetImporter *importer = new MeshmoonOpenAssetImporter(Fw()->Asset());
    connect(importer, SIGNAL(ImportDone(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)), 
        this, SLOT(OnAssimpImportCompleted(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)));
    importer->Import(data, len, asset->Name(), asset->DiskSource(), asset->shared_from_this());
}

void MeshmoonAssimpPlugin::OnAssimpImportCompleted(MeshmoonOpenAssetImporter *importer, bool success, const ImportInfo &info)
{
    if (!importer)
        return;

#ifdef ASSIMP_ENABLED
    /** @todo We need to get info here what EC_Mesh is using this reference.
        This way we could set the materials to EC_Mesh::materialRefs with AttributeChange::LocalOnly
        for only the submesh indexes that have a empty material ref.
        With the below system if anything is put as a material ref in the editor, all these
        materials will be reseted immediately.
        
        @note There are plans to associate EntityWeakPtr to AssetTransfers so we can do this.
        Right now we would have to iterate the whole scene for EC_Mesh and manually compare
        strings which is not good at all when there are a lot of entities.
    */    
    OgreMeshAsset *meshAsset = dynamic_cast<OgreMeshAsset*>(importer->DestinationMeshAsset().get());
    if (meshAsset)
    {
        // If loading has completed successfully, apply created materials.
        // EC_Mesh will overwrite these later on whatever is defined in materialRefs.
        if (success && meshAsset->ogreMesh.get())
        {
            for (ushort sbi=0; sbi<meshAsset->ogreMesh->getNumSubMeshes(); ++sbi)
            {
                if (info.materials.contains(sbi))
                {
                    OgreMaterialAssetPtr assetPtr = Fw()->Asset()->FindAsset<OgreMaterialAsset>(info.materials[sbi]);
                    if (assetPtr.get() && assetPtr->ogreMaterial.get())
                    {
                        Ogre::SubMesh *submesh = meshAsset->ogreMesh->getSubMesh(sbi);
                        if (submesh)
                        {
                            // Don't create strings and stuff to stack if it never gets printed out.
                            if (IsLogChannelEnabled(LogChannelDebug))
                                LogDebug(LC + QString("Applying material to submesh %1 assetRef=%2 ogreName=%3").arg(sbi).arg(assetPtr->Name()).arg(QString::fromStdString(assetPtr->ogreMaterial->getName())));

                            submesh->setMaterialName(assetPtr->ogreMaterial->getName());
                        }
                    }
                }
            }
        }

        // Let mesh asset know that we are done.
        meshAsset->OnAssimpConversionDone(success);
    }
#else
    LogWarning(LC + "Import completed on a build where ASSIMP_ENABLED is not defined, cannot continue loading.");
#endif

    importer->deleteLater();
}

bool MeshmoonAssimpPlugin::HasImportInformation(const QString &assetRef) const
{
    return importInfoCache_.find(assetRef) != importInfoCache_.end();
}

void MeshmoonAssimpPlugin::AddImportInformation(const QString &assetRef, const ImportInfo &importInformation)
{
    if (!HasImportInformation(assetRef))
        importInfoCache_.insert(assetRef, importInformation);
}

ImportInfo MeshmoonAssimpPlugin::ImportInformation(const QString &assetRef) const
{
    QHash<QString, ImportInfo>::const_iterator it = importInfoCache_.find(assetRef);
    return it != importInfoCache_.end() ? it.value() : ImportInfo();
}

extern "C"
{
    DLLEXPORT void TundraPluginMain(Framework *fw)
    {
        Framework::SetInstance(fw);
        fw->RegisterModule(new MeshmoonAssimpPlugin());
    }
}
