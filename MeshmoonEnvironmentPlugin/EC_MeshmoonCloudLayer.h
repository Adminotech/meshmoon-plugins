/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#pragma once

#include "MeshmoonEnvironmentPluginApi.h"
#include "MeshmoonEnvironmentPluginFwd.h"

#include "EC_MeshmoonSky.h"
#include "IComponent.h"
#include "Math/float2.h"

#include "AssetReference.h"
#include "AssetRefListener.h"

/// Adds cloud layer to EC_MeshmoonSky.
class MESHMOON_ENVIRONMENT_API EC_MeshmoonCloudLayer : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonCloudLayer", 508)

public:
    /// @todo Use upper camel case
    enum CloudType
    {
        CIRROCUMULUS = 0,           ///<  High planar cloud puffs.
        CIRRUS_FIBRATUS,            ///<  High, thicker and fibrous clouds that signal changing weather.
        STRATUS,                    ///<  Low clouds represented as a slab.
        CUMULUS_MEDIOCRIS,          ///<  Low, puffy clouds on fair days.
        CUMULUS_CONGESTUS,          ///<  Large cumulus clouds that could turn into a thunderhead.
        CUMULONIMBUS_CAPPILATUS,    ///<  Big storm clouds.
        STRATOCUMULUS               ///<  Low, dense, puffy clouds with some sun breaks between them.
    };

    /// @todo Use upper camel case
    enum PrecipitationType
    {
        NO_PRECIPITATION = 0,       ///< No precipitation.
        RAIN,                       ///< Rain.
        DRY_SNOW,                   ///< Dry snow.
        WET_SNOW,                   ///< Wet snow.
        SLEET                       ///< Sleet.
    };

    /// Cloud type. Default: CUMULUS_MEDIOCRIS.
    /** @see CloudType enum. */
    Q_PROPERTY(uint cloudType READ getcloudType WRITE setcloudType);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, cloudType);

    /// Precipitation type. Default: NO_PRECIPITATION.
    /** @see PrecipitationType enum. */
    Q_PROPERTY(uint precipitationType READ getprecipitationType WRITE setprecipitationType);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, precipitationType);

    /// Precipitation type with range of [1,30]. Default: 1.0.
    /** If precipitationType attribute is NO_PRECIPITATION, this attribute has no effect. */
    Q_PROPERTY(float precipitationIntensity READ getprecipitationIntensity WRITE setprecipitationIntensity);
    DEFINE_QPROPERTY_ATTRIBUTE(float, precipitationIntensity);    

    Q_PROPERTY(bool infinite READ getinfinite WRITE setinfinite);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, infinite);

    Q_PROPERTY(bool fadeTowardEdges READ getfadeTowardEdges WRITE setfadeTowardEdges);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, fadeTowardEdges);

    Q_PROPERTY(bool cloudWrapping READ getcloudWrapping WRITE setcloudWrapping);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, cloudWrapping);

    Q_PROPERTY(float altitude READ getaltitude WRITE setaltitude);
    DEFINE_QPROPERTY_ATTRIBUTE(float, altitude);

    Q_PROPERTY(float length READ getlength WRITE setlength);
    DEFINE_QPROPERTY_ATTRIBUTE(float, length);

    Q_PROPERTY(float width READ getwidth WRITE setwidth);
    DEFINE_QPROPERTY_ATTRIBUTE(float, width);
    
    Q_PROPERTY(float thickness READ getthickness WRITE setthickness);
    DEFINE_QPROPERTY_ATTRIBUTE(float, thickness);

    Q_PROPERTY(float density READ getdensity WRITE setdensity);
    DEFINE_QPROPERTY_ATTRIBUTE(float, density);

    Q_PROPERTY(float alpha READ getalpha WRITE setalpha);
    DEFINE_QPROPERTY_ATTRIBUTE(float, alpha);

    Q_PROPERTY(float2 position READ getposition WRITE setposition);
    DEFINE_QPROPERTY_ATTRIBUTE(float2, position);

    Q_PROPERTY(bool enabled READ getenabled WRITE setenabled);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, enabled);

    Q_PROPERTY(AssetReference cloudAsset READ getcloudAsset WRITE setcloudAsset);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReference, cloudAsset);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonCloudLayer(Scene* scene);
    ~EC_MeshmoonCloudLayer();
    /// @endcond

public slots:
    /// Enables and disables this cloud layer locally with additional fade time in milliseconds.
    /** This will set the enabled attribute as LocalOnly, it wont replicate over the network.
        @note If you want to use the default fade time or replicate the change, 
        use the enabled attribute setter instead. */
    void SetEnabledLocal(bool enabled_, uint fadeMsecs = 350);

    /// Serializes current cloud layer and return the data.
    QByteArray Serialize();
    
    /// Serializes current cloud layer to a file.
    /** @note You should use the .meshmooncloudlayer suffix on your filename.
        This ensures the asset is usable in this components asset reference. */
    bool SaveToFile(const QString& file);
    
private slots:
    /// Creates clouds with type.
    void CreateClouds(int type);
    
    /// Destroys current clouds and creates clouds with type.
    void RecreateClouds(int type);
    
    /// Reseeds existing clouds.
    void ReseedClouds();
    
    /// Destroys current clouds.
    void DestroyClouds();

    /// Detects parent entitys EC_MeshmoonSky.
    void OnDetectSky();
    
    /// Handler for EC_MeshmoonSky signal.
    void OnCloudCoverageChanged(EC_MeshmoonSky::CloudCoverage coverage);
    
    /// Detect parent entitys EC_MeshmoonSky additions.
    void OnComponentAdded(Entity *entity, IComponent *component, AttributeChange::Type change);
    
    /// Detect parent entitys EC_MeshmoonSky removals.
    void OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change);
    
    /// Asset based cloud layer handlers.
    void OnCloudLayerAssetLoaded(AssetPtr asset);
    void OnCloudLayerAssetFailed(IAssetTransfer* transfer, QString reason);

private:
    /// IComponent override.
    void AttributesChanged();

    /// Atmosphere from parent entitys EC_MeshmoonSky.
    SilverLining::Atmosphere *ParentAtmosphere();
    
    /// Is external cloud layers enabled in the parent entitys EC_MeshmoonSky.
    bool IsCloudLayerComponentsEnabled() const;

    /// Sets designable to attribute metadata.
    template<typename T> 
    void SetDesignable(Attribute<T> &attribute, bool designable);
    
    /// Boolean that tracks if current clouds have been loaded from asset.
    /** We wont allow any runtime modifications of attributes if loaded from asset. */
    bool loadedFromAsset_;

    /// Logging channel.
    const QString LC;

    /// Cloud asset ref listener.
    AssetRefListenerPtr assetReferenceListener_;    
    
    /// Cloud layer handle.
    int cloudLayerHandle_; 
    
    /// Used for set enabled fade time.
    int fadeMsecs_;
    
    /// Cloud layer ptr.  
    SilverLining::CloudLayer *cloudLayer_;
    
    /// Weak ptr for parent entitys EC_MeshmoonSky.
    MeshmoonSkyWeakPtr parentSky_;
};
COMPONENT_TYPEDEFS(MeshmoonCloudLayer);
