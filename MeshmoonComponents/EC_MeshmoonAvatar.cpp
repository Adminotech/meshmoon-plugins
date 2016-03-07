/**
    @author Admino Technologies Ltd.

    Copyright Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "EC_MeshmoonAvatar.h"
#include "MeshmoonComponents.h"

#include "MeshmoonAssimpPlugin.h"
#include "MeshmoonOpenAssetImporter.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "CoreJsonUtils.h"
#include "LoggingFunctions.h"

#include "Scene.h"
#include "Entity.h"
#include "IAttribute.h"

#include "IAsset.h"
#include "IAssetTransfer.h"
#include "AssetRefListener.h"

#include "OgreMeshAsset.h"
#include "OgreSkeletonAsset.h"
#include "OgreMeshAsset.h"
#include "OgreSkeletonSerializer.h"

#include "EC_Placeable.h"
#include "EC_Mesh.h"

#include <QFileInfo>

static const QString MESHMOONAVATAR_SUFFIX = ".meshmoonavatar";
static const QStringList MESHMOONAVATAR_SUFFIXES = QStringList() << ".dae" << ".fbx";

EC_MeshmoonAvatar::EC_MeshmoonAvatar(Scene* scene) :
    IComponent(scene),
    INIT_ATTRIBUTE_VALUE(appearanceRef, "Appearance ref", AssetReference("", "Binary")),
    importer_(0),
    assetsToDownload_(0)
{
}

EC_MeshmoonAvatar::~EC_MeshmoonAvatar()
{
    assetData_.Reset();
    jsonData_.Reset();
    avatarAssetListener_.reset();
    SAFE_DELETE(importer_);
}

void EC_MeshmoonAvatar::AttributesChanged()
{
    if (!framework->IsHeadless() && appearanceRef.ValueChanged())
    {
        QString ref = appearanceRef.Get().ref.trimmed();
        if (ref.isEmpty())
            return;

        QString extension = ref.mid(ref.lastIndexOf("."));
        if (!ref.endsWith(MESHMOONAVATAR_SUFFIX, Qt::CaseInsensitive) && !MESHMOONAVATAR_SUFFIXES.contains(extension, Qt::CaseInsensitive))
        {
            LogWarning("MeshmoonAvatar: Avatar appearance ref support: " + MESHMOONAVATAR_SUFFIX + ", " + MESHMOONAVATAR_SUFFIXES.join(", "));
            return;
        }

        if (!avatarAssetListener_.get())
        {
            avatarAssetListener_ = MAKE_SHARED(AssetRefListener);
            connect(avatarAssetListener_.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(OnAvatarAppearanceLoaded(AssetPtr)), Qt::UniqueConnection);
            connect(avatarAssetListener_.get(), SIGNAL(TransferFailed(IAssetTransfer *, QString)), this, SLOT(OnAvatarAppearanceFailed(IAssetTransfer*, QString)));
        }

        /// @todo Use normal HttpAssetProvider to fetch the .dae asset. Otherwise in the future OgreMeshAsset will eat up the loading!?
        avatarAssetListener_->HandleAssetRefChange(framework->Asset(), ref, "Binary");
    }
}

void EC_MeshmoonAvatar::LoadJson(AssetPtr asset)
{
    bool ok = false;
    QVariantMap map = TundraJson::Parse(asset->RawData(), &ok).toMap();
    if(ok)
    {
        jsonData_.name = map.contains("name") ? map["name"].toString() : "";
        jsonData_.geometryFile = map.contains("geometry") ? map["geometry"].toString() : "";
        jsonData_.frameRate = map.contains("framerate") ? map["framerate"].toInt() : 0;

        if(map.contains("transform"))
        {
            QVariantMap tMap = map["transform"].toMap();

            QVariantList pos = tMap.contains("pos") ? tMap["pos"].toList() : QVariantList();
            QVariantList rot = tMap.contains("rot") ? tMap["rot"].toList() : QVariantList();
            QVariantList scale = tMap.contains("scale") ? tMap["scale"].toList() : QVariantList();

            if(pos.size() == 3)
                jsonData_.pos = float3(pos[0].toFloat(), pos[1].toFloat(), pos[2].toFloat());
            else
                jsonData_.pos = float3::zero;

            if(rot.size() == 3)
                jsonData_.rot = float3(rot[0].toFloat(), rot[1].toFloat(), rot[2].toFloat());
            else
                jsonData_.rot = float3::zero;

            if(scale.size() == 3)
                jsonData_.scale = float3(scale[0].toFloat(), scale[1].toFloat(), scale[2].toFloat());
            else
                jsonData_.scale = float3(1, 1, 1);
        }

        if(map.contains("animations"))
        {
            QVariantList animations = map["animations"].toList();
            foreach(QVariant anim, animations)
            {
                QVariantMap animation = anim.toMap();

                QString animName = animation.contains("name") ? animation["name"].toString() : "";
                QString animFile = animation.contains("src") ? animation["src"].toString() : "";

                AnimationData data;
                data.name = animName;
                data.path = animFile;
                jsonData_.animations.push_back(data);
            }
        }
        else
            LogWarning("MeshmoonAvatar: No animations");
    }
    else
        LogWarning("MeshmoonAvatar: Cannot load JSON data");

    jsonData_.loaded = true;
}

void EC_MeshmoonAvatar::OnAvatarAppearanceLoaded(AssetPtr asset)
{
    assetData_.Reset();
    jsonData_.Reset();

    QString extension(asset->Name().right(asset->Name().size() - asset->Name().lastIndexOf(".")));
    if (!asset->Name().endsWith(MESHMOONAVATAR_SUFFIX, Qt::CaseInsensitive) && !MESHMOONAVATAR_SUFFIXES.contains(extension, Qt::CaseInsensitive))
    {
        LogWarning("MeshmoonAvatar: Avatar appearance ref is not valid file: " + asset->Name());
        framework->Asset()->ForgetAsset(asset, false);
        return;
    }
    if (asset->Name().endsWith(MESHMOONAVATAR_SUFFIX, Qt::CaseInsensitive))
    {
        LoadJson(asset);

        assetsToDownload_ = 0;

        if (jsonData_.geometryFile.trimmed() != "")
            LoadAvatarAsset(framework->Asset()->ResolveAssetRef(asset->Name(), jsonData_.geometryFile));

        foreach(AnimationData data, jsonData_.animations)
        {
            if(data.path != jsonData_.geometryFile)
                LoadAvatarAsset(framework->Asset()->ResolveAssetRef(asset->Name(), data.path));
        }
        return;
    }
    BinaryAsset *binaryAsset = dynamic_cast<BinaryAsset*>(asset.get());
    if (!binaryAsset)
    {
        // This asset might have already been with a mesh type. Load a fake binary data from disk.
        binaryAsset = new BinaryAsset(framework->Asset(), "Binary", asset->Name());
        if (!binaryAsset->LoadFromFile(asset->DiskSource()))
        {
            LogError("MeshmoonAvatar: Loaded asset is not type of BinaryAsset!");
            framework->Asset()->ForgetAsset(asset, false);
            return;
        }
    }

    // Form our generated asset references
    QString fullRefWithoutProtocol;
    framework->Asset()->ParseAssetRef(asset->Name(), 0, 0, 0, &fullRefWithoutProtocol);
    if (fullRefWithoutProtocol.isEmpty())
        fullRefWithoutProtocol = asset->Name();
    
    QString meshAssetName = fullRefWithoutProtocol; 
    QString skelAssetName = fullRefWithoutProtocol;
    
    meshAssetName = "generated://" + meshAssetName.mid(0, meshAssetName.lastIndexOf(".")) + ".mesh";
    skelAssetName = "generated://" + skelAssetName.mid(0, skelAssetName.lastIndexOf(".")) + ".skeleton";

    // Check if this asset is already loaded
    AssetPtr existingMesh = framework->Asset()->GetAsset(meshAssetName);
    AssetPtr existingSkel = framework->Asset()->GetAsset(skelAssetName);
    
    assetData_.binaryAsset = asset;
    if (!existingMesh || !existingSkel)
    {
        if (existingMesh.get())
            LogWarning("MeshmoonAvatar: Mesh already imported once, overwriting current: " + meshAssetName);
        if (existingSkel.get())
            LogWarning("MeshmoonAvatar: Skeleton already imported once, overwriting current: " + skelAssetName);
            
        assetData_.meshAsset = existingMesh.get() ? existingMesh : framework->Asset()->CreateNewAsset("OgreMesh", meshAssetName);
        assetData_.skeletonAsset = existingSkel.get() ? existingSkel : framework->Asset()->CreateNewAsset("OgreSkeleton", skelAssetName);

        try
        {
            SAFE_DELETE(importer_);
            importer_ = new MeshmoonOpenAssetImporter(framework->Asset());
            connect(importer_, SIGNAL(ImportDone(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)), 
                SLOT(OnImportCompleted(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)));
            if (!importer_->Import(&binaryAsset->data[0], binaryAsset->data.size(), binaryAsset->Name(), binaryAsset->DiskSource(), assetData_.meshAsset, assetData_.skeletonAsset))
                throw Ogre::Exception(Ogre::Exception::ERR_INTERNAL_ERROR, "Internal Assimp importer error.", "");
        }
        catch(Ogre::Exception &ex)
        {
            LogError("MeshmoonAvatar: Failed to load avatar data from " + asset->Name() + ": " + QString::fromStdString(ex.getDescription()));
            SAFE_DELETE(importer_);
        }
    }
    else
    {
        assetData_.meshAsset = existingMesh;
        assetData_.skeletonAsset = existingSkel;
        
        ImportInfo colladaInfo;
        MeshmoonAssimpPlugin *assimpPlugin = framework->GetModule<MeshmoonAssimpPlugin>();
        if (assimpPlugin)
        {
            if (assimpPlugin->HasImportInformation(asset->Name()))
                colladaInfo = assimpPlugin->ImportInformation(asset->Name());
            else
                LogWarning("MeshmoonAvatar: Mesh and Skeleton assets already loaded but could not find materials: " + asset->Name());
        }

        OnImportCompleted(0, true, colladaInfo);
    }
}

void EC_MeshmoonAvatar::LoadAvatarAsset(QString assetUrl)
{
    framework->Asset()->ForgetAsset(assetUrl, false);
    AssetTransferPtr transfer = framework->Asset()->RequestAsset(assetUrl, "Binary");
    connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(OnTransferSucceeded(AssetPtr)), Qt::UniqueConnection);
    connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), this, SLOT(OnTransferFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);
    ++assetsToDownload_;
}

void EC_MeshmoonAvatar::OnTransferSucceeded(AssetPtr asset)
{
    if(asset->Name().split("/").last() == jsonData_.geometryFile)
        jsonData_.geometryAsset = asset;

    foreach(AnimationData data, jsonData_.animations)
    {
        if(asset->Name().split("/").last() == data.path && data.path != jsonData_.geometryFile)
            jsonData_.animationAssets.insert(data.name, asset);

        if(data.path == jsonData_.geometryFile)
            jsonData_.mainAnimation = data.name;
    }

    --assetsToDownload_;

    if(assetsToDownload_ <= 0)
        AssetsLoaded();
}

void EC_MeshmoonAvatar::OnTransferFailed(IAssetTransfer * /*transfer*/, QString /*url*/)
{
    --assetsToDownload_;

    if(assetsToDownload_ <= 0)
        AssetsLoaded();
}

void EC_MeshmoonAvatar::OnAvatarAppearanceFailed(IAssetTransfer* /*transfer*/, QString /*reason*/)
{
    LogWarning("MeshmoonAvatar: Cannot instantiate avatar, source asset could not be loaded.");
}

void EC_MeshmoonAvatar::AssetsLoaded()
{
    QString meshAssetName = jsonData_.geometryFile;
    QString skelAssetName = jsonData_.geometryFile;

    meshAssetName = "generated://" + meshAssetName.mid(0, meshAssetName.lastIndexOf(".")) + ".mesh";
    skelAssetName = "generated://" + skelAssetName.mid(0, skelAssetName.lastIndexOf(".")) + ".skeleton";

    // Check if this asset is already loaded
    AssetPtr existingMesh = framework->Asset()->GetAsset(meshAssetName);
    AssetPtr existingSkel = framework->Asset()->GetAsset(skelAssetName);

    // If assets are loaded, forget them to ensure correct animation data.
    if (existingMesh)
    {
        framework->Asset()->ForgetAsset(existingMesh, false);
        existingMesh.reset();
    }
    if (existingSkel)
    {
        framework->Asset()->ForgetAsset(existingSkel, false);
        existingSkel.reset();
    }

    if (!existingMesh || !existingSkel)
    {
        try
        {
            SAFE_DELETE(importer_);
            importer_ = new MeshmoonOpenAssetImporter(framework->Asset());

            BinaryAsset *binaryAsset = dynamic_cast<BinaryAsset*>(jsonData_.geometryAsset.get());
            if(!binaryAsset)
                return;

            assetData_.meshAsset = existingMesh.get() ? existingMesh : framework->Asset()->CreateNewAsset("OgreMesh", meshAssetName);
            assetData_.skeletonAsset = existingSkel.get() ? existingSkel : framework->Asset()->CreateNewAsset("OgreSkeleton", skelAssetName);

            QList<AnimationAssetData> animationAssets;

            // Create animation asset data list
            QMap<QString, AssetPtr>::iterator it;
            for(it = jsonData_.animationAssets.begin(); it != jsonData_.animationAssets.end(); ++it)
            {
                QString name = it.key();
                AssetPtr asset = it.value();

                BinaryAsset *binaryAsset = dynamic_cast<BinaryAsset*>(asset.get());
                if(!binaryAsset)
                    return;

                AnimationAssetData data;
                data.data_ = &binaryAsset->data[0];
                data.numBytes = binaryAsset->data.size();
                data.assetRef  = binaryAsset->Name();
                data.diskSource = binaryAsset->DiskSource();
                data.name = name;

                animationAssets.push_back(data);
            }

            connect(importer_, SIGNAL(ImportDone(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)), 
                SLOT(OnImportCompleted(MeshmoonOpenAssetImporter*, bool, const ImportInfo&)));
            if (!importer_->ImportWithExternalAnimations(&binaryAsset->data[0], binaryAsset->data.size(), binaryAsset->Name(), binaryAsset->DiskSource(), assetData_.meshAsset, assetData_.skeletonAsset, jsonData_.mainAnimation, animationAssets))
                throw Ogre::Exception(Ogre::Exception::ERR_INTERNAL_ERROR, "Internal Assimp importer error.", "");
        }
        catch(Ogre::Exception /*&ex*/)
        {
            LogError("MeshmoonAvatar: Failed to load avatar data.");
            SAFE_DELETE(importer_);
        }
    }
    else
    {
        assetData_.meshAsset = existingMesh;
        assetData_.skeletonAsset = existingSkel;

        ImportInfo colladaInfo;
        MeshmoonAssimpPlugin *assimpPlugin = framework->GetModule<MeshmoonAssimpPlugin>();
        if (assimpPlugin)
        {
            if (assimpPlugin->HasImportInformation(jsonData_.geometryAsset->Name()))
                colladaInfo = assimpPlugin->ImportInformation(jsonData_.geometryAsset->Name());
            else
                LogWarning("MeshmoonAvatar: Mesh and Skeleton assets already loaded but could not find materials: " + jsonData_.geometryAsset->Name());
        }

        OnImportCompleted(0, true, colladaInfo);
    }
}

void EC_MeshmoonAvatar::OnImportCompleted(MeshmoonOpenAssetImporter* /*importer*/, bool success, const ImportInfo &colladaInfo)
{
    if (!success)
    {
        if (importer_)
            importer_->deleteLater();
        importer_ = 0;
        return;
    }

    // Store collada info if not known.
    MeshmoonAssimpPlugin *assimpPlugin = framework->GetModule<MeshmoonAssimpPlugin>();
    if (assetData_.binaryAsset.get() && assimpPlugin && !assimpPlugin->HasImportInformation(assetData_.binaryAsset->Name()))
        assimpPlugin->AddImportInformation(assetData_.binaryAsset->Name(), colladaInfo);
        
    // Check that needed components are created.
    ComponentPtr placeable = ParentEntity()->GetOrCreateComponent(EC_Placeable::TypeNameStatic());
    EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(ParentEntity()->GetOrCreateComponent(EC_Mesh::TypeNameStatic()).get());
    
    placeable->SetTemporary(true);
    mesh->SetTemporary(true);

    // Assign Ogre refs and other information.
    if (assetData_.meshAsset->IsLoaded())
    {
        mesh->SetPlaceable(placeable);
        if(jsonData_.loaded)
        {
            Transform transform;
            transform.pos = jsonData_.pos;
            transform.rot = jsonData_.rot;
            transform.scale = jsonData_.scale;
            mesh->setnodeTransformation(transform);
        }
        else
            mesh->nodeTransformation.Set(colladaInfo.transform, AttributeChange::LocalOnly);
        mesh->meshRef.Set(AssetReference(assetData_.meshAsset->Name(), assetData_.meshAsset->Type()), AttributeChange::LocalOnly);
    }
    if (assetData_.skeletonAsset->IsLoaded())
    {
        ParentEntity()->GetOrCreateComponent("EC_AnimationController");
        mesh->skeletonRef.Set(AssetReference(assetData_.skeletonAsset->Name(), assetData_.skeletonAsset->Type()), AttributeChange::LocalOnly);
    }
    if (!colladaInfo.materials.empty())
    {
        uint currentIndex = 0;
        AssetReferenceList meshMaterial; 
        foreach(uint key, colladaInfo.materials.keys())
        {
            while (currentIndex < key)
            {
                meshMaterial.Append(AssetReference());
                currentIndex++;
            }
            meshMaterial.Append(AssetReference(colladaInfo.materials[key], "OgreMaterial"));
            currentIndex = key + 1;      
        }
        if (meshMaterial.Size() > 0)
            mesh->materialRefs.Set(meshMaterial, AttributeChange::LocalOnly);
    }

    // Delete source .dae binary asset from memory.
    if (assetData_.binaryAsset.get() && assetData_.binaryAsset->Type() == "Binary" && dynamic_cast<BinaryAsset*>(assetData_.binaryAsset.get()) != 0)
        framework->Asset()->ForgetAsset(assetData_.binaryAsset->Name(), false);
    
    // Remove asset pointers for recounting.
    assetData_.Reset();
    jsonData_.Reset();

    // Fire signal to inform that avatar is ready to be used.
    emit AvatarReady();
    
    // Free importer, has lots of assimp internals allocated.
    if (importer_)
        importer_->deleteLater();
    importer_ = 0;
}
