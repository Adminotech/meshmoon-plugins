/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#include "StableHeaders.h"

#include "EC_MeshmoonWater.h"
#include "EC_MeshmoonSky.h"

#include "Application.h"
#include "FrameAPI.h"
#include "SceneAPI.h"
#include "EC_Camera.h"
#include "EC_SkyX.h"
#include "EC_Sky.h"
#include "Profiler.h"
#include "Entity.h"
#include "Scene.h"
#include "Math/MathFunc.h"
#include "OgreRenderingModule.h"
#include "OgreWorld.h"
#include "OgreSceneManager.h"
#include "AttributeMetadata.h"
#include "Renderer.h"

#ifdef MESHMOON_TRITON
#include <Triton.h>
#include <TritonCommon.h>
#endif

#ifdef DIRECTX_ENABLED
#undef SAFE_DELETE
#undef SAFE_DELETE_ARRAY
#include <OgreD3D9RenderWindow.h>
#endif

/// @cond PRIVATE
static const char* TRITON_USERNAME      = "";
static const char* TRITON_LICENCE_KEY   = "";
/// @endcond

EC_MeshmoonWater::EC_MeshmoonWater(Scene *scene) :
    IComponent(scene),
    INIT_ATTRIBUTE_VALUE(simulationModel, "Water Type", static_cast<uint>(Tessendorf)),
    INIT_ATTRIBUTE_VALUE(sprayEnabled, "Spray Enabled", false),
    INIT_ATTRIBUTE_VALUE(drawDebug, "Draw Debug", false),
    INIT_ATTRIBUTE_VALUE(enableHeightQueries, "Enable Height Queries", false),
    INIT_ATTRIBUTE_VALUE(waveChoppiness, "Wave Choppiness", 2.0f),
    INIT_ATTRIBUTE_VALUE(waterColor, "Water Color", Color(0.0f, 0.2f, 0.3f)),
    INIT_ATTRIBUTE_VALUE(seaLevel, "Sea Level", 0.0f),
    INIT_ATTRIBUTE_VALUE(beaufortScale, "Beaufort Scale", 4), // Moderate breeze
    INIT_ATTRIBUTE_VALUE(visible, "Visible", true),
    INIT_ATTRIBUTE_VALUE(reflectionsEnabled, "Reflections Enabled", false),
    INIT_ATTRIBUTE_VALUE(reflectionIntensity, "Reflection Intensity", 0.6f),
    LC("[EC_MeshmoonWater]: "),
    tritonVisible_(true),
#ifndef MESHMOON_SERVER_BUILD
    renderQueueListener_(0),
    renderSystemListener_(0),
    renderTargetListener_(0),
#endif
    activeCamera_(0)
{
    // Water type metadata
    static AttributeMetadata simulationModelMetadata, beaufortMetadata, reflectionIntensityMetadata;
    static bool metadataInitialized = false;
    if (!metadataInitialized)
    {
        simulationModelMetadata.enums[static_cast<int>(Tessendorf)] = "Tessendorf";
        simulationModelMetadata.enums[static_cast<int>(PiersonMoskowitz)] = "Pierson-Moskowitz";
        simulationModelMetadata.enums[static_cast<int>(Jonswap)] = "Jonswap";

        beaufortMetadata.enums[0] = "Calm";
        beaufortMetadata.enums[1] = "Light air";
        beaufortMetadata.enums[2] = "Light breeze";
        beaufortMetadata.enums[3] = "Gentle breeze";
        beaufortMetadata.enums[4] = "Moderate breeze";
        beaufortMetadata.enums[5] = "Fresh breeze";
        beaufortMetadata.enums[6] = "Strong breeze";
        beaufortMetadata.enums[7] = "Moderate gale";
        beaufortMetadata.enums[8] = "Fresh gale";
        beaufortMetadata.enums[9] = "Strong gale";
        beaufortMetadata.enums[10] = "Storm";
        beaufortMetadata.enums[11] = "Violent storm";
        beaufortMetadata.enums[12] = "Hurricane force";
        
        reflectionIntensityMetadata.minimum = "0.0";
        reflectionIntensityMetadata.maximum = "1.0";
        reflectionIntensityMetadata.step = "0.1";
        
        metadataInitialized = true;
    }

    simulationModel.SetMetadata(&simulationModelMetadata);
    beaufortScale.SetMetadata(&beaufortMetadata);
    reflectionIntensity.SetMetadata(&reflectionIntensityMetadata);
    
    connect(this, SIGNAL(ParentEntitySet()), SLOT(OnParentEntitySet()));
 
    // These can fail if created as unparented! Accounted in OnParentEntitySet().
    if (scene)
        world_ = scene->Subsystem<OgreWorld>();
    OgreRenderingModule* renderingModule = (framework ? framework->Module<OgreRenderingModule>() : 0);
    if (renderingModule)
        renderer_ = renderingModule->Renderer();
}

EC_MeshmoonWater::~EC_MeshmoonWater()
{
    DestroyOcean();
    renderer_.reset();
}

void EC_MeshmoonWater::OnParentEntitySet()
{
    if (!framework || framework->IsHeadless())
        return;

    OgreRendererPtr renderer = Renderer();
    if (!renderer)
    {
        // Try to resolve the renderer now. Might have not been done in ctor if constructed as unparented!
        OgreRenderingModule* renderingModule = framework->Module<OgreRenderingModule>();
        if (!renderingModule)
            return;
        renderer_ = renderingModule->Renderer();
        if (renderer_.expired())
            return;
        renderer = renderer_.lock();
    }
        
    if (world_.expired() && ParentScene())
        world_ = ParentScene()->GetWorld<OgreWorld>();

    connect(renderer.get(), SIGNAL(DeviceCreated()), this, SLOT(RecreateOcean()), Qt::UniqueConnection);
    connect(renderer.get(), SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)), Qt::UniqueConnection);
    connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);

    OnActiveCameraChanged(renderer->MainCamera());

    // Detect sky components if already in scene, otherwise will be detected via ComponentAdded signal
    EntityList ents = ParentScene()->EntitiesWithComponent(EC_MeshmoonSky::TypeIdStatic());
    for (EntityList::iterator iter = ents.begin(); iter != ents.end(); ++iter)
    {
        IComponent *comp = (*iter)->Component(EC_MeshmoonSky::TypeIdStatic()).get();
        if (comp) OnComponentAdded(0, comp, AttributeChange::LocalOnly);
    }
    ents = (windConditionsComp_.expired() ? ParentScene()->EntitiesWithComponent(EC_SkyX::TypeIdStatic()) : EntityList());
    for (EntityList::iterator iter = ents.begin(); iter != ents.end(); ++iter)
    {
        IComponent *comp = (*iter)->Component(EC_SkyX::TypeIdStatic()).get();
        if (comp) OnComponentAdded(0, comp, AttributeChange::LocalOnly);
    }

    connect(ParentScene(), SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)), 
        this, SLOT(OnComponentAdded(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);
    connect(ParentScene(), SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), 
        this, SLOT(OnComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), Qt::UniqueConnection);

    CreateOcean();
}

void EC_MeshmoonWater::ClearSwells()
{
#ifdef MESHMOON_TRITON
    if(framework->IsHeadless())
        return;
    if(!state_.environment)
        return;

    state_.environment->ClearSwells();
#endif
}

void EC_MeshmoonWater::ClearWinds()
{
#ifdef MESHMOON_TRITON
    if(framework->IsHeadless())
        return;
    if(!state_.environment)
        return;

    state_.environment->ClearWindFetches();
#endif
}

bool EC_MeshmoonWater::AddSwell(float waveLength, float waveHeight, float direction)
{
#ifndef MESHMOON_TRITON
    return false;
#else
    if(framework->IsHeadless() || !state_.environment)
        return false;
    state_.environment->AddSwell(waveLength, waveHeight, DegToRad(direction));
    return true;
#endif
}

bool EC_MeshmoonWater::AddWind(float speed, float direction)
{
#ifndef MESHMOON_TRITON
    return false;
#else
    if(framework->IsHeadless() || !state_.environment)
        return false;

    Triton::WindFetch fetch;
    fetch.SetWind(speed, DegToRad(direction));
    state_.environment->AddWindFetch(fetch);
    return true;
#endif
}

float EC_MeshmoonWater::HeightAt(const float3& point, const float3& direction)
{
#ifndef MESHMOON_TRITON
    return 0.0f;
#else
    if (framework->IsHeadless() || !state_.ocean)
        return 0.0f;

    Triton::Vector3 tDirection = ToTritonVector3(direction);
    Triton::Vector3 location = ToTritonVector3(point);
    Triton::Vector3 normal;

    float height = 0.0f;
    if (state_.ocean->GetHeight(location, tDirection, height, normal))
        return height + seaLevel.Get();
    return 0.0f;
#endif
}

float3 EC_MeshmoonWater::NormalAt(const float3& point, const float3& direction)
{
#ifndef MESHMOON_TRITON
    return float3::nan;
#else
    if (framework->IsHeadless() || !state_.ocean)
        return float3::nan;

    Triton::Vector3 tDirection = ToTritonVector3(direction);
    Triton::Vector3 location = ToTritonVector3(point);
    Triton::Vector3 normal;

    float height = 0.0f;
    if (state_.ocean->GetHeight(location, tDirection, height, normal))
        return float3(normal.x, normal.y, normal.z);
    return float3::nan;
#endif
}

void EC_MeshmoonWater::OnUpdate(float elapsedTime)
{
#ifdef MESHMOON_TRITON
    if (!visible.Get() || !ParentScene() || !state_.environment || !state_.ocean)
        return;

    PROFILE(EC_MeshmoonWater_Update);

    if (!windConditionsComp_.expired())
    {
        if (windConditionsComp_.lock().get()->TypeId() == EC_MeshmoonSky::TypeIdStatic())
        {
            EC_MeshmoonSky *meshmoonSky = dynamic_cast<EC_MeshmoonSky*>(windConditionsComp_.lock().get());
            if (meshmoonSky)
            {
                float windDirFloored = Floor(fmod(meshmoonSky->windDirection.Get() + 180.0f, 360.0f));
                float windSpeedFloored = Floor(meshmoonSky->windSpeed.Get());
                if (windSpeedFloored != state_.windSpeedPerSec || windDirFloored != Floor(state_.windDirDegrees))
                {
                    state_.windDirDegrees = windDirFloored;
                    state_.windSpeedPerSec = windSpeedFloored;
                    UpdateWeatherConditions();
                }
            }
        }
        else if (windConditionsComp_.lock().get()->TypeId() == EC_SkyX::TypeIdStatic())
        {
            EC_SkyX *skyx = dynamic_cast<EC_SkyX*>(windConditionsComp_.lock().get());
            if (skyx)
            {
                /// SkyX-To-Triton wind direction needs a small adjustment for them to align properly.
                /// (-skyxDir + 90 deg) and we round the float + modulo with 360 degrees to get [0,359] range.
                float tritonWindDir = fmod(Floor(-skyx->windDirection.Get() + 90.0f), 360.0f);
                if (skyx->windSpeed.Get() != state_.windSpeedPerSec || tritonWindDir != Floor(state_.windDirDegrees))
                {
                    state_.windDirDegrees = tritonWindDir;
                    state_.windSpeedPerSec = skyx->windSpeed.Get();
                    UpdateWeatherConditions();
                }
            }
        }
    }
#endif
}

void EC_MeshmoonWater::OnActiveCameraChanged(Entity *newMainWindowCamera)
{
    if (framework->IsHeadless())
        return;
    activeCamera_ = (newMainWindowCamera ? newMainWindowCamera->Component<EC_Camera>().get() : 0);
}

void EC_MeshmoonWater::CreateOcean()
{
#ifdef MESHMOON_TRITON
    if (!visible.Get() || state_.ocean)
        return;

    Ogre::SceneManager *ogreSceneManager = OgreSceneManager();
    if (!ogreSceneManager)
    {
        LogError(LC + "Failed to initialize Triton library, renderer not available!");
        return;
    }

    OgreRendererPtr renderer = Renderer();
    if (!renderer)
        return;

    // On Windows assuming we're using D3D9, but fall back to OpenGL if that's not the case.
    void* d3dDevice = 0;
#ifdef DIRECTX_ENABLED
    Ogre::D3D9RenderWindow* renderWindow = dynamic_cast<Ogre::D3D9RenderWindow*>(renderer->GetCurrentRenderWindow());
    if (renderWindow)
        d3dDevice = renderWindow->getD3D9Device();
#endif

    state_.environment = new Triton::Environment();
    state_.environment->SetLicenseCode(TRITON_USERNAME, TRITON_LICENCE_KEY);

    QString resourcePath = QDir::toNativeSeparators(QDir(Application::InstallationDirectory()).absoluteFilePath("media/meshmoon/triton"));
    state_.resourceLoader = new Triton::ResourceLoader(resourcePath.toStdString().c_str());

    Triton::EnvironmentError error = state_.environment->Initialize(Triton::FLAT_YUP,
        d3dDevice == 0 ? Triton::OPENGL_2_0 : Triton::DIRECTX_9,
        state_.resourceLoader, d3dDevice);
    if (error != Triton::SUCCEEDED)
    {
        LogError(LC + "Error while initializing Triton.");
        return;
    }

    // Create ocean.
    if (!state_.Setup(Triton::Ocean::Create(state_.environment, static_cast<Triton::WaterModelTypes>(simulationModel.Get()+1), enableHeightQueries.Get()),
            static_cast<SimulationModel>(simulationModel.Get()), reflectionsEnabled.Get(), enableHeightQueries.Get()))
    {
        LogError(LC + "Failed to create Triton ocean!");
        DestroyOcean();
        return;
    }
    
    // Set attributes
    state_.ocean->EnableSpray(sprayEnabled.Get());
    state_.ocean->EnableWireframe(drawDebug.Get());
    state_.ocean->SetChoppiness(waveChoppiness.Get());
    state_.ocean->SetRefractionColor(ToTritonVector3(waterColor.Get()));
    state_.environment->SetSeaLevel(seaLevel.Get());
    
    UpdateWeatherConditions();
    
    // Add listeners to render ocean in right time.
    renderQueueListener_ = new MeshmoonWaterRenderQueueListener(this, 0xFFFFFFF0);
    renderSystemListener_ = new MeshmoonWaterRenderSystemListener(this);
    renderTargetListener_ = new MeshmoonWaterRenderTargetListener(this);

    ogreSceneManager->addRenderQueueListener(renderQueueListener_);
    ogreSceneManager->getRenderQueue()->getQueueGroup(renderQueueListener_->renderQueueId); // Registers our custom queue id to be executed.
    Ogre::Root::getSingleton().getRenderSystem()->addListener(renderSystemListener_);
#endif
}

void EC_MeshmoonWater::DestroyOcean()
{
#ifdef MESHMOON_TRITON
    if (!state_.IsCreated())
        return;

    try
    {
        Ogre::SceneManager *ogreSceneManager = OgreSceneManager();
        if (ogreSceneManager)
        {
            if (renderQueueListener_)
                ogreSceneManager->removeRenderQueueListener(renderQueueListener_);
        }
        else
            LogWarning(LC + "Failed to uninitialize Triton library cleanly from Ogre, rendering scene not available!");

        if (renderSystemListener_)
            Ogre::Root::getSingleton().getRenderSystem()->removeListener(renderSystemListener_);
    }
    catch(const Ogre::Exception &ex)
    {
        LogError(LC + QString::fromStdString(ex.getDescription()));
    }

    SAFE_DELETE(renderQueueListener_);
    SAFE_DELETE(renderSystemListener_);
    SAFE_DELETE(renderTargetListener_);
    SAFE_DELETE(state_.environment);
    SAFE_DELETE(state_.ocean);
    SAFE_DELETE(state_.resourceLoader);
    
    state_.Reset();
#endif
}

void EC_MeshmoonWater::RecreateOcean()
{
    DestroyOcean();
    CreateOcean();
}

void EC_MeshmoonWater::SetVisible(bool display)
{
    if (tritonVisible_ == display)
        return;
    tritonVisible_ = display;
    
    Ogre::SceneManager *ogreSceneManager = OgreSceneManager();
    if (!ogreSceneManager)
        return;

    if (!tritonVisible_)
        DestroyOcean();
    else
        CreateOcean();
}

EC_MeshmoonWater::WaterState &EC_MeshmoonWater::State()
{
    return state_;
}

OgreRendererPtr EC_MeshmoonWater::Renderer() const
{
    return (renderer_.expired() ? OgreRendererPtr(): renderer_.lock());
}

EC_Camera *EC_MeshmoonWater::ActiveCamera() const
{
    return activeCamera_;
}

Ogre::SceneManager* EC_MeshmoonWater::OgreSceneManager() const
{
    OgreWorld *ogreWorld = world_.lock().get();
    return (ogreWorld ? ogreWorld->OgreSceneManager() : 0);
}

void EC_MeshmoonWater::DeviceLost()
{
#ifdef MESHMOON_TRITON
    if (state_.ocean)
        state_.ocean->D3D9DeviceLost();
#endif
}

void EC_MeshmoonWater::DeviceRestored()
{
#ifdef MESHMOON_TRITON
    if (state_.ocean)
    {
        state_.ocean->D3D9DeviceReset();
        
        // Update the reflection map texture to ocean, to avoid crashing.
        renderQueueListener_->SetReflectionMap();
    }
#endif
}

void EC_MeshmoonWater::AttributesChanged()
{
#ifdef MESHMOON_TRITON
    if (framework->IsHeadless())
        return;

    // No-op on the default 'true' value on the first set.
    if (visible.ValueChanged())
        SetVisible(visible.Get());

    // If ocean created, live update attributes
    if (state_.ocean)
    {
        if (sprayEnabled.ValueChanged())
            state_.ocean->EnableSpray(sprayEnabled.Get());
        if (drawDebug.ValueChanged())
            state_.ocean->EnableWireframe(drawDebug.Get());
        if (waveChoppiness.ValueChanged())
            state_.ocean->SetChoppiness(waveChoppiness.Get());
        if (waterColor.ValueChanged())
            state_.ocean->SetRefractionColor(ToTritonVector3(waterColor.Get()));
    }

    // If environment created, live update attributes
    if (state_.environment)
    {
        if (seaLevel.ValueChanged())
        {
            if (renderTargetListener_)
                renderTargetListener_->UpdateReflectionPlane();
            state_.environment->SetSeaLevel(seaLevel.Get());
        }
        if (beaufortScale.ValueChanged())
            UpdateWeatherConditions();
    }

    // Water type and height query attribute changes require recreation.
    if (simulationModel.ValueChanged() && simulationModel.Get() > static_cast<uint>(Jonswap))
    {
        LogWarning(QString("EC_MeshmoonWater: Water type %1 is out of range with. Clamping to closest 'Jonswap'.").arg(simulationModel.Get()));
        simulationModel.Set(Jonswap, AttributeChange::Disconnected);
    }
    
    // Finally create the ocean/environment if not yet created.
    if (simulationModel.ValueChanged() || enableHeightQueries.ValueChanged() || (visible.Get() ? reflectionsEnabled.ValueChanged() : false))
    {
        // Don't recreated ocean if the current one matches all the properties that forces us to recreate it.
        if (!state_.IsCreated() || !state_.Matches(static_cast<SimulationModel>(simulationModel.Get()), reflectionsEnabled.Get(), enableHeightQueries.Get()))
            RecreateOcean();
    }
    
    if (reflectionsEnabled.ValueChanged() && reflectionsEnabled.Get())
        RemoveSkyReflectionArtifacts();
#endif
}

void EC_MeshmoonWater::OnComponentAdded(Entity *entity, IComponent *component, AttributeChange::Type change)
{
    if (component && component->TypeId() == EC_Sky::TypeIdStatic())
        RemoveSkyReflectionArtifacts(component);

    if (!windConditionsComp_.expired())
    {
        // If knows is skyx and incoming is our water, it takes priority.
        if (component && windConditionsComp_.lock()->TypeId() == EC_SkyX::TypeIdStatic() && component->TypeId() == EC_MeshmoonSky::TypeIdStatic())
            windConditionsComp_.reset();
        else
            return;
    }

    if (component && (component->TypeId() == EC_MeshmoonSky::TypeIdStatic() || component->TypeId() == EC_SkyX::TypeIdStatic()))
        windConditionsComp_ = component->shared_from_this();
}

void EC_MeshmoonWater::OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change)
{
    if (!windConditionsComp_.expired() && windConditionsComp_.lock().get() == component)
        windConditionsComp_.reset();
}

void EC_MeshmoonWater::RemoveSkyReflectionArtifacts(IComponent *component)
{
    if (!reflectionsEnabled.Get())
        return;

    // Disable sky draw distance if reflections are enabled
    if (component)
    {
        if (component->TypeId() == EC_Sky::TypeIdStatic())
        {
            EC_Sky *sky = dynamic_cast<EC_Sky*>(component);
            if (sky)
            {
                LogWarning(LC + "EC_Sky cannot be rendered into EC_MeshmoonWater reflections, disabling sky artifacts locally.");
                sky->distance.Set(1.0, AttributeChange::LocalOnly);
            }
        }
        return;
    }
    
    // No input was given, loop the whole scene and recursively call this function.
    EntityList ents = ParentScene()->EntitiesWithComponent(EC_Sky::TypeIdStatic());
    for (EntityList::iterator iter = ents.begin(); iter != ents.end(); ++iter)
    {
        IComponent *comp = (*iter)->Component(EC_Sky::TypeIdStatic()).get();
        if (comp) RemoveSkyReflectionArtifacts(comp);
    }
}

void EC_MeshmoonWater::UpdateWeatherConditions()
{
#ifdef MESHMOON_TRITON
    if (!state_.environment)
        return;

    PROFILE(EC_MeshmoonWater_UpdateWeatherConditions);

    state_.environment->ClearWindFetches();
    state_.environment->SimulateSeaState(beaufortScale.Get(), DegToRad(fmod(state_.windDirDegrees, 360.0f)));
    if (state_.windSpeedPerSec != 0.0f)
        AddWind(state_.windSpeedPerSec, state_.windDirDegrees);
#endif
}

/// @cond PRIVATE
#if 0
OceanRotorWash::OceanRotorWash(Triton::Ocean *ocean, float rotorDiameter, float3 position, float velocity, float3 rotorDownDirection) :
    rotorDiameter_(rotorDiameter),
    position_(position),
    velocity_(velocity),
    rotorDownDirection_(rotorDownDirection),
    tritonRotorWash_(0),
    currentTime_(0)
{
    if(!ocean)
        return;

    tritonRotorWash_ = new Triton::RotorWash(ocean, rotorDiameter_, true);
}

void OceanRotorWash::Update(float elapsedTime)
{
    if(!tritonRotorWash_)
        return;

    currentTime_ += elapsedTime;

    Triton::Vector3 tPosition, tDirection;
    tPosition.x = position_.x;
    tPosition.y = position_.y;
    tPosition.z = position_.z;

    tDirection.x = rotorDownDirection_.x;
    tDirection.y = rotorDownDirection_.y;
    tDirection.z = rotorDownDirection_.z;

    tritonRotorWash_->Update(tPosition, tDirection, velocity_, currentTime_);
}

OceanRotorWash::~OceanRotorWash()
{
    SAFE_DELETE(tritonRotorWash_);
}

OceanImpact::OceanImpact(Triton::Ocean *ocean, float impactorDiameter, float mass, float3 position, float3 direction) :
    impactorDiameter_(impactorDiameter),
    mass_(mass),
    position_(position),
    direction_(direction),
    tritonImpact_(0)
{
    tritonImpact_ = new Triton::Impact(ocean, impactorDiameter_, 100, true);
}

void OceanImpact::Update(float elapsedTime)
{
    if (!tritonImpact_)
        return;

    currentTime_ += elapsedTime;

    Triton::Vector3 tPosition, tDirection;
    tPosition = Triton::Vector3(0, 5, 0);
    tDirection = Triton::Vector3(0, -1, 0);

    tritonImpact_->Trigger(tPosition, tDirection, 100, currentTime_);
}
#endif
// EC_MeshmoonWater::WaterState

EC_MeshmoonWater::WaterState::WaterState()
{
    Reset();
}

bool EC_MeshmoonWater::WaterState::IsCreated() const
{
#ifndef MESHMOON_TRITON
    return false;
#else
    return (ocean && environment && resourceLoader);
#endif
}

bool EC_MeshmoonWater::WaterState::Matches(SimulationModel simulationModel_, bool reflections_, bool heightQueries_) const
{
    return (IsCreated() && simulationModel == simulationModel_ && reflections == reflections_ && heightQueries == heightQueries_);
}

bool EC_MeshmoonWater::WaterState::Setup(Triton::Ocean *ocean_, SimulationModel simulationModel_, bool reflections_, bool heightQueries_)
{
#ifndef MESHMOON_TRITON
    UNREFERENCED_PARAM(ocean_)
    UNREFERENCED_PARAM(simulationModel_)
    UNREFERENCED_PARAM(reflections_)
    UNREFERENCED_PARAM(heightQueries_)
    return false;
#else
    ocean = ocean_;
    simulationModel = simulationModel_;
    reflections = reflections_;
    heightQueries = heightQueries_;
    return (ocean != 0);
#endif
}

void EC_MeshmoonWater::WaterState::Reset()
{
    simulationModel = Tessendorf;
    reflections = false;
    heightQueries = false;
    windDirDegrees = 0.0f;
    windSpeedPerSec = 0.0f;
#ifdef MESHMOON_TRITON
    ocean = 0;
    environment = 0;
    resourceLoader = 0;
#endif
}

/// @endcond
