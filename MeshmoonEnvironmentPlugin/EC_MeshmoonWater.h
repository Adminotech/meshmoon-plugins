/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file  
    @brief  */

#pragma once

#include "MeshmoonEnvironmentPluginApi.h"
#include "MeshmoonEnvironmentPluginFwd.h"
#include "MeshmoonWaterUtils.h"

#include "OgreModuleFwd.h"
#include "IComponent.h"
#include "Color.h"
#include "Math/float3.h"

/// Adds a photorealistic water to the scene.
class MESHMOON_ENVIRONMENT_API EC_MeshmoonWater : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonWater", 506) // Note this is the closed source EC Meshmoon range ID.

public:
    /// Simulation model enum that applies to the 'simulationModel' attribute: { 0 = "Tessendorf", 1 = "Pierson-Moskowitz", 2 = "Jonswap" }
    enum SimulationModel
    {
        Tessendorf = 0,
        PiersonMoskowitz,
        Jonswap
    };

    /// Simulation model. Default: Tessendorf.
    /** @see EC_MeshmoonWater::SimulationModel enum. */
    Q_PROPERTY(uint simulationModel READ getsimulationModel WRITE setsimulationModel);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, simulationModel);

    /// Scale of wind speed at sea with a range of [0,12]. Default: 4 Moderate breeze.
    /** @note Wind direction and speed can also be fetched from the environment, eg. EC_MeshmoonSky or EC_SkyX.
        @see http://en.wikipedia.org/wiki/Beaufort_scale */
    Q_PROPERTY(uint beaufortScale READ getbeaufortScale WRITE setbeaufortScale);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, beaufortScale);

    /// Water color. Default: Color(0.0, 0.2, 0.3).
    Q_PROPERTY(Color waterColor READ getwaterColor WRITE setwaterColor);
    DEFINE_QPROPERTY_ATTRIBUTE(Color, waterColor);

    /// If spray is enabled when water collides with solid objects. Default: false.
    Q_PROPERTY(bool sprayEnabled READ getsprayEnabled WRITE setsprayEnabled);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, sprayEnabled);

    /// If debug drawing is enabled. Use for debugging only. Default: false.
    Q_PROPERTY(bool drawDebug READ getdrawDebug WRITE setdrawDebug);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, drawDebug);

    /// Set to true if you need to use HeightAt and NormalAt functions to query water height information. Default: false.
    Q_PROPERTY(bool enableHeightQueries READ getenableHeightQueries WRITE setenableHeightQueries);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, enableHeightQueries);

    /// Choppiness of waves. Default: 2.0.
    Q_PROPERTY(float waveChoppiness READ getwaveChoppiness WRITE setwaveChoppiness);
    DEFINE_QPROPERTY_ATTRIBUTE(float, waveChoppiness);

    /// Sea height level. Default 0.
    Q_PROPERTY(float seaLevel READ getseaLevel WRITE setseaLevel);
    DEFINE_QPROPERTY_ATTRIBUTE(float, seaLevel);

    /// Reflection intensity. Default 0.6.
    Q_PROPERTY(float reflectionIntensity READ getreflectionIntensity WRITE setreflectionIntensity);
    DEFINE_QPROPERTY_ATTRIBUTE(float, reflectionIntensity);
    
    /// If sea is visible. Default: true.
    Q_PROPERTY(bool visible READ getvisible WRITE setvisible);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, visible);

    /// If environmental reflections should be enabled. Default: false.
    Q_PROPERTY(bool reflectionsEnabled READ getreflectionsEnabled WRITE setreflectionsEnabled);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, reflectionsEnabled);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonWater(Scene *scene);
    ~EC_MeshmoonWater();

    friend class MeshmoonWaterRenderQueueListener;
    friend class MeshmoonWaterRenderTargetListener;
    friend class MeshmoonWaterRenderSystemListener;
    /// @endcond

public slots:   
    /// Add new water swell.
    /** @param Wave length.
        @param Wave height.
        @param Direction in degrees.
        @return True if succeeded, otherwise false. */
    bool AddSwell(float waveLength, float waveHeight, float direction);

    /// Add new wind.
    /** @param The wind speed in world units per second.
        @param Direction in degrees.
        @return True if succeeded, otherwise false. */
    bool AddWind(float speed, float direction);

    /// Clears all water swells.
    void ClearSwells();

    /// Clears all wind.
    void ClearWinds();

    /// Query water height.
    /** @param Point/position of query.
        @param Direction vector.
        @return Water height at point if query succeeded, otherwise 0.0f. */
    float HeightAt(const float3& point, const float3& direction = float3(0, -1, 0));

    /// Query water normal.
    /** @param Point/position of query.
        @param Direction vector.
        @return Water normal at point if query succeeded, otherwise float3::nan. */
    float3 NormalAt(const float3& point, const float3& direction = float3(0, -1, 0));

private slots:
    /// Parent entity set handler.
    void OnParentEntitySet();

    /// Component added handler.
    void OnComponentAdded(Entity *entity, IComponent *component, AttributeChange::Type change);
    
    /// Component removed handler.
    void OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change);
    
    /// EC_Sky artifact remover.
    void RemoveSkyReflectionArtifacts(IComponent *component = 0);

    /// Component main update.
    void OnUpdate(float elapsedTime);

    /// Active camera changed handler.
    void OnActiveCameraChanged(Entity *newMainWindowCamera);

    /// Creates Triton ocean with current attributes.
    void CreateOcean();

    /// Removes Triton ocean.
    void DestroyOcean();

    /// Recreates Triton ocean.
    void RecreateOcean();

    /// Sets visibility. Called automatically from visible attribute changes.
    void SetVisible(bool visible);
    
    /// Updates weather conditions with beaufortScale attribute and potential additional winds.
    void UpdateWeatherConditions();

private:
    struct WaterState
    {
        WaterState();
        
        // If all Triton ptrs are valid.
        bool IsCreated() const;
        
        // Returns if current water matches the given parameters.
        bool Matches(SimulationModel simulationModel_, bool reflections_, bool heightQueries_) const;
        
        // Setup state.
        bool Setup(Triton::Ocean *ocean_, SimulationModel simulationModel_, bool reflections_, bool heightQueries_);
        
        // Reset state. @note Does not free ptrs.
        void Reset();

        // Currently created ocean settings.
        SimulationModel simulationModel;
        bool reflections;
        bool heightQueries;
#ifndef MESHMOON_SERVER_BUILD
        // Triton specific data
        Triton::Ocean *ocean;
        Triton::Environment *environment;
        Triton::ResourceLoader *resourceLoader;
#endif
        /// Progression state.
        float windDirDegrees;
        float windSpeedPerSec;
    };
    
    /// Get state.
    WaterState &State();
    
    /// Get Renderer.
    OgreRendererPtr Renderer() const;
    
    /// Get active camera
    EC_Camera *ActiveCamera() const;
    
    /// Get Ogre scene manager.
    Ogre::SceneManager* OgreSceneManager() const;
    
    /// Render system listener notified here if device loss happens.
    void DeviceLost();

    /// Render system listener notified here if device is restored after a device loss.
    void DeviceRestored();

#ifndef MESHMOON_SERVER_BUILD
    MeshmoonWaterRenderQueueListener  *RenderQueueListener() const { return renderQueueListener_; }
    MeshmoonWaterRenderSystemListener *RenderSystemListener() const { return renderSystemListener_; }
    MeshmoonWaterRenderTargetListener *RenderTargetListener() const { return renderTargetListener_; }
#endif

    // Internal boolean for tracking visibility/creation state.
    bool tritonVisible_;

    /// IComponent override
    void AttributesChanged();

    /// Log channel identifier
    const QString LC;

    /// Triton state.
    WaterState state_;

#ifndef MESHMOON_SERVER_BUILD
    // Water rendering utils.
    MeshmoonWaterRenderQueueListener  *renderQueueListener_;
    MeshmoonWaterRenderSystemListener *renderSystemListener_;
    MeshmoonWaterRenderTargetListener *renderTargetListener_;
#endif

    ComponentWeakPtr windConditionsComp_;
    OgreWorldWeakPtr world_;
    OgreRenderer::RendererWeakPtr renderer_;
    EC_Camera *activeCamera_;
};
COMPONENT_TYPEDEFS(MeshmoonWater);

/// @cond PRIVATE
#if 0 /// @todo OceanRotorWash and OceanImpact not used for anything right now
namespace Triton
{
    class Impact;
    class RotorWash;
    class WakeGenerator;
}
class MESHMOON_ENVIRONMENT_API OceanRotorWash : public QObject
{
    Q_OBJECT

public:
    OceanRotorWash(Triton::Ocean *ocean, float rotorDiameter, float3 position, float velocity, float3 rotorDownDirection);
    ~OceanRotorWash();

    void Update(float elapsedTime);

public slots:
    void SetRotorDiameter(float diameter) { rotorDiameter_ = diameter; }
    void SetPosition(const float3 &position) { position_ = position; }
    void SetVelocity(float velocity) { velocity_ = velocity; }
    void SetRotorDownDirection(const float3 &direction) { rotorDownDirection_ = direction; }

    float RotorDiameter() const { return rotorDiameter_; }
    float3 Position() const { return position_; }
    float Velocity() const { return velocity_; }
    float3 RotorDownDirection() const { return rotorDownDirection_; }

private:
    float rotorDiameter_;
    float3 position_;
    float velocity_;
    float3 rotorDownDirection_;

    float currentTime_;

    Triton::RotorWash *tritonRotorWash_;
};

class MESHMOON_ENVIRONMENT_API OceanImpact : public QObject
{
    Q_OBJECT

public:
    OceanImpact(Triton::Ocean *ocean, float impactorDiameter, float mass, float3 position, float3 direction);

    void SetPosition(const float3 &position) { position_ = position; }
    void SetDirection(const float3 &direction) { direction_ = direction; }

    float ImpactorDiameter() const { return impactorDiameter_; }
    float3 Position() const { return position_; }
    float Mass() const { return mass_; }
    
    void Update(float elapsedTime);

private:
    float impactorDiameter_;
    float mass_;
    float3 position_;
    float3 direction_;

    float currentTime_;

    Triton::Impact *tritonImpact_;
};
#endif
/// @endcond
