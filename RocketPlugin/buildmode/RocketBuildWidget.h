/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildWidget.h
    @brief  RocketBuilWidget is a build tool and UI for easy environment authoring. */

#pragma once

#include "RocketFwd.h"
#include "SceneFwd.h"

#include "RocketBuildEditor.h"
#include "RocketSceneWidget.h"
#include "RocketBlockPlacer.h"
#include "storage/MeshmoonStorageItem.h"

#include "ui_RocketBuildWidget.h"

#include "EC_Fog.h"
#include "EC_SkyX.h"
#include "EC_MeshmoonSky.h"

#include "AssetFwd.h"
#include "OgreModuleFwd.h"
#include "PhysicsModuleFwd.h"
#include "FrameworkFwd.h"
#include "InputFwd.h"

#include <QPointer>

class EntityListWidgetItem;
class AssetsWindow;
class EC_RigidBody;

class QColorDialog;
class QButtonGroup;

/// Build tool and UI for easy environment authoring.
class RocketBuildWidget : public RocketSceneWidget
{
    Q_OBJECT
    Q_ENUMS(BuildMode)
    Q_ENUMS(BuildCameraMode)
    Q_ENUMS(EnvironmentSettingMode)

public:
    /// @cond PRIVATE
    explicit RocketBuildWidget(RocketPlugin *plugin);
    virtual ~RocketBuildWidget();

    RocketBuildContextWidget *contextWidget;
    /// @endcond

    /// Different build modes/pages.
    enum BuildMode
    {
        EnvironmentMode,
        //TerrainMode, /**< @todo When enabling TerrainMode, remove +1 hack in SetMode */
        LightsMode,
        PhysicsMode,
        TeleportsMode,
        BlocksMode,
        ScenePackager
    };

    /// Different camera modes.
    enum BuildCameraMode
    {
        AvatarCameraMode,
        FreeLookCameraMode,
        SceneCameraMode,
        NumCameraModes
    };

    /// Different setting levels.
    enum EnvironmentSettingMode
    {
        ES_None,
        Simple,  ///< Sky or WaterPlane component
        Complex, ///< SkyX or EC_MeshmoonWater component
        ComplexMeshmoon ///< EC_MeshmoonSky
    };

    /// Returns list of components that should be added to a new building block depending on the user's selection on the UI.
    std::vector<ComponentPtr> ComponentsForNewBlock();

    /** Returns if the widget is pinned and should not 
        be resized/positioned by external logic. */
    bool IsPinned() const;

public slots:   
    /// RocketSceneWidget override.
    void Show();
    
    /// RocketSceneWidget override.
    void Hide(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF &sceneRect = QRectF());
    
    /// RocketSceneWidget override
    void SetVisible(bool visible);

    /// The scene of which content we are currently editing.
    ScenePtr Scene() const { return currentScene.lock(); }
    void SetScene(const ScenePtr &scene);

    // Currently active build mode.
    BuildMode Mode() const { return buildMode; }
    void SetMode(BuildMode mode);

    // Currently active editor mode.
    BuildCameraMode CameraMode() const { return cameraMode; }
    void SetCameraMode(BuildCameraMode mode);

    /// Sky setting/mode.
    EnvironmentSettingMode SkyMode() const { return skyMode; }
    void SetSkyMode(EnvironmentSettingMode mode);

    /// Water setting/mode.
    EnvironmentSettingMode WaterMode() const { return waterMode; }
    void SetWaterMode(EnvironmentSettingMode mode);

    /// Is fog enabled.
    bool IsFogEnabled() const { return FogMode() != EC_Fog::None; }
    void SetFogEnabled(bool enabled);
    void SetFogMode(EC_Fog::FogMode mode);

    /// Current fog mode.
    EC_Fog::FogMode FogMode() const;

    /// Rocket environment Sky/SkyX component.
    ComponentPtr SkyComponent() const { return sky.lock(); }
    void SetSkyComponent(const ComponentPtr &component);

    /// Rocket environment WaterPlane/Hydrax component.
    ComponentPtr WaterComponent() const { return water.lock(); }
    void SetWaterComponent(const ComponentPtr &component);

    /// Rocket environment Fog component.
    ComponentPtr FogComponent() const { return fog.lock(); }
    void SetFogComponent(const ComponentPtr &component);
    
    /// Rocket environment Shadow Setup component.
    ComponentPtr ShadowSetupComponent() const { return shadowSetup.lock(); }
    void SetShadowSetupComponent(const ComponentPtr &component);

    /// Rocket environment Terrain component.
    ComponentPtr TerrainComponent() const { return terrain.lock(); }
    void SetTerrainComponent(const ComponentPtr &component);

    /// Rocket environment EnvironmentLight component.
    ComponentPtr EnvironmentLightComponent() const { return environmentLight.lock(); }
    void SetEnvironmentLightComponent(const ComponentPtr &component);

    /// Rocket OgreCompositor (post-processing) component.
    /** @param name Name of the effect, used also for the component's name, f.ex. "HDR" or "Bloom". */
    ComponentPtr CompositorComponent(const QString &name) const
    {
        std::map<QString, ComponentWeakPtr>::const_iterator it = compositors.find(name);
        return it != compositors.end() ? it->second.lock() : ComponentPtr();
    }

    /** @param name Name of the effect, used also for the component's name, f.ex. "HDR" or "Bloom". */
    void SetCompositorComponent(const QString &name, const ComponentPtr &component);

    /// Adds new light to be managed with the editor.
    /** @return List widget item representing the Light component, or null, if item for the Light already exists. */
    EntityListWidgetItem *AddLight(const ComponentPtr &light, bool select = false);
    void RemoveLight(const EntityPtr &lightEntity, bool removeParentEntity);
    void RemoveLight(const ComponentPtr &light, bool removeParentEntity);

    /// Current cloud type, if Complex sky (SkyX) used.
    EC_SkyX::CloudType CloudType() const;
    void SetCloudType(EC_SkyX::CloudType type);
    
    void SetCloudType(EC_MeshmoonSky::CloudCoverage type);

    void SetPhysicsDebugEnabled(bool);
    bool IsPhysicsDebugEnabled() const { return physicsDebugEnabled; }

    /// Converts an EC type name to a more user-friendly component name by omitting the "EC_" prefix and converting to title case, f.ex. "EC_WebBrowser" -> "Web Browser".
    static QString UserFriendlyComponentName(const QString &componentType);

    /// Returns the default value for a compositor parameter.
    /** The default values are fixed and manually observed. */
    static double CompositorParameterDefaultValue(const char *compositorName, const char *parameterName);
    /// Returns value for a compositor parameter.
    /** If the compositor is not currently in use, the default value is returned. */
    double CompositorParameterValue(const char *compositorName, const char *parameterName) const;
    /// Sets value for a compositor parameter.
    void SetCompositorParameterValue(const char *compositorName, const char *parameterName, double value);

    QString AssetFilename(QString assetRef);

private:
    friend class RocketBuildEditor;

    // Helpers
    template <typename T> T *Component(const ComponentWeakPtr &comp) const { return dynamic_pointer_cast<T>(comp.lock()).get(); }
    void SetComponentInternal(const ComponentPtr &component, ComponentWeakPtr &destination, const char *handlerSlot);
    void ConnectToScene(const ScenePtr &scene) const;
    void DisconnectFromScene(const ScenePtr &scene);

    void InitializeCameraModes();
    /// Refreshes UI of the current page.
    void RefreshCurrentPage() { RefreshPage(buildMode); }
    /// Refreshes UI of a specific page.
    void RefreshPage(BuildMode mode);
    /// Performs per-page-specific cleanup.
    void OnPageClosing(BuildMode mode);
    
    // Teleport
    void ClearTeleportItems();
    void SetActiveTeleportEntity(const EntityPtr &entity);
    void RefreshActiveTeleportSettings();
    
    EntityListWidgetItem *AddTeleport(const ComponentPtr &tp, bool select = false);
    void RemoveTeleport(const EntityPtr &lightEntity, bool removeParentEntity);
    void RemoveTeleport(const ComponentPtr &light, bool removeParentEntity);

    // Lights
    void ClearLightItems();
    void CreateSceneCamera();
    void FocusCameraToEntity(const EntityPtr &entity);

    RocketPlugin *rocket;
    RocketBuildEditor *owner;
    Ui::RocketBuildWidget mainWidget;

    // Currently edited scene -related:
    SceneWeakPtr currentScene;
    OgreWorldWeakPtr ogreWorld;
    PhysicsWorldWeakPtr bulletWorld;
    
    // Rocket Environment Settings:
    BuildMode buildMode;
    BuildCameraMode cameraMode;
    EnvironmentSettingMode skyMode;
    EnvironmentSettingMode waterMode;
    bool fogEnabled;
    EC_Fog::FogMode prevFogMode;

    // Environment components:
    ComponentWeakPtr water; ///< Null, WaterPlane, Hydrax or MeshmoonWater.
    ComponentWeakPtr sky; ///< Null, Sky or SkyX.
    ComponentWeakPtr shadowSetup; ///< Null, SceneShadowSetup.
    ComponentWeakPtr fog;
    std::map<QString, ComponentWeakPtr> compositors;
    ComponentWeakPtr terrain;
    ComponentWeakPtr environmentLight; ///< Null, EnvironmentLight or SkyX.

    QString originalAssetReference;
    //QString blocksScriptRef;
    
    ComponentWeakPtr activeTeleport;
    ComponentWeakPtr activeLight;
    ComponentWeakPtr activePlaceable; ///< Placeable of the currently active object(light or similar).
    ComponentWeakPtr activeMesh; ///< Mesh of the currently active object.
    
    EntityWeakPtr activeEntity;
    EntityWeakPtr activeLightEntity;
    EntityWeakPtr activeTeleportEntity;
    EntityWeakPtr activeTeleportDestinationEntity;
    
    shared_ptr<TransformEditor> teleportTransformEditor;

    QListWidgetItem *lastClickedListItem;
    std::list<weak_ptr<EC_Mesh> > activeGroup;

    // Physics-related:
    bool physicsDebugEnabled;
    std::list<weak_ptr<EC_RigidBody> > rigidBodies;
    ComponentWeakPtr activeRigidBody; ///< RigidBody of the currently active object.

    // Camera biz:
    std::vector<weak_ptr<EC_Camera> > cameras;

    // Ui/editor/etc.-related:
    QPointer<QColorDialog> colorDialog;
    Color originalColor;
    QPointer<AssetsWindow> assetsWindow;

    // Blocks
    typedef std::map<u32, ComponentPtr> ComponentMap;
    QButtonGroup *componentButtonGroup;
    ComponentMap userSelectedComponents;
    ComponentMap entitysExistingComponents;
    QPointer<RocketComponentConfigurationDialog> componentConfigurationWidget;

    struct MaterialSelector
    {
        MaterialSelector() : button(0), asset(0), index(0), selecting(false) {}
        QPushButton *button;
        MeshmoonAsset *asset;
        uint index;
        bool selecting;
    };

    QList<MaterialSelector> materialSelectors;

    RocketScenePackager *scenePackager;
    QMenu *buildingComponentMenu;

    MeshmoonStorageItem lastStorageLocation_;
    
    RocketOgreSceneRenderer *sceneAxisRenderer;

private slots:
    void Resize();
    void RepositionSceneAxisOverlay();

    void SetPinned(bool pinned);
        
    void ShowHelp();

    void RefreshEnvironmentPage();
    void RefreshTerrainPage();
    void RefreshTeleportsPage();
    void RefreshLightsPage();
    void RefreshPhysicsPage();
    void RefreshBlocksPage();
    void RefreshScenePackagerPage();
    void RefreshBlocksVisualsSettings();

    /// Keeps the camera combo box in sync with the active camera in case the active camera is changed outside of the build UI.
    void RefreshSelectedCameraMode(Entity *activeCam);

    // These keep our UI in sync in case of scene being modified outside this editor.
    void OnEntityRemoved(Entity *entity);
    void OnComponentAdded(Entity *entity, IComponent *component);
    void OnComponentRemoved(Entity *entity, IComponent *component);

    void SetMode(int modeIndex) { SetMode(static_cast<BuildMode>(modeIndex)); }
    void SetCameraMode(int modeIndex) { SetCameraMode(static_cast<BuildCameraMode>(modeIndex)); }

    void SetSkyMode(int modeIndex);
    void OpenMaterialPickerForSky();
    void SetSkyMaterial(const AssetPtr &asset);
    void SetSkyMaterial(const MeshmoonStorageItem &item);
    void RestoreOriginalSkyMaterial();
    void SetSkyDirection(int direction);
    void SetSkySpeed(int speed);
    void SetSkyTime(const QTime &);
    void SetSkyTime(int value); /**< @param value Time as int [0, 2399] that will be converted to float by dividing by 100. */
    void SetSkyTimeMultiplier(double value);
    void SetSkyLatitude(double value);
    void SetSkyLongitude(double value);
    void SetSkyLightIntensity(double value);
    void SetSkyGodRayIntensity(double value);
    void SetSkyGamma(double value);
    void SetSkyDate();
    void SetSkyTimeMode(int value);
    void SetSkyTimeZone(const QString &value);
    QString GetSkyTimeZoneString(EC_MeshmoonSky::TimeZone zone);

    void SetWaterMode(int modeIndex);
    void OpenMaterialPickerForWater();
    void SetWaterMaterial(const AssetPtr &asset);
    void SetWaterMaterial(const MeshmoonStorageItem &item);
    void RestoreOriginalWaterMaterial();
    void ShowSkyAdvancedOptions(bool);
    void ShowWaterAdvancedOptions(bool);
    void ShowFogAdvancedOptions(bool);
    void SetWaterHeight(double);
    void SetWaterPlaneHeight(double);
    //void SetHydraxHeight(double);
    
    void SetMeshmoonWaterHeight(double value);
    void SetMeshmoonSimModel(int index);
    void SetMeshmoonWaterWeatherCondition(int index);
    void SetMeshmoonWaterChoppiness(double choppiness);
    void SetMeshmoonWaterReflectionIntensity(double intensity);
    void SetMeshmoonWaterSpray(bool enabled);
    void SetMeshmoonReflections(bool enabled);
    
    void SetShadowFirstSplitDist(double value);
    void SetShadowFarDist(double value);
    void SetShadowFadeDist(double value);
    void SetShadowSplitLambda(double value);
    void SetShadowDepthBias(double value);
    void ShowShadowSetupAdvancedOptions(bool enabled);
    void EnableShadowDebugPanels(bool enabled);
    
    void OpenAssetPicker(const QString &assetType, const char *onAssetChanged, const char *onAssetPicked, const char *onPickCanceled);
    void OpenStoragePicker(const QStringList &fileSuffixes, const char *onAssetPicked, const char *onPickCanceled);

    void RefreshSkySettings();
    void RefreshWaterSettings();
    void RefreshFogSettings();
    void RefreshShadowSetupSettings();
    void RefreshCompositorSettings();
    void RefreshTerrainSettings();
    void RefreshEnvironmentLightSettings();
    void RefreshActiveLightSettings();

    void OpenColorPickerForFogColor();
    void OpenColorPickerForSunlightColor();
    void OpenColorPickerForAmbientLightColor();
    void OpenColorPickerForLightDiffuseColor();
    void OpenColorPickerForLightSpecularColor();
    void OpenColorPickerForMeshmoonWater();
    void OpenColorPicker(const Color &initialColor, const char *changedHandler, const char *selectedHandler, 
                         const char *canceledHandler, const QString &colorButtonObjectName);

    void SetCloudType(int type);

    void SetFogModeInternal(int index); /**< @param index Fog mode check box item index. */
    void SetFogColor(const QColor &color, bool undoable);
    void SetFogColorNoUndo(const QColor &color) { SetFogColor(color, false); }
    void SetFogColorUndo(const QColor &color) { SetFogColor(color, true); }
    void RestoreFogColor() { SetFogColor(originalColor, false); }
    void SetFogExpDensity(double);
    void SetFogStartDistance(double);
    void SetFogEndDistance(double);

    void SetHdrEnabled(bool);
    void SetBloomEnabled(bool);
    void SetCompositorEnabled(const QString &name, bool enabled);

    void SetBloomBlurWeight(double value) { SetCompositorParameterValue("Bloom", "BlurWeight", value); }
    void SetBloomOriginalImageWeight(double value) { SetCompositorParameterValue("Bloom", "OriginalImageWeight", value); }
    void SetHdrBloomWeight(double value) { SetCompositorParameterValue("HDR", "BloomWeight", value); }
    void SetHdrMinLuminance(double value) { SetCompositorParameterValue("HDR", "LuminanceMin", value); }
    void SetHdrMaxLuminance(double value) { SetCompositorParameterValue("HDR", "LuminanceMax", value); }

    void SetSunlightColor(const QColor &color, bool undoable);
    void SetSunlightColorNoUndo(const QColor &color) { SetSunlightColor(color, false); }
    void SetSunlightColorUndo(const QColor &color) { SetSunlightColor(color, true); }
    void RestoreSunlightColor() { SetSunlightColor(originalColor, false); }

    void SetAmbientLightColor(const QColor &color, bool undoable);
    void SetAmbientLightColorNoUndo(const QColor &color) { SetAmbientLightColor(color, false); }
    void SetAmbientLightColorUndo(const QColor &color) { SetAmbientLightColor(color, true); }
    void RestoreAmbientLightColor() { SetAmbientLightColor(originalColor, false); }
    
    void SetMeshmoonWaterColor(const QColor &color, bool undoable);
    void SetMeshmoonWaterColorNoUndo(const QColor &color) { SetMeshmoonWaterColor(color, false); }
    void SetMeshmoonWaterColorUndo(const QColor &color) { SetMeshmoonWaterColor(color, true); }
    void RestoreMeshmoonWaterColor() { SetMeshmoonWaterColor(originalColor, false); }

    void SetEnvironmentLightDirectionFromCamera();
    
    // Teleport
    void SetActiveTeleport(QListWidgetItem *item);
    void SetActiveTeleportFromSelection(QListWidgetItem *selected, QListWidgetItem *previous);
    void AddNewTeleportToSelectedEntity();
    void AddNewTeleport();
    void AddNewTeleport(ScenePtr &scene, EntityPtr &target, bool setupPlaceable);
    void RemoveCurrentlyActiveTeleport();
    void ExecuteCurrentlyActiveTeleport();
    
    void SetTelePosX(double v);
    void SetTelePosY(double v);
    void SetTelePosZ(double v);
    void SetTelePos(const float3 &pos);
    
    void SetTeleRotX(double v);
    void SetTeleRotY(double v);
    void SetTeleRotZ(double v);
    void SetTeleRot(const float3 &rot);
    
    void SetTeleProximity(int v);
    void SetTeleConfirm(bool v);
    void SetTeleOnProximity(bool v);
    void SetTeleOnClick(bool v);
    
    void SetTeleTriggerEntity();
    void SetTeleDestinationSpace();
    void SetTeleDestinationName();
    
    void OnTeleportDestinationAttributeChanged(IAttribute *attribute, AttributeChange::Type change);
    void OnTeleportAttributeChanged(IAttribute *attribute, AttributeChange::Type change);

    // Lights
    void SetActiveLightFromSelection(QListWidgetItem *selected, QListWidgetItem *previous);
    void SetActiveLight(QListWidgetItem *item);
    void SetActiveLightEntity(const EntityPtr &entity);
    void FocusCameraToLight(QListWidgetItem *lightItem);

    void CreateNewLight();
    void CloneCurrentlyActiveLight();
    void RemoveCurrentlyActiveLight();
    void RefreshCurrentObjectSelection();
    void UpdateEntityName(IAttribute *);

    void SetLightType(int);

    void SetLightDiffuseColor(const QColor &color, bool undoable);
    void SetLightDiffuseColorNoUndo(const QColor &color) { SetLightDiffuseColor(color, false); }
    void SetLightDiffuseColorUndo(const QColor &color) { SetLightDiffuseColor(color, true); }
    void RestoreLightDiffuseColor() { SetLightDiffuseColor(originalColor, false); }
    
    void SetLightSpecularColor(const QColor &color, bool undoable);
    void SetLightSpecularColorNoUndo(const QColor &color) { SetLightSpecularColor(color, false); }
    void SetLightSpecularColorUndo(const QColor &color) { SetLightSpecularColor(color, true); }
    void RestoreLightSpecularColor() { SetLightSpecularColor(originalColor, false); }

    void SetColorInternal(QColorDialog *colorDialog, Attribute<Color> *attr, const Color &color, bool undoable);
    void SetButtonColor(QAbstractButton *button, const Color &color);

    void SetLightDirectionFromCamera();
    void SetLightCastShadowsEnabled(bool);
    void SetLightRange(double);
    void SetLightBrightness(double);
    void SetLightInnerAngle(double);
    void SetLightOuterAngle(double);

    /// Performs per-frame visualization tasks. @todo Move to RocketBuildEditor
    void Update(float frametime);

    void RefreshPlaceableOptions();
    void SetPlaceableScale(EC_Placeable *p, const float3 &size);
    void SetPlaceableScaleX(double);
    void SetPlaceableScaleY(double);
    void SetPlaceableScaleZ(double);

    void SetActivePhysicsEntity(const EntityPtr &entity);
    void ApplyPhysicsForEntity(bool);
    void SetPhysicsShape(int shapeIndex);
    void SetRigidBodySize(EC_RigidBody *rb, const float3 &size);
    void SetRigidBodySizeX(double);
    void SetRigidBodySizeY(double);
    void SetRigidBodySizeZ(double);
    void RefreshPhysicsEntityOptions();

    void SetActiveEntity(const EntityList &);

    void SetBlockPlacerMode(int mode);
    void SetBlockPlacementMode(int mode);

    /// Show selection dialog for asset storage.
    void ShowStorageMeshSelection();

    /// Show selection dialog for asset library.
    void ShowBlockMeshSelection();

    /// Show selection dialog for asset storage.
    void ShowStorageMaterialSelection();
    
    /// Shows material editor for clicked material selection widget.
    void ShowStorageMaterialEditor();

    /// Show selection dialog for asset library.
    void ShowBlockMaterialSelection();
    void ShowBlockMaterialSelection(int submeshIndex);

    /// Set current placer mesh with a storage item.
    void SetBlockMesh(const MeshmoonStorageItem &item);

    /// Set current placer mesh with a library asset.
    void SetBlockMesh(MeshmoonAsset *asset);

    /// Set current placer material with a storage item.
    /** @note No index, takes the first material that is in selecting state. */
    void SetBlockMaterial(const MeshmoonStorageItem &item);

    /// Set current placer material with a library asset.
    /** @note No index, takes the first material that is in selecting state. */
    void SetBlockMaterial(MeshmoonAsset *asset);

    /// Set current placer material for index.
    void SetBlockMaterial(uint materialIndex, MeshmoonAsset *asset);
    void CreateMaterialButton(MaterialSelector &material, int row, int col);

    void SetMaxBlockDistance(double value);

//    void ShowBlockFunctionalitySelector();
//    void AddWebBrowserFunctionality();
//    void AddMediaBrowserFunctionality();
//    void ClearFunctionality();

//    void OpenScriptPickerForBlocks();
//    void SetBlocksScript(const AssetPtr &asset);
//    void RestoreOriginalBlocksScript();

    void ShowBlocksFunctionalityOptions(bool);
    void ShowComponentEditor(int componentTypeId, bool skipMenu = false);
    void CloseComponentEditor();

    void AddOrRemoveComponent(QObject *sender);
    ComponentPtr AddOrRemoveComponent(bool enabled, u32 componentTypeId);
    void EditComponent(int componentTypeId);
    void AddWebBrowserFunctionality(const ComponentPtr &component);
    void AddMediaBrowserFunctionality(const ComponentPtr &component);

    void OnStorageSelectionCanceled();
    
    bool ShouldShowContextWidget() const;
};
Q_DECLARE_METATYPE(RocketBuildWidget*)
Q_DECLARE_METATYPE(RocketBuildWidget::BuildMode)
Q_DECLARE_METATYPE(RocketBuildWidget::BuildCameraMode)
Q_DECLARE_METATYPE(RocketBuildWidget::EnvironmentSettingMode)
