/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#include "StableHeaders.h"
#include "EC_MeshmoonCloudLayer.h"
#include "EC_MeshmoonSky.h"

#include "Framework.h"
#include "Math/MathFunc.h"
#include "Scene.h"
#include "Entity.h"
#include "AttributeMetadata.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAsset.h"

#ifdef MESHMOON_SILVERLINING
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) // Suppress warnings leaking from SilverLining.
#endif
#include <SilverLining.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // ~ifndef MESHMOON_SILVERLINING

static const QString cloudLayerAssetSuffix = "meshmooncloudlayer";

EC_MeshmoonCloudLayer::EC_MeshmoonCloudLayer(Scene* scene) :
    IComponent(scene),
    INIT_ATTRIBUTE_VALUE(cloudType, "Cloud Type", CUMULUS_MEDIOCRIS),
    INIT_ATTRIBUTE_VALUE(precipitationType, "Precipitation", NO_PRECIPITATION),
    INIT_ATTRIBUTE_VALUE(precipitationIntensity, "Precipitation Intensity", 1.0f),
    INIT_ATTRIBUTE_VALUE(infinite, "Infinite", true),
    INIT_ATTRIBUTE_VALUE(fadeTowardEdges, "Fade Toward Edges", true),
    INIT_ATTRIBUTE_VALUE(cloudWrapping, "Cloud Wrapping", true),
    INIT_ATTRIBUTE_VALUE(altitude, "Altitude", 3000.0f),
    INIT_ATTRIBUTE_VALUE(length, "Base Length", 3000.0f),
    INIT_ATTRIBUTE_VALUE(width, "Base Width", 3000.0f),
    INIT_ATTRIBUTE_VALUE(thickness, "Thickness", 100.0f),
    INIT_ATTRIBUTE_VALUE(density, "Density", 0.7f),
    INIT_ATTRIBUTE_VALUE(alpha, "Alpha", 1.0f),
    INIT_ATTRIBUTE_VALUE(position, "Position", float2::zero),
    INIT_ATTRIBUTE_VALUE(enabled, "Enabled", true),
    INIT_ATTRIBUTE_VALUE(cloudAsset, "Asset Ref", AssetReference("", "Binary")),
    LC("[EC_MeshmoonCloudLayer]: " ),
    cloudLayerHandle_(-1),
    cloudLayer_(0),
    loadedFromAsset_(false),
    fadeMsecs_(350)
{
    static AttributeMetadata dummyMetadata, cloudTypeMetadata, densityAlphaMetadata, sizeMetadata,
                             thicknessMetadata, precipitationTypeMetadata, precipitationIntensityMetadata;
    static bool metadataInitialized = false;
    if (!metadataInitialized)
    {
        cloudTypeMetadata.enums[CIRROCUMULUS] = "Cirrocumulus";
        cloudTypeMetadata.enums[CIRRUS_FIBRATUS] = "Cirrus Fibratus";
        cloudTypeMetadata.enums[STRATUS] = "Stratus";
        cloudTypeMetadata.enums[CUMULUS_MEDIOCRIS] = "Cumulus Mediocris";
        cloudTypeMetadata.enums[CUMULUS_CONGESTUS] = "Cumulus Congestus";
        cloudTypeMetadata.enums[CUMULONIMBUS_CAPPILATUS] = "Cumulonumbus Cappilatus";
        cloudTypeMetadata.enums[STRATOCUMULUS] = "Stratocumulus";
        
        precipitationTypeMetadata.enums[NO_PRECIPITATION] = "No Precipitation";
        precipitationTypeMetadata.enums[RAIN] = "Rain";
        precipitationTypeMetadata.enums[DRY_SNOW] = "Dry Snow";
        precipitationTypeMetadata.enums[WET_SNOW] = "Wet Snow";
        precipitationTypeMetadata.enums[SLEET] = "Sleet";

        densityAlphaMetadata.maximum = "1.0";
        densityAlphaMetadata.minimum = "0.0";
        densityAlphaMetadata.step = "0.1";

        sizeMetadata.minimum = "1.0";
        sizeMetadata.maximum = "200000.0";
        sizeMetadata.step = "500.0";

        thicknessMetadata.minimum = "0.0";
        thicknessMetadata.step = "100.0";
        
        precipitationIntensityMetadata.minimum = "1.0";
        precipitationIntensityMetadata.maximum = "30.0";
        precipitationIntensityMetadata.step = "1.0";

        metadataInitialized = true;
    }

    cloudType.SetMetadata(&cloudTypeMetadata);
    density.SetMetadata(&densityAlphaMetadata);
    alpha.SetMetadata(&densityAlphaMetadata);
    altitude.SetMetadata(&sizeMetadata);
    width.SetMetadata(&sizeMetadata);
    length.SetMetadata(&sizeMetadata);
    thickness.SetMetadata(&thicknessMetadata);
    precipitationIntensity.SetMetadata(&precipitationIntensityMetadata);
    precipitationType.SetMetadata(&precipitationTypeMetadata);
    position.SetMetadata(&dummyMetadata);

    // Create AssetRefListenerPtr for cloudAsset.
    assetReferenceListener_ = MAKE_SHARED(AssetRefListener);
    connect(assetReferenceListener_.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(OnCloudLayerAssetLoaded(AssetPtr)));
    connect(assetReferenceListener_.get(), SIGNAL(TransferFailed(IAssetTransfer *, QString)), this, SLOT(OnCloudLayerAssetFailed(IAssetTransfer*, QString)));
    
    // Parent entity set
    connect(this, SIGNAL(ParentEntitySet()), this, SLOT(OnDetectSky()));
}

EC_MeshmoonCloudLayer::~EC_MeshmoonCloudLayer()
{
    DestroyClouds();
    parentSky_.reset();
}

void EC_MeshmoonCloudLayer::OnDetectSky()
{
    if (framework->IsHeadless())
        return;
    if (!ParentScene())
        return;

    MeshmoonSkyPtr sky = ParentEntity()->Component<EC_MeshmoonSky>();
    if (sky)
    {
        parentSky_ = sky;
        connect(sky.get(), SIGNAL(CloudCoverageChanged(EC_MeshmoonSky::CloudCoverage)), this, SLOT(OnCloudCoverageChanged(EC_MeshmoonSky::CloudCoverage)), Qt::UniqueConnection);
        connect(sky.get(), SIGNAL(AboutToDestroyClouds()), this, SLOT(DestroyClouds()), Qt::UniqueConnection);
        
        CreateClouds(cloudType.Get());
    }
    
    connect(ParentScene(), SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)), 
        this, SLOT(OnComponentAdded(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    connect(ParentScene(), SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), 
        this, SLOT(OnComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
}

void EC_MeshmoonCloudLayer::OnCloudCoverageChanged(EC_MeshmoonSky::CloudCoverage coverage)
{
    if (coverage == EC_MeshmoonSky::USE_EXTERNAL_CLOUDLAYERS)
        CreateClouds(cloudType.Get());
}

void EC_MeshmoonCloudLayer::OnComponentAdded(Entity *entity, IComponent *component, AttributeChange::Type change)
{
    if (entity == ParentEntity() && component->TypeId() == EC_MeshmoonSky::TypeIdStatic())
        OnDetectSky();
}

void EC_MeshmoonCloudLayer::OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change)
{
    if (entity == ParentEntity() && component->TypeId() == EC_MeshmoonSky::TypeIdStatic())
    {
        DestroyClouds();
        parentSky_.reset();
    }
}

void EC_MeshmoonCloudLayer::CreateClouds(int type)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(type)
#else
    loadedFromAsset_ = false;

    SilverLining::Atmosphere *atmosphere = ParentAtmosphere();
    if (!atmosphere || !IsCloudLayerComponentsEnabled())
        return;
    
    QString ref = cloudAsset.Get().ref.trimmed();
    if (ref.isEmpty())
    {
        // Create cloud layer from attribute data.
        cloudLayer_ = SilverLining::CloudLayerFactory::Create(static_cast<CloudTypes>(type));
        cloudLayerHandle_ = atmosphere->GetConditions()->AddCloudLayer(cloudLayer_);

        cloudLayer_->SetIsInfinite(infinite.Get());
        cloudLayer_->SetFadeTowardEdges(fadeTowardEdges.Get());
        cloudLayer_->SetCloudWrapping(cloudWrapping.Get());
        cloudLayer_->SetBaseAltitude(altitude.Get());
        cloudLayer_->SetThickness(thickness.Get());
        cloudLayer_->SetBaseLength(length.Get());
        cloudLayer_->SetBaseWidth(width.Get());
        cloudLayer_->SetDensity(density.Get());
        cloudLayer_->SetAlpha(alpha.Get());
        cloudLayer_->SetLayerPosition(position.Get().x, position.Get().y);
        cloudLayer_->SetEnabled(enabled.Get());
        cloudLayer_->SetPrecipitation(SilverLining::CloudLayer::NONE, 0.0);
        cloudLayer_->SetPrecipitation(static_cast<int>(precipitationType.Get()), static_cast<double>(Max(1.0f, precipitationIntensity.Get())));

        cloudLayer_->SeedClouds(*atmosphere);
    }
    else if (assetReferenceListener_->Asset())
    {
        // Create cloud layer from source file.
        const QString diskSource = QDir::toNativeSeparators(assetReferenceListener_->Asset()->DiskSource());
        if (diskSource.isEmpty())
        {
            LogError(LC + "Cloud asset disk source doees s  noot exist after download, aborting load!");
            return;
        }
        
        // Get cloud type from file.
        QFile sourceFile(diskSource);
        if (!sourceFile.open(QIODevice::ReadOnly))
        {
            LogError(LC + "Cannot detect cloud type from cloud source file, aborting load!");
            return;
        }
    
        // Get the last quint8 from the end, our stored cloud type.
        quint8 sourceType;
        QDataStream sourceStream(&sourceFile);
        sourceStream.skipRawData(sourceFile.size()-1);
        sourceStream >> sourceType;
        sourceFile.close();
        
        if (sourceType < 0 || sourceType >= NUM_CLOUD_TYPES)
        {
            LogError(LC + "Found cloud type from cloud asset that is out of range, aborting creation!");
            return;
        }
        
        loadedFromAsset_ = true;
        
        // This enum needs to be set so the correct type is created for the asset load!
        cloudType.Set(static_cast<uint>(sourceType), AttributeChange::LocalOnly);
        
        cloudLayer_ = SilverLining::CloudLayerFactory::Create(static_cast<CloudTypes>(cloudType.Get()));
        cloudLayerHandle_ = atmosphere->GetConditions()->AddCloudLayer(cloudLayer_);

        const std::string diskSourceStd = diskSource.toStdString();
        if (!cloudLayer_->Restore(*atmosphere, diskSourceStd.c_str()))
        {
            LogError(LC + "Failed to load cloud layer from source asset data!");
            DestroyClouds();
            return;
        }

        // Make our other attributes reflect the settings in the asset file that we just restored/loaded.
        double east, south;
        cloudLayer_->GetLayerPosition(east, south);
        
        position.Set(float2(east, south), AttributeChange::Disconnected);
        infinite.Set(cloudLayer_->GetIsInfinite(), AttributeChange::Disconnected);
        fadeTowardEdges.Set(cloudLayer_->GetFadeTowardEdges(), AttributeChange::Disconnected);
        cloudWrapping.Set(cloudLayer_->GetCloudWrapping(), AttributeChange::Disconnected);
        altitude.Set(cloudLayer_->GetBaseAltitude(), AttributeChange::Disconnected);
        thickness.Set(cloudLayer_->GetThickness(), AttributeChange::Disconnected);
        length.Set(cloudLayer_->GetBaseLength(), AttributeChange::Disconnected);
        width.Set(cloudLayer_->GetBaseWidth(), AttributeChange::Disconnected);
        density.Set(cloudLayer_->GetDensity(), AttributeChange::Disconnected);
        alpha.Set(cloudLayer_->GetAlpha(), AttributeChange::Disconnected);
        enabled.Set(cloudLayer_->GetEnabled(), AttributeChange::Disconnected);
        
        // We only set the one precipitation effect so we expect it to be like that in the file.
        const SL_MAP(int, double) &precipitation = cloudLayer_->GetPrecipitation();
        if (precipitation.size() >= 1 && precipitation.begin() != precipitation.end())
        {
            precipitationType.Set(static_cast<uint>(precipitation.begin()->first), AttributeChange::Disconnected);
            precipitationIntensity.Set(static_cast<float>(precipitation.begin()->second), AttributeChange::Disconnected);
        }
    }
#endif
}

void EC_MeshmoonCloudLayer::RecreateClouds(int type)
{
    DestroyClouds();
    CreateClouds(type);
}

void EC_MeshmoonCloudLayer::ReseedClouds()
{
#ifdef MESHMOON_SILVERLINING
    if (!cloudLayer_)
        return;

    SilverLining::Atmosphere *atmosphere = ParentAtmosphere();
    if (atmosphere)
        cloudLayer_->SeedClouds(*atmosphere);
#endif
}

void EC_MeshmoonCloudLayer::DestroyClouds()
{
#ifdef MESHMOON_SILVERLINING
    SilverLining::Atmosphere *atmosphere = ParentAtmosphere();
    if (atmosphere && cloudLayerHandle_ != -1)
    {
        if (!atmosphere->GetConditions()->RemoveCloudLayer(cloudLayerHandle_))
            LogError(LC + "Failed to remove cloud layer with id " + QString::number(cloudLayerHandle_));
    }
    cloudLayerHandle_ = -1;
    cloudLayer_ = 0;
    
    loadedFromAsset_ = false;
#endif
}

void EC_MeshmoonCloudLayer::SetEnabledLocal(bool enabled_, uint fadeMsecs)
{
    if (enabled.Get() != enabled_)
    {
        fadeMsecs_ = fadeMsecs;
        enabled.Set(enabled_, AttributeChange::LocalOnly);
    }
}

QByteArray EC_MeshmoonCloudLayer::Serialize()
{
#ifndef MESHMOON_SILVERLINING
    return QByteArray();
#else
    if (!cloudLayer_)
        return QByteArray();

    QString tempPath = framework->Asset()->GenerateTemporaryNonexistingAssetFilename("." + cloudLayerAssetSuffix);
    bool success = cloudLayer_->Save(tempPath.toStdString().c_str());
    if (!success)
    {
        LogError(LC + "Cannot serialize cloud layer.");
        return QByteArray();
    }

    QFile tempFile(tempPath);
    if (tempFile.open(QIODevice::ReadOnly))
    {
        // Write cloud layer type as extra data at the end of the file.
        QByteArray arr;
        QDataStream stream(arr);
        stream << tempFile.readAll();
        stream << static_cast<quint8>(cloudType.Get());
        
        tempFile.close();
        tempFile.remove();
        return arr;
    }
    else
    {
        LogError(LC + QString("Failed to open %1 for reading.").arg(tempPath));
        return QByteArray();
    }
#endif
}

bool EC_MeshmoonCloudLayer::SaveToFile(const QString& file)
{
#ifndef MESHMOON_SILVERLINING
    return false;
#else
    if (!cloudLayer_)
        return false;
        
    if (!file.endsWith(cloudLayerAssetSuffix, Qt::CaseInsensitive))
        LogWarning(LC + "Saving cloud layer to file that does not have ." + cloudLayerAssetSuffix + " suffix.");

    if (!cloudLayer_->Save(file.toStdString().c_str()))
    {
        LogError(LC + QString("Failed to save cloud layer to file %1").arg(file));
        return false;
    }

    QFile temp(file);
    if (temp.open(QIODevice::Append))
    {
        // Write cloud layer type as extra data at the end of the file.
        QDataStream stream(&temp);
        stream << static_cast<quint8>(cloudType.Get());
        temp.close();
        return true;
    }
    return false;
#endif
}

void EC_MeshmoonCloudLayer::OnCloudLayerAssetLoaded(AssetPtr asset)
{
    RecreateClouds(cloudType.Get());
}

void EC_MeshmoonCloudLayer::OnCloudLayerAssetFailed(IAssetTransfer* transfer, QString reason)
{
    LogWarning(LC + "Failed to load cloud layer from asset.");
}

SilverLining::Atmosphere *EC_MeshmoonCloudLayer::ParentAtmosphere()
{
#ifndef MESHMOON_SILVERLINING
    return 0;
#else
    MeshmoonSkyPtr sky = ParentEntity() ? ParentEntity()->Component<EC_MeshmoonSky>() : MeshmoonSkyPtr();
    if (sky)
    {
        if (parentSky_.expired())
            parentSky_ = sky;
        return sky->Atmosphere();
    }
    
    // Failed to get from parent entity. Check if our weak ptr is still alive and use it.
    // This will happen in our dtor when the sky is not removed before this component.
    if (!parentSky_.expired())
        return parentSky_.lock()->Atmosphere();
    return 0;
#endif
}

bool EC_MeshmoonCloudLayer::IsCloudLayerComponentsEnabled() const
{
    EC_MeshmoonSky *sky = ParentEntity() ? ParentEntity()->Component<EC_MeshmoonSky>().get() : 0;
    if (sky)
        return sky->IsCloudLayerComponentsEnabled();
    return false;
}

void EC_MeshmoonCloudLayer::AttributesChanged()
{
#ifdef MESHMOON_SILVERLINING
    if (framework->IsHeadless())
        return;
    
    /// If a source file is set, load from it and skip attributes (all of them?)
    if (cloudAsset.ValueChanged())
    {
        QString ref = cloudAsset.Get().ref.trimmed();
        if (!ref.isEmpty())
        {
            DestroyClouds();

            if (ref.endsWith(cloudLayerAssetSuffix, Qt::CaseInsensitive))
            {
                assetReferenceListener_->HandleAssetRefChange(framework->Asset(), ref, "Binary");
                loadedFromAsset_ = true;
            }
            else
                LogError(LC + "Asset reference suffix ." + cloudLayerAssetSuffix + " expected, got instead: " + ref);
            return;
        }
        // Destroy clouds that were loaded from asset.
        else if (loadedFromAsset_)
            DestroyClouds();
    }
    
    // We cannot allow runtime changes to be applied to a cloud layer
    // that was loaded from a source file. This is very crash prone.
    // The ref attribute needs to be cleared for the attributes to take effect.
    if (loadedFromAsset_)
        return;
        
    CloudType type = static_cast<CloudType>(cloudType.Get());

    // Layer not yet created of type has changed.
    // This will create the layer and set all attributes to it.
    if (cloudType.ValueChanged())
    {
        if (type < 0 || type >= NUM_CLOUD_TYPES)
        {
            LogWarning(LC + "Cloud type out of range. Restoring into range with CIRROCUMULUS.");
            cloudType.Set(static_cast<uint>(EC_MeshmoonCloudLayer::CIRROCUMULUS), AttributeChange::Disconnected);
        }

        // Make the editor hide attributes that are ignored for certain cloud types.
        bool cirrType = (type == CIRROCUMULUS || type == CIRRUS_FIBRATUS);
        bool enableThickness = (type != CUMULONIMBUS_CAPPILATUS);
        SetDesignable(position, !cirrType);
        SetDesignable(thickness, enableThickness);

        if (!cloudLayer_ || cloudLayer_->GetType() != static_cast<CloudTypes>(type))
        {
            RecreateClouds(cloudType.Get());
            return;
        }
    }
    if (!cloudLayer_)
        return;
        
    /// Modifying certain properties requires us to recreate or re-seed depending on the cloud type.
    bool triggersChanged = (altitude.ValueChanged()  || length.ValueChanged()  || width.ValueChanged() ||
                            thickness.ValueChanged() || density.ValueChanged() || position.ValueChanged());

    if (triggersChanged && (type == CIRROCUMULUS || type == CIRRUS_FIBRATUS))
    {
        RecreateClouds(cloudType.Get());
        return;
    }

    // Update properties to already created cloud layer.
    if (infinite.ValueChanged())
        cloudLayer_->SetIsInfinite(infinite.Get());
    if (fadeTowardEdges.ValueChanged())
        cloudLayer_->SetFadeTowardEdges(fadeTowardEdges.Get());
    if (cloudWrapping.ValueChanged())
        cloudLayer_->SetCloudWrapping(cloudWrapping.Get());
    if (altitude.ValueChanged())
        cloudLayer_->SetBaseAltitude(altitude.Get());
    if (length.ValueChanged())
        cloudLayer_->SetBaseLength(length.Get());
    if (width.ValueChanged())
        cloudLayer_->SetBaseWidth(width.Get());
    if (thickness.ValueChanged())
        cloudLayer_->SetThickness(thickness.Get());
    if (density.ValueChanged())
        cloudLayer_->SetDensity(density.Get());
    if (alpha.ValueChanged())
        cloudLayer_->SetAlpha(alpha.Get());
    if (position.ValueChanged())
        cloudLayer_->SetLayerPosition(position.Get().x, position.Get().y);
    if (enabled.ValueChanged())
    {
        cloudLayer_->SetEnabled(enabled.Get(), fadeMsecs_);
        fadeMsecs_ = 350;
    }
    if (precipitationType.ValueChanged() || precipitationIntensity.ValueChanged())
    {
        cloudLayer_->SetPrecipitation(SilverLining::CloudLayer::NONE, 0.0);
        cloudLayer_->SetPrecipitation(static_cast<int>(precipitationType.Get()), static_cast<double>(Max(1.0f, precipitationIntensity.Get())));
    }

    /// Modifying certain properties requires us to re-seed the layer, but not recreate it.
    if (triggersChanged)
        ReseedClouds();
#endif
}

template<typename T> 
void EC_MeshmoonCloudLayer::SetDesignable(Attribute<T> &attribute, bool designable)
{
    if (!attribute.Metadata())
    {
        LogError(LC + "Cannot set designable for attribute without metadata!");
        return;
    }
    if (attribute.Metadata()->designable != designable)
    {
        attribute.Metadata()->designable = designable;
        attribute.EmitAttributeMetadataChanged();
    }
}
