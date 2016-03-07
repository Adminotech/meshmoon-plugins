/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#include "StableHeaders.h"
#include "EC_Camera.h"
#include "EC_MeshmoonSky.h"
#include "EC_MeshmoonCloudLayer.h"
#include "MeshmoonSkyUtils.h"

#include "Math/float3.h"
#include "Math/MathFunc.h"
#include "AttributeMetadata.h"
#include "Application.h"
#include "Framework.h"
#include "Profiler.h"
#include "Entity.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "FrameAPI.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "ConfigAPI.h"
#include "OgreWorld.h"
#include "OgreRenderingModule.h"
#include "Renderer.h"

#ifdef DIRECTX_ENABLED
#undef SAFE_DELETE
#undef SAFE_DELETE_ARRAY
#include <OgreD3D9RenderWindow.h>
#endif

#ifdef MESHMOON_SILVERLINING
// Suppress warnings leaking from SilverLining.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100) 
#endif
#include <SilverLining.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <LocalTime.h>
#include <Location.h>
#include <WindVolume.h>
#endif // ~ifndef MESHMOON_SILVERLINING

// SilverLining license code and username
static const char* SILVERLINING_USERNAME        = "";
static const char* SILVERLINING_LICENCE_KEY     = "";
static const float TIME_SIMULATED_SECOND        = (1.0f / 60.0f) / 60.0f; // (1 hour / mins) / secs
static const float TIME_SIMULATED_INTERVAL      = (1.0f / 60.0f); // 60 fps
static const int   TIME_RELIGHT_INTERVAL_MSEC   = 33; // ~30fps

struct EC_MeshmoonSky::Impl
{
    Impl() :
        activeCamera(0),
#ifdef MESHMOON_SILVERLINING
        atmosphere(0),
        renderQueueListener(0),
        renderSystemListener(0),
#endif
        cloudCoverage(static_cast<EC_MeshmoonSky::CloudCoverage>(-1)),
        tSimulatedTime(0.0f),
        rocketGraphicsMode(5)
    {
    }

    OgreRenderer::RendererPtr renderer;
    EC_Camera *activeCamera;
    // Current cloud type
    CloudCoverage cloudCoverage;
    OgreWorldWeakPtr world;
    std::string silverLiningLightName;
    float tSimulatedTime;
    int rocketGraphicsMode;

    // Silverlining objects.
#ifdef MESHMOON_SILVERLINING
    MeshmoonSkyRenderQueueListener *renderQueueListener;
    MeshmoonSkyRenderSystemListener *renderSystemListener;
    SilverLining::Atmosphere *atmosphere;
    SilverLining::LocalTime silverLiningTime;
    SilverLining::Location silverLiningLocation;
    SilverLining::WindVolume silverLiningWindConditions;
#endif
};

EC_MeshmoonSky::EC_MeshmoonSky(Scene *scene) :
    IComponent(scene),
    INIT_ATTRIBUTE_VALUE(cloudCoverage, "Cloud Coverage", static_cast<uint>(USE_EXTERNAL_CLOUDLAYERS)),
    INIT_ATTRIBUTE_VALUE(timeZone, "Time Zone", static_cast<uint>(USE_LONGITUDE)),
    INIT_ATTRIBUTE_VALUE(timeMode, "Time Mode", static_cast<uint>(REAL_TIME)),
    INIT_ATTRIBUTE_VALUE(godRayIntensity, "God Ray Intensity", 0.7f),
    INIT_ATTRIBUTE_VALUE(lightSourceBrightness, "Light Source Intensity", 0.5f),
    INIT_ATTRIBUTE_VALUE(gamma, "Gamma", 1.8f),
    INIT_ATTRIBUTE_VALUE(longitude, "Longitude", 0.0f),
    INIT_ATTRIBUTE_VALUE(latitude, "Latitude", 0.0f),
    INIT_ATTRIBUTE_VALUE(windDirection, "Wind Direction in Degrees", 0.0f),
    INIT_ATTRIBUTE_VALUE(windSpeed, "Wind Speed m/s", 0.0f),
    INIT_ATTRIBUTE_VALUE(lensFlares, "Lens Flares", false),
    INIT_ATTRIBUTE_VALUE(time, "Time", 14.0f),
    INIT_ATTRIBUTE_VALUE(timeMultiplier, "Time Multiplier", 1.0f),
    INIT_ATTRIBUTE_VALUE(date, "Date", ""),
    impl(new Impl),
    LC("[EC_MeshmoonSky]: ")
{
    static AttributeMetadata cloudsMetadata, timeZoneMetadata, lightSourceBrightnessMetadata, godRayIntensityMetadata,
                             timeModeMetadata, timeMetadata, timeMultiplierMetadata, latitudeMetadata, 
                             longitudeMetadata, gammaMetadata, dateDummyMetadata;
    static bool metadataInitialized = false;
    if (!metadataInitialized)
    {
        cloudsMetadata.enums[USE_EXTERNAL_CLOUDLAYERS] = "Use Cloud Layer Components";
        cloudsMetadata.enums[NO_CLOUDS] = "No Clouds";
        cloudsMetadata.enums[FAIR] = "Fair";
        cloudsMetadata.enums[PARTLY_CLOUDY] = "Partly Cloudy";
        cloudsMetadata.enums[MOSTLY_CLOUDY] = "Mostly Cloudy";
        cloudsMetadata.enums[OVERCAST] = "Overcast";
        cloudsMetadata.enums[STORM] = "Stormy Clouds";

        // 0 -> 12
        timeZoneMetadata.enums[GMT]  = "UTC+0 Greenwich Mean Time, UTC, Western European (WET)";
        timeZoneMetadata.enums[CET]  = "UTC+1 Central European";
        timeZoneMetadata.enums[EET]  = "UTC+2 Eastern European";
        timeZoneMetadata.enums[BT]   = "UTC+3 Baghdad Time";
        timeZoneMetadata.enums[GET]  = "UTC+4 Georgia Standard";
        timeZoneMetadata.enums[PKT]  = "UTC+5 Pakistan Standard";
        timeZoneMetadata.enums[BST]  = "UTC+6 Bangladesh Standard";
        timeZoneMetadata.enums[THA]  = "UTC+7 Thailand Standard";
        timeZoneMetadata.enums[CCT]  = "UTC+8 China Coast";
        timeZoneMetadata.enums[JST]  = "UTC+9 Japan Standard";
        timeZoneMetadata.enums[GST]  = "UTC+10 Guam Standard";
        timeZoneMetadata.enums[SBT]  = "UTC+11 Solomon Island";
        timeZoneMetadata.enums[IDLE] = "UTC+12 International Date Line East, NZST (New Zealand Standard)";
        // -1 -> -12
        timeZoneMetadata.enums[WAT]  = "UTC-1 West Africa";
        timeZoneMetadata.enums[AT]   = "UTC-2 Azores";
        timeZoneMetadata.enums[BRT]  = "UTC-3 Brasilia";
        timeZoneMetadata.enums[AST]  = "UTC-4 Atlantic Standard";
        timeZoneMetadata.enums[EST]  = "UTC-5 Eastern Standard";
        timeZoneMetadata.enums[CST]  = "UTC-6 Central Standard";
        timeZoneMetadata.enums[MST]  = "UTC-7 Mountain Standard";
        timeZoneMetadata.enums[PST]  = "UTC-8 Pacific Standard";
        timeZoneMetadata.enums[YST]  = "UTC-9 Yukon Standard";
        timeZoneMetadata.enums[AHST] = "UTC-10 Alaska-Hawaii Standard; Central Alaska (CAT), Hawaii Std (HST)";
        timeZoneMetadata.enums[NT]   = "UTC-11 Nome";
        timeZoneMetadata.enums[IDLW] = "UTC-12 International Date Line West";
        // From longitude
        timeZoneMetadata.enums[USE_LONGITUDE] = "Detect Time Zone from Longitude";

        timeModeMetadata.enums[REAL_TIME] = "Real-time";
        timeModeMetadata.enums[SIMULATED_TIME] = "Simulated time";
        
        godRayIntensityMetadata.step = "0.01";
        godRayIntensityMetadata.minimum = "0.0";
        
        lightSourceBrightnessMetadata.step = "0.1";
        lightSourceBrightnessMetadata.minimum = "0.0";
        lightSourceBrightnessMetadata.maximum = "1.0";

        timeMetadata.step = "0.1";
        timeMetadata.minimum = "0.0";
        timeMetadata.maximum = "24.0";

        timeMultiplierMetadata.step = "0.1";
        timeMultiplierMetadata.minimum = "0.0";

        latitudeMetadata.step = "15.0";
        latitudeMetadata.minimum = "-90.0";
        latitudeMetadata.maximum = "90.0";

        longitudeMetadata.step = "15.0";
        longitudeMetadata.minimum = "-180.0";
        longitudeMetadata.maximum = "180.0";
        
        gammaMetadata.step = "0.1";
        gammaMetadata.minimum = "0.5";
        gammaMetadata.maximum = "50.0";

        metadataInitialized = true;
    }

    cloudCoverage.SetMetadata(&cloudsMetadata);
    timeZone.SetMetadata(&timeZoneMetadata);
    timeMode.SetMetadata(&timeModeMetadata);
    godRayIntensity.SetMetadata(&godRayIntensityMetadata);
    lightSourceBrightness.SetMetadata(&lightSourceBrightnessMetadata);
    time.SetMetadata(&timeMetadata);
    timeMultiplier.SetMetadata(&timeMultiplierMetadata);
    latitude.SetMetadata(&latitudeMetadata);
    longitude.SetMetadata(&longitudeMetadata);
    date.SetMetadata(&dateDummyMetadata);
    gamma.SetMetadata(&gammaMetadata);

    connect(this, SIGNAL(ParentEntitySet()), this, SLOT(OnParentEntitySet()));
}

EC_MeshmoonSky::~EC_MeshmoonSky()
{
    DestroySky();
    delete impl;
}

void EC_MeshmoonSky::OnParentEntitySet()
{
    if (framework->IsHeadless())
        return;
        
    impl->rocketGraphicsMode = framework->Config()->Read("adminotech", "clientplugin", "graphicsmode", 5).toInt();
    impl->world = ParentScene()->Subsystem<OgreWorld>();

    /// Initialize active camera and listen to camera changes.
    OgreRenderingModule* renderingModule = framework->Module<OgreRenderingModule>();
    impl->renderer = renderingModule ? renderingModule->Renderer() : OgreRenderer::RendererPtr();
    if (!impl->renderer.get())
        return;

    connect(impl->renderer.get(), SIGNAL(DeviceCreated()), this, SLOT(RecreateSkyAndClouds()), Qt::UniqueConnection);
    connect(impl->renderer.get(), SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)), Qt::UniqueConnection);
    connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);

    OnActiveCameraChanged(impl->renderer->MainCamera());

    CreateSky();
}

void EC_MeshmoonSky::OnUpdate(float frametime)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(frametime)
#else
    if (!impl->atmosphere)
        return;
        
    PROFILE(EC_MeshmoonSky_Update)

    Ogre::Light *light = OgreLight();
    if (light)
    {
        PROFILE(EC_MeshmoonSky_Lights)
        
        Ogre::ColourValue lightColor;
        impl->atmosphere->GetSunOrMoonColor(&lightColor.r, &lightColor.g, &lightColor.b);

        // Manually apply brightness multiplier to the sun diffuse color
        float brightness = Max(Min(lightSourceBrightness.Get(), 1.0f), 1e-3f);
        lightColor.r *= brightness;
        lightColor.g *= brightness;
        lightColor.b *= brightness;
        light->setDiffuseColour(lightColor);
        light->setSpecularColour(lightColor * 0.5);

        float3 lightDirection;
        impl->atmosphere->GetSunOrMoonPosition(&lightDirection.x, &lightDirection.y, &lightDirection.z);
        light->setDirection(ToOgreVector(lightDirection.Neg())); // lightDirection is Earth-To-LightSource, flip for LightSource-To-Earth.
        
        Ogre::SceneManager *ogreSceneManager = OgreSceneManager();
        if (ogreSceneManager)
        {
            Ogre::ColourValue ambientColor;
            impl->atmosphere->GetAmbientColor(&ambientColor.r, &ambientColor.g, &ambientColor.b);
            ogreSceneManager->setAmbientLight(ambientColor);
        }
    }
    
    if (static_cast<TimeMode>(timeMode.Get()) == SIMULATED_TIME)
    {
        // We want to limit updating interval as it will trigger setting time
        // to SilverLining that will in turn trigger relighting the scene which may be expensive.
        impl->tSimulatedTime += frametime;
        if (impl->tSimulatedTime >= TIME_SIMULATED_INTERVAL)
        {
            // <1.0 we dont progress time at all.
            //  1.0 sets to the library to progress clock automatically in real time.
            // >1.0 sped up clock.
            float multiplier = timeMultiplier.Get();
            if (multiplier > 1.0f)
            {
                PROFILE(EC_MeshmoonSky_SimulatedTime)
                time.Set(time.Get() + ((TIME_SIMULATED_SECOND * impl->tSimulatedTime) * multiplier), AttributeChange::LocalOnly);
            }
            impl->tSimulatedTime = 0.0f;
        }
    }
#endif
}

void EC_MeshmoonSky::CreateOgreLight()
{
#ifdef MESHMOON_SILVERLINING
    if (!OgreSceneManager() || impl->world.expired())
        return;
        
    DestroyOgreLight();

    impl->silverLiningLightName = impl->world.lock()->GenerateUniqueObjectName("Meshmoon_EC_MeshmoonSky_SilverLiningLight");
    Ogre::Light *silverLiningLight = OgreSceneManager()->createLight(impl->silverLiningLightName);
    if (silverLiningLight)
    {
        silverLiningLight->setType(Ogre::Light::LT_DIRECTIONAL);
        silverLiningLight->setSpecularColour(0.0f,0.0f,0.0f);
        silverLiningLight->setCastShadows(true);
    }
    else
        impl->silverLiningLightName = "";
#endif
}

void EC_MeshmoonSky::DestroyOgreLight()
{
    if (OgreLight())
        OgreSceneManager()->destroyLight(impl->silverLiningLightName);
    impl->silverLiningLightName = "";
}

int EC_MeshmoonSky::CreateCloudLayer(AssetPtr asset)
{
    if (!asset.get())
        return -1;

    if (!asset->DiskSource().isEmpty())
    {
        int layerId = CreateCloudLayer(asset->DiskSource());
        if (layerId != -1)
            return layerId;
    }
    return CreateCloudLayer(asset->RawData());
}

int EC_MeshmoonSky::CreateCloudLayer(const QByteArray &data)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(data)
   return -1;
#else
    if (!impl->atmosphere)
        return -1;

    if (data.size() <= 0)
    {
        LogError(LC + "Cannot detect cloud type from cloud source file, aborting load!");
        return -1;
    }

    // Get the last quint8 from the end, our stored cloud type.
    quint8 sourceType;
    QDataStream sourceStream(data);
    sourceStream.skipRawData(data.size()-1);
    sourceStream >> sourceType;

    /*    
    SilverLining::CloudLayer *cloudLayer = SilverLining::CloudLayerFactory::Create(static_cast<CloudTypes>(sourceType));
    if (cloudLayer)
    {
        // This must be something simple but I cant figure 
        // out how to get QByteArray to a suitable std::istream

        std::istream stdStream;
        if (cloudLayer->Unserialize(*impl->atmosphere, stdStream))
            return impl->atmosphere->GetConditions()->AddCloudLayer(cloudLayer);
        else
            LogError(LC + "Creating cloud layer from source file failed.");
    }
    */
    LogWarning(LC + "CreateCloudLayer QByteArray overload not implemented!");
    return -1;
#endif
}

int EC_MeshmoonSky::CreateCloudLayer(const QString &diskSource)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(diskSource)
    return -1;
#else
    if (!impl->atmosphere)
        return -1;
    if (!QFile::exists(diskSource))
    {
        LogError(LC + "Cloud layer source file does not exist: " + diskSource);
        return -1;
    }
    
    // Get cloud type from file.
    QFile sourceFile(diskSource);
    if (!sourceFile.open(QIODevice::ReadOnly))
    {
        LogError(LC + "Cannot detect cloud type from cloud source file, aborting load!");
        return -1;
    }
    if (sourceFile.size() <= 0)
    {
        LogError(LC + "Cannot detect cloud type from cloud source file, aborting load!");
        return -1;
    }

    // Get the last quint8 from the end, our stored cloud type.
    quint8 sourceType;
    QDataStream sourceStream(&sourceFile);
    sourceStream.skipRawData(sourceFile.size()-1);
    sourceStream >> sourceType;
    sourceFile.close();

    SilverLining::CloudLayer *cloudLayer = SilverLining::CloudLayerFactory::Create(static_cast<CloudTypes>(sourceType));
    if (cloudLayer)
    {
        std::string pathStd = diskSource.toStdString();
        if (cloudLayer->Restore(*impl->atmosphere, pathStd.c_str()))
            return impl->atmosphere->GetConditions()->AddCloudLayer(cloudLayer);
        else
            LogError(LC + "Creating cloud layer from source file failed.");
    }
    return -1;
#endif
}

int EC_MeshmoonSky::CreateCloudLayer(uint type, double altitude, double thickness, double baseWidth, double baseLength, 
                                      double density, double alpha, float2 position, bool infinite)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(type)
    UNREFERENCED_PARAM(altitude)
    UNREFERENCED_PARAM(thickness)
    UNREFERENCED_PARAM(baseWidth)
    UNREFERENCED_PARAM(baseLength)
    UNREFERENCED_PARAM(density)
    UNREFERENCED_PARAM(alpha)
    UNREFERENCED_PARAM(position)
    UNREFERENCED_PARAM(infinite)
    return -1;
#else
    if (!impl->atmosphere)
        return -1;

    SilverLining::CloudLayer *cloudLayer = SilverLining::CloudLayerFactory::Create(static_cast<CloudTypes>(type));
    if (cloudLayer)
    {
        SetCloudLayerAttributes(cloudLayer, altitude, thickness, baseWidth, baseLength, density, alpha, position, infinite);
        return impl->atmosphere->GetConditions()->AddCloudLayer(cloudLayer);
    }
    return -1;
#endif
}

Ogre::Light *EC_MeshmoonSky::OgreLight() const
{
    return (OgreSceneManager() && !impl->silverLiningLightName.empty() ? OgreSceneManager()->getLight(impl->silverLiningLightName) : 0);
}

void EC_MeshmoonSky::RecreateSkyAndClouds()
{
    DestroySky();
    CreateSky();
    CreateClouds(static_cast<CloudCoverage>(cloudCoverage.Get()));
}

void EC_MeshmoonSky::CreateSky()
{
#ifdef MESHMOON_SILVERLINING
    DestroySky();
    
    if (!OgreSceneManager())
    {
        LogError(LC + "Cannot create sky, Ogre scene manager is null!");
        return;
    }

    // Initialize SilverLining atmosphere.
    // On Windows assuming we're using D3D9, but fall back to OpenGL if that's not the case.
    void *d3dDevice = 0;
#ifdef DIRECTX_ENABLED
    Ogre::D3D9RenderWindow* renderWindow = dynamic_cast<Ogre::D3D9RenderWindow*>(impl->renderer->GetCurrentRenderWindow());
    if (renderWindow)
        d3dDevice = renderWindow->getD3D9Device();
#endif

    impl->atmosphere = new SilverLining::Atmosphere(SILVERLINING_USERNAME, SILVERLINING_LICENCE_KEY);

    QString resourcePath = QDir::toNativeSeparators(QDir(Application::InstallationDirectory()).absoluteFilePath("media/meshmoon/silverlining"));
    int error = impl->atmosphere->Initialize(d3dDevice == 0 ? SilverLining::Atmosphere::OPENGL : SilverLining::Atmosphere::DIRECTX9, 
        resourcePath.toStdString().c_str(), false, d3dDevice);

    if (error != SilverLining::Atmosphere::E_NOERROR)
    {
        LogError(LC + "Initialization of SilverLining failed.");
        return;
    }
    
    float3 vec = (ParentScene() ? ParentScene()->UpVector() : float3::unitY);
    impl->atmosphere->SetUpVector(vec.x, vec.y, vec.z);
    vec = (ParentScene() ? ParentScene()->RightVector() : float3::unitX);
    impl->atmosphere->SetRightVector(vec.x, vec.y, vec.z);

    impl->renderQueueListener = new MeshmoonSkyRenderQueueListener(this, 0xFFFFFFF0);
    impl->renderSystemListener = new MeshmoonSkyRenderSystemListener(this);
    OgreSceneManager()->addRenderQueueListener(impl->renderQueueListener);
    Ogre::Root::getSingleton().getRenderSystem()->addListener(impl->renderSystemListener);

    CreateOgreLight();
    Update();
#endif
}

void EC_MeshmoonSky::DestroySky()
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere)
        return;

    DestroyClouds();
    DestroyOgreLight();

    try
    {
        Ogre::SceneManager *ogreSceneManager = OgreSceneManager();
        if (ogreSceneManager)
        {
            if (impl->renderQueueListener)
                ogreSceneManager->removeRenderQueueListener(impl->renderQueueListener);
        }
        else
            LogWarning(LC + "Failed to uninitialize SilverLining library cleanly from Ogre, rendering scene not available!");

        if (impl->renderSystemListener)
            Ogre::Root::getSingleton().getRenderSystem()->removeListener(impl->renderSystemListener);
    }
    catch(const Ogre::Exception &ex)
    {
        LogError(LC + QString::fromStdString(ex.getDescription()));
    }

    SAFE_DELETE(impl->renderQueueListener);
    SAFE_DELETE(impl->renderSystemListener);
    SAFE_DELETE(impl->atmosphere);
#endif
}

void EC_MeshmoonSky::DestroyClouds()
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere)
        return;
    
    // Give external clouds opportunity to delete their clouds
    // before RemoveAllCloudLayers call.
    emit AboutToDestroyClouds();

    impl->atmosphere->GetConditions()->RemoveAllCloudLayers();
    impl->atmosphere->GetConditions()->ClearWindVolumes();
    impl->cloudCoverage = static_cast<EC_MeshmoonSky::CloudCoverage>(-1);
#endif
}

void EC_MeshmoonSky::OnActiveCameraChanged(Entity *cameraEnt)
{
    EC_Camera* camera = cameraEnt ? cameraEnt->Component<EC_Camera>().get() : 0;
    if (camera)
        impl->activeCamera = camera;
}

void EC_MeshmoonSky::CreateClouds(CloudCoverage cloudCoverage)
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere || impl->cloudCoverage == cloudCoverage || cloudCoverage < USE_EXTERNAL_CLOUDLAYERS || cloudCoverage > STORM)
        return;
    
    DestroyClouds();
        
    impl->cloudCoverage = cloudCoverage;
    emit CloudCoverageChanged(impl->cloudCoverage);
    
    /** If USE_EXTERNAL_CLOUDLAYERS we are done, the above CloudCoverageChanged signal notified all
        external EC_MeshmoonCloudLayer components to create their clouds to our atmosphere. */
    if (impl->cloudCoverage > NO_CLOUDS)
    {
        QDir dir = QDir(Application::InstallationDirectory()).absoluteFilePath("media/meshmoon/silverlining/Clouds/meshmoon/");
            
        // Fair is a base for all of the preset coverages.
        CreateCloudLayer(dir.absoluteFilePath("fair.meshmooncloudlayer"));
        //impl->atmosphere->GetConditions()->SetPresetConditions(SilverLining::AtmosphericConditions::FAIR, *impl->atmosphere);

        switch (impl->cloudCoverage)
        {
            case PARTLY_CLOUDY:
            {
                CreateCloudLayer(dir.absoluteFilePath("partlycloudy.meshmooncloudlayer"));
                //impl->atmosphere->GetConditions()->SetPresetConditions(SilverLining::AtmosphericConditions::PARTLY_CLOUDY, *impl->atmosphere);
                break;
            }
            case MOSTLY_CLOUDY:
            {
                if (HasHighGraphics() || HasUltraGraphics())
                    CreateCloudLayer(dir.absoluteFilePath("mostlycloudy.meshmooncloudlayer"));
                else
                    CreateCloudLayer(dir.absoluteFilePath("partlycloudy.meshmooncloudlayer"));
                    
                //impl->atmosphere->GetConditions()->SetPresetConditions(SilverLining::AtmosphericConditions::MOSTLY_CLOUDY, *impl->atmosphere);
                break;
            }
            case OVERCAST:
            {
                CreateCloudLayer(dir.absoluteFilePath("overcast.meshmooncloudlayer"));
                //impl->atmosphere->GetConditions()->SetPresetConditions(SilverLining::AtmosphericConditions::OVERCAST, *impl->atmosphere);
                break;
            }
            case STORM:
            {
                if (HasHighGraphics() || HasUltraGraphics())
                {
                    CreateCloudLayer(dir.absoluteFilePath("mostlycloudy.meshmooncloudlayer"));
                    CreateCloudLayer(dir.absoluteFilePath("storm1.meshmooncloudlayer"));
                    CreateCloudLayer(dir.absoluteFilePath("storm2.meshmooncloudlayer"));

                    if (HasUltraGraphics())
                        CreateCloudLayer(dir.absoluteFilePath("storm3.meshmooncloudlayer"));
                }
                // Low and Medium graphics
                else
                {
                    CreateCloudLayer(dir.absoluteFilePath("partlycloudy.meshmooncloudlayer"));
                    CreateCloudLayer(dir.absoluteFilePath("storm1.meshmooncloudlayer"));

                    if (HasMediumGraphics())
                        CreateCloudLayer(dir.absoluteFilePath("storm2.meshmooncloudlayer"));
                }

                /*CreateCloudLayer((uint)CUMULONIMBUS_CAPPILATUS, 2000.0, 100, 15000, 10000, 0.7, 0.9, float2(6000, 6000), false);
                CreateCloudLayer((uint)CUMULONIMBUS_CAPPILATUS, 2000.0, 100, 10000, 15000, 0.7, 0.9, float2(-4000, 4000), false);
                CreateCloudLayer((uint)CUMULONIMBUS_CAPPILATUS, 2000.0, 100, 15000, 15000, 0.7, 0.9, float2(1000, -6000), false);
                CreateCloudLayer((uint)CUMULONIMBUS_CAPPILATUS, 5000, 100, 5000, 5000, 0.7, 0.9, float2(3000, 6000), false);*/
                break;
            }
        }
        
        /* Temp code for creating different preset coverages to disk!
        SL_MAP(int, SilverLining::CloudLayer*) &layers = impl->atmosphere->GetConditions()->GetCloudLayers();
        for (SL_MAP(int, SilverLining::CloudLayer*)::iterator iter = layers.begin(); iter != layers.end(); ++iter)
        {
            std::string path = framework->Asset()->GenerateTemporaryNonexistingAssetFilename("cloudlayer.meshmooncloudlayer").toStdString();
            if (iter->second->Save(path.c_str()))
            {
                QFile temp(path.c_str());
                if (temp.open(QIODevice::Append))
                {
                    // Write cloud layer type as extra data at the end of the file.
                    QDataStream stream(&temp);
                    stream << static_cast<quint8>(iter->second->GetType());
                    temp.close();
                }
                qDebug() << path.c_str();
            }
        }*/
    }
    
    emit CloudsCreated();
#endif
}

bool EC_MeshmoonSky::SetCloudLayerAttributes(SilverLining::CloudLayer* layer, double altitude, double thickness,
                                             double baseWidth, double baseLength, double density, double alpha,
                                             float2 position, bool infinite)
{
#ifndef MESHMOON_SILVERLINING
    UNREFERENCED_PARAM(layer)
    UNREFERENCED_PARAM(altitude)
    UNREFERENCED_PARAM(thickness)
    UNREFERENCED_PARAM(baseWidth)
    UNREFERENCED_PARAM(baseLength)
    UNREFERENCED_PARAM(density)
    UNREFERENCED_PARAM(alpha)
    UNREFERENCED_PARAM(position)
    UNREFERENCED_PARAM(infinite)
     return false;
#else
    if (!layer || !impl->atmosphere)
        return false;

    layer->SetBaseAltitude(altitude);
    layer->SetThickness(thickness);
    layer->SetBaseWidth(baseWidth);
    layer->SetBaseLength(baseLength);
    layer->SetDensity(density);
    layer->SetAlpha(alpha);
    layer->SetLayerPosition(position.x, position.y);
    layer->SetIsInfinite(infinite);
    
    layer->SetFadeTowardEdges(true);
    layer->SeedClouds(*impl->atmosphere);
    return true;
#endif
}

void EC_MeshmoonSky::AttributesChanged()
{
#ifdef MESHMOON_SILVERLINING
    if (framework->IsHeadless())
        return;

    // godRayIntensity is set and used inside MeshmoonSkyRenderQueueListener when drawing

    if (cloudCoverage.ValueChanged())
        CreateClouds(static_cast<CloudCoverage>(cloudCoverage.Get()));
    if (timeZone.ValueChanged() || timeMode.ValueChanged() || longitude.ValueChanged() || latitude.ValueChanged() ||
        time.ValueChanged() || timeMultiplier.ValueChanged() || date.ValueChanged())
    {
        // Validation
        if (latitude.ValueChanged() && (latitude.Get() < -90.0f || latitude.Get() > 90.0f))
            latitude.Set(0.0f, AttributeChange::Disconnected);
        if (longitude.ValueChanged() && (longitude.Get() < -180.0f || longitude.Get() > 180.0f))
            longitude.Set(0.0f, AttributeChange::Disconnected);
        if (time.ValueChanged() && (time.Get() < 0.0f || time.Get() > 24.0f))
            time.Set(0.0f, AttributeChange::Disconnected);
        if (timeMultiplier.ValueChanged() && timeMultiplier.Get() < 0.0f)
            timeMultiplier.Set(0.0f, AttributeChange::Disconnected);

        UpdateTime();
    }
    if (lensFlares.ValueChanged() && impl->atmosphere)
        impl->atmosphere->EnableLensFlare(lensFlares.Get());
    if (gamma.ValueChanged() && impl->atmosphere)
        impl->atmosphere->SetGamma(gamma.Get());
    if (windDirection.ValueChanged() || windSpeed.ValueChanged())
        UpdateWeatherConditions();
#endif
}

void EC_MeshmoonSky::Update()
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere)
        return;

    UpdateTime(true);
    UpdateWeatherConditions();
    
    impl->atmosphere->EnableLensFlare(lensFlares.Get());
    impl->atmosphere->SetGamma(gamma.Get());
#endif
}

void EC_MeshmoonSky::UpdateTime(bool forced)
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere)
        return;

    TimeMode mode = static_cast<TimeMode>(timeMode.Get());
    TimeZone zone = static_cast<TimeZone>(timeZone.Get());
    bool locationChanged = false;
    bool timeChanged = false;
    bool timeZoneFromLongitude = (zone == USE_LONGITUDE);
    bool realTimeMode = (mode == REAL_TIME);
    
    QString systemTimeToSimulated = "";
    
    if (forced || timeMode.ValueChanged())
    {
        // Set time passage and relight update to 30fps, however
        // our simulated time SetTime (60fps) invocations might force relighting.
        if (realTimeMode || timeMultiplier.Get() == 1.0f)
            impl->atmosphere->GetConditions()->EnableTimePassage(true, TIME_RELIGHT_INTERVAL_MSEC);
        else
            impl->atmosphere->GetConditions()->EnableTimePassage(false, TIME_RELIGHT_INTERVAL_MSEC);

        SetDesignable(timeZone, realTimeMode);
        SetDesignable(time, !realTimeMode);
        SetDesignable(timeMultiplier, !realTimeMode);
        SetDesignable(date, !realTimeMode);
    }
    if (forced || timeZone.ValueChanged() || timeMode.ValueChanged())
        SetDesignable(longitude, timeZoneFromLongitude || !realTimeMode);

    if (mode == REAL_TIME)
    {
        impl->silverLiningTime.SetFromSystemTime();
        
        int localTimeZone = FloorInt(static_cast<float>(impl->silverLiningTime.GetTimeZone()));
#ifdef __APPLE__
        // Bug: On Mac OS X, Silverlining or the system is lying about the time zone
        localTimeZone *= -1;
        impl->silverLiningTime.SetTimeZone(localTimeZone);
#endif
        int targetTimeZone = (timeZoneFromLongitude ? FloorInt(longitude.Get() / 15.0f) : timeZone.Get());
        
        // Convert the enums 13 -> 24 to -1 -> -12 time zones
        if (!timeZoneFromLongitude && targetTimeZone > 12)
            targetTimeZone = -(targetTimeZone - 12);

        /*qDebug() << "Local zone" << localTimeZone << "Target zone" << targetTimeZone << "From lon" << timeZoneFromLongitude;
        qDebug() << QString("%1.%2.%3").arg(impl->silverLiningTime.GetDay()).arg(impl->silverLiningTime.GetMonth()).arg(impl->silverLiningTime.GetYear()) 
                 << QString("%1:%2").arg(impl->silverLiningTime.GetHour()).arg(impl->silverLiningTime.GetMinutes());*/
            
        if (localTimeZone != targetTimeZone)
        {
            int hour = impl->silverLiningTime.GetHour();

            if (localTimeZone >= 0 && targetTimeZone >= 0)
            {
                // +2 +5 -> +=  3
                // +5 +2 -> += -3
                hour += (targetTimeZone - localTimeZone);
                //qDebug() << "#1" << (targetTimeZone - localTimeZone);
            }
            else if (localTimeZone <= 0 && targetTimeZone >= 0)
            {
                // -2 +5 -> +=  7
                hour += ((-localTimeZone) + targetTimeZone);
                //qDebug() << "#2" << ((-localTimeZone) + targetTimeZone);
            }
            else if (localTimeZone >= 0 && targetTimeZone <= 0)
            {
                // +2 -5 -> += -7
                hour += ((-localTimeZone) + targetTimeZone);
                //qDebug() << "#3" << ((-localTimeZone) + targetTimeZone);
            }
            else if (localTimeZone <= 0 && targetTimeZone <= 0)
            {
                // -2 -5 -> += -3
                hour += (targetTimeZone + (-localTimeZone));
                //qDebug() << "#4" << (targetTimeZone + (-localTimeZone));
            }
            if (hour >= 24)
            {
                hour -= 24;
                impl->silverLiningTime.SetDay(impl->silverLiningTime.GetDay()+1);
            }
            else if (hour < 0)
            {
                hour += 24;
                impl->silverLiningTime.SetDay(impl->silverLiningTime.GetDay()-1);
            }
            impl->silverLiningTime.SetHour(hour);
        }
        timeChanged = true;

        impl->silverLiningLocation.SetAltitude(0.0);
        impl->silverLiningLocation.SetLatitude(static_cast<double>(latitude.Get()));
        impl->silverLiningLocation.SetLongitude(timeZoneFromLongitude ? static_cast<double>(longitude.Get()) : targetTimeZone * 15.0);
        locationChanged = true;
    }
    else if (mode == SIMULATED_TIME)
    {
        if (forced || longitude.ValueChanged())
        {
            impl->silverLiningTime.SetTimeZone(FloorInt(longitude.Get() / 15.0f));
            timeChanged = true;
        }

        if (forced || date.ValueChanged())
        {
            QString dateStr = date.Get().trimmed();
            if (!dateStr.isEmpty())
            {
                if (!dateStr.contains(".") && !dateStr.contains("/"))
                {
                    LogError(LC + QString("Date attribute '%1' format invalid. Use 'DD.MM.YYYY' or 'DD/MM/YYYY'. Skipping setting time and date.").arg(dateStr));
                    return;
                }
                QStringList dateParts = dateStr.split((dateStr.contains(".") ? "." : "/"), QString::SkipEmptyParts);
                if (dateParts.size() < 3)
                {
                    LogError(LC + QString("Date attribute '%1' does not contain all needed parts. Use 'DD.MM.YYYY' or 'DD/MM/YYYY'. Skipping setting time and date.").arg(dateStr));
                    return;
                }
                bool ok = false;
                int day = dateParts[0].toInt(&ok);
                if (!ok)
                {
                    LogWarning(LC + QString("Date attributes '%1' day part is not a number. Defaulting to 1.").arg(dateStr));
                    day = 1;
                }
                if (day < 1 || day > 31)
                {
                    LogWarning(LC + QString("Date attributes '%1' day part is out of range. Defaulting to 1.").arg(dateStr));
                    day = 1;
                }
                ok = false;
                int month = dateParts[1].toInt(&ok);
                if (!ok)
                {
                    LogWarning(LC + QString("Date attributes '%1' month part is not a number. Defaulting to 1.").arg(dateStr));
                    month = 1;
                }
                if (month < 1 || month > 12)
                {
                    LogWarning(LC + QString("Date attributes '%1' day part is out of range. Defaulting to 1.").arg(dateStr));
                    month = 1;
                }
                ok = false;
                int year = dateParts[2].toInt(&ok);
                if (!ok)
                {
                    LogWarning(LC + QString("Date attributes '%1' year part is not a number. Defaulting to 2013.").arg(dateStr));
                    year = 2013;
                }

                impl->silverLiningTime.SetDay(day);
                impl->silverLiningTime.SetMonth(month);
                impl->silverLiningTime.SetYear(year);
                
            }
            else
            {
                SilverLining::LocalTime systemTime;
                systemTime.SetFromSystemTime();
                impl->silverLiningTime.SetDay(systemTime.GetDay());
                impl->silverLiningTime.SetMonth(systemTime.GetMonth());
                impl->silverLiningTime.SetYear(systemTime.GetYear());

                // We want to trigger LocalOnly change as the last thing in this function.
                // Otherwise we will come here again, then continue from this line, 
                // this recursion may screw the ValueChanged() logic used in this function.
                systemTimeToSimulated = QString("%1.%2.%3").arg(systemTime.GetDay()).arg(systemTime.GetMonth()).arg(systemTime.GetYear());
            }
            timeChanged = true;
        }
        
        if (forced || time.ValueChanged())
        {
            float timeNow = time.Get();
            
            float hour = Floor(timeNow);
            int hourInt = FloorInt(hour);
            impl->silverLiningTime.SetHour(hourInt < 24 ? hourInt : 0);

            float minuteDecimals = (hourInt > 0 ? fmod(timeNow, hour) : timeNow);
            float minutes = (minuteDecimals * 60.0f);
            int minutesInt = FloorInt(minutes);
            impl->silverLiningTime.SetMinutes(minutesInt);

            float secondDecimals = (minutesInt > 0 ? fmod(minutes, Floor(minutes)) : minutes);
            double seconds = static_cast<double>(secondDecimals * 60.0f);
            impl->silverLiningTime.SetSeconds(seconds);
            
            //qDebug() << time.Get() << "Hour" << hour << "Minute decimals" << minuteDecimals << "Minutes" << minutes;
            //qDebug() << QString("%1%2.%3%4").arg(minutesInt < 10 ? "0" : "").arg(minutesInt).arg(seconds < 10.0 ? "0" : "").arg(FloorInt(secondDecimals * 60.0f));

            timeChanged = true;
        }

        if (forced || latitude.ValueChanged() || longitude.ValueChanged())
        {
            impl->silverLiningLocation.SetAltitude(0.0);
            impl->silverLiningLocation.SetLatitude(static_cast<double>(latitude.Get()));
            impl->silverLiningLocation.SetLongitude(static_cast<double>(longitude.Get()));
            locationChanged = true;
        }
    }
    else
    {
        LogWarning(LC + QString("Unknown time mode attribute value %1, cannot set time.").arg(timeMode.Get()));
        return;
    }
    
    if (locationChanged)
    {
        //qDebug() << QString("Lon %1 Lat %2").arg(impl->silverLiningLocation.GetLongitude()).arg(impl->silverLiningLocation.GetLatitude());
        impl->atmosphere->GetConditions()->SetLocation(impl->silverLiningLocation);
    }
    if (timeChanged)
    {
        /*qDebug() << QString("%1.%2.%3").arg(impl->silverLiningTime.GetDay()).arg(impl->silverLiningTime.GetMonth()).arg(impl->silverLiningTime.GetYear()) 
                 << QString("%1:%2").arg(impl->silverLiningTime.GetHour()).arg(impl->silverLiningTime.GetMinutes());*/
        impl->atmosphere->GetConditions()->SetTime(impl->silverLiningTime);
    }
    //qDebug() << "";

    if (!systemTimeToSimulated.trimmed().isEmpty())
        date.Set(systemTimeToSimulated.trimmed(), AttributeChange::LocalOnly);
#endif
}

void EC_MeshmoonSky::UpdateWeatherConditions()
{
#ifdef MESHMOON_SILVERLINING
    if (!impl->atmosphere)
        return;

    impl->silverLiningWindConditions.SetDirection(static_cast<double>(windDirection.Get()));
    impl->silverLiningWindConditions.SetWindSpeed(static_cast<double>(windSpeed.Get()));

    impl->atmosphere->GetConditions()->ClearWindVolumes();
    impl->atmosphere->GetConditions()->SetWind(impl->silverLiningWindConditions);

    /*qDebug() << impl->silverLiningWindConditions.GetDirection() << impl->silverLiningWindConditions.GetWindSpeed();*/
#endif
}

Ogre::SceneManager* EC_MeshmoonSky::OgreSceneManager() const
{
    OgreWorld *ogreWorld = impl->world.lock().get();
    return ogreWorld ? ogreWorld->OgreSceneManager() : 0;
}

bool EC_MeshmoonSky::IsCloudLayerComponentsEnabled() const
{
    return (impl->cloudCoverage == USE_EXTERNAL_CLOUDLAYERS);
}

SilverLining::Atmosphere *EC_MeshmoonSky::Atmosphere() const
{
#ifndef MESHMOON_SILVERLINING
    return 0;
#else
    return impl->atmosphere;
#endif
}

EC_Camera *EC_MeshmoonSky::ActiveCamera() const
{
    return impl->activeCamera;
}

void EC_MeshmoonSky::DeviceLost()
{
#ifdef MESHMOON_SILVERLINING
    if (impl->atmosphere)
        impl->atmosphere->D3D9DeviceLost();
#endif
}

void EC_MeshmoonSky::DeviceRestored()
{
#ifdef MESHMOON_SILVERLINING
    if (impl->atmosphere)
        impl->atmosphere->D3D9DeviceReset();
#endif
}

float EC_MeshmoonSky::GodRayIntensity()
{
    // GraphicsCustom, GraphicsHigh and GraphicsUltra
    if (impl->rocketGraphicsMode >= 1 && impl->rocketGraphicsMode < 4)
        return godRayIntensity.Get();
    return 0.0f;
}

bool EC_MeshmoonSky::HasLowGraphics() const
{
    return (impl->rocketGraphicsMode == 5);
}

bool EC_MeshmoonSky::HasMediumGraphics() const
{
    return (impl->rocketGraphicsMode == 4);
}

bool EC_MeshmoonSky::HasHighGraphics() const
{
    return (impl->rocketGraphicsMode == 3);
}

bool EC_MeshmoonSky::HasUltraGraphics() const
{
    return (impl->rocketGraphicsMode == 2 || impl->rocketGraphicsMode == 1);
}

template<typename T> 
void EC_MeshmoonSky::SetDesignable(Attribute<T> &attribute, bool designable)
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
