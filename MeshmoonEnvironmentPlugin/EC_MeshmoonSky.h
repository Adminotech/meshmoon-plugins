/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#pragma once

#include "MeshmoonEnvironmentPluginApi.h"
#include "MeshmoonEnvironmentPluginFwd.h"

#include "OgreModuleFwd.h"
#include "AssetFwd.h"
#include "IComponent.h"
#include "Math/float2.h"

class MeshmoonSkyRenderSystemListener;

/// Adds a photorealistic sky to the scene.
class MESHMOON_ENVIRONMENT_API EC_MeshmoonSky : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonSky", 507) // Note this is the closed source EC Meshmoon range ID.

public:
    /// Clouds coverage enumeration.
    /// @todo Use upper camel case
    enum CloudCoverage
    {
        USE_EXTERNAL_CLOUDLAYERS = 0, ///< Use EC_MeshmoonCloudLayer components from the parent entity to create clouds.
        NO_CLOUDS                = 1, ///< No clouds.
        FAIR                     = 2, ///< Fair amount of clouds.
        PARTLY_CLOUDY            = 3, ///< Partly cloudy.
        MOSTLY_CLOUDY            = 4, ///< Mostly cloudy.
        OVERCAST                 = 5, ///< Overcast.
        STORM                    = 6  ///< Storm.
    };
    
    /// An enumeration of defined time zones worldwide.
    /** Time zones are expressed as the hour correction (prior to daylight savings adjustments)
        from GMT. This enum provides names for the civilian time zones, and notes their military
        equivalents.
        @todo Use upper camel case
        @todo Idea: would "BangladeshStandard" or "UtcPlus6" be nicer instead of the "cryptic" abbreviation.*/
    enum TimeZone
    {
        // 0 -> 12
        GMT  = 0,   ///< UTC Greenwich Mean Time, UTC, Western European (WET)
        CET  = 1,   ///< UTC+1 Central European
        EET  = 2,   ///< UTC+2 Eastern European
        BT   = 3,   ///< UTC+3 Baghdad Time 
        GET  = 4,   ///< UTC+4 Georgia Standard 
        PKT  = 5,   ///< UTC+5 Pakistan Standard 
        BST  = 6,   ///< UTC+6 Bangladesh Standard
        THA  = 7,   ///< UTC+7 Thailand Standard
        CCT  = 8,   ///< UTC+8 China Coast
        JST  = 9,   ///< UTC+9 Japan Standard
        GST  = 10,  ///< UTC+10 Guam Standard
        SBT  = 11,  ///< UTC+11 Solomon Islands
        IDLE = 12,  ///< UTC+12 International Date Line East, NZST (New Zealand Standard)
        // -1 -> -12
        WAT  = 13,  ///< UTC-1 West Africa
        AT   = 14,  ///< UTC-2 Azores
        BRT  = 15,  ///< UTC-3 Brasilia
        AST  = 16,  ///< UTC-4 Atlantic Standard
        EST  = 17,  ///< UTC-5 Eastern Standard
        CST  = 18,  ///< UTC-6 Central Standard
        MST  = 19,  ///< UTC-7 Mountain Standard
        PST  = 20,  ///< UTC-8 Pacific Standard
        YST  = 21,  ///< UTC-9 Yukon Standard
        AHST = 22,  ///< UTC-10 Alaska-Hawaii Standard; Central Alaska (CAT), Hawaii Std (HST)
        NT   = 23,  ///< UTC-11 Nome
        IDLW = 24,  ///< UTC-12 International Date Line West

        USE_LONGITUDE = 25 ///< Use the longitude attribute instead of time zone.
    };
    
    /// An enumeration of time/clock modes. @see timeMode attribute.
    /** @todo Use upper camel case */
    enum TimeMode
    {
        /// Clock runs in real time. 
        /** Uses timezone, longitude, latitude attributes and current date. Date, time and timeMultiplier attributes are ignored. */
        REAL_TIME      = 0,
        /// Clock runs with simulated time. 
        /** Uses date, time and timeMultiplier attributes. Location and time zone from longitude and latitude. Timezone attribute is ignored.
            Use this mode to set a custom date and time. Time multiplier <1.0 stops clock progression (static time), 1.0 runs clock in real 
            time and >1.0 a sped up clock. 
            @see The timeMultiplier attribute. */
        SIMULATED_TIME = 1
    };

    /// Cloud coverage. Default: USE_EXTERNAL_CLOUDLAYERS.
    /** @see CloudCoverage enum. */
    Q_PROPERTY(uint cloudCoverage READ getcloudCoverage WRITE setcloudCoverage);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, cloudCoverage);

    /// Time zone. Default: USE_LONGITUDE.
    /** USE_LONGITUDE will use the longitude attribute to approximate the time zone [-12,12] with longitude/15.0.
        If any other enumeration [-12,12] is used eg. EET 'UTC+2' the longitude attribute will be ignored and replaced with timeZone*15.0.
        @see TimeZone enum. */
    Q_PROPERTY(uint timeZone READ gettimeZone WRITE settimeZone);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, timeZone);

    /// Time/clock mode. Default: REAL_TIME.
    /** @see TimeMode enum */
    Q_PROPERTY(uint timeMode READ gettimeMode WRITE settimeMode);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, timeMode);

    /// Longitude world coordinate. Range [-180,180]. Default: 0.0.
    /** This attribute is ignored if any other than USE_LONGITUDE is defined as timeZone attribute.
        If timeZone is USE_LONGITUDE the time zone [-12,12] will be approximated with longitude/15.0. */
    Q_PROPERTY(float longitude READ getlongitude WRITE setlongitude);
    DEFINE_QPROPERTY_ATTRIBUTE(float, longitude);

    /// Latitude world coordinate. Range [-90,90]. Default: 0.0.
    Q_PROPERTY(float latitude READ getlatitude WRITE setlatitude);
    DEFINE_QPROPERTY_ATTRIBUTE(float, latitude);

    /// God ray intensity. Default: 0.7.
    Q_PROPERTY(float godRayIntensity READ getgodRayIntensity WRITE setgodRayIntensity);
    DEFINE_QPROPERTY_ATTRIBUTE(float, godRayIntensity);

    /// Sun/Moon light intensity reflected to the scene. Default 0.5.
    Q_PROPERTY(float lightSourceBrightness READ getlightSourceBrightness WRITE setlightSourceBrightness);
    DEFINE_QPROPERTY_ATTRIBUTE(float, lightSourceBrightness);    

    /// Gamma for sky rendering. Default: 1.8.
    Q_PROPERTY(float gamma READ getgamma WRITE setgamma);
    DEFINE_QPROPERTY_ATTRIBUTE(float, gamma);  

    /// Wind direction, in degrees from North. Default: 0.0.
    Q_PROPERTY(float windDirection READ getwindDirection WRITE setwindDirection);
    DEFINE_QPROPERTY_ATTRIBUTE(float, windDirection);

    /// Wind speed in meters per second. Default: 0.0.
    Q_PROPERTY(float windSpeed READ getwindSpeed WRITE setwindSpeed);
    DEFINE_QPROPERTY_ATTRIBUTE(float, windSpeed);

    /// If lens flares are enabled. Default: false.
    Q_PROPERTY(bool lensFlares READ getlensFlares WRITE setlensFlares);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, lensFlares);

    /// Current time. Range [0.0-24.0] with >=24.0 interpreted as 0.0. Default: 14.0.
    /** Used when timeMode attribute is SIMULATED_TIME. */
    Q_PROPERTY(float time READ gettime WRITE settime);
    DEFINE_QPROPERTY_ATTRIBUTE(float, time);

    /// Time multiplier. Range >=0.0. Default: 1.0.
    /** timeMultiplier = <1.0 -> Time does not progress, static time.
        timeMultiplier = 2.0 -> 1 real time second equals to 2 seconds of simulated time.
        timeMultiplier = 60.0 -> 1 real time second equals to 60 seconds (1 minute) of simulated time.
        Used when timeMode attribute is SIMULATED_TIME. */
    Q_PROPERTY(float timeMultiplier READ gettimeMultiplier WRITE settimeMultiplier);
    DEFINE_QPROPERTY_ATTRIBUTE(float, timeMultiplier);

    /// Date as a string in 'DD.MM.YYYY' or 'DD/MM/YYYY' format. Default: Current system date.
    /** Used when timeMode attribute is SIMULATED_TIME. Empty value gets date from current system. */
    Q_PROPERTY(QString date READ getdate WRITE setdate);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, date);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonSky(Scene *scene);
    ~EC_MeshmoonSky();

#ifdef MESHMOON_SILVERLINING
    friend class MeshmoonSkyRenderQueueListener;
    friend class MeshmoonSkyRenderSystemListener;
#endif

    /// Returns the Ogre light for this sky.
    Ogre::Light *OgreLight() const;

    /// Returns the atmosphere.
    SilverLining::Atmosphere *Atmosphere() const;

    EC_Camera *ActiveCamera() const;

    /// Returns if cloud coverage is USE_EXTERNAL_CLOUDLAYERS.
    bool IsCloudLayerComponentsEnabled() const;
    /// @endcond

public slots:
    /// Creates a cloud layer with given asset.
    /** @param Cloud layer .meshmooncloudlayer asset.
        @return Created cloud layer handle, -1 if failed. */
    int CreateCloudLayer(AssetPtr asset);
    
    /// Creates a cloud layer with given layer file.
    /** @note EC_MeshmoonCloudLayer::SaveToFile and Serialize can be used to create these cloud assets.
        @param Absolute path to disk source to a .meshmooncloudlayer asset.
        @return Created cloud layer handle, -1 if failed. */
    int CreateCloudLayer(const QString &diskSource);
    
    /** Creates cloud layer from raw asset data.
        @note This raw data should still be originating from a .meshmooncloudlayer asset.
        @param Raw cloud layer data.
        @return Created cloud layer handle, -1 if failed.*/
    int CreateCloudLayer(const QByteArray &data);
    
    /// Creates a cloud layer with given properties.
    /** @note If the cloud type or for any reason the clouds are recreated
        after this call, the layer will be removed. Connect to CloudsCreated 
        signal to recreate your custom clouds.
        @param Type that matches to EC_MeshmoonCloudLayer::CloudType enum. 
        @param Altitude in meters.
        @param Thickness.
        @param Base width in meters. 
        @param Base lenght in meters.
        @param Density.
        @param Alpha.
        @param Position in world coordinates. 
        @param If layer is infinite.
        @return Created cloud layer handle, -1 if failed.*/
    int CreateCloudLayer(uint type, double altitude = 5000, double thickness = 100, 
                         double baseWidth = 3000.0, double baseLength = 3000.0, double density = 1.0, double alpha = 0.8,
                         float2 position = float2::zero, bool infinite = true);

signals:
    /// Cloud coverage type changed.
    void CloudCoverageChanged(EC_MeshmoonSky::CloudCoverage coverage);

    /// About to destroy all clouds for this skys atmosphere.
    void AboutToDestroyClouds();
    
    /// Clouds have now been created.
    /** If you added custom layers from CreateCloudLayer now 
        is the time to add them again if you so choose. */
    void CloudsCreated();

private slots:
    /// Parent set handler.
    void OnParentEntitySet();

    /// Component main update.
    void OnUpdate(float frametime);
    
    /// Recreates the atmosphere and clouds.
    void RecreateSkyAndClouds();

    /// Initializes the atmosphere. Removes current sky if any.
    void CreateSky();
    
    /// Destroys clouds and atmosphere.
    void DestroySky();
    
    /// Create clouds.
    void CreateClouds(CloudCoverage cloudCoverage);
        
    /// Destroys clouds.
    void DestroyClouds();
    
    /// Create Ogre light for silverlining.
    void CreateOgreLight();
    
    /// Destroy Ogre light for silverlining.
    void DestroyOgreLight();
    
    /// Active camera changed.
    void OnActiveCameraChanged(Entity *cameraEnt);
    
    /// Updates atmosphere with all attributes.
    /** Calls all existing specialized Update*() functions. */
    void Update();
    
    /// Update time from the attributes to atmosphere.
    void UpdateTime(bool forced = false);
    
    /// Update weather condition attributes to atmosphere.
    void UpdateWeatherConditions();

private:
    /// Render system listener notified here if device loss happens.
    void DeviceLost();
    
    /// Render system listener notified here if device is restored after a device loss.
    void DeviceRestored();
    
    /// Returns the god ray intensity.
    /** @note This function will return 0.0f if Rocket graphics setting is not high enough for god rays! */
    float GodRayIntensity();

    bool HasLowGraphics() const;
    bool HasMediumGraphics() const;
    bool HasHighGraphics() const;
    bool HasUltraGraphics() const; ///< @note Custom setting will report Ultra here!

    /// IComponent override.
    void AttributesChanged();

    /// Set attributes 
    bool SetCloudLayerAttributes(SilverLining::CloudLayer* layer, double altitude, double thickness, 
                                 double baseWidth, double baseLength, double density, double alpha,
                                 float2 position, bool infinite);

    /// Sets designable metadata to attribute.
    template<typename T> 
    void SetDesignable(Attribute<T> &attribute, bool designable);

    /// Return current Ogre worlds scene.
    Ogre::SceneManager* OgreSceneManager() const;

    struct Impl;
    Impl *impl;
    /// Log channel.
    const QString LC;
};
COMPONENT_TYPEDEFS(MeshmoonSky);
