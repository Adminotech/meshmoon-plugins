/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonComponentsApi.h"
#include "IComponent.h"
#include "AssetFwd.h"
#include "OgreModuleFwd.h"
#include "BinaryAsset.h"
#include "AssetReference.h"

#include "Math/float3.h"

#include <QHash>
#include <QString>

/// @cond PRIVATE

class MeshmoonOpenAssetImporter;
struct ImportInfo;

struct MeshmoonAvatarData
{
    AssetPtr binaryAsset;
    AssetPtr meshAsset;
    AssetPtr skeletonAsset;
    
    void Reset()
    {
        binaryAsset.reset();
        meshAsset.reset();
        skeletonAsset.reset();
    }
};

struct AnimationData
{
    QString name;
    QString path;
};

struct MeshmoonAvatarJsonData
{
    QString name;
    QString mainAnimation;

    QString geometryFile;
    AssetPtr geometryAsset;

    int frameRate;

    float3 pos;
    float3 rot;
    float3 scale;

    QList<AnimationData> animations;
    QMap<QString, AssetPtr> animationAssets;

    bool loaded;

    MeshmoonAvatarJsonData() : loaded(false), frameRate(0), pos(float3::zero), rot(float3::zero), scale(float3::one) {}

    void Reset() { *this = MeshmoonAvatarJsonData(); }
};

/// @endcond


/// Makes entity an avatar using COLLADA (.dae) or MeshmoonAvatar (.meshmoonavatar) as the description.
class MESHMOON_COMPONENTS_API EC_MeshmoonAvatar : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonAvatar", 500) // Note this is the closed source EC Meshmoon range ID.

public:
    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonAvatar(Scene* scene);
    /// @endcond
    virtual ~EC_MeshmoonAvatar();

    Q_PROPERTY(AssetReference appearanceRef READ getappearanceRef WRITE setappearanceRef);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReference, appearanceRef);
    
private slots:
    /// Avatar asset loaded.
    void OnAvatarAppearanceLoaded(AssetPtr asset);

    /// Avatar asset failed to load.
    void OnAvatarAppearanceFailed(IAssetTransfer* transfer, QString reason);
    
    /// Avatar file was imported.
    void OnImportCompleted(MeshmoonOpenAssetImporter *importer, bool success, const ImportInfo &colladaInfo);
    
    /// Asset transfer succeeded
    void OnTransferSucceeded(AssetPtr asset);

    /// Asset transfer failed
    void OnTransferFailed(IAssetTransfer *transfer, QString url);

signals:
    void AvatarReady();

private:
    /// IComponent override.
    void AttributesChanged();

    // Load json data
    void LoadJson(AssetPtr asset);

    // Load avatar asset
    void LoadAvatarAsset(QString assetUrl);

    void AssetsLoaded();

    /// Ref listener for the avatar asset
    AssetRefListenerPtr avatarAssetListener_;
    
    /// Last set avatar asset
    //weak_ptr<BinaryAsset> avatarAsset_;
    
    /// Created data.
    MeshmoonAvatarData assetData_;

    // Json data
    MeshmoonAvatarJsonData jsonData_;
    
    /// Assimp importer for collada files.
    MeshmoonOpenAssetImporter *importer_;

    int assetsToDownload_;
};
COMPONENT_TYPEDEFS(MeshmoonAvatar);
