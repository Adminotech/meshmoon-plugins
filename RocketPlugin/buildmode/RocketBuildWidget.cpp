/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuilWidget.cpp
    @brief  RocketBuilWidget is a build tool and UI for easy environment authoring. */

#include "StableHeaders.h"
#define MATH_OGRE_INTEROP
#include "DebugOperatorNew.h"

#include "RocketBuildWidget.h"
#include "RocketBuildContextWidget.h"
#include "RocketBuildComponentWidgets.h"
#include "RocketBuildEditor.h"
#include "RocketBlockPlacer.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "RocketTaskbar.h"
#include "RocketMenu.h"
#include "RocketScenePackager.h"

#include "editors/RocketOgreSceneRenderer.h"
#include "utils/RocketAnimations.h"

#include "MeshmoonBackend.h"
#include "MeshmoonUser.h"
#include "MeshmoonAssetLibrary.h"
#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageHelpers.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "EC_WebBrowser.h"
#include "EC_MediaBrowser.h"
#include "EC_MeshmoonWater.h"
#include "EC_MeshmoonSky.h"
#include "EC_MeshmoonTeleport.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"

#include "EC_WaterPlane.h"
#include "EC_Sky.h"
#include "EC_EnvironmentLight.h"
#include "EC_Terrain.h"
#include "EC_Fog.h"
#include "EC_Light.h"
#include "EC_SkyX.h"
#include "EC_Hydrax.h"
#include "EC_Placeable.h"
#include "EC_SceneShadowSetup.h"
#include "OgreWorld.h"
#include "Renderer.h"
#include "ECEditorWindow.h"
#include "FrameAPI.h"
#include "EC_Camera.h"
#include "TransformEditor.h"
#include "UndoManager.h"
#include "UndoCommands.h"
#include "AssetsWindow.h"
#include "PhysicsWorld.h"
#include "EC_RigidBody.h"
#include "Application.h"
#include "EC_Mesh.h"
#include "EC_Name.h"
#include "InputAPI.h"
#include "EC_Avatar.h"
#include "EC_MeshmoonAvatar.h"
#include "OgreRenderingModule.h"
#include "OgreMeshAsset.h"
#include "OgreMaterialAsset.h"
#include "Geometry/Plane.h"
#include "ConfigAPI.h"
#include "EC_Script.h"
//#include "EC_SlideShow.h"
#include "ECEditorModule.h"
#include "Math/MathFunc.h"
#include "EC_OgreCompositor.h"

#include <QGraphicsScene>
#include <QColorDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSignalMapper>

#include <Ogre.h>
#include <OgreCommon.h>
#include <OgreMatrix4.h>
#include <OgreCamera.h>

#include <math.h>

#include "MemoryLeakCheck.h"

namespace
{
    // General UI settings
    const ConfigData cBuildModePinned("adminotech", "build mode", "pinned", false);
    const ConfigData cBuildModeSetting("adminotech", "build mode", "build mode", RocketBuildWidget::BlocksMode);
    const ConfigData cShowSkyAdvSetting("adminotech", "build mode", "show sky advanced options", false);
    const ConfigData cShowWaterAdvSetting("adminotech", "build mode", "show water advanced options", false);
    const ConfigData cShowShadowAdvSetting("adminotech", "build mode", "show shadow advanced options", false);
    const ConfigData cShowFogAdvSetting("adminotech", "build mode", "show fog advanced options", false);

    // Blocks UI settings
    const ConfigData cBlockPhysicsSetting("adminotech", "build mode", "apply physics", true);
    const ConfigData cBlockFunctionalitySetting("adminotech", "build mode", "apply functionality", true);
    const ConfigData cBlockLastWebUrl("adminotech", "build mode", "last web url", "http://");
    const ConfigData cBlockLastMediaUrl("adminotech", "build mode", "last media url", "http://");
    const ConfigData cBlockLastSubmeshIndex("adminotech", "build mode", "last submesh index", 0);

    const char * const cBuildingBlockTag = "building block";
    const char * const cHdrName = "HDR";
    const char * const cBloomName = "Bloom";

    /*void ClearLayout(QLayout *layout)
    {
        QLayoutItem *item;
        while((item = layout->itemAt(0)) != 0)
        {
            QWidget *w = item->widget();
            SAFE_DELETE(item);
            SAFE_DELETE(w);
        }
    }*/
}

RocketBuildWidget::RocketBuildWidget(RocketPlugin *plugin) :
    RocketSceneWidget(plugin->GetFramework()),
    rocket(plugin),
    owner(plugin->BuildEditor()),
    contextWidget(0),
    sceneAxisRenderer(0),
    cameraMode(AvatarCameraMode),
    skyMode(ES_None),
    waterMode(ES_None),
    prevFogMode(EC_Fog::None),
    physicsDebugEnabled(false),
    lastClickedListItem(0),
    scenePackager(0),
    buildingComponentMenu(0)
{
    // Read initial UI settings from the config.
    ConfigAPI &cfg = *Fw()->Config();
    buildMode = static_cast<BuildMode>(cfg.DeclareSetting(cBuildModeSetting).toInt());
    const bool showSkyAdvanced = cfg.DeclareSetting(cShowSkyAdvSetting).toBool();
    const bool showWaterAdvanced = cfg.DeclareSetting(cShowWaterAdvSetting).toBool();
    const bool showShadowAdvanced = cfg.DeclareSetting(cShowShadowAdvSetting).toBool();
    const bool showFogAdvanced = cfg.DeclareSetting(cShowFogAdvSetting).toBool();
    const bool blockApplyPhysics = cfg.DeclareSetting(cBlockPhysicsSetting).toBool();
    const bool blockApplyFunctionality = cfg.DeclareSetting(cBlockFunctionalitySetting).toBool();

    // Set up the UI.
    mainWidget.setupUi(widget_);

    connect(mainWidget.labelHelpButton, SIGNAL(clicked()), SLOT(ShowHelp()));

#ifdef __APPLE__
    mainWidget.buildModeComboBox->setEditable(true);
    mainWidget.cameraModeComboBox->setEditable(true);

    mainWidget.buildModeComboBox->setInsertPolicy(QComboBox::NoInsert);
    mainWidget.cameraModeComboBox->setInsertPolicy(QComboBox::NoInsert);
#endif

    mainWidget.buildModeComboBox->addItem(tr("Environment"));
    //mainWidget.buildModeComboBox->addItem(tr("Terrain"));
    mainWidget.buildModeComboBox->addItem(tr("Lights"));
    mainWidget.buildModeComboBox->addItem(tr("Physics"));
    mainWidget.buildModeComboBox->addItem(tr("Teleports"));
    mainWidget.buildModeComboBox->addItem(tr("Build"));
    mainWidget.buildModeComboBox->addItem(tr("Space Optimizer"));
    mainWidget.buildModeComboBox->setCurrentIndex(buildMode);

    mainWidget.cameraModeComboBox->addItem(tr("Avatar"));
    mainWidget.cameraModeComboBox->addItem(tr("Free Look"));
    //mainWidget.cameraModeComboBox->addItem(tr("Scene"));
    mainWidget.cameraModeComboBox->setCurrentIndex(cameraMode);

    mainWidget.skyComboBox->addItem(tr("None"));
    mainWidget.skyComboBox->addItem(tr("Simple"));
    mainWidget.skyComboBox->addItem(tr("SkyX"));
    mainWidget.skyComboBox->addItem(tr("Meshmoon Sky"));
    mainWidget.skyComboBox->setCurrentIndex(skyMode);

    mainWidget.waterComboBox->addItem(tr("None"));
    mainWidget.waterComboBox->addItem(tr("Simple"));
    mainWidget.waterComboBox->addItem(tr("Complex"));
    mainWidget.waterComboBox->setCurrentIndex(waterMode);

    connect(mainWidget.buildModeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetMode(int)));
    connect(mainWidget.cameraModeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetCameraMode(int)));

    // Sky settings
    mainWidget.cloudTypeComboBox->setCurrentIndex(EC_SkyX::None);

    connect(mainWidget.skyComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetSkyMode(int)));
    connect(mainWidget.skyAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowSkyAdvancedOptions(bool)));
    connect(mainWidget.cloudTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetCloudType(int)));
    connect(mainWidget.timeModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetSkyTimeMode(int)));
    connect(mainWidget.timeZoneComboBox, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(SetSkyTimeZone(const QString&)));
    connect(mainWidget.timeMultiplierSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyTimeMultiplier(double)));
    connect(mainWidget.latitudeSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyLatitude(double)));
    connect(mainWidget.longitudeSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyLongitude(double)));
    connect(mainWidget.skyLightIntensitySpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyLightIntensity(double)));
    connect(mainWidget.skyGodRayIntensitySpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyGodRayIntensity(double)));
    connect(mainWidget.skyGammaSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetSkyGamma(double)));
    connect(mainWidget.skyTimeSlider, SIGNAL(valueChanged(int)), this, SLOT(SetSkyTime(int)));
    connect(mainWidget.skyDirSlider, SIGNAL(valueChanged(int)), this, SLOT(SetSkyDirection(int)));
    connect(mainWidget.skySpeedSlider, SIGNAL(valueChanged(int)), this, SLOT(SetSkySpeed(int)));
    connect(mainWidget.skyMaterialButton, SIGNAL(clicked()), this, SLOT(OpenMaterialPickerForSky()));   
    connect(mainWidget.skyDateEdit, SIGNAL(editingFinished()), this, SLOT(SetSkyDate()));

    // Water settings
    mainWidget.waterHeightSpinBox->setRange(-FLT_MAX, FLT_MAX);
    connect(mainWidget.waterComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetWaterMode(int)));
    connect(mainWidget.waterAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowWaterAdvancedOptions(bool)));
    connect(mainWidget.waterHeightSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetWaterHeight(double)));
    connect(mainWidget.waterMaterialButton, SIGNAL(clicked()), this, SLOT(OpenMaterialPickerForWater()));
    connect(mainWidget.EC_MeshmoonWaterTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetMeshmoonSimModel(int)));
    connect(mainWidget.EC_MeshmoonWaterWeatherConditionComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetMeshmoonWaterWeatherCondition(int)));
    connect(mainWidget.EC_MeshmoonWaterColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForMeshmoonWater()));
    connect(mainWidget.EC_MeshmoonWaterChoppinessSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetMeshmoonWaterChoppiness(double)));
    connect(mainWidget.EC_MeshmoonWaterReflectionIntensitySpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetMeshmoonWaterReflectionIntensity(double)));
    connect(mainWidget.EC_MeshmoonWaterSprayCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetMeshmoonWaterSpray(bool)));
    connect(mainWidget.EC_MeshmoonWaterReflectionsCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetMeshmoonReflections(bool)));
    
    // Shadow setup settings
    connect(mainWidget.shadowsAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowShadowSetupAdvancedOptions(bool)));
    connect(mainWidget.shadowFirstSplitDistSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowFirstSplitDist(double)));
    connect(mainWidget.shadowFarDistanceSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowFarDist(double)));
    connect(mainWidget.shadowFadeDistSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowFadeDist(double)));
    connect(mainWidget.shadowSplitLambdaSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowSplitLambda(double)));
    connect(mainWidget.shadowDepthBias1SpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowDepthBias(double)));
    connect(mainWidget.shadowDepthBias2SpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowDepthBias(double)));
    connect(mainWidget.shadowDepthBias3SpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowDepthBias(double)));
    connect(mainWidget.shadowDepthBias4SpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetShadowDepthBias(double)));
    connect(mainWidget.shadowDebugPanelsCheckBox, SIGNAL(toggled(bool)), this, SLOT(EnableShadowDebugPanels(bool)));
    
    mainWidget.shadowDepthBias1SpinBox->setProperty("shadowBiasIndex", 1);
    mainWidget.shadowDepthBias2SpinBox->setProperty("shadowBiasIndex", 2);
    mainWidget.shadowDepthBias3SpinBox->setProperty("shadowBiasIndex", 3);
    mainWidget.shadowDepthBias4SpinBox->setProperty("shadowBiasIndex", 4);

    // Fog settings    
    mainWidget.fogModeComboBox->addItem(tr("Exponentially"), EC_Fog::Exponentially);
    mainWidget.fogModeComboBox->addItem(tr("ExponentiallySquare"), EC_Fog::ExponentiallySquare);
    mainWidget.fogModeComboBox->addItem(tr("Linear"), EC_Fog::Linear);

    // Max usable value for for exponential modes seems be around 0.4
    mainWidget.fogExpDensitySpinBox->setRange(0.f, 0.4f);
    //mainWidget.fogExpDensitySpinBox->setSingleStep(0.0001);
    mainWidget.fogStartDistanceSpinBox->setRange(1.f, FLT_MAX);
    mainWidget.fogEndDistanceSpinBox->setRange(1.f, FLT_MAX);

    connect(mainWidget.fogAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowFogAdvancedOptions(bool)));
    connect(mainWidget.fogColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForFogColor()), Qt::UniqueConnection);
    connect(mainWidget.fogButton, SIGNAL(toggled(bool)), this, SLOT(SetFogEnabled(bool)), Qt::UniqueConnection);
    connect(mainWidget.fogModeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetFogModeInternal(int)), Qt::UniqueConnection);
    connect(mainWidget.fogExpDensitySpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetFogExpDensity(double)), Qt::UniqueConnection);
    connect(mainWidget.fogStartDistanceSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetFogStartDistance(double)), Qt::UniqueConnection);
    connect(mainWidget.fogEndDistanceSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetFogEndDistance(double)), Qt::UniqueConnection);
    
    // Post-process
    connect(mainWidget.hdrCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetHdrEnabled(bool)), Qt::UniqueConnection);
    connect(mainWidget.bloomCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetBloomEnabled(bool)), Qt::UniqueConnection);

    connect(mainWidget.bloomBlurWeightSpinBox, SIGNAL(valueChanged(double)), SLOT(SetBloomBlurWeight(double)));
    connect(mainWidget.bloomOriginalImageWeightSpinBox, SIGNAL(valueChanged(double)), SLOT(SetBloomOriginalImageWeight(double)));

    connect(mainWidget.hdrBloomWeightSpinBox, SIGNAL(valueChanged(double)), SLOT(SetHdrBloomWeight(double)));
    connect(mainWidget.hdrMinLuminanceSpinBox, SIGNAL(valueChanged(double)), SLOT(SetHdrMinLuminance(double)));
    connect(mainWidget.hdrMaxLuminanceSpinBox, SIGNAL(valueChanged(double)), SLOT(SetHdrMaxLuminance(double)));

    mainWidget.hdrAdvancedOptionsWidget->setVisible(false);
    mainWidget.bloomAdvancedOptionsWidget->setVisible(false);
    
    // Teleports
    //   List
    mainWidget.teleportsListWidget->setSelectionMode(QListWidget::SingleSelection);
    connect(mainWidget.teleportsListWidget, SIGNAL(itemPressed(QListWidgetItem*)), SLOT(SetActiveTeleport(QListWidgetItem*)));
    connect(mainWidget.teleportsListWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(SetActiveTeleportFromSelection(QListWidgetItem*, QListWidgetItem*)));
    
    //   Buttons
    connect(mainWidget.newTeleportButton, SIGNAL(clicked()), SLOT(AddNewTeleport()));
    connect(mainWidget.addTeleportToEntityButton, SIGNAL(clicked()), SLOT(AddNewTeleportToSelectedEntity()));
    connect(mainWidget.removeTeleportButton, SIGNAL(clicked()), SLOT(RemoveCurrentlyActiveTeleport()));
    connect(mainWidget.executeTeleportButton, SIGNAL(clicked()), SLOT(ExecuteCurrentlyActiveTeleport()));
    
    //   Attributes
    connect(mainWidget.telePosXSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTelePosX(double)));
    connect(mainWidget.telePosYSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTelePosY(double)));
    connect(mainWidget.telePosZSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTelePosZ(double)));
    connect(mainWidget.teleRotXSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTeleRotX(double)));
    connect(mainWidget.teleRotYSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTeleRotY(double)));
    connect(mainWidget.teleRotZSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetTeleRotZ(double)));
    connect(mainWidget.teleportProximitySpinBox, SIGNAL(valueChanged(int)), this, SLOT(SetTeleProximity(int)));
    connect(mainWidget.confirmTeleportCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetTeleConfirm(bool)));
    connect(mainWidget.teleportOnProximityCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetTeleOnProximity(bool)));
    connect(mainWidget.teleportOnClickCheckBox, SIGNAL(toggled(bool)), this, SLOT(SetTeleOnClick(bool)));
    
    connect(mainWidget.teleportTriggerEntityLineEdit, SIGNAL(editingFinished()), this, SLOT(SetTeleTriggerEntity()));
    connect(mainWidget.teleportDestinationLineEdit, SIGNAL(editingFinished()), this, SLOT(SetTeleDestinationSpace()));
    connect(mainWidget.teleportDestinationNameLineEdit, SIGNAL(editingFinished()), this, SLOT(SetTeleDestinationName()));
    
    // Lights
    //   List
    mainWidget.lightListWidget->setSelectionMode(QListWidget::SingleSelection);
    connect(mainWidget.lightListWidget, SIGNAL(itemPressed(QListWidgetItem *)), SLOT(SetActiveLight(QListWidgetItem *)));
    connect(mainWidget.lightListWidget, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(SetActiveLightFromSelection(QListWidgetItem*, QListWidgetItem*)));
    connect(mainWidget.lightListWidget, SIGNAL(itemDoubleClicked(QListWidgetItem *)), SLOT(FocusCameraToLight(QListWidgetItem *)));
    
    //   Environment light
    connect(mainWidget.sunlightColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForSunlightColor()));
    connect(mainWidget.ambientLightColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForAmbientLightColor()));
    connect(mainWidget.setEnvLightDirFromCamButton, SIGNAL(clicked()), SLOT(SetEnvironmentLightDirectionFromCamera()));

    //   Add/remove
    connect(mainWidget.newLightButton, SIGNAL(clicked()), SLOT(CreateNewLight()));
    connect(mainWidget.cloneLightButton, SIGNAL(clicked()), SLOT(CloneCurrentlyActiveLight()));
    connect(mainWidget.removeLightButton, SIGNAL(clicked()), SLOT(RemoveCurrentlyActiveLight()));

    //   Combo boxes
    mainWidget.lightTypeComboBox->addItem(tr("Point"), EC_Light::PointLight);
    mainWidget.lightTypeComboBox->addItem(tr("Spot"), EC_Light::Spotlight);
    mainWidget.lightTypeComboBox->addItem(tr("Directional"), EC_Light::DirectionalLight);

    //   Buttons
    connect(mainWidget.lightDiffuseColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForLightDiffuseColor()));
    connect(mainWidget.lightSpecularColorButton, SIGNAL(clicked()), SLOT(OpenColorPickerForLightSpecularColor()));
    connect(mainWidget.setLightDirectionFromCameraButton, SIGNAL(clicked()), SLOT(SetLightDirectionFromCamera()));
    
    //   Properties
    connect(mainWidget.lightTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetLightType(int)));
    connect(mainWidget.lightCastShadowsButton, SIGNAL(toggled(bool)), this, SLOT(SetLightCastShadowsEnabled(bool)));
    connect(mainWidget.lightRangeSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetLightRange(double)));
    connect(mainWidget.lightBrightnessSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetLightBrightness(double)));
    connect(mainWidget.lightInnerAngleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetLightInnerAngle(double)));
    connect(mainWidget.lightOuterAngleSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetLightOuterAngle(double)));

    // Physics
    mainWidget.physicsApplyButton->setVisible(false);
    mainWidget.framePhysics->setVisible(false);
    mainWidget.physicsEntityTitle->setVisible(false);

    connect(mainWidget.physicsDebugButton, SIGNAL(toggled(bool)), this, SLOT(SetPhysicsDebugEnabled(bool)), Qt::UniqueConnection);
    connect(mainWidget.physicsApplyButton, SIGNAL(toggled(bool)), SLOT(ApplyPhysicsForEntity(bool)));
    connect(mainWidget.shapeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(SetPhysicsShape(int)), Qt::UniqueConnection);
    connect(mainWidget.physicsSizeXLabelSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetRigidBodySizeX(double)));
    connect(mainWidget.physicsSizeYLabelSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetRigidBodySizeY(double)));
    connect(mainWidget.physicsSizeZLabelSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetRigidBodySizeZ(double)));

    // Blocks
    connect(mainWidget.scaleXSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetPlaceableScaleX(double)));
    connect(mainWidget.scaleYSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetPlaceableScaleY(double)));
    connect(mainWidget.scaleZSpinBox, SIGNAL(valueChanged(double)), this, SLOT(SetPlaceableScaleZ(double)));
    mainWidget.blocksPlaceableOptionsWidget->setVisible(false);

    connect(mainWidget.blocksFunctionalityCheckBox, SIGNAL(toggled(bool)), SLOT(ShowBlocksFunctionalityOptions(bool)));

    mainWidget.blocksVisualsCheckBox->setEnabled(false);
    mainWidget.blocksVisualsCheckBox->setChecked(true);
    mainWidget.blocksVisualsOptionsWidget->setVisible(false);

    mainWidget.blocksPhysicsCheckBox->setChecked(blockApplyPhysics);
    mainWidget.blocksPhysicsOptionsWidget->setVisible(false);

    mainWidget.blocksFunctionalityCheckBox->setChecked(blockApplyFunctionality);
    ShowBlocksFunctionalityOptions(blockApplyFunctionality);

    // Placer mode
    QButtonGroup *placerModeGroup = new QButtonGroup(this);
    placerModeGroup->setExclusive(true);
    placerModeGroup->addButton(mainWidget.blockAddButton, RocketBlockPlacer::CreateBlock);
    placerModeGroup->addButton(mainWidget.blockRemoveButton, RocketBlockPlacer::RemoveBlock);
    placerModeGroup->addButton(mainWidget.blockEditButton, RocketBlockPlacer::EditBlock);
    placerModeGroup->addButton(mainWidget.blockCloneButton, RocketBlockPlacer::CloneBlock);
    connect(placerModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(SetBlockPlacerMode(int)));

    // Placement mode
    QButtonGroup *placementModeGroup = new QButtonGroup(this);
    placementModeGroup->setExclusive(true);
    placementModeGroup->addButton(mainWidget.blockSnapPlacementButton, RocketBlockPlacer::SnapPlacement);
    placementModeGroup->addButton(mainWidget.blockFreePlacementButton, RocketBlockPlacer::FreePlacement);
    connect(placementModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(SetBlockPlacementMode(int)));

    componentButtonGroup = new QButtonGroup(this);
    componentButtonGroup->setExclusive(false);
    connect(componentButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(ShowComponentEditor(int)));

    mainWidget.blockMaxDistanceSpinBox->setSingleStep(1.f);
    mainWidget.blockMaxDistanceSpinBox->setRange(0.f, 200.f);
    mainWidget.blockMaxDistanceSpinBox->setValue(owner->BlockPlacer()->MaxBlockDistance());
    connect(mainWidget.blockMaxDistanceSpinBox, SIGNAL(valueChanged(double)), SLOT(SetMaxBlockDistance(double)));

    connect(mainWidget.skyAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowSkyAdvancedOptions(bool)));
    connect(mainWidget.waterAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowWaterAdvancedOptions(bool)));
    connect(mainWidget.fogAdvancedOptionsButton, SIGNAL(toggled(bool)), SLOT(ShowFogAdvancedOptions(bool)));

    // Misc.
    connect(Fw()->Frame(), SIGNAL(Updated(float)), SLOT(Update(float)));
    connect(Fw()->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), SLOT(Resize()));
    connect(Fw()->Module<OgreRenderingModule>()->Renderer().get(), SIGNAL(MainCameraChanged(Entity *)), SLOT(RefreshSelectedCameraMode(Entity *)));
    connect(owner->BlockPlacer(), SIGNAL(SettingsChanged()), SLOT(RefreshBlocksPage()));

    connect(owner, SIGNAL(SelectionChanged(const EntityList &)), SLOT(SetActiveEntity(const EntityList &)));
    connect(owner, SIGNAL(SelectionChanged(const EntityList &)), SLOT(RefreshCurrentObjectSelection()));
    
    hide();

    SetMode(buildMode);
    //SetCameraMode(cameraMode);
    ShowSkyAdvancedOptions(showSkyAdvanced);
    ShowWaterAdvancedOptions(showWaterAdvanced);
    ShowFogAdvancedOptions(showFogAdvanced);
    ShowShadowSetupAdvancedOptions(showShadowAdvanced);
    
    SetPinned(cfg.DeclareSetting(cBuildModePinned).toBool());
    connect(mainWidget.pinBuildMode, SIGNAL(toggled(bool)), SLOT(SetPinned(bool)));

    Fw()->Ui()->GraphicsScene()->addItem(this);
    assert(scene());
}

RocketBuildWidget::~RocketBuildWidget()
{
    // Save UI state
    ConfigAPI &cfg = *Fw()->Config();
    cfg.Write(cBuildModeSetting, buildMode);
    cfg.Write(cShowSkyAdvSetting, mainWidget.skyAdvancedOptionsButton->isChecked());
    cfg.Write(cShowWaterAdvSetting, mainWidget.waterAdvancedOptionsButton->isChecked());
    cfg.Write(cShowShadowAdvSetting, mainWidget.shadowsAdvancedOptionsButton->isChecked());
    cfg.Write(cShowFogAdvSetting, mainWidget.fogAdvancedOptionsButton->isChecked());
    cfg.Write(cBlockPhysicsSetting, mainWidget.blocksPhysicsCheckBox->isChecked());
    cfg.Write(cBlockFunctionalitySetting, mainWidget.blocksFunctionalityCheckBox->isChecked());
    cfg.Write(cBuildModePinned, mainWidget.pinBuildMode->isChecked());

    ClearLightItems();

    CloseComponentEditor();
    SAFE_DELETE(contextWidget);
    SAFE_DELETE(scenePackager);

    Fw()->Ui()->GraphicsScene()->removeItem(this);
}

std::vector<ComponentPtr> RocketBuildWidget::ComponentsForNewBlock()
{
    std::vector<ComponentPtr> components;
    if (!Scene())
        return components;
        
    // Time to close the component editor, they are being retrieved.
    CloseComponentEditor();
    
    // Get current mesh asset from block placer.
    MeshmoonAsset *meshAsset = owner->BlockPlacer()->MeshAsset();

    // Name
    shared_ptr<EC_Name> name = Fw()->Scene()->CreateComponent<EC_Name>(0);
    if (meshAsset)
    {
        name->name.Set(meshAsset->name, AttributeChange::Disconnected);
        
        // Is this a 'building block', detect from metadata
        if (meshAsset->ContainsTag(cBuildingBlockTag))
            name->description.Set(cBuildingBlockTag, AttributeChange::Disconnected);
    }
    components.push_back(name);

    // Placeable
    shared_ptr<EC_Placeable> placeable = Fw()->Scene()->CreateComponent<EC_Placeable>(0);
    placeable->SetWorldTransform(owner->BlockPlacer()->Placeable()->WorldTransform());
    components.push_back(placeable);

    // Visuals i.e. Mesh
    if (mainWidget.blocksVisualsCheckBox->isChecked() && meshAsset)
    {
        shared_ptr<EC_Mesh> mesh = Fw()->Scene()->CreateComponent<EC_Mesh>(0);

        // Library mesh
        mesh->meshRef.Set(meshAsset->AssetReference(), AttributeChange::Disconnected);

        // Library materials
        AssetReferenceList materials("OgreMaterial");
        
        // Detect submesh count from metadata or from the actual loaded mesh asset (in case this is not a library asset with metadata).
        int numSubmeshes = meshAsset->metadata.value("submeshes", 0).toInt();
        if (numSubmeshes <= 0)
        {
            OgreMeshAsset *loadedAsset = qobject_cast<OgreMeshAsset*>(meshAsset->Asset(framework_->Asset()).get());
            if (loadedAsset && loadedAsset->IsLoaded())
                numSubmeshes = (int)loadedAsset->NumSubmeshes();
        }
        for(int subMeshIdx = 0; subMeshIdx < numSubmeshes; ++subMeshIdx)
        {
            // Check if there is a selected material for this submesh
            bool found = false;
            for(int i = 0; i < owner->BlockPlacer()->Materials().size(); ++i)
            {
                MeshmoonAsset *asset = owner->BlockPlacer()->Materials()[i].libraryAsset;
                if (i == subMeshIdx && asset)
                {
                    materials.Append(asset->AssetReference());
                    found = true;
                    break;
                }
            }
            if (!found)
                materials.Append(AssetReference());
        }

        mesh->materialRefs.Set(materials, AttributeChange::Disconnected);

        components.push_back(mesh);
    }

    // Physics i.e. RigidBody
    if (mainWidget.blocksPhysicsCheckBox->isChecked() && meshAsset)
    {
        shared_ptr<EC_RigidBody> rigidBody = Fw()->Scene()->CreateComponent<EC_RigidBody>(0);

        // If the 'rigidbodyshape' metadata is not found, TriMesh is used.
        rigidBody->shapeType.Set(StringToRigidBodyShapeType(meshAsset->metadata.value("rigidbodyshape").toString()), AttributeChange::Disconnected);
        rigidBody->size.Set(owner->BlockPlacer()->Mesh()->WorldAABB().Size(), AttributeChange::Disconnected);

        components.push_back(rigidBody);
    }

    // Functionality components
    if (mainWidget.blocksFunctionalityCheckBox->isChecked())
        for(ComponentMap::const_iterator it = userSelectedComponents.begin(); it != userSelectedComponents.end(); ++it)
            components.push_back(it->second);

    return components;
}

void RocketBuildWidget::SetVisible(bool visible)
{
    if (!Fw()->Scene()->MainCameraScene())
        return;

    if (visible)
        Show();
    else
    {
        // Ask scene packager if it can be closed, confirmed via user dialog.
        if (scenePackager && scenePackager->IsRunning())
        {
            if (scenePackager->RequestStop())
                connect(scenePackager, SIGNAL(Stopped()), this, SLOT(Hide()), Qt::UniqueConnection);
            return;
        }
        Hide();
    }
}

void RocketBuildWidget::Show()
{
    if (!IsHiddenOrHiding())
        return;

    SetScene(Fw()->Scene()->MainCameraScene()->shared_from_this());

    if (!sceneAxisRenderer)
    {
        // Renderer
        sceneAxisRenderer = new RocketOgreSceneRenderer(rocket, QSize(80,80), Ogre::PF_A8R8G8B8);
        sceneAxisRenderer->SetAmbientLightColor(Color(1,1,1));
        sceneAxisRenderer->SetBackgroundColor(Color(0,0,0,0));
        sceneAxisRenderer->CreateLight(Ogre::Light::LT_DIRECTIONAL, Color(QColor(200, 200, 200)));       

        // Objects
        sceneAxisRenderer->CreateMesh("Ogre Media:MeshmoonGizmoPosition.mesh", QStringList() << "Ogre Media:MeshmoonXAxis.material" 
            << "Ogre Media:MeshmoonYAxis.material" << "Ogre Media:MeshmoonZAxis.material", Transform(float3::zero, float3::zero, float3(1,1,-1)));
        sceneAxisRenderer->CreateMesh("Ogre Media:MeshmoonGizmoPosition.mesh", QStringList(), Transform(float3::zero, float3::zero, float3(-1,-1,1)));

        // Overlay
        sceneAxisRenderer->CreateOverlay(float2(80.f, 80.f), float2(0.f, 0.f));
    }

    Resize();
    Animate(size(), pos(), -1.0, Fw()->Ui()->GraphicsScene()->sceneRect(), RocketAnimations::AnimateRight);

    RefreshCurrentPage();    
}

void RocketBuildWidget::Hide(RocketAnimations::Animation anim, const QRectF &sceneRect)
{
    OnPageClosing(buildMode);

    if (contextWidget)
        contextWidget->Hide();

    RocketSceneWidget::Hide();

    SAFE_DELETE(sceneAxisRenderer);
}

void RocketBuildWidget::SetMode(BuildMode mode)
{
    OnPageClosing(buildMode);
    buildMode = mode;
    RefreshPage(buildMode);

    mainWidget.contextWidget->setCurrentIndex(buildMode > 0 ? buildMode + 1 : buildMode); /**< @todo Fow now terrain is missing, hence the + 1. */

    // Try resetting width if a tab expanded the parent.
    if (IsVisible())
    {
        if (animations_->IsRunning())
            return;
        QTimer::singleShot(1, this, SLOT(Resize()));
    }
}

// http://www.ogre3d.org/forums/viewtopic.php?f=2&t=26244&start=0
/*float4x4 BuildScaledOrthoMatrix(float left, float right, float bottom, float top, float near_, float far_)
{
    float invw = 1 / (right - left);
    float invh = 1 / (top - bottom);
    float invd = 1 / (far_ - near_);

    float4x4 proj = float4x4::identity;
    proj.v[0][0] = 2 * invw;
    proj.v[0][3] = -(right + left) * invw;
    proj.v[1][1] = 2 * invh;
    proj.v[1][3] = -(top + bottom) * invh;
    proj.v[2][2] = -2 * invd;
    proj.v[2][3] = -(far_ + near_) * invd;
    proj.v[3][3] = 1;
    return proj;
}*/

void RocketBuildWidget::SetCameraMode(BuildCameraMode mode)
{
    if (cameras.empty())
        InitializeCameraModes();

    float3x4 previousCameraTm = float3x4::identity;
    if (!cameras[cameraMode].expired())
        previousCameraTm = cameras[cameraMode].lock()->ParentEntity()->Component<EC_Placeable>()->WorldTransform();

    cameraMode = mode;

    if (!cameras[cameraMode].expired())
    {
        if (cameraMode != AvatarCameraMode && !previousCameraTm.IsIdentity())
            cameras[cameraMode].lock()->ParentEntity()->Component<EC_Placeable>()->SetWorldTransform(previousCameraTm);

        cameras[cameraMode].lock()->SetActive();

        /// @todo Animate transition?
    }
}

void RocketBuildWidget::Resize()
{
    RocketTaskbar *taskbar = rocket->Taskbar();
    const qreal taskbarHeight = (taskbar && taskbar->isVisible() ? taskbar->Rect().height() : 0.f);
    const qreal paddingTop = (taskbar && taskbar->isVisible() && !taskbar->IsBottomAnchored() ? taskbar->Rect().bottom() : 0.f);

    const QRectF sceneRect = Fw()->Ui()->GraphicsScene()->sceneRect();
    const qreal h = sceneRect.height() - taskbarHeight;

    // QWidget
    widget_->setFixedHeight(h);

    // Proxy
    setPos(0.f, paddingTop);
    resize(Max(0.2f * (float)sceneRect.width(), 390.f), h);

    RepositionSceneAxisOverlay();
}

void RocketBuildWidget::RepositionSceneAxisOverlay()
{
    if (!sceneAxisRenderer || !sceneAxisRenderer->OgreOverlayElement())
        return;

    RocketTaskbar *taskbar = rocket->Taskbar();
    qreal y = (taskbar && taskbar->isVisible() && !taskbar->IsBottomAnchored() ? taskbar->Rect().bottom() : 0.0);
    if (contextWidget && contextWidget->isVisible())
    {
        QPointF cPos = contextWidget->scenePos();
        y += contextWidget->size().height();
        if (cPos.y() < 0.0)
            y += cPos.y();
    }
    qreal x = size().width();
    if (scenePos().x() < 0.0)
        x += scenePos().x();
    
    if (x < 0.0) x = 0.0;
    if (y < 0.0) y = 0.0;
    sceneAxisRenderer->SetOverlayPosition(x, y);
}

void RocketBuildWidget::ShowHelp()
{
    QToolTip::showText(QCursor::pos(), mainWidget.labelHelpButton->toolTip());
}

void RocketBuildWidget::RefreshSelectedCameraMode(Entity *camEntity)
{
    if (cameras.empty())
        InitializeCameraModes();

    EC_Camera *cam = (camEntity ? camEntity->Component<EC_Camera>().get() : 0);
    for(size_t i = 0; i < cameras.size(); ++i)
        if (!cameras[i].expired() && cameras[i].lock().get() == cam)
        {
            mainWidget.cameraModeComboBox->blockSignals(true);
            mainWidget.cameraModeComboBox->setCurrentIndex((int)i);
            mainWidget.cameraModeComboBox->blockSignals(false);
            break;
        }
}

void RocketBuildWidget::SetSkyMode(EnvironmentSettingMode mode)
{
    bool skyWasDisabled = (skyMode != mode && skyMode == ES_None);
    
    skyMode = mode;
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    mainWidget.skyAdvancedOptionsButton->setVisible(skyMode != ES_None);
    if (mainWidget.skyAdvancedOptionsButton->isVisible())
    {
        mainWidget.skyAdvancedOptionsButton->setText(mainWidget.skyAdvancedOptionsButton->isChecked() ? "Hide Advanced Options" : "Advanced Options");

        // By default show the advanced options when going from None to another mode.
        if (skyWasDisabled)
            mainWidget.skyAdvancedOptionsButton->setChecked(true);
    }

    switch(skyMode)
    {
        default:
        case ES_None:
        {
            SetSkyComponent(ComponentPtr());
            SetEnvironmentLightComponent(ComponentPtr());
            mainWidget.skyAdvancedOptionsButton->setChecked(false);
            break;
        }
        case Simple: // Use Sky for sky and EnvironmentLight for environmentLight.
        {
            if (!Component<EC_Sky>(sky))
                SetSkyComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_Sky>("RocketSky"));
            if (!Component<EC_EnvironmentLight>(environmentLight))
                SetEnvironmentLightComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_EnvironmentLight>("RocketEnvironmentLight"));
            break;
        }
        case Complex: // Use SkyX for both sky and environmentLight.
        {
            if (!Component<EC_SkyX>(sky))
                SetSkyComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_SkyX>("RocketSky"));
            if (!Component<EC_SkyX>(environmentLight))
                SetEnvironmentLightComponent(sky.lock());
            break;
        }
        case ComplexMeshmoon:
        {
            if (!Component<EC_MeshmoonSky>(sky))
                SetSkyComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_MeshmoonSky>("RocketSky"));
        }
    }

    ConnectToScene(scene);
}

void RocketBuildWidget::SetWaterMode(EnvironmentSettingMode mode)
{
    bool waterWasDisabled = (waterMode != mode && waterMode == ES_None);

    // Save current/previous water height for possible re-use.
    float previousWaterHeight = 0.f;
    if (Component<EC_WaterPlane>(water))
        previousWaterHeight = Component<EC_WaterPlane>(water)->position.Get().y;
    /*else if (Component<EC_Hydrax>(water))
        previousWaterHeight = Component<EC_Hydrax>(water)->position.Get().y;*/
    else if (Component<EC_MeshmoonWater>(water))
        previousWaterHeight = Component<EC_MeshmoonWater>(water)->seaLevel.Get();

    waterMode = mode;
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    mainWidget.waterAdvancedOptionsButton->setVisible(waterMode != ES_None);
    if (mainWidget.waterAdvancedOptionsButton->isVisible())
        mainWidget.waterAdvancedOptionsButton->setText(mainWidget.waterAdvancedOptionsButton->isChecked() ? tr("Hide Advanced Options") : tr("Advanced Options"));

    switch(waterMode)
    {
    case ES_None:
    default:
        SetWaterComponent(ComponentPtr());
        mainWidget.waterAdvancedOptionsButton->setChecked(false);
        break;
    case Simple:
        if (!Component<EC_WaterPlane>(water))
            SetWaterComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_WaterPlane>("RocketWater"));
        break;
    case Complex:
        /*if (!Component<EC_Hydrax>(water))
            SetWaterComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_Hydrax>("RocketWater"));*/
        if (!Component<EC_MeshmoonWater>(water))
            SetWaterComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_MeshmoonWater>("RocketWater"));
        break;
    }

    // Restore possible previous water height if changing between Simple and Complex
    // Calls RefreshWaterSettings.
    SetWaterHeight(previousWaterHeight);
    ShowWaterAdvancedOptions(mainWidget.waterAdvancedOptionsButton->isChecked());

    // By default show the advanced options when going from None to another mode.
    if (mainWidget.waterAdvancedOptionsButton->isVisible() && waterWasDisabled)
        mainWidget.waterAdvancedOptionsButton->setChecked(true);
    
    ConnectToScene(scene);
}

void RocketBuildWidget::SetFogEnabled(bool enabled)
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    mainWidget.fogButton->setText(enabled ? tr("Enabled") : tr("Disabled"));
    mainWidget.fogAdvancedOptionsButton->setVisible(enabled);
    if (mainWidget.fogAdvancedOptionsButton->isVisible())
        mainWidget.fogAdvancedOptionsButton->setText(mainWidget.fogAdvancedOptionsButton->isChecked() ? tr("Hide Advanced Options") : tr("Advanced Options"));

    if (enabled)
    {
        if (!Component<EC_Fog>(fog))
        {
            SetFogComponent(owner->EnvironmentEntity()->GetOrCreateComponent<EC_Fog>("RocketFog"));

            // The defaults in EC_Fog are not that great. Set saner defaults.
            if (Component<EC_Fog>(fog))
            {
                Component<EC_Fog>(fog)->startDistance.Set(1750.0f, AttributeChange::Default);
                Component<EC_Fog>(fog)->endDistance.Set(3000.0f, AttributeChange::Default);
            }
        }
        if (prevFogMode != EC_Fog::None)
            SetFogMode(prevFogMode); // Restore previously used mode
    }
    else
    {
        SetFogComponent(ComponentPtr());
        mainWidget.fogAdvancedOptionsButton->setChecked(false);
    }
    
    ShowFogAdvancedOptions(enabled);

    ConnectToScene(scene);
}

void RocketBuildWidget::SetFogMode(EC_Fog::FogMode mode)
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
    {
        prevFogMode = static_cast<EC_Fog::FogMode>(ec->mode.Get());
        owner->SetAttribute(ec->mode, static_cast<int>(mode));
    }

    RefreshFogSettings(); // Different fog modes can have different settings.
}

void RocketBuildWidget::SetScene(const ScenePtr &scene)
{
    // Disconnect from previous scene if not expired
    ScenePtr previous = Scene();
    if (previous)
        DisconnectFromScene(previous);

    currentScene = scene;
    ogreWorld = (scene ? scene->Subsystem<OgreWorld>() : OgreWorldWeakPtr());
    bulletWorld = (scene ? scene->Subsystem<PhysicsWorld>() : PhysicsWorldWeakPtr());

    owner->SetScene(scene);

    if (scene)
        ConnectToScene(scene);

    if (!ogreWorld.expired())
        RefreshSelectedCameraMode(ogreWorld.lock()->Renderer()->MainCamera());
}

void RocketBuildWidget::OnEntityRemoved(Entity *entity)
{
    ComponentPtr comp = entity->Component(EC_Light::ComponentTypeId);
    if (comp)
        RemoveLight(comp, false);
        
    comp = entity->Component(EC_MeshmoonTeleport::ComponentTypeId);
    if (comp)
        RemoveTeleport(comp, false);
}

void RocketBuildWidget::OnComponentAdded(Entity * /*entity*/, IComponent *component)
{
    /// @todo For now simply re-using code of OnComponentRemoved instead of copy-pasting it here.
    switch(component->TypeId())
    {
    case EC_Light::ComponentTypeId:
        AddLight(component->shared_from_this());
        break;
    case EC_MeshmoonTeleport::ComponentTypeId:
        AddTeleport(component->shared_from_this());
        break;
    default:
        OnComponentRemoved(0/*entity*/, component);
        break;
    }
}

void RocketBuildWidget::OnComponentRemoved(Entity * /*entity*/, IComponent *component)
{
    /// @todo More fine-grained handling for others than Lights too.
    switch(component->TypeId())
    {
    case EC_Sky::ComponentTypeId:
    case EC_SkyX::ComponentTypeId:
    case EC_WaterPlane::ComponentTypeId:
    case EC_Hydrax::ComponentTypeId:
    case EC_Fog::ComponentTypeId:
    case EC_OgreCompositor::ComponentTypeId:
        RefreshEnvironmentPage();
        break;
    case EC_Terrain::ComponentTypeId:
        RefreshTerrainPage();
        break;
    case EC_MeshmoonTeleport::ComponentTypeId:
        RemoveTeleport(component->shared_from_this(), false);
        break;
    case EC_EnvironmentLight::ComponentTypeId:
        RefreshLightsPage();
    case EC_Light::ComponentTypeId:
        RemoveLight(component->shared_from_this(), false);
        break;
    case EC_RigidBody::ComponentTypeId:
        RefreshPhysicsPage();
        break;
    default: // Not interested in these components.
        break;
    }
}

void RocketBuildWidget::SetWaterMode(int mode)
{
    if (!water.expired())
    {           
        QMessageBox::StandardButton result = rocket->Notifications()->ExecSplashDialog(
            tr("This will remove your current water and forgets its settings.<br><br><b>Continue Anyway?</b>"), ":/images/icon-boat.png",
            QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);

        if (result != QMessageBox::Yes)
        {
            mainWidget.waterComboBox->blockSignals(true);
            mainWidget.waterComboBox->setCurrentIndex(waterMode);
            mainWidget.waterComboBox->blockSignals(false);
            return;
        }
    }

    SetWaterMode(static_cast<EnvironmentSettingMode>(mode));
}

void RocketBuildWidget::OpenMaterialPickerForWater()
{
    originalAssetReference = "";
    EC_WaterPlane *ec = Component<EC_WaterPlane>(water);
    if (ec)
    {
        originalAssetReference = ec->materialRef.Get().ref;
        //OpenAssetPicker("OgreMaterial", SLOT(SetWaterMaterial(const AssetPtr &)), SLOT(SetWaterMaterial(const AssetPtr&)), SLOT(RestoreOriginalWaterMaterial()));
        OpenStoragePicker(QStringList() << "material", SLOT(SetWaterMaterial(const MeshmoonStorageItem&)), SLOT(RestoreOriginalWaterMaterial()));
    }
}

void RocketBuildWidget::SetWaterMaterial(const AssetPtr &asset)
{
    EC_WaterPlane *ec = Component<EC_WaterPlane>(water);
    if (ec)
    {
        owner->SetAttribute(ec->materialRef, AssetReference(asset->Name()));
        mainWidget.waterMaterialButton->setText(AssetFilename(asset->Name()));
    }
}

void RocketBuildWidget::SetWaterMaterial(const MeshmoonStorageItem &item)
{
    EC_WaterPlane *ec = Component<EC_WaterPlane>(water);
    if (ec)
    {
        owner->SetAttribute(ec->materialRef, AssetReference(item.relativeAssetRef));
        mainWidget.waterMaterialButton->setText(AssetFilename(item.filename));
    }
}

void RocketBuildWidget::RestoreOriginalWaterMaterial()
{
    EC_WaterPlane *ec = Component<EC_WaterPlane>(water);
    if (ec && !originalAssetReference.isEmpty())
    {
        owner->SetAttribute(ec->materialRef, AssetReference(originalAssetReference));
        mainWidget.waterMaterialButton->setText(AssetFilename(originalAssetReference));
    }
}

void RocketBuildWidget::ShowSkyAdvancedOptions(bool show)
{
    mainWidget.skyAdvancedOptionsButton->setText(show ? tr("Hide Advanced Options") : tr("Advanced Options"));
    mainWidget.skyAdvancedOptionsFrame->setVisible(show);
}

void RocketBuildWidget::ShowWaterAdvancedOptions(bool show)
{
    if (mainWidget.waterAdvancedOptionsButton->isHidden() || (!Component<EC_MeshmoonWater>(water) && !Component<EC_WaterPlane>(water)))
        show = false;
    if (mainWidget.waterAdvancedOptionsButton->isChecked() != show)
    {
        mainWidget.waterAdvancedOptionsButton->blockSignals(true);
        mainWidget.waterAdvancedOptionsButton->setChecked(show);
        mainWidget.waterAdvancedOptionsButton->blockSignals(false);
    }
    mainWidget.waterAdvancedOptionsButton->setText(show ? tr("Hide Advanced Options") : tr("Advanced Options"));

    EC_MeshmoonWater *meshmoonWater = Component<EC_MeshmoonWater>(water);
    mainWidget.waterEC_MeshmoonWaterOptionsFrame->setVisible(show && meshmoonWater != 0);
    mainWidget.waterEC_WaterPlaneOptionsFrame->setVisible(show && meshmoonWater == 0);
    
    // Height manipulator is always shown if mode if not 'None'
    mainWidget.waterHeightLabel->setVisible(mainWidget.waterAdvancedOptionsButton->isVisible());
    mainWidget.waterHeightSpinBox->setVisible(mainWidget.waterAdvancedOptionsButton->isVisible());

    if (show && meshmoonWater)
        SetButtonColor(mainWidget.EC_MeshmoonWaterColorButton, meshmoonWater->waterColor.Get());
}

void RocketBuildWidget::ShowFogAdvancedOptions(bool show)
{
    if (show && !mainWidget.fogButton->isChecked())
        show = false;
    
    mainWidget.fogAdvancedOptionsButton->blockSignals(true);
    mainWidget.fogAdvancedOptionsButton->setText(show ? tr("Hide Advanced Options") : tr("Advanced Options"));
    mainWidget.fogAdvancedOptionsButton->setChecked(show);
    mainWidget.fogAdvancedOptionsButton->blockSignals(false);
    
    mainWidget.fogAdvancedOptionsWidget->setVisible(show);
}

void RocketBuildWidget::SetCloudType(int type)
{
    if (Component<EC_SkyX>(sky))
        SetCloudType(static_cast<EC_SkyX::CloudType>(type));
    else if (Component<EC_MeshmoonSky>(sky))
        SetCloudType(static_cast<EC_MeshmoonSky::CloudCoverage>(type));
}

void RocketBuildWidget::SetCloudType(EC_SkyX::CloudType type)
{
    EC_SkyX *ec = Component<EC_SkyX>(sky);
    if (ec)
        owner->SetAttribute(ec->cloudType, static_cast<int>(type));
}

void RocketBuildWidget::SetCloudType(EC_MeshmoonSky::CloudCoverage type)
{
    EC_MeshmoonSky *ec = Component<EC_MeshmoonSky>(sky);
    if (ec)
        owner->SetAttribute(ec->cloudCoverage, static_cast<uint>(type));
}

EC_SkyX::CloudType RocketBuildWidget::CloudType() const
{
    EC_SkyX *ec = Component<EC_SkyX>(sky);
    return (ec ? static_cast<EC_SkyX::CloudType>(ec->cloudType.Get()) : EC_SkyX::None);
}

void RocketBuildWidget::SetPhysicsDebugEnabled(bool enabled)
{
    physicsDebugEnabled = enabled;
    PhysicsWorldPtr w = bulletWorld.lock();
    if (w)
        w->SetDebugGeometryEnabled(physicsDebugEnabled);

    for(std::list<weak_ptr<EC_RigidBody> >::iterator it = rigidBodies.begin(); it != rigidBodies.end(); ++it)
        if (!(*it).expired())
            (*it).lock()->drawDebug.Set(physicsDebugEnabled, AttributeChange::LocalOnly);

    if (mainWidget.physicsDebugButton)
        mainWidget.physicsDebugButton->setText(tr("Draw Debug: %1").arg(physicsDebugEnabled ? tr("Enabled") : tr("Disabled")));
    
    /// @todo clean up expired components?
}

void RocketBuildWidget::SetFogModeInternal(int index)
{
    SetFogMode(static_cast<EC_Fog::FogMode>(mainWidget.fogModeComboBox->itemData(index).toUInt()));
}

void RocketBuildWidget::RefreshPage(BuildMode mode)
{
    switch(mode)
    {
    case EnvironmentMode:
    default:
        RefreshEnvironmentPage();
        break;
//    case TerrainMode:
//        RefreshTerrainPage();
//        break;
    case TeleportsMode:
        RefreshTeleportsPage();
        break;
    case LightsMode:
        RefreshLightsPage();
        break;
    case PhysicsMode:
        RefreshPhysicsPage();
        break;
    case BlocksMode:
        owner->BlockPlacer()->Show();
        RefreshBlocksPage();
        break;
    case ScenePackager:
        RefreshScenePackagerPage();
        break;
    }
}

void RocketBuildWidget::OpenColorPicker(const Color &initialColor, const char *changedHandler, const char *selectedHandler,
                                        const char *canceledHandler, const QString &colorButtonObjectName)
{
    if (colorDialog)
        colorDialog->close();
    // Qt bug: if initial color is black the dialog doesn't emit currentColorChanged until some color
    // is selected from the ready palette of colors instead of the "free selection" of colors.
    originalColor = initialColor;
    if (initialColor == Color::Black)
        colorDialog = new QColorDialog(Fw()->Ui()->MainWindow());
    else
        colorDialog = new QColorDialog(initialColor, Fw()->Ui()->MainWindow());
    colorDialog->setProperty("colorButtonName", colorButtonObjectName);
    colorDialog->setOption(QColorDialog::ShowAlphaChannel);
    colorDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(colorDialog, SIGNAL(currentColorChanged(const QColor &)), changedHandler, Qt::UniqueConnection);
    connect(colorDialog, SIGNAL(colorSelected(const QColor &)), selectedHandler, Qt::UniqueConnection);
    connect(colorDialog, SIGNAL(rejected()), canceledHandler, Qt::UniqueConnection);

    colorDialog->show();
}

void RocketBuildWidget::SetFogColor(const QColor &color, bool undoable)
{
    SetColorInternal(qobject_cast<QColorDialog *>(sender()), (Component<EC_Fog>(fog) ? &Component<EC_Fog>(fog)->color : 0), color, undoable);
}

void RocketBuildWidget::ConnectToScene(const ScenePtr &scene) const
{
    connect(scene.get(), SIGNAL(EntityRemoved(Entity *, AttributeChange::Type)),
        this, SLOT(OnEntityRemoved(Entity *)), Qt::UniqueConnection);
    connect(scene.get(), SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentAdded(Entity *, IComponent *)), Qt::UniqueConnection);
    connect(scene.get(), SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentRemoved(Entity *, IComponent *)), Qt::UniqueConnection);
}

void RocketBuildWidget::DisconnectFromScene(const ScenePtr &scene)
{
    disconnect(scene.get(), SIGNAL(EntityRemoved(Entity *, AttributeChange::Type)),
        this, SLOT(OnEntityRemoved(Entity *)));
    disconnect(scene.get(), SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentAdded(Entity *, IComponent *)));
    disconnect(scene.get(), SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentRemoved(Entity *, IComponent *)));
}

/// @todo Take RTS camera into account?
void RocketBuildWidget::InitializeCameraModes()
{
    cameras.clear();

    ScenePtr scene = Scene();
    assert(scene);
    if (!scene)
        return;

    cameras.resize(NumCameraModes);
    for(size_t i = 0; i < NumCameraModes; ++i)
        cameras[i] = weak_ptr<EC_Camera>();

    shared_ptr<EC_Camera> freeLookCam;
    std::vector<shared_ptr<EC_Camera> > cams = scene->Components<EC_Camera>();
    for(size_t i = 0; i < cams.size(); ++i)
    {
        if (cams[i]->ParentEntity()->Name().contains("Avatar", Qt::CaseInsensitive))
            cameras[AvatarCameraMode] = cams[i];
        if (cams[i]->ParentEntity()->Name().contains("AdminoFree" /*"FreeLook"*/, Qt::CaseInsensitive))
            cameras[FreeLookCameraMode] = freeLookCam = cams[i];
    }
    // We prefer admino freelook app, but use the default if its not in scene.
    /// @todo Kind of hack, try to fix this by doing our own buildmode freecamera on startup.
    if (!freeLookCam)
    {
        for(size_t i = 0; i < cams.size(); ++i)
            if (cams[i]->ParentEntity()->Name().contains("FreeLook", Qt::CaseInsensitive))
                cameras[FreeLookCameraMode] = freeLookCam = cams[i];
    }

    if (freeLookCam)
    {
/*
        EntityPtr sceneCamEntity = freeLookCam->ParentEntity()->Clone(true, true, "SceneCamera");
        shared_ptr<EC_Camera> sceneCam = sceneCamEntity->Component<EC_Camera>();
        sceneCam->perspectiveProjection.Set(false, AttributeChange::Default);
        cameras[SceneCameraMode] = sceneCam;
*/
/*
        float scale = 0.5f; // Your scale here.
        float w = ogreWorld.lock()->Renderer()->WindowWidth();
        float h = ogreWorld.lock()->Renderer()->WindowHeight();
        float4x4 p = BuildScaledOrthoMatrix(w / scale / -2.0f, w  / scale /  2.0f, h / scale / -2.0f, h / scale /  2.0f, 0, 1000);
        sceneCam->OgreCamera()->setCustomProjectionMatrix(true, p);
*/
    }
}

void RocketBuildWidget::RefreshEnvironmentPage()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    // 1) Sky
    Entity::ComponentVector existing;
    if (sky.expired())
    {
        existing = scene->Components(EC_Sky::TypeIdStatic());
        skyMode = ES_None;
        if (!existing.empty())
        {
            skyMode = Simple;
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra Sky
                LogWarning("RocketBuildEditor: More than one Sky component found in the scene.");
            }
            SetSkyComponent(existing[0]);
        }
        existing = scene->Components(EC_SkyX::TypeIdStatic());
        if (!existing.empty())
        {
            skyMode = Complex;
            if (Component<EC_Sky>(sky))
            {
                /// @todo Confirmation dialog for deleting WaterPlane
                LogWarning("RocketBuildEditor: Sky will be replaced with SkyX.");
            }
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra SkyX
                LogWarning("RocketBuildEditor: More than one SkyX component found in the scene.");
            }
            SetSkyComponent(existing[0]);
        }
        existing = scene->Components(EC_MeshmoonSky::TypeIdStatic());
        if (!existing.empty())
        {
            skyMode = ComplexMeshmoon;
            if (Component<EC_Sky>(sky))
            {
                /// @todo Confirmation dialog for deleting WaterPlane
                LogWarning("RocketBuildEditor: Sky will be replaced with MeshmoonSky.");
            }
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra SkyX
                LogWarning("RocketBuildEditor: More than one MeshmoonSky component found in the scene.");
            }
            SetSkyComponent(existing[0]);
        }

        SetSkyMode(skyMode);
    }
    // 2) Water
    if (water.expired())
    {
        waterMode = ES_None;
        existing = scene->Components(EC_WaterPlane::TypeIdStatic());
        if (!existing.empty())
        {
            waterMode = Simple;
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra WaterPlane
                LogWarning("RocketBuildEditor: More than one WaterPlane component found in the scene.");
            }
            SetWaterComponent(existing[0]);
        }
        /*existing = scene->Components(EC_Hydrax::TypeIdStatic());
        if (!existing.empty())
        {
            waterMode = Complex;
            if (Component<EC_WaterPlane>(water))
            {
                /// @todo Confirmation dialog for deleting WaterPlane
                LogWarning("RocketBuildEditor: WaterPlane will be replaced with Hydrax.");
            }
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra Hydrax
                LogWarning("RocketBuildEditor: More than one Hydrax component found in the scene.");
            }
            SetWaterComponent(existing[0]);
        }*/
        existing = scene->Components(EC_MeshmoonWater::TypeIdStatic());
        if (!existing.empty())
        {
            waterMode = Complex;
            if (Component<EC_MeshmoonWater>(water))
            {
                /// @todo Confirmation dialog for deleting WaterPlane
                LogWarning("RocketBuildEditor: WaterPlane and Hydrax will be replaced with MeshmoonWater.");
            }
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra Hydrax
                LogWarning("RocketBuildEditor: More than one MeshmoonWater component found in the scene.");
            }
            SetWaterComponent(existing[0]);
        }
        SetWaterMode(waterMode);
    }
    // 3) Fog
    if (fog.expired())
    {
        fogEnabled = false;
        existing = scene->Components(EC_Fog::TypeIdStatic());
        if (!existing.empty())
        {
            fogEnabled = true;
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra Fog
                LogWarning("RocketBuildEditor: More than one Fog component found in the scene.");
            }
            SetFogComponent(existing[0]);
        }
        SetFogEnabled(fogEnabled);
    }
    // 4) Shadow setup
    if (shadowSetup.expired())
    {
        ComponentPtr setupComponent = owner->EnvironmentEntity()->GetOrCreateComponent(EC_SceneShadowSetup::TypeIdStatic(), "RocketShadowSetup");
        if (setupComponent.get())
            SetShadowSetupComponent(setupComponent);
    }
    else
        RefreshShadowSetupSettings();
    // 5) Post-Processing, take only account the ones that are part of the Rocket environment entity.
    {
        compositors.clear();
        existing = owner->EnvironmentEntity()->ComponentsOfType(EC_OgreCompositor::TypeIdStatic());
        for(size_t i = 0; i < existing.size(); ++i)
            SetCompositorComponent(existing[i]->Name(), existing[i]);
    }

    ConnectToScene(scene);
}

void RocketBuildWidget::RefreshTerrainPage()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    if (terrain.expired())
    {
        Entity::ComponentVector existing = scene->Components(EC_Terrain::TypeIdStatic());
        if (!existing.empty())
        {
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra Terrain
                LogWarning("RocketBuildEditor: More than one Terrain component found in the scene.");
            }
            SetTerrainComponent(existing[0]);
        }
    }
}

void RocketBuildWidget::RefreshTeleportsPage()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);
    
    ClearTeleportItems();
    Entity::ComponentVector existing = scene->Components(EC_MeshmoonTeleport::TypeIdStatic());
    for(size_t i = 0; i < existing.size(); ++i)
        AddTeleport(existing[i]);

    RefreshActiveTeleportSettings();

    ConnectToScene(scene);
}

void RocketBuildWidget::RefreshLightsPage()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    // 1) Environment light
    Entity::ComponentVector existing;
    if (environmentLight.expired())
    {
        existing = scene->Components(EC_EnvironmentLight::TypeIdStatic());
        if (!existing.empty())
        {
            if (existing.size() > 1)
            {
                /// @todo Confirmation dialog for deleting extra EnvironmentLight
                LogWarning("RocketBuildEditor: More than one EnvironmentLight components found in the scene.");
            }
            SetEnvironmentLightComponent(existing[0]);
        }

        existing = scene->Components(EC_SkyX::TypeIdStatic());
        if (!existing.empty())
        {
            if (Component<EC_EnvironmentLight>(environmentLight))
            {
                /// @todo Confirmation dialog for deleting EnvironmentLight
                LogWarning("RocketBuildEditor: EnvironmentLight will be replaced with SkyX.");
            }
            SetEnvironmentLightComponent(existing[0]);
        }
    }

    RefreshEnvironmentLightSettings();

    // 2) Other lights
    ClearLightItems();
    existing = scene->Components(EC_Light::TypeIdStatic());
    for(size_t i = 0; i < existing.size(); ++i)
        AddLight(existing[i]);

    RefreshActiveLightSettings();

    ConnectToScene(scene);
}

void RocketBuildWidget::RefreshBlocksPage()
{
    // Placer mode
    mainWidget.blockAddButton->blockSignals(true);
    mainWidget.blockCloneButton->blockSignals(true);
    mainWidget.blockEditButton->blockSignals(true);
    mainWidget.blockRemoveButton->blockSignals(true);

    const RocketBlockPlacer::PlacerMode placerMode = owner->BlockPlacer()->BlockPlacerMode();
    mainWidget.blockAddButton->setChecked(placerMode == RocketBlockPlacer::CreateBlock);
    mainWidget.blockCloneButton->setChecked(placerMode == RocketBlockPlacer::CloneBlock);
    mainWidget.blockEditButton->setChecked(placerMode == RocketBlockPlacer::EditBlock);
    mainWidget.blockRemoveButton->setChecked(placerMode == RocketBlockPlacer::RemoveBlock);

    mainWidget.blockAddButton->blockSignals(false);
    mainWidget.blockRemoveButton->blockSignals(false);
    mainWidget.blockEditButton->blockSignals(false);
    mainWidget.blockCloneButton->blockSignals(false);

    // Placement mode
    const RocketBlockPlacer::PlacementMode placementMode = owner->BlockPlacer()->BlockPlacementMode();
    mainWidget.blockSnapPlacementButton->blockSignals(true);
    mainWidget.blockFreePlacementButton->blockSignals(true);
    mainWidget.blockSnapPlacementButton->setChecked(placementMode == RocketBlockPlacer::SnapPlacement);
    mainWidget.blockFreePlacementButton->setChecked(placementMode == RocketBlockPlacer::FreePlacement);
    mainWidget.blockSnapPlacementButton->blockSignals(false);
    mainWidget.blockFreePlacementButton->blockSignals(false);

    // Enable and disable functionality depending on the used mode.
    const bool creating = (placerMode == RocketBlockPlacer::CreateBlock || placerMode == RocketBlockPlacer::CloneBlock);
    const bool creatingOrEditing = (creating || (placerMode == RocketBlockPlacer::EditBlock && !activeEntity.expired()));
    mainWidget.blockSnapPlacementButton->setEnabled(creating);
    mainWidget.blockFreePlacementButton->setEnabled(creating);
    mainWidget.blocksPhysicsCheckBox->setVisible(creating);

    // Visuals
    if (placerMode == RocketBlockPlacer::CreateBlock && owner->BlockPlacer()->Placeable())
    {
        connect(owner->BlockPlacer()->Placeable().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            SLOT(RefreshPlaceableOptions()), Qt::UniqueConnection);
    }
    else if (owner->BlockPlacer()->Placeable())
    {
        disconnect(owner->BlockPlacer()->Placeable().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            this, SLOT(RefreshPlaceableOptions()));
    }
    RefreshBlocksVisualsSettings();

    QString modeText;
    switch(placerMode)
    {
    case RocketBlockPlacer::CreateBlock: modeText = tr("Create "); break;
    case RocketBlockPlacer::CloneBlock: modeText = tr("Clone "); break;
    case RocketBlockPlacer::EditBlock: modeText = tr("Edit "); break;
    default:
        break;
    }

    QLayoutItem *item;
    while((item = mainWidget.componentGridLayout->takeAt(0)) != 0)
    {
        QWidget *w = item->widget();
        QAbstractButton *b = qobject_cast<QAbstractButton*>(w);
        if (b) componentButtonGroup->removeButton(b);
        SAFE_DELETE(item);
        SAFE_DELETE(w);
    }

    // In remove mode don't show any extra UI
    if (placerMode == RocketBlockPlacer::RemoveBlock)
    {
        mainWidget.blocksPlaceableOptionsWidget->setVisible(false);
        mainWidget.blocksVisualsCheckBox->setVisible(false);
        mainWidget.blocksPhysicsCheckBox->setVisible(false);
        mainWidget.blocksFunctionalityCheckBox->setVisible(false);
        mainWidget.blocksVisualsOptionsWidget->setVisible(false);
        mainWidget.blocksFunctionalityOptionsWidget->setVisible(false);
    }
    else
    {
        mainWidget.blocksPlaceableOptionsWidget->setVisible(placerMode == RocketBlockPlacer::CreateBlock ||
            (placerMode == RocketBlockPlacer::EditBlock && !activeEntity.expired()));

        // Check if we are editing a block asset
        const bool editingLibraryAsset = (placerMode == RocketBlockPlacer::EditBlock && !activeEntity.expired() &&
            activeEntity.lock()->Description().contains(cBuildingBlockTag, Qt::CaseInsensitive));

        MeshmoonAsset *blockMeshAsset = 0;
        QString meshRef = (!activeEntity.expired() && activeEntity.lock()->Component<EC_Mesh>().get() ? activeEntity.lock()->Component<EC_Mesh>()->meshRef.Get().ref : "");
        if (editingLibraryAsset)
            blockMeshAsset = rocket->AssetLibrary()->FindAsset(meshRef);
        else if (placerMode == RocketBlockPlacer::EditBlock && !meshRef.trimmed().isEmpty())
            blockMeshAsset = owner->BlockPlacer()->GetOrCreateCachedStorageAsset(MeshmoonAsset::Mesh, framework_->Asset()->ResolveAssetRef("", meshRef));
        else
            blockMeshAsset = owner->BlockPlacer()->MeshAsset();

        const bool showVisuals = blockMeshAsset && (placerMode == RocketBlockPlacer::CreateBlock || placerMode == RocketBlockPlacer::EditBlock);
        mainWidget.blocksVisualsCheckBox->setVisible(showVisuals);

        mainWidget.blocksPhysicsCheckBox->setVisible(placerMode != RocketBlockPlacer::EditBlock);
        mainWidget.blocksFunctionalityCheckBox->setVisible(true);

        mainWidget.blocksVisualsOptionsWidget->setVisible(showVisuals);

        mainWidget.blocksFunctionalityOptionsWidget->setVisible(placerMode != RocketBlockPlacer::CloneBlock && mainWidget.blocksFunctionalityCheckBox->isChecked());

        mainWidget.blocksVisualsCheckBox->setText(modeText + tr("Visuals"));
        mainWidget.blocksPhysicsCheckBox->setText(modeText + tr("Physics"));
        mainWidget.blocksFunctionalityCheckBox->setText(modeText + tr("Functionality"));
           
        mainWidget.blocksFunctionalityCheckBox->setEnabled(creating);
        mainWidget.blocksFunctionalityOptionsWidget->setEnabled(creatingOrEditing);

        // In edit mode the entity has changed so reset our knowledge of components.
        entitysExistingComponents.clear();

        // List of component types we want to show.
        QStringList componentTypes;
        componentTypes << EC_WebBrowser::TypeNameStatic() << EC_MediaBrowser::TypeNameStatic();

        // Only show the ones we want users to be able to edit via build tools.
        int row = 0, col = 0;
        const int itemsPerRow = 2;
        foreach(const QString &componentType, componentTypes)
        {
            // We can't setup the buttons in edit mode as the clone source changes via mouse hover in scene.
            if (placerMode == RocketBlockPlacer::CloneBlock)
                continue;

            // Check if active edited entity has this component
            if (placerMode == RocketBlockPlacer::EditBlock)
            {
                if (activeEntity.expired())
                    continue;
                    
                ComponentPtr existing = activeEntity.lock()->Component(componentType);
                if (existing.get())
                    entitysExistingComponents[existing->TypeId()] = existing;
            }

            QPushButton *button = new QPushButton(UserFriendlyComponentName(componentType), widget_);
            button->setProperty("tundraComponentTypeName", componentType);
            button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
            button->setCheckable(true);

            const u32 componentTypeId = Fw()->Scene()->ComponentTypeIdForTypeName(componentType);
            if (placerMode == RocketBlockPlacer::CreateBlock)
                button->setChecked(userSelectedComponents.find(componentTypeId) != userSelectedComponents.end());
            else if (placerMode == RocketBlockPlacer::EditBlock)
                button->setChecked(entitysExistingComponents.find(componentTypeId) != entitysExistingComponents.end());

            componentButtonGroup->addButton(button, componentTypeId);
            mainWidget.componentGridLayout->addWidget(button, row, col);
            if (++col >= itemsPerRow)
            {
                ++row;
                col = 0;
            }
        }

        if (componentButtonGroup->buttons().isEmpty())
        {
            // Leave the toggle in clone mode but don't show in 
            if (placerMode != RocketBlockPlacer::CloneBlock)
                mainWidget.blocksFunctionalityCheckBox->setVisible(false);
            mainWidget.blocksFunctionalityOptionsWidget->setVisible(false);
        }
    }

    mainWidget.blockMaxDistanceSpinBox->blockSignals(true);
    mainWidget.blockMaxDistanceSpinBox->setValue(owner->BlockPlacer()->MaxBlockDistance());
    mainWidget.blockMaxDistanceSpinBox->blockSignals(false);
}

QString RocketBuildWidget::UserFriendlyComponentName(const QString &componentType)
{
    /// @todo If we have more special cases, create a "EC_Something"-"Something Nicer" map.
    if (componentType == EC_MediaBrowser::TypeNameStatic())
        return "Media Player"; // EC_MediaBrowser is a special case
    else
        return IComponent::EnsureTypeNameWithoutPrefix(componentType).split(QRegExp("(?=[A-Z])"), QString::SkipEmptyParts).join(" ");
}

double RocketBuildWidget::CompositorParameterDefaultValue(const char *compositorName, const char *parameterName)
{
    if (strcmp(compositorName, cBloomName) == 0)
    {
        if (strcmp(parameterName, "BlurWeight") == 0) return 0.1;
        else if (strcmp(parameterName, "OriginalImageWeight") == 0) return 1.0;
    }
    else if (strcmp(compositorName, cHdrName) == 0)
    {
        if (strcmp(parameterName, "BloomWeight") == 0) return 1.0;
        else if (strcmp(parameterName, "LuminanceMin") == 0) return 0.0;
        else if (strcmp(parameterName, "LuminanceMax") == 0) return 1.0;
    }

    return 0.0;
}

double RocketBuildWidget::CompositorParameterValue(const char *compositorName, const char *parameterName) const
{
    EC_OgreCompositor *ec = Component<EC_OgreCompositor>(CompositorComponent(compositorName));
    if (ec)
    {
        QVariantList params = ec->parameters.Get();
        for(int i = 0; i < params.size(); ++i)
        {
            QStringList kvp = params[i].toString().split("=");
            if (kvp.size() == 2 && kvp[0].contains(parameterName))
                return kvp[1].toDouble();
        }
    }

    return CompositorParameterDefaultValue(compositorName, parameterName);
}

void RocketBuildWidget::SetCompositorParameterValue(const char *compositorName, const char *parameterName, double value)
{
    EC_OgreCompositor *ec = Component<EC_OgreCompositor>(CompositorComponent(compositorName));
    if (ec)
    {
        QVariantList params = ec->parameters.Get();
        // Remove possible existing value.
        for(int i = 0; i < params.size(); ++i)
        {
            QStringList kvp = params[i].toString().split("=");
            if (kvp.size() == 2 && kvp[0].contains(parameterName))
            {
                params.removeAt(i);
                break;
            }
        }

        // Append new value to the list.
        params.append(QStringList(QStringList() << parameterName << QString::number(value)).join("="));

        owner->SetAttribute(ec->parameters, params);
    }
}

QString RocketBuildWidget::AssetFilename(QString assetRef)
{
    if (assetRef.startsWith("Ogre Media:", Qt::CaseInsensitive))
        assetRef = assetRef.mid(11);
    if (assetRef.contains("/"))
        assetRef = assetRef.split("/", QString::SkipEmptyParts).last();
    return assetRef;
}

void RocketBuildWidget::RefreshScenePackagerPage()
{
    if (!scenePackager)
        scenePackager = new RocketScenePackager(rocket);

    if (!mainWidget.sceneOptimizerPage->layout())
    {
        QHBoxLayout *layout = new QHBoxLayout(mainWidget.sceneOptimizerPage);
        layout->addWidget(scenePackager->Widget());
        mainWidget.sceneOptimizerPage->setLayout(layout);
    }
    if (scenePackager)
        scenePackager->Show();
}

void RocketBuildWidget::RefreshBlocksVisualsSettings()
{
    // Get the current block mesh asset
    EntityPtr editedEntity = activeEntity.lock();
    if (editedEntity && !editedEntity->Component<EC_Mesh>().get())
    {
        //LogWarning("[RocketBuildEditor]: Mesh null in active entity. Cannot update the visuals view");
        return;
    }

    // Edit mode with a entity that contains our 'building block' metadata?
    const bool editingLibraryAsset = (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock && editedEntity &&
        editedEntity->Description().contains(cBuildingBlockTag, Qt::CaseInsensitive));

    /** Get the mesh asset
        1) [EDIT] From library by mesh reference (editing existing 'building block' entity)
        3) [EDIT] If editing and not found from library, get or create item via block placer (this works for ANY mesh, if its in storage or not)
        2) [CREATING] If creating and not found from library, get active mesh asset from block placer (loads from config once edit mode is entered) */
    MeshmoonAsset *blockMeshAsset = 0;
    QString meshRef = (!activeEntity.expired() && activeEntity.lock()->Component<EC_Mesh>().get() ? activeEntity.lock()->Component<EC_Mesh>()->meshRef.Get().ref : "");
    if (editingLibraryAsset)
        blockMeshAsset = rocket->AssetLibrary()->FindAsset(meshRef);
    else if (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock && !meshRef.trimmed().isEmpty())
        blockMeshAsset = owner->BlockPlacer()->GetOrCreateCachedStorageAsset(MeshmoonAsset::Mesh, framework_->Asset()->ResolveAssetRef("", meshRef));
    else
        blockMeshAsset = owner->BlockPlacer()->MeshAsset();

    // Get the current button and check if we need to recreate it.
    QLayoutItem *currentButtonItem = mainWidget.blockMeshLayout->itemAt(0);
    QPushButton *currentButton = (currentButtonItem ? qobject_cast<QPushButton*>(currentButtonItem->widget()) : 0);
    MeshmoonAsset *currentButtonMeshAsset = (currentButton ? qvariant_cast<MeshmoonAsset*>(currentButton->property("asset")) : 0);
    if ((!currentButtonMeshAsset || !blockMeshAsset) || currentButtonMeshAsset != blockMeshAsset)
    {
        // Clear layout
        QLayoutItem *item;
        while((item = mainWidget.blockMeshLayout->takeAt(0)) != 0)
        {
            QWidget *w = item->widget();
            SAFE_DELETE(item);
            SAFE_DELETE(w);
        }

        // Create button
        QPushButton *meshSelector = rocket->AssetLibrary()->CreateAssetWidget(blockMeshAsset, QSize(155, 120), true, true, blockMeshAsset != 0 ? QFileInfo(blockMeshAsset->assetRef).fileName() : "");
        meshSelector->setChecked(false);
        meshSelector->setCheckable(false);
        connect(meshSelector, SIGNAL(clicked()), this, SLOT(ShowBlockMeshSelection()), Qt::UniqueConnection);

        QPushButton *storageButton = new QPushButton(meshSelector);
        storageButton->setObjectName("storagePickButton");
        storageButton->setToolTip("Pick mesh from storage...");
        storageButton->setFixedSize(28,28);
        storageButton->setGeometry(meshSelector->width()-28-5, 5, 28, 28);
        storageButton->setIconSize(QSize(24,24));
        storageButton->setIcon(QPixmap(":/images/icon-download-64x64.png").scaled(24,24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        storageButton->setStyleSheet(QString("QPushButton { border: 0px; border-radius: 3px; background-color: rgba(200,200,200,200); color: black; max-width: 28px; min-width: 28px; max-height: 28px; min-height: 28px; }") + 
                                             "QPushButton:hover { border: 1px solid rgb(243, 154, 41); background-color: rgba(255,255,255,200); }");
        connect(storageButton, SIGNAL(clicked()), SLOT(ShowStorageMeshSelection()), Qt::UniqueConnection);

        QLabel *blocksTitleMeshSelection = new QLabel("Shape");
        blocksTitleMeshSelection->setIndent(0);
        blocksTitleMeshSelection->setObjectName(QString::fromUtf8("blocksTitleMeshSelection"));
        blocksTitleMeshSelection->setStyleSheet("QLabel#blocksTitleMeshSelection { color: rgba(100,10,10,40); font-size: 32px; padding: 0px; }");
        blocksTitleMeshSelection->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        blocksTitleMeshSelection->setAlignment(Qt::AlignLeft|Qt::AlignTop);

        mainWidget.blockMeshLayout->addWidget(meshSelector);
        mainWidget.blockMeshLayout->addWidget(blocksTitleMeshSelection);
        mainWidget.blockMeshLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Expanding, QSizePolicy::Fixed));
    }

    /** Detect submesh count so we can create the correct amount of material slots.
        1) [EDIT] If we have a valid entity ptr, get the material count there.
        2) [CREATE] Try getting the 'submeshes' metadata from the mesh asset. 
           This will fail for storage items, in this casebelow we will download the mesh. */
    int numSubmeshes = 0;
    if (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock && editedEntity)
        numSubmeshes = editedEntity->Component<EC_Mesh>()->NumMaterials();
    if (numSubmeshes <= 0 && blockMeshAsset)
    {
        bool ok = false;
        numSubmeshes = blockMeshAsset->metadata.value("submeshes", 0).toInt(&ok);
        if (!ok) numSubmeshes = 0;
        
        // If creating from a storage or any other mesh ref for that matter, try getting the asset from the asset system.
        if (numSubmeshes <= 0 && owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::CreateBlock)
        {
            AssetPtr meshAsset = blockMeshAsset->Asset(framework_->Asset());

            // Already in the system
            OgreMeshAsset *ogreMeshAsset = meshAsset.get() ? qobject_cast<OgreMeshAsset*>(meshAsset.get()) : 0;
            if (ogreMeshAsset)
            {
                if (ogreMeshAsset->IsLoaded())
                    numSubmeshes = static_cast<int>(ogreMeshAsset->NumSubmeshes());
                else
                    connect(ogreMeshAsset, SIGNAL(Loaded(AssetPtr)), SLOT(RefreshBlocksVisualsSettings()), Qt::UniqueConnection);
            }
            // Not in the system, request now. Note that we are 'competing' here with block placers mesh request, but thats ok.
            // We want the material selectors be available for selection as soon as the mesh loads and we know the submesh count.
            else
            {
                AssetTransferPtr meshTransfer = framework_->Asset()->RequestAsset(blockMeshAsset->assetRef);
                connect(meshTransfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(RefreshBlocksVisualsSettings()), Qt::UniqueConnection);
            }
        }
    }
    
    int index = 0, row = 0, col = 0;
    while(index < numSubmeshes)
    {
        // Allocate new slot if necessary
        if (index >= materialSelectors.size())
            materialSelectors.push_back(MaterialSelector());

        // Submesh index
        MaterialSelector &selector = materialSelectors[index];
        selector.index = index;

        // Material asset. Try to get a valid asset ptr, null is also fine which creates a empty clickable selection button.
        if (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock && editedEntity)
        {
            const AssetReferenceList &mats = editedEntity->Component<EC_Mesh>()->materialRefs.Get();
            const QString materialRef = (index < mats.Size() ? mats[index].ref : "");
            selector.asset = rocket->AssetLibrary()->FindAsset(materialRef);
            if (!selector.asset && !materialRef.isEmpty())
                selector.asset = owner->BlockPlacer()->GetOrCreateCachedStorageAsset(MeshmoonAsset::Material, framework_->Asset()->ResolveAssetRef("", materialRef));
        }
        else
            selector.asset = (index < owner->BlockPlacer()->Materials().size() ? owner->BlockPlacer()->Materials()[index].libraryAsset : 0);

        // Create button and advance indexes
        CreateMaterialButton(selector, row, col);
        if (++col >= QFormLayout::SpanningRole)
        {
            col = 0;
            ++row;
        }
        ++index;
    }

    /** Show and enable only material selectors that are needed.
        If the count is unknown (0), we want to keep the buttons visible (but disabled) until the mesh has 
        been loaded and the button count can be deduced. Otherwise the buttons will all hide and some 
        pop back up in a short period of time. This flickering will look quite bad for the user. */
    bool titleIndicatorFound = false;
    for(int i = 0; i < materialSelectors.size(); ++i)
    {
        QPushButton *materialButton = materialSelectors[i].button;
        if (materialButton)
        {
            if (!blockMeshAsset || numSubmeshes > 0)
            {
                materialButton->setVisible(i < numSubmeshes);

                // Adjust the big materials title padding so it stays in one place if the second material is hidden.
                if (materialSelectors[i].index == 1)
                {
                    titleIndicatorFound = true;
                    mainWidget.blocksTitleMaterialSelection->setStyleSheet(QString("padding: 0px; padding-left: %1px;").arg((i < numSubmeshes ? 0 : 75)));
                }
            }
            materialButton->setEnabled(numSubmeshes > 0);
        }
    }
    // If the material index 1 button was not found, set the correct padding.
    if (!titleIndicatorFound)
        mainWidget.blocksTitleMaterialSelection->setStyleSheet("padding: 0px; padding-left: 75px;");
}

void RocketBuildWidget::OnStorageSelectionCanceled()
{
    // Store the last storage location into memory for the next dialog open
    RocketStorageSelectionDialog *dialog = qobject_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageLocation_ = dialog->CurrentDirectory();
}

void RocketBuildWidget::ShowStorageMeshSelection()
{
    RocketStorageSelectionDialog *dialog = rocket->Storage()->ShowSelectionDialog(QStringList() << "mesh", true, lastStorageLocation_);
    if (dialog)
    {
        connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), SLOT(SetBlockMesh(const MeshmoonStorageItem&)), Qt::UniqueConnection);
        connect(dialog, SIGNAL(Canceled()), SLOT(OnStorageSelectionCanceled()), Qt::UniqueConnection);
    }
}

void RocketBuildWidget::ShowBlockMeshSelection()
{
    MeshmoonAssetSelectionDialog *dialog = rocket->AssetLibrary()->ShowSelectionDialog(MeshmoonAsset::Mesh, MeshmoonLibrarySource(), cBuildingBlockTag);
    if (dialog)
        connect(dialog, SIGNAL(AssetSelected(MeshmoonAsset*)), this, SLOT(SetBlockMesh(MeshmoonAsset*)), Qt::UniqueConnection);
}

void RocketBuildWidget::ShowStorageMaterialSelection()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button)
        return;

    int submeshIndex = -1;
    for(int i = 0; i < materialSelectors.size(); ++i)
    {
        // Reset selecting state for all. ShowBlockMaterialSelection will set selecting true for the right material.
        materialSelectors[i].selecting = false;
        if (materialSelectors[i].button && materialSelectors[i].button->findChild<QPushButton*>("storagePickButton") == button)
            submeshIndex = i;
    }
    if (submeshIndex >= 0 && submeshIndex < materialSelectors.size())
    {
        materialSelectors[submeshIndex].selecting = true;
        RocketStorageSelectionDialog *dialog = rocket->Storage()->ShowSelectionDialog(QStringList() << "material", true, lastStorageLocation_);
        if (dialog)
        {
            connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), SLOT(SetBlockMaterial(const MeshmoonStorageItem&)), Qt::UniqueConnection);
            connect(dialog, SIGNAL(Canceled()), SLOT(OnStorageSelectionCanceled()), Qt::UniqueConnection);
        }
    }
}

void RocketBuildWidget::ShowStorageMaterialEditor()
{
    QString materialRef = (sender() != 0 ? sender()->property("materialAssetRef").toString() : "");
    if (materialRef.isEmpty())
        return;

    // Auth now so we can open the editor.
    if (!rocket->Storage()->IsAuthenticated())
    {
        MeshmoonStorageAuthenticationMonitor *auth = rocket->Storage()->Authenticate();
        auth->setProperty("materialAssetRef", materialRef);
        connect(auth, SIGNAL(Completed()), SLOT(ShowStorageMaterialEditor()), Qt::UniqueConnection);
        return;
    }

    rocket->Storage()->OpenVisualEditor(rocket->Storage()->ItemForUrl(materialRef));
}

void RocketBuildWidget::ShowBlockMaterialSelection()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button)
        return;

    int submeshIndex = -1;
    for(int i = 0; i < materialSelectors.size(); ++i)
    {
        // Reset selecting state for all. ShowBlockMaterialSelection will set selecting true for the right material.
        materialSelectors[i].selecting = false;
        if (materialSelectors[i].button == button)
            submeshIndex = i;
    }
    ShowBlockMaterialSelection(submeshIndex);
}

void RocketBuildWidget::ShowBlockMaterialSelection(int submeshIndex)
{
    if (submeshIndex >= 0 && submeshIndex < materialSelectors.size())
    {
        materialSelectors[submeshIndex].selecting = true;
        MeshmoonAssetSelectionDialog *dialog = rocket->AssetLibrary()->ShowSelectionDialog(MeshmoonAsset::Material);
        if (dialog)
            connect(dialog, SIGNAL(AssetSelected(MeshmoonAsset*)), this, SLOT(SetBlockMaterial(MeshmoonAsset*)), Qt::UniqueConnection);
        else
            materialSelectors[submeshIndex].selecting = false;
    }
}

void RocketBuildWidget::SetBlockMesh(const MeshmoonStorageItem &item)
{
    if (item.IsNull())
        return;

    RocketStorageSelectionDialog *dialog = qobject_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageLocation_ = dialog->CurrentDirectory();

    SetBlockMesh(owner->BlockPlacer()->GetOrCreateCachedStorageAsset(MeshmoonAsset::Mesh, item.fullAssetRef, item.filename));
}

void RocketBuildWidget::SetBlockMesh(MeshmoonAsset *asset)
{
    owner->BlockPlacer()->SetMeshAsset(asset);
}

void RocketBuildWidget::SetBlockMaterial(const MeshmoonStorageItem &item)
{
    if (item.IsNull())
        return;

    RocketStorageSelectionDialog *dialog = qobject_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageLocation_ = dialog->CurrentDirectory();

    for(int i = 0; i < materialSelectors.size(); ++i)
    {
        if (materialSelectors[i].selecting)
            SetBlockMaterial(materialSelectors[i].index, owner->BlockPlacer()->GetOrCreateCachedStorageAsset(MeshmoonAsset::Material, item.fullAssetRef, item.filename));
    }
}

void RocketBuildWidget::SetBlockMaterial(MeshmoonAsset *asset)
{
    for(int i = 0; i < materialSelectors.size(); ++i)
    {
        if (materialSelectors[i].selecting)
        {
            SetBlockMaterial(materialSelectors[i].index, asset);
            break;
        }
    }
}

void RocketBuildWidget::SetBlockMaterial(uint materialIndex, MeshmoonAsset *asset)
{
    if (materialIndex < (uint)materialSelectors.size())
    {
        materialSelectors[materialIndex].selecting = false;
        owner->BlockPlacer()->SetMaterialAsset(materialIndex, asset);
    }
}

void RocketBuildWidget::CreateMaterialButton(MaterialSelector &material, int row, int col)
{
    // Check if this button already has the correct asset in it, in that case we can skip recreation.
    MeshmoonAsset *currentButtonMaterialAsset = (material.button ? qvariant_cast<MeshmoonAsset*>(material.button->property("asset")) : 0);
    if (currentButtonMaterialAsset != 0 && material.asset != 0 && currentButtonMaterialAsset == material.asset)
    {
        QPushButton *editorButton = material.button->findChild<QPushButton*>("materialEditorButton");
        if (editorButton && material.asset && material.asset->IsStorageRef(rocket->Storage()) && owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock)
            editorButton->show();
        else if (editorButton)
            editorButton->hide();
        return;
    }

    // Clean up possible existing button from the layout
    bool existingFound = false;
    for(int i = 0; i < mainWidget.blockMaterialLayout->rowCount(); ++i)
    {
        for(int j = 0; j < QFormLayout::SpanningRole; ++j)
        {
            QLayoutItem *item = mainWidget.blockMaterialLayout->itemAt(i, (QFormLayout::ItemRole)j);
            if (i == row && j == col)
            {
                mainWidget.blockMaterialLayout->removeItem(item);
                SAFE_DELETE(item);
                SAFE_DELETE(material.button);   
                existingFound = true;
            }
            if (existingFound) break;
        }
        if (existingFound) break;
    }

    if (row < 0 || col < 0)
        return;

    // Tooltip and material display name
    QString tooltip = material.asset ? QFileInfo(material.asset->assetRef).fileName() : "";
    QString displayName = material.asset ? QString::number(material.index+1) : tr("Pick..");
    if (material.asset && material.asset->previewImageUrl.isEmpty() && !material.asset->metadata.contains("rgba"))
    {
        displayName = QFileInfo(material.asset->assetRef).baseName().replace("-", " ").replace("_", " ");
        displayName = displayName.left(1).toUpper() + displayName.mid(1);
        displayName = QString::number(material.index+1) + " " + displayName;
    }

    // Create selection button
    material.button = rocket->AssetLibrary()->CreateAssetWidget(material.asset, QSize(75, 75), true, false, tooltip, displayName, "", Qt::white, 20);
    material.button->setChecked(false);
    material.button->setCheckable(false);
    connect(material.button, SIGNAL(clicked()), this, SLOT(ShowBlockMaterialSelection()), Qt::UniqueConnection);

    // For materials make the text a bit smaller
    QLabel *nameLabel = material.button->findChild<QLabel*>("nameLabel");
    if (nameLabel) nameLabel->setStyleSheet("color: rgb(70,70,70); font-size: 11px;");

    // Storage pick button
    QPushButton *storageButton = new QPushButton(material.button);
    storageButton->setObjectName("storagePickButton");
    storageButton->setToolTip("Pick material from storage...");
    storageButton->setFixedSize(24,24);
    storageButton->setGeometry(material.button->width()-24-4, 4, 24, 24);
    storageButton->setIconSize(QSize(18,18));
    storageButton->setIcon(QPixmap(":/images/icon-download-64x64.png").scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    storageButton->setStyleSheet(QString("QPushButton { border: 0px; border-radius: 3px; background-color: rgba(200,200,200,200); color: black; max-width: 24px; min-width: 24px; max-height: 24px; min-height: 24px; }") + 
                                         "QPushButton:hover { border: 1px solid rgb(243, 154, 41); background-color: rgba(255,255,255,200); }");
    connect(storageButton, SIGNAL(clicked()), SLOT(ShowStorageMaterialSelection()), Qt::UniqueConnection);

    // If this is a storage ref, we want to show a open visual editor button so the user does not have to open storage to access it
    QPushButton *editorButton = material.button->findChild<QPushButton*>("materialEditorButton");
    if (material.asset && material.asset->IsStorageRef(rocket->Storage()) && owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock)
    {
        if (!editorButton)
        {
            editorButton = new QPushButton(material.button);
            editorButton->setObjectName("materialEditorButton");
            editorButton->setToolTip("Open material editor...");
            editorButton->setFixedSize(24,24);
            editorButton->setGeometry(material.button->width()-(24+4)*2, 4, 24, 24);
            editorButton->setIconSize(QSize(18,18));
            editorButton->setIcon(QPixmap(":/images/icon-visual-editor-32x32.png").scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            editorButton->setStyleSheet(QString("QPushButton { border: 0px; border-radius: 3px; background-color: rgba(200,200,200,200); color: black; max-width: 24px; min-width: 24px; max-height: 24px; min-height: 24px; }") + 
                "QPushButton:hover { border: 1px solid rgb(243, 154, 41); background-color: rgba(255,255,255,200); }");
            connect(editorButton, SIGNAL(clicked()), SLOT(ShowStorageMaterialEditor()), Qt::UniqueConnection);
        }
        editorButton->setProperty("materialAssetRef", material.asset->assetRef);
        editorButton->show();
    }
    else if (editorButton)
        editorButton->hide();
    
    mainWidget.blockMaterialLayout->setWidget(row, (QFormLayout::ItemRole)col, material.button);
}

void RocketBuildWidget::RefreshPhysicsPage()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    rigidBodies.clear();
    std::vector<shared_ptr<EC_RigidBody> > existing = scene->Components<EC_RigidBody>();

    rigidBodies.insert(rigidBodies.begin(), existing.begin(), existing.end());

    mainWidget.physicsDebugButton->setDisabled(bulletWorld.expired());
    PhysicsWorldPtr w = bulletWorld.lock();
    if (w)
    {
        disconnect(mainWidget.physicsDebugButton, SIGNAL(toggled(bool)), this, SLOT(SetPhysicsDebugEnabled(bool)));
        mainWidget.physicsDebugButton->setChecked(w->IsDebugGeometryEnabled());
        mainWidget.physicsDebugButton->setText(tr("Draw Debug: %1").arg(mainWidget.physicsDebugButton->isChecked() ? tr("Enabled") : tr("Disabled")));
        connect(mainWidget.physicsDebugButton, SIGNAL(toggled(bool)), this, SLOT(SetPhysicsDebugEnabled(bool)), Qt::UniqueConnection);
    }
    
    RefreshPhysicsEntityOptions();
}

void RocketBuildWidget::RefreshPhysicsEntityOptions()
{
    EC_RigidBody *rigid = Component<EC_RigidBody>(activeRigidBody);
    if (rigid && !activeEntity.expired())
    {
        QList<QWidget*> suppressSignals;
        suppressSignals << mainWidget.shapeComboBox << mainWidget.physicsSizeXLabelSpinBox 
                        << mainWidget.physicsSizeYLabelSpinBox << mainWidget.physicsSizeZLabelSpinBox;

        foreach (QWidget *w, suppressSignals)
            w->blockSignals(true); 

        mainWidget.shapeComboBox->clear();
        mainWidget.shapeComboBox->addItem(tr("Box"), EC_RigidBody::Shape_Box);
        mainWidget.shapeComboBox->addItem(tr("Sphere"), EC_RigidBody::Shape_Sphere);
        mainWidget.shapeComboBox->addItem(tr("Cylinder"), EC_RigidBody::Shape_Cylinder);
        mainWidget.shapeComboBox->addItem(tr("Capsule"), EC_RigidBody::Shape_Capsule);
        mainWidget.shapeComboBox->addItem(tr("Cone"), EC_RigidBody::Shape_Cone);
        mainWidget.shapeComboBox->addItem(tr("High Detail Mesh Shape"), EC_RigidBody::Shape_TriMesh);
        mainWidget.shapeComboBox->addItem(tr("Low Detail Mesh Shape"), EC_RigidBody::Shape_ConvexHull);

        // Show height field option only if applicable
        const EC_RigidBody::ShapeType currentShape = static_cast<EC_RigidBody::ShapeType>(rigid->shapeType.Get());
        if (currentShape == EC_RigidBody::Shape_HeightField || activeEntity.lock()->Component<EC_Terrain>())
            mainWidget.shapeComboBox->addItem(tr("Terrain Height Field"), EC_RigidBody::Shape_HeightField);

        switch(currentShape)
        {
        case EC_RigidBody::Shape_Box:
        case EC_RigidBody::Shape_Sphere:
        case EC_RigidBody::Shape_Cylinder:
        case EC_RigidBody::Shape_Capsule:
            mainWidget.shapeComboBox->setCurrentIndex(currentShape); // 1:1 mapping for these
            break;
        case EC_RigidBody::Shape_Cone:
            mainWidget.shapeComboBox->setCurrentIndex(4);
            break;
        case EC_RigidBody::Shape_TriMesh:
            mainWidget.shapeComboBox->setCurrentIndex(5);
            break;
        case EC_RigidBody::Shape_ConvexHull:
            mainWidget.shapeComboBox->setCurrentIndex(6);
            break;
        case EC_RigidBody::Shape_HeightField:
            mainWidget.shapeComboBox->setCurrentIndex(7);
            break;
        }

        // Show size spin boxes only for primitive shape types.
        const bool isPrimitiveShape = rigid->IsPrimitiveShape();
        mainWidget.labelPhysicsShapeName->setVisible(!isPrimitiveShape);
        mainWidget.frameSizeWidgets->setVisible(isPrimitiveShape);
        if (isPrimitiveShape)
        {
            const float3 &size = rigid->size.Get();
            mainWidget.physicsSizeXLabelSpinBox->setValue(size.x);
            mainWidget.physicsSizeYLabelSpinBox->setValue(size.y);
            mainWidget.physicsSizeZLabelSpinBox->setValue(size.z);
            // size.z applicable only for box, and size.y is not applicable for sphere:
            const bool isBox = (currentShape == EC_RigidBody::Shape_Box);
            const bool isNotSphere = (currentShape != EC_RigidBody::Shape_Sphere);
            mainWidget.physicsSizeXLabel->setText(isBox ? tr("w") : tr("r"));
            mainWidget.physicsSizeYLabel->setVisible(isNotSphere);
            mainWidget.physicsSizeYLabelSpinBox->setVisible(isNotSphere);
            mainWidget.physicsSizeZLabel->setVisible(isBox);
            mainWidget.physicsSizeZLabelSpinBox->setVisible(isBox);
        }
        else
        {
            mainWidget.labelPhysicsShapeName->setText(currentShape == EC_RigidBody::Shape_HeightField ? tr("Using Terrain Shape") : tr("Using Mesh Shape"));
        }

        // Start listening for changes
        connect(rigid, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(RefreshPhysicsEntityOptions()), Qt::UniqueConnection);

        foreach (QWidget *w, suppressSignals)
            w->blockSignals(false);
    }

    mainWidget.physicsDebugButton->setVisible(activeEntity.expired());
    mainWidget.framePhysics->setVisible(rigid != 0);
    mainWidget.physicsEntityTitle->setVisible(activeEntity.expired());
    mainWidget.physicsApplyButton->setVisible(!activeEntity.expired());
    mainWidget.physicsApplyButton->setText(tr("Apply physics: ") + (rigid != 0 ? tr("True") : tr("False")));
}

void RocketBuildWidget::SetActivePhysicsEntity(const EntityPtr &entity)
{
    if (!activeRigidBody.expired())
        Component<EC_RigidBody>(activeRigidBody)->drawDebug.Set(false, AttributeChange::LocalOnly);

    activeRigidBody = (entity ? entity->Component<EC_RigidBody>() : ComponentWeakPtr());

    if (!activeRigidBody.expired())
        Component<EC_RigidBody>(activeRigidBody)->drawDebug.Set(true, AttributeChange::LocalOnly);

    if (!bulletWorld.expired())
        bulletWorld.lock()->SetDebugGeometryEnabled(!activeRigidBody.expired());

    RefreshPhysicsEntityOptions();
}

void RocketBuildWidget::ApplyPhysicsForEntity(bool /*apply*/)
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    EntityPtr e = activeEntity.lock();
    if (e)
    {
        shared_ptr<EC_RigidBody> rb = e->Component<EC_RigidBody>();
        if (rb)
        {
            e->RemoveComponent(rb);
            rb.reset();
        }
        else
        {
            activeRigidBody = rb = e->CreateComponent<EC_RigidBody>();
            rb->drawDebug.Set(true, AttributeChange::LocalOnly);
            /// @todo When physics are initially applied, adjust the box shape to mesh's scale?
            /*
            EC_RigidBody::ShapeType shape = static_cast<EC_RigidBody::ShapeType>(mainWidget.shapeComboBox->itemData(shapeIndex).toUInt());
            rb->shapeType.Set(shape, AttributeChange::Default);
            if (shape >= EC_RigidBody::Shape_Capsule)
                SetRigidBodySize(rb, float3::one); // for non-primitive shapes, reset size to one
            */
        }
    }

    RefreshPhysicsEntityOptions();

    ConnectToScene(scene);
}

void RocketBuildWidget::SetPhysicsShape(int shapeIndex)
{
    EC_RigidBody *rb = Component<EC_RigidBody>(activeRigidBody);
    if (rb)
    {
        disconnect(rb, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), this, SLOT(RefreshPhysicsEntityOptions()));

        EC_RigidBody::ShapeType shape = static_cast<EC_RigidBody::ShapeType>(mainWidget.shapeComboBox->itemData(shapeIndex).toUInt());
        rb->shapeType.Set(shape, AttributeChange::Default);
        if (shape >= EC_RigidBody::Shape_Capsule)
            SetRigidBodySize(rb, float3::one); // for non-primitive shapes, reset size to one
        RefreshPhysicsEntityOptions(); // different shapes can have different options

        connect(rb, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(RefreshPhysicsEntityOptions()), Qt::UniqueConnection);
    }
}

void RocketBuildWidget::SetRigidBodySize(EC_RigidBody *rb, const float3 &size)
{
    disconnect(rb, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), this, SLOT(RefreshPhysicsEntityOptions()));

    owner->SetAttribute(rb->size, size);

    connect(rb, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(RefreshPhysicsEntityOptions()), Qt::UniqueConnection);
}

void RocketBuildWidget::SetRigidBodySizeX(double x)
{
    EC_RigidBody *rb = Component<EC_RigidBody>(activeRigidBody);
    if (rb)
    {
        float3 size = rb->size.Get();
        size.x = x;
        SetRigidBodySize(rb, size);
    }
}

void RocketBuildWidget::SetRigidBodySizeY(double y)
{
    EC_RigidBody *rb = Component<EC_RigidBody>(activeRigidBody);
    if (rb)
    {
        float3 size = rb->size.Get();
        size.y = y;
        SetRigidBodySize(rb, size);
    }
}

void RocketBuildWidget::SetRigidBodySizeZ(double z)
{
    EC_RigidBody *rb = Component<EC_RigidBody>(activeRigidBody);
    if (rb)
    {
        float3 size = rb->size.Get();
        size.z = z;
        SetRigidBodySize(rb, size);
    }
}

void RocketBuildWidget::ClearTeleportItems()
{
    mainWidget.teleportsListWidget->clearSelection();
    while(mainWidget.teleportsListWidget->count() > 0)
    {
        QListWidgetItem *item = mainWidget.teleportsListWidget->takeItem(0);
        SAFE_DELETE(item);
    }
}

void RocketBuildWidget::ClearLightItems()
{
    mainWidget.lightListWidget->clearSelection();
    while(mainWidget.lightListWidget->count() > 0)
    {
        QListWidgetItem *item = mainWidget.lightListWidget->takeItem(0);
        SAFE_DELETE(item);
    }
}

void RocketBuildWidget::CreateSceneCamera()
{
    /// @todo Implement
}

void RocketBuildWidget::FocusCameraToEntity(const EntityPtr &entity)
{
    if (cameraMode == AvatarCameraMode)
        return;

    QMenu dummyMenu;
    QList<QObject*> targets;
    targets.push_back(entity.get());

    /// @todo For now, simple quick'n'dirty implementation utilizing CameraApplication.js
    Fw()->Ui()->EmitContextMenuAboutToOpen(&dummyMenu, targets);
    foreach(QAction *action, dummyMenu.actions())
        if (action->text() == "Locate")
        {
            action->activate(QAction::ActionEvent());
            break;
        }
}

void RocketBuildWidget::SetWaterHeight(double value)
{
    if (Component<EC_WaterPlane>(water))
        SetWaterPlaneHeight(value);
    /*else if (Component<EC_Hydrax>(water))
        SetHydraxHeight(value);*/
    else if (Component<EC_MeshmoonWater>(water))
        SetMeshmoonWaterHeight(value);
}

void RocketBuildWidget::SetWaterPlaneHeight(double value)
{
    EC_WaterPlane *ec = Component<EC_WaterPlane>(water);
    if (ec)
    {
        float3 pos = ec->position.Get();
        pos.y = static_cast<float>(value);
        owner->SetAttribute(ec->position, pos);
    }
}

/*void RocketBuildWidget::SetHydraxHeight(double value)
{
    EC_Hydrax *ec = Component<EC_Hydrax>(water);
    if (ec)
    {
        float3 pos = ec->position.Get();
        pos.y = static_cast<float>(value);
        owner->SetAttribute(ec->position, pos);
    }
}*/

void RocketBuildWidget::SetMeshmoonWaterHeight(double value)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->seaLevel, static_cast<float>(value));
}

void RocketBuildWidget::SetMeshmoonSimModel(int index)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->simulationModel, static_cast<uint>(index));
}
void RocketBuildWidget::SetMeshmoonWaterWeatherCondition(int index)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->beaufortScale, static_cast<uint>(index));
}
void RocketBuildWidget::SetMeshmoonWaterChoppiness(double choppiness)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->waveChoppiness, static_cast<float>(choppiness));
}

void RocketBuildWidget::SetMeshmoonWaterReflectionIntensity(double intensity)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->reflectionIntensity, Min(Max(static_cast<float>(intensity), 0.0f), 1.0f));
}

void RocketBuildWidget::SetMeshmoonWaterSpray(bool enabled)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->sprayEnabled, enabled);
}

void RocketBuildWidget::SetMeshmoonReflections(bool enabled)
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        owner->SetAttribute(ec->reflectionsEnabled, enabled);
}

void RocketBuildWidget::SetShadowFirstSplitDist(double value)
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (ec)
        owner->SetAttribute(ec->firstSplitDist, static_cast<float>(value));
}

void RocketBuildWidget::SetShadowFarDist(double value)
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (ec)
        owner->SetAttribute(ec->farDist, static_cast<float>(value));
}

void RocketBuildWidget::SetShadowFadeDist(double value)
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (ec)
        owner->SetAttribute(ec->fadeDist, static_cast<float>(value));
}

void RocketBuildWidget::SetShadowSplitLambda(double value)
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (ec)
        owner->SetAttribute(ec->splitLambda, static_cast<float>(value));
}

void RocketBuildWidget::SetShadowDepthBias(double value)
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (!ec)
        return;

    QDoubleSpinBox *w = qobject_cast<QDoubleSpinBox*>(sender());
    if (!w)
        return;
        
    int index = w->property("shadowBiasIndex").toInt();
    if (index == 1)
        owner->SetAttribute(ec->depthBias1, static_cast<float>(value));
    else if (index == 2)
        owner->SetAttribute(ec->depthBias2, static_cast<float>(value));
    else if (index == 3)
        owner->SetAttribute(ec->depthBias3, static_cast<float>(value));
    else if (index == 4)
        owner->SetAttribute(ec->depthBias4, static_cast<float>(value));
}

void RocketBuildWidget::ShowShadowSetupAdvancedOptions(bool enabled)
{
    mainWidget.shadowsAdvancedOptionsWidget->setVisible(enabled);
}

void RocketBuildWidget::EnableShadowDebugPanels(bool enabled)
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    if (scene->Subsystem<OgreWorld>()->IsShadowDebugOverlaysVisible() != enabled)
        scene->Subsystem<OgreWorld>()->SetShadowDebugOverlays(enabled);
}

void RocketBuildWidget::OpenAssetPicker(const QString &assetType, const char *onAssetChanged, const char *onAssetPicked, const char *onPickCanceled)
{
    if (assetsWindow)
        assetsWindow->close();

    assetsWindow = new AssetsWindow(assetType, Fw(), Fw()->Ui()->MainWindow());

    connect(assetsWindow, SIGNAL(SelectedAssetChanged(AssetPtr)), onAssetChanged);
    connect(assetsWindow, SIGNAL(AssetPicked(AssetPtr)), onAssetPicked);
    connect(assetsWindow, SIGNAL(PickCanceled()), onPickCanceled);

    assetsWindow->setAttribute(Qt::WA_DeleteOnClose);
    assetsWindow->setWindowFlags(Qt::Tool);
    assetsWindow->show();
}

void RocketBuildWidget::OpenStoragePicker(const QStringList &fileSuffixes, const char *onAssetPicked, const char *onPickCanceled)
{
    if (!rocket || !rocket->Storage())
        return;

    RocketStorageSelectionDialog *dialog = rocket->Storage()->ShowSelectionDialog(fileSuffixes, true, lastStorageLocation_);
    if (dialog)
    {
        connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), onAssetPicked, Qt::UniqueConnection);
        connect(dialog, SIGNAL(Canceled()), onPickCanceled, Qt::UniqueConnection);
    }
}

void RocketBuildWidget::SetFogExpDensity(double value)
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
        owner->SetAttribute(ec->expDensity, static_cast<float>(value));
}

void RocketBuildWidget::SetFogStartDistance(double value)
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
        owner->SetAttribute(ec->startDistance, static_cast<float>(value));
}

void RocketBuildWidget::SetFogEndDistance(double value)
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
        owner->SetAttribute(ec->endDistance, static_cast<float>(value));
}

void RocketBuildWidget::SetHdrEnabled(bool enabled)
{
    SetCompositorEnabled(cHdrName, enabled);
    mainWidget.hdrAdvancedOptionsWidget->setVisible(enabled);
}

void RocketBuildWidget::SetBloomEnabled(bool enabled)
{
    SetCompositorEnabled(cBloomName, enabled);
    mainWidget.bloomAdvancedOptionsWidget->setVisible(enabled);
}

void RocketBuildWidget::SetCompositorEnabled(const QString &name, bool enabled)
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    EC_OgreCompositor *ec = Component<EC_OgreCompositor>(CompositorComponent(name));
    if (enabled)
    {
        if (!ec)
            ec = owner->EnvironmentEntity()->GetOrCreateComponent<EC_OgreCompositor>(name).get();
        ec->compositorName.Set(name, AttributeChange::Default);
        ec->enabled.Set(enabled, AttributeChange::Default);
        SetCompositorComponent(name, ec->shared_from_this());
    }
    else
    {
        SetCompositorComponent(name, ComponentPtr());
    }

    ConnectToScene(scene);
}

void RocketBuildWidget::RefreshSkySettings()
{
    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.skyComboBox << mainWidget.cloudTypeComboBox << mainWidget.timeMultiplierSpinBox
                    << mainWidget.skyTimeSlider << mainWidget.skyDirSlider << mainWidget.skySpeedSlider
                    << mainWidget.timeModeComboBox << mainWidget.timeZoneComboBox << mainWidget.latitudeSpinBox << mainWidget.longitudeSpinBox
                    << mainWidget.skyLightIntensitySpinBox << mainWidget.skyGammaSpinBox << mainWidget.skyGodRayIntensitySpinBox << mainWidget.timeModeComboBox;
        
    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true);
        
    QList<QWidget*> EC_MeshmoonSky_widgets_hide;

    bool isComplex = false;
    bool isComplexMeshmoon = false;
    if (Component<EC_MeshmoonSky>(sky))
    {
        isComplex = true;
        isComplexMeshmoon = true;

        EC_MeshmoonSky *ec = Component<EC_MeshmoonSky>(sky);
        
        mainWidget.cloudTypeLabel->setText(tr("Cloud Coverage"));
        mainWidget.cloudTypeComboBox->clear();
        mainWidget.cloudTypeComboBox->addItem(tr("Use Cloud Layer Components"));
        mainWidget.cloudTypeComboBox->addItem(tr("No Clouds"));
        mainWidget.cloudTypeComboBox->addItem(tr("Fair"));
        mainWidget.cloudTypeComboBox->addItem(tr("Partly Cloudy"));
        mainWidget.cloudTypeComboBox->addItem(tr("Mostly Cloudy"));
        mainWidget.cloudTypeComboBox->addItem(tr("Overcast"));
        mainWidget.cloudTypeComboBox->addItem(tr("Storm"));
        mainWidget.cloudTypeComboBox->setCurrentIndex(ec->cloudCoverage.Get());

        mainWidget.timeMultiplierSpinBox->setValue(ec->timeMultiplier.Get());
        const double floatTime = ec->time.Get();
        double hours, mins;
        mins = modf(floatTime, &hours);
        mins *= 60.f;

        mainWidget.skyDateEdit->setText(ec->date.Get());
        
        mainWidget.skyTimeSlider->setValue(floatTime * 100.f);
        mainWidget.skyTimeValueLabel->setText(QTime(static_cast<int>(hours), static_cast<int>(mins)).toString("HH:mm"));

        mainWidget.skyDirSlider->setValue(FloorInt(ec->windDirection.Get()) % 360);
        mainWidget.skyDirValueLabel->setText(QString("%1&deg;").arg(mainWidget.skyDirSlider->value()));

        mainWidget.skySpeedSlider->setValue(Min(FloorInt(ec->windSpeed.Get()), mainWidget.skySpeedSlider->maximum()));
        mainWidget.skySpeedValueLabel->setText(QString("%1 m/s").arg(mainWidget.skySpeedSlider->value()));
        
        mainWidget.latitudeSpinBox->setValue(ec->latitude.Get());
        mainWidget.longitudeSpinBox->setValue(ec->longitude.Get());
        
        mainWidget.skyLightIntensitySpinBox->setValue(ec->lightSourceBrightness.Get());
        mainWidget.skyGammaSpinBox->setValue(ec->gamma.Get());
        mainWidget.skyGodRayIntensitySpinBox->setValue(ec->godRayIntensity.Get());

        mainWidget.timeModeComboBox->setCurrentIndex((int)ec->timeMode.Get());

        QString zoneStr = GetSkyTimeZoneString(static_cast<EC_MeshmoonSky::TimeZone>(ec->timeZone.Get()));
        int zoneIndex = mainWidget.timeZoneComboBox->findText(zoneStr);
        if (zoneIndex != -1)
            mainWidget.timeZoneComboBox->setCurrentIndex(zoneIndex);

        // Hide stuff that is ignored.
        bool realTimeMode = (ec->timeMode.Get() == 0);
        if (realTimeMode)
            EC_MeshmoonSky_widgets_hide << mainWidget.skyTimeLabel << mainWidget.skyTimeValueLabel << mainWidget.skyTimeSlider
                << mainWidget.timeMultiplierLabel << mainWidget.timeMultiplierSpinBox << mainWidget.skyDateLabel << mainWidget.skyDateEdit;
        else
            EC_MeshmoonSky_widgets_hide << mainWidget.timeZoneLabel << mainWidget.timeZoneComboBox;

        // Simulated time or not 'Detect Time Zone from Longitude' as timezone
        if (realTimeMode && mainWidget.timeZoneComboBox->currentText().compare("Detect Time Zone from Longitude", Qt::CaseInsensitive) != 0)
            EC_MeshmoonSky_widgets_hide << mainWidget.longitudeLabel << mainWidget.longitudeSpinBox;

        mainWidget.skyComboBox->setCurrentIndex(ComplexMeshmoon);
    }
    else if (Component<EC_SkyX>(sky))
    {
        isComplex = true;

        EC_SkyX *ec = Component<EC_SkyX>(sky);

        mainWidget.cloudTypeLabel->setText(tr("Cloud Type"));
        mainWidget.cloudTypeComboBox->clear();
        mainWidget.cloudTypeComboBox->addItem(tr("None"));
        mainWidget.cloudTypeComboBox->addItem(tr("Normal"));
        mainWidget.cloudTypeComboBox->addItem(tr("Volumetric"));
        mainWidget.cloudTypeComboBox->setCurrentIndex(ec->cloudType.Get());

        mainWidget.timeMultiplierSpinBox->setValue(ec->timeMultiplier.Get());
        const double floatTime = ec->time.Get();
        double hours, mins;
        mins = modf(floatTime, &hours);
        mins *= 60.f;

        mainWidget.skyTimeSlider->setValue(floatTime * 100.f);
        mainWidget.skyTimeValueLabel->setText(QTime(static_cast<int>(hours), static_cast<int>(mins)).toString("HH:mm"));
        
        mainWidget.skyDirSlider->setValue(FloorInt(ec->windDirection.Get()) % 360);
        mainWidget.skyDirValueLabel->setText(QString("%1&deg;").arg(mainWidget.skyDirSlider->value()));
        
        mainWidget.skySpeedSlider->setValue(Min(FloorInt(ec->windSpeed.Get()), mainWidget.skySpeedSlider->maximum()));
        mainWidget.skySpeedValueLabel->setText(QString("%1 m/s").arg(mainWidget.skySpeedSlider->value()));
        
        mainWidget.skyComboBox->setCurrentIndex(Complex);
    }
    else if (Component<EC_Sky>(sky))
    {
        mainWidget.skyMaterialButton->setText(AssetFilename(Component<EC_Sky>(sky)->materialRef.Get().ref));
        mainWidget.skyComboBox->setCurrentIndex(Simple);
    }
    else
        mainWidget.skyComboBox->setCurrentIndex(ES_None);

    QList<QWidget*> EC_SkyBox_widgets;
    EC_SkyBox_widgets << mainWidget.skyMaterialLabel << mainWidget.skyMaterialButton;

    QList<QWidget*> EC_SkyX_widgets;
    EC_SkyX_widgets << mainWidget.skyTimeLabel << mainWidget.skyTimeSlider << mainWidget.skyTimeValueLabel << mainWidget.skyDirLabel
        << mainWidget.skyDirSlider << mainWidget.skyDirValueLabel << mainWidget.skySpeedLabel << mainWidget.skySpeedSlider
        << mainWidget.skySpeedValueLabel << mainWidget.cloudTypeLabel << mainWidget.cloudTypeComboBox
        << mainWidget.timeMultiplierLabel << mainWidget.timeMultiplierSpinBox;

    QList<QWidget*> EC_MeshmoonSky_widgets;
    EC_MeshmoonSky_widgets << mainWidget.timeModeLabel << mainWidget.timeModeComboBox << mainWidget.timeZoneLabel << mainWidget.timeZoneComboBox
        << mainWidget.latitudeLabel << mainWidget.latitudeSpinBox << mainWidget.longitudeLabel << mainWidget.longitudeSpinBox
        << mainWidget.skyLightIntensityLabel << mainWidget.skyLightIntensitySpinBox << mainWidget.skyGammaLabel << mainWidget.skyGammaSpinBox
        << mainWidget.skyGodRayIntensityLabel << mainWidget.skyGodRayIntensitySpinBox << mainWidget.skyDateLabel << mainWidget.skyDateEdit;

    foreach (QWidget *w, EC_SkyBox_widgets)
        w->setVisible(!isComplex);
    foreach (QWidget *w, EC_SkyX_widgets)
    {
        if (!isComplex && !isComplexMeshmoon)
            w->setVisible(false);
        else if (isComplex && !isComplexMeshmoon)
            w->setVisible(isComplex);
        else if (isComplexMeshmoon)
            w->setVisible(isComplexMeshmoon && !EC_MeshmoonSky_widgets_hide.contains(w));
    }
    foreach (QWidget *w, EC_MeshmoonSky_widgets)
        w->setVisible(isComplexMeshmoon && !EC_MeshmoonSky_widgets_hide.contains(w));

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false); 
}

void RocketBuildWidget::RefreshWaterSettings()
{
    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.waterComboBox << mainWidget.waterHeightSpinBox
                    << mainWidget.EC_MeshmoonWaterTypeComboBox << mainWidget.EC_MeshmoonWaterWeatherConditionComboBox
                    << mainWidget.EC_MeshmoonWaterChoppinessSpinBox << mainWidget.EC_MeshmoonWaterSprayCheckBox << mainWidget.EC_MeshmoonWaterReflectionsCheckBox
                    << mainWidget.EC_MeshmoonWaterReflectionIntensitySpinBox;
    
    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true);      

    mainWidget.waterComboBox->setCurrentIndex(waterMode);

    EC_MeshmoonWater *meshmoonWater = Component<EC_MeshmoonWater>(water);
    EC_WaterPlane *waterPlane = Component<EC_WaterPlane>(water);

    if (waterPlane)
    {
        mainWidget.waterHeightSpinBox->setValue(waterPlane->position.Get().y);
        mainWidget.waterMaterialButton->setText(!waterPlane->materialRef.Get().ref.trimmed().isEmpty() ? AssetFilename(waterPlane->materialRef.Get().ref) : AssetFilename(waterPlane->materialName.Get()));
    }
    else if (meshmoonWater)
    {
        mainWidget.waterHeightSpinBox->setValue(meshmoonWater->seaLevel.Get());
        mainWidget.EC_MeshmoonWaterTypeComboBox->setCurrentIndex(meshmoonWater->simulationModel.Get());
        mainWidget.EC_MeshmoonWaterWeatherConditionComboBox->setCurrentIndex(meshmoonWater->beaufortScale.Get());
        mainWidget.EC_MeshmoonWaterChoppinessSpinBox->setValue(meshmoonWater->waveChoppiness.Get());
        mainWidget.EC_MeshmoonWaterSprayCheckBox->setChecked(meshmoonWater->sprayEnabled.Get());
        mainWidget.EC_MeshmoonWaterReflectionsCheckBox->setChecked(meshmoonWater->reflectionsEnabled.Get());
        mainWidget.EC_MeshmoonWaterReflectionIntensitySpinBox->setEnabled(meshmoonWater->reflectionsEnabled.Get());
        mainWidget.EC_MeshmoonWaterReflectionIntensitySpinBox->setValue(meshmoonWater->reflectionsEnabled.Get() ? meshmoonWater->reflectionIntensity.Get() : 0.0);
        SetButtonColor(mainWidget.EC_MeshmoonWaterColorButton, meshmoonWater->waterColor.Get());
    }
    else
        mainWidget.waterHeightSpinBox->setValue(0.0f);

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false);
}

void RocketBuildWidget::RefreshFogSettings()
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
    {
        QList<QWidget*> suppressSignals;
        suppressSignals << mainWidget.fogButton << mainWidget.fogModeComboBox << mainWidget.fogExpDensitySpinBox
                        << mainWidget.fogStartDistanceSpinBox << mainWidget.fogEndDistanceSpinBox;

        foreach (QWidget *w, suppressSignals)
            w->blockSignals(true); 
        
        SetButtonColor(mainWidget.fogColorButton, ec->color.Get());

        EC_Fog::FogMode mode = static_cast<EC_Fog::FogMode>(ec->mode.Get());
        bool isLinear = (mode == EC_Fog::Linear);
        mainWidget.fogButton->setChecked(mode != EC_Fog::None);
        mainWidget.fogButton->setText(mainWidget.fogButton->isChecked() ? "Enabled" : "Disabled");        
        mainWidget.fogExpDensitySpinBox->setValue(ec->expDensity.Get());
        mainWidget.fogStartDistanceSpinBox->setValue(ec->startDistance.Get());
        mainWidget.fogEndDistanceSpinBox->setValue(ec->endDistance.Get());
        mainWidget.fogModeComboBox->setCurrentIndex(FogMode() - 1);
        if (isLinear) // Do not allow settings start distance >= end distance, or end distance <= start distance
        {
            mainWidget.fogEndDistanceSpinBox->setMinimum(mainWidget.fogStartDistanceSpinBox->value());
            mainWidget.fogStartDistanceSpinBox->setMaximum(mainWidget.fogEndDistanceSpinBox->value());
        }

        mainWidget.fogExpDensitySpinBox->setVisible(!isLinear);
        mainWidget.fogExpDensityLabel->setVisible(!isLinear);
        mainWidget.fogStartDistanceLabel->setVisible(isLinear);
        mainWidget.fogEndDistanceLabel->setVisible(isLinear);
        mainWidget.fogStartDistanceSpinBox->setVisible(isLinear);
        mainWidget.fogEndDistanceSpinBox->setVisible(isLinear);

        foreach (QWidget *w, suppressSignals)
            w->blockSignals(false); 
    }
}

void RocketBuildWidget::RefreshShadowSetupSettings()
{
    EC_SceneShadowSetup *ec = Component<EC_SceneShadowSetup>(shadowSetup);
    if (!ec)
        return;

    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.shadowFirstSplitDistSpinBox << mainWidget.shadowFarDistanceSpinBox << mainWidget.shadowFadeDistSpinBox
        << mainWidget.shadowSplitLambdaSpinBox << mainWidget.shadowDepthBias1SpinBox << mainWidget.shadowDepthBias2SpinBox
        << mainWidget.shadowDepthBias3SpinBox << mainWidget.shadowDepthBias4SpinBox << mainWidget.shadowDebugPanelsCheckBox;

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true); 

    mainWidget.shadowFirstSplitDistSpinBox->setValue(ec->firstSplitDist.Get());   
    mainWidget.shadowFarDistanceSpinBox->setValue(ec->farDist.Get());
    mainWidget.shadowFadeDistSpinBox->setValue(ec->fadeDist.Get());
    mainWidget.shadowSplitLambdaSpinBox->setValue(ec->splitLambda.Get());

    mainWidget.shadowDepthBias1SpinBox->setValue(ec->depthBias1.Get());
    mainWidget.shadowDepthBias2SpinBox->setValue(ec->depthBias2.Get());
    mainWidget.shadowDepthBias3SpinBox->setValue(ec->depthBias3.Get());
    mainWidget.shadowDepthBias4SpinBox->setValue(ec->depthBias4.Get());

    ScenePtr scene = Scene();
    if (scene)
        mainWidget.shadowDebugPanelsCheckBox->setChecked(scene->Subsystem<OgreWorld>()->IsShadowDebugOverlaysVisible());

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false); 
}

void RocketBuildWidget::RefreshCompositorSettings()
{   
    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.hdrCheckBox << mainWidget.hdrBloomWeightSpinBox << mainWidget.hdrMinLuminanceSpinBox
                    << mainWidget.hdrMaxLuminanceSpinBox << mainWidget.bloomCheckBox << mainWidget.bloomBlurWeightSpinBox
                    << mainWidget.bloomOriginalImageWeightSpinBox;

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true); 

    const bool hdrEnabled = (CompositorComponent(cHdrName) != ComponentPtr());
    mainWidget.hdrCheckBox->setChecked(hdrEnabled);
    mainWidget.hdrAdvancedOptionsWidget->setVisible(hdrEnabled);
    if (hdrEnabled)
    {
        mainWidget.hdrBloomWeightSpinBox->setValue(CompositorParameterValue(cHdrName, "BloomWeight"));
        mainWidget.hdrMinLuminanceSpinBox->setValue(CompositorParameterValue(cHdrName, "LuminanceMin"));
        mainWidget.hdrMaxLuminanceSpinBox->setValue(CompositorParameterValue(cHdrName, "LuminanceMax"));
    }

    const bool bloomEnabled = (CompositorComponent(cBloomName) != ComponentPtr());
    mainWidget.bloomCheckBox->setChecked(bloomEnabled);
    mainWidget.bloomAdvancedOptionsWidget->setVisible(bloomEnabled);
    if (bloomEnabled)
    {
        mainWidget.bloomBlurWeightSpinBox->setValue(CompositorParameterValue(cBloomName, "BlurWeight"));
        mainWidget.bloomOriginalImageWeightSpinBox->setValue(CompositorParameterValue(cBloomName, "OriginalImageWeight"));
    }

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false); 
}

void RocketBuildWidget::RefreshEnvironmentLightSettings()
{
    bool hasEnvLight = false;
    Color sunlightColor, ambientLightColor;
    if (Component<EC_EnvironmentLight>(environmentLight))
    {
        EC_EnvironmentLight *ec = Component<EC_EnvironmentLight>(environmentLight);
        sunlightColor = ec->sunColor.Get();
        ambientLightColor = ec->ambientColor.Get();
        hasEnvLight = true;
        mainWidget.envLightDirectionLabel->show();
        mainWidget.setEnvLightDirFromCamButton->show();
    }
    else if (Component<EC_SkyX>(environmentLight))
    {
        EC_SkyX *ec = Component<EC_SkyX>(environmentLight);
        sunlightColor = ec->sunlightDiffuseColor.Get();
        ambientLightColor = ec->ambientLightColor.Get();
        hasEnvLight = true;
        mainWidget.envLightDirectionLabel->hide();
        mainWidget.setEnvLightDirFromCamButton->hide();
    }

    mainWidget.envLightSettingsWidget->setVisible(hasEnvLight);
    mainWidget.envLightMissingLabel->setVisible(!hasEnvLight);

    SetButtonColor(mainWidget.sunlightColorButton, sunlightColor);
    SetButtonColor(mainWidget.ambientLightColorButton, ambientLightColor);
}

void RocketBuildWidget::SetSkyComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, sky, SLOT(RefreshSkySettings()));
    RefreshSkySettings();
}

void RocketBuildWidget::SetWaterComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, water, SLOT(RefreshWaterSettings()));
    RefreshWaterSettings();
}

void RocketBuildWidget::SetFogComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, fog, SLOT(RefreshFogSettings()));
    RefreshFogSettings();
}

void RocketBuildWidget::SetShadowSetupComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, shadowSetup, SLOT(RefreshShadowSetupSettings()));
    RefreshShadowSetupSettings();
}

void RocketBuildWidget::SetTerrainComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, terrain, SLOT(RefreshTerrainSettings()));
    RefreshTerrainSettings();
}

void RocketBuildWidget::SetEnvironmentLightComponent(const ComponentPtr &component)
{
    SetComponentInternal(component, environmentLight, SLOT(RefreshEnvironmentLightSettings()));
    RefreshEnvironmentLightSettings();
}

void RocketBuildWidget::SetCompositorComponent(const QString &name, const ComponentPtr &component)
{
    if (name != cHdrName && name != cBloomName)
    {
        LogWarning("RocketBuildWidget::SetCompositorComponent: trying to set unsupported compositor \"" + name + "\", ignoring.");
        return;
    }

    if (!compositors[name].expired())
    {
        disconnect(compositors[name].lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            this, SLOT(RefreshCompositorSettings()));
        compositors[name].lock()->ParentEntity()->RemoveComponent(compositors[name].lock());
    }

    compositors[name] = component;

    if (!compositors[name].expired())
    {
        connect(compositors[name].lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            this, SLOT(RefreshCompositorSettings()), Qt::UniqueConnection);
    }

    RefreshCompositorSettings();
}

EntityListWidgetItem *RocketBuildWidget::AddTeleport(const ComponentPtr &tp, bool select)
{
    EntityPtr entity = tp->ParentEntity()->shared_from_this();
    
    // Check if already in list
    EntityListWidgetItem *item = 0;
    for(int i = 0; i < mainWidget.teleportsListWidget->count(); ++i)
    {
        EntityListWidgetItem *iter = static_cast<EntityListWidgetItem *>(mainWidget.teleportsListWidget->item(i));
        if (iter && iter->Entity() == entity)
        {
            item = iter;
            break;
        }
    }
    if (!item)
    {
        EC_Name *name = tp->ParentEntity()->Component<EC_Name>().get();
        item = new EntityListWidgetItem(name->name.Get(), mainWidget.teleportsListWidget, entity);
        mainWidget.teleportsListWidget->addItem(item);
        
        // Listen to possible name changes.
        connect(name, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(UpdateEntityName(IAttribute *)), Qt::UniqueConnection);
    }
    if (select)
    {
        lastClickedListItem = 0;
        mainWidget.teleportsListWidget->setCurrentItem(item, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Clear);
    }
    return item;
}

void RocketBuildWidget::RemoveTeleport(const EntityPtr &teleportEntity, bool removeParentEntity)
{
    if (!teleportEntity)
        return;

    for(int i = 0; i < mainWidget.teleportsListWidget->count(); ++i)
    {
        EntityListWidgetItem *item = static_cast<EntityListWidgetItem *>(mainWidget.teleportsListWidget->item(i));
        if (item->Entity() == teleportEntity)
        {
            lastClickedListItem = 0;
            mainWidget.teleportsListWidget->setCurrentItem(item, QItemSelectionModel::Deselect | QItemSelectionModel::Clear);          

            owner->SetSelection(EntityPtr());
            SetActiveTeleportEntity(EntityPtr());
            SAFE_DELETE(item);

            /// @todo undo stack
            if (removeParentEntity)
            {
                ScenePtr scene = Scene();
                if (scene)
                    scene->RemoveEntity(teleportEntity->Id());
            }
            else
            {
                teleportEntity->RemoveComponents(EC_MeshmoonTeleport::ComponentTypeId);
            }

            RefreshCurrentObjectSelection();
            break;
        }
    }
}

void RocketBuildWidget::RemoveTeleport(const ComponentPtr &light, bool removeParentEntity)
{
    if (light)
        RemoveTeleport(light->ParentEntity()->shared_from_this(), removeParentEntity);
}

EntityListWidgetItem *RocketBuildWidget::AddLight(const ComponentPtr &light, bool select)
{
    EntityPtr entity = light->ParentEntity()->shared_from_this();
    
    // Check if already in list
    EntityListWidgetItem *item = 0;
    for(int i = 0; i < mainWidget.lightListWidget->count(); ++i)
    {
        EntityListWidgetItem *iter = static_cast<EntityListWidgetItem *>(mainWidget.lightListWidget->item(i));
        if (iter && iter->Entity() == entity)
        {
            item = iter;
            break;
        }
    }

    if (!item)
    {
        EC_Name *name = light->ParentEntity()->Component<EC_Name>().get();
        item = new EntityListWidgetItem(name->name.Get(), mainWidget.lightListWidget, entity);
        mainWidget.lightListWidget->addItem(item);
        
        // Listen to possible name changes.
        connect(name, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(UpdateEntityName(IAttribute *)), Qt::UniqueConnection);
    }
    if (select)
    {
        lastClickedListItem = 0;
        mainWidget.lightListWidget->setCurrentItem(item, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Clear);
    }
    return item;
}

void RocketBuildWidget::RemoveLight(const EntityPtr &lightEntity, bool removeParentEntity)
{
    if (!lightEntity)
        return;

    for(int i = 0; i < mainWidget.lightListWidget->count(); ++i)
    {
        EntityListWidgetItem *item = static_cast<EntityListWidgetItem *>(mainWidget.lightListWidget->item(i));
        if (item->Entity() == lightEntity)
        {
            lastClickedListItem = 0;
            mainWidget.teleportsListWidget->setCurrentItem(item, QItemSelectionModel::Deselect | QItemSelectionModel::Clear);          

            EntityPtr empty;
            owner->SetSelection(empty);
            SetActiveLightEntity(empty);
            SAFE_DELETE(item);

            /// @todo undo stack
            if (removeParentEntity)
            {
                ScenePtr scene = Scene();
                if (scene)
                    scene->RemoveEntity(lightEntity->Id());
            }
            else
            {
                lightEntity->RemoveComponents(EC_Light::ComponentTypeId);
            }

            RefreshCurrentObjectSelection();
            break;
        }
    }
}

void RocketBuildWidget::RemoveLight(const ComponentPtr &light, bool removeParentEntity)
{
    if (light)
        RemoveLight(light->ParentEntity()->shared_from_this(), removeParentEntity);
}

void RocketBuildWidget::SetComponentInternal(const ComponentPtr &component, ComponentWeakPtr &destination, const char *handlerSlot)
{
    if (!destination.expired())
    {
        disconnect(destination.lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), this, handlerSlot);
        destination.lock()->ParentEntity()->RemoveComponent(destination.lock());
    }

    destination = component;

    if (!destination.expired())
    {
        connect(destination.lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
            this, handlerSlot, Qt::UniqueConnection);
    }
}

void RocketBuildWidget::SetSkyMode(int mode)
{
    if (!sky.expired())
    {
        QMessageBox::StandardButton result = rocket->Notifications()->ExecSplashDialog(
            tr("This will remove your current sky and forgets its settings.<br><br><b>Continue Anyway?</b>"), ":/images/icon-sky.png",
            QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);

        if (result != QMessageBox::Yes)
        {
            mainWidget.skyComboBox->blockSignals(true);
            mainWidget.skyComboBox->setCurrentIndex(skyMode);
            mainWidget.skyComboBox->blockSignals(false);
            return;
        }
    }
    SetSkyMode(static_cast<EnvironmentSettingMode>(mode));
}

void RocketBuildWidget::OpenMaterialPickerForSky()
{
    originalAssetReference = "";
    EC_Sky *ec = Component<EC_Sky>(sky);
    if (ec)
    {
        originalAssetReference = ec->materialRef.Get().ref;
        //OpenAssetPicker("OgreMaterial", SLOT(SetSkyMaterial(const AssetPtr &)), SLOT(SetSkyMaterial(const AssetPtr&)), SLOT(RestoreOriginalSkyMaterial()));
        OpenStoragePicker(QStringList() << "material", SLOT(SetSkyMaterial(const MeshmoonStorageItem&)), SLOT(RestoreOriginalSkyMaterial()));
    }
}

void RocketBuildWidget::SetSkyMaterial(const AssetPtr &asset)
{
    EC_Sky *ec = Component<EC_Sky>(sky);
    if (ec)
        owner->SetAttribute(ec->materialRef, AssetReference(asset->Name()));
}

void RocketBuildWidget::SetSkyMaterial(const MeshmoonStorageItem &item)
{
    EC_Sky *ec = Component<EC_Sky>(sky);
    if (ec)
    {
        owner->SetAttribute(ec->materialRef, AssetReference(item.relativeAssetRef));
        mainWidget.skyMaterialButton->setText(AssetFilename(item.filename));
    }
}

void RocketBuildWidget::RestoreOriginalSkyMaterial()
{
    EC_Sky *ec = Component<EC_Sky>(sky);
    if (ec && !originalAssetReference.isEmpty())
        owner->SetAttribute(ec->materialRef, AssetReference(originalAssetReference));
}

void RocketBuildWidget::SetSkyTime(int time)
{
    EC_SkyX *skyx = Component<EC_SkyX>(sky);
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (skyx || meshmoonSky)
        owner->SetAttribute(skyx ? skyx->time : meshmoonSky->time, static_cast<float>(time) / 100.f);
}

void RocketBuildWidget::SetSkyTime(const QTime &time)
{
    EC_SkyX *skyx = Component<EC_SkyX>(sky);
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (skyx || meshmoonSky)
        owner->SetAttribute(skyx ? skyx->time : meshmoonSky->time, static_cast<float>(time.hour()) + static_cast<float>(time.minute()) / 60.f);
}

void RocketBuildWidget::SetSkyDirection(int direction)
{
    EC_SkyX *skyx = Component<EC_SkyX>(sky);
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (skyx || meshmoonSky)
    {
        float mod = fmod(static_cast<float>(direction), 360.0f);
        owner->SetAttribute(skyx ? skyx->windDirection : meshmoonSky->windDirection, mod);
    }
}

void RocketBuildWidget::SetSkySpeed(int speed)
{
    EC_SkyX *skyx = Component<EC_SkyX>(sky);
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (skyx || meshmoonSky)
        owner->SetAttribute(skyx ? skyx->windSpeed : meshmoonSky->windSpeed, static_cast<float>(speed));
}

void RocketBuildWidget::SetSkyTimeMultiplier(double value)
{
    EC_SkyX *skyx = Component<EC_SkyX>(sky);
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (skyx || meshmoonSky)
        owner->SetAttribute(skyx ? skyx->timeMultiplier : meshmoonSky->timeMultiplier, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyLatitude(double value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->latitude, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyLongitude(double value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->longitude, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyLightIntensity(double value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->lightSourceBrightness, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyGodRayIntensity(double value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->godRayIntensity, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyGamma(double value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->gamma, static_cast<float>(value));
}

void RocketBuildWidget::SetSkyDate()
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (!meshmoonSky)
        return;

    mainWidget.skyDateEdit->blockSignals(true);

    bool process = true;
    QString dateStr = mainWidget.skyDateEdit->text().trimmed();
    if (!dateStr.isEmpty())
    {
        if (!dateStr.contains(".") && !dateStr.contains("/"))
        {
            LogError(QString("Date attribute '%1' format invalid. Use 'DD.MM.YYYY' or 'DD/MM/YYYY'. Skipping setting time and date.").arg(dateStr));
            process = false;
            mainWidget.skyDateEdit->setText("Invalid date, use 'DD.MM.YYYY'");
        }
        QStringList dateParts = dateStr.split((dateStr.contains(".") ? "." : "/"), QString::SkipEmptyParts);
        if (dateParts.size() < 3)
        {
            LogError(QString("Date attribute '%1' does not contain all needed parts. Use 'DD.MM.YYYY' or 'DD/MM/YYYY'. Skipping setting time and date.").arg(dateStr));
            process = false;
            mainWidget.skyDateEdit->setText("Invalid date, use 'DD.MM.YYYY'");
        }
    }
    if (process)
        owner->SetAttribute(meshmoonSky->date, dateStr);
    
    mainWidget.skyDateEdit->blockSignals(false);
}

void RocketBuildWidget::SetSkyTimeMode(int value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (meshmoonSky)
        owner->SetAttribute(meshmoonSky->timeMode, static_cast<uint>(value));
}

void RocketBuildWidget::SetSkyTimeZone(const QString &value)
{
    EC_MeshmoonSky *meshmoonSky = Component<EC_MeshmoonSky>(sky);
    if (!meshmoonSky)
        return;
        
    EC_MeshmoonSky::TimeZone zone;
    QString v = value.toUpper().trimmed();
    
    // -12 -> -1
    if (v == "DETECT TIME ZONE FROM LONGITUDE")
        zone = EC_MeshmoonSky::USE_LONGITUDE;
    else if (v == "UTC-12")
        zone = EC_MeshmoonSky::IDLW;
    else if (v == "UTC-11")
        zone = EC_MeshmoonSky::NT;
    else if (v == "UTC-10")
        zone = EC_MeshmoonSky::AHST;
    else if (v == "UTC-9")
        zone = EC_MeshmoonSky::YST;
    else if (v == "UTC-8")
        zone = EC_MeshmoonSky::PST;
    else if (v == "UTC-7")
        zone = EC_MeshmoonSky::MST;
    else if (v == "UTC-6")
        zone = EC_MeshmoonSky::CST;
    else if (v == "UTC-5")
        zone = EC_MeshmoonSky::EST;
    else if (v == "UTC-4")
        zone = EC_MeshmoonSky::AST;
    else if (v == "UTC-3")
        zone = EC_MeshmoonSky::BRT;
    else if (v == "UTC-2")
        zone = EC_MeshmoonSky::AT;
    else if (v == "UTC-1")
        zone = EC_MeshmoonSky::WAT;
    // 0
    else if (v == "UTC")
        zone = EC_MeshmoonSky::GMT;
    // +1 -> +12 
    else if (v == "UTC+1")
        zone = EC_MeshmoonSky::CET;
    else if (v == "UTC+2")
        zone = EC_MeshmoonSky::EET;
    else if (v == "UTC+3")
        zone = EC_MeshmoonSky::BT;
    else if (v == "UTC+4")
        zone = EC_MeshmoonSky::GET;
    else if (v == "UTC+5")
        zone = EC_MeshmoonSky::PKT;
    else if (v == "UTC+6")
        zone = EC_MeshmoonSky::BST;
    else if (v == "UTC+7")
        zone = EC_MeshmoonSky::THA;
    else if (v == "UTC+8")
        zone = EC_MeshmoonSky::CCT;
    else if (v == "UTC+9")
        zone = EC_MeshmoonSky::JST;
    else if (v == "UTC+10")
        zone = EC_MeshmoonSky::GST;
    else if (v == "UTC+11")
        zone = EC_MeshmoonSky::SBT;
    else if (v == "UTC+12")
        zone = EC_MeshmoonSky::IDLE;
    else
    {
        LogError("Failed to resolve enum from time zone: " + value);
        return;
    }

    owner->SetAttribute(meshmoonSky->timeZone, static_cast<uint>(zone));
}

QString RocketBuildWidget::GetSkyTimeZoneString(EC_MeshmoonSky::TimeZone zone)
{
    // -12 -> -1
    QString v = "";
    if (zone == EC_MeshmoonSky::USE_LONGITUDE)
        v = "Detect Time Zone from Longitude";
    else if (zone == EC_MeshmoonSky::IDLW)
        v = "UTC-12";
    else if (zone == EC_MeshmoonSky::NT)
        v = "UTC-11";
    else if (zone == EC_MeshmoonSky::AHST)
        v = "UTC-10";
    else if (zone == EC_MeshmoonSky::YST)
        v = "UTC-9";
    else if (zone == EC_MeshmoonSky::PST)
        v = "UTC-8";
    else if (zone == EC_MeshmoonSky::MST)
        v = "UTC-7";
    else if (zone == EC_MeshmoonSky::CST)
        v = "UTC-6";
    else if (zone == EC_MeshmoonSky::EST)
        v = "UTC-5";
    else if (zone == EC_MeshmoonSky::AST)
        v = "UTC-4";
    else if (zone == EC_MeshmoonSky::BRT)
        v = "UTC-3";
    else if (zone == EC_MeshmoonSky::AT)
        v = "UTC-2";
    else if (zone == EC_MeshmoonSky::WAT)
        v = "UTC-1";
    // 0
    else if (zone == EC_MeshmoonSky::GMT)
        v = "UTC";
    // +1 -> +12 
    else if (zone == EC_MeshmoonSky::CET)
        v = "UTC+1";
    else if (zone == EC_MeshmoonSky::EET)
        v = "UTC+2";
    else if (zone == EC_MeshmoonSky::BT)
        v = "UTC+3";
    else if (zone == EC_MeshmoonSky::GET)
        v = "UTC+4";
    else if (zone == EC_MeshmoonSky::PKT)
        v = "UTC+5";
    else if (zone == EC_MeshmoonSky::BST)
        v = "UTC+6";
    else if (zone == EC_MeshmoonSky::THA)
        v = "UTC+7";
    else if (zone == EC_MeshmoonSky::CCT)
        v = "UTC+8";
    else if (zone == EC_MeshmoonSky::JST)
        v = "UTC+9";
    else if (zone == EC_MeshmoonSky::GST)
        v = "UTC+10";
    else if (zone == EC_MeshmoonSky::SBT)
        v = "UTC+11";
    else if (zone == EC_MeshmoonSky::IDLE)
        v = "UTC+12";

    return v;
}

EC_Fog::FogMode RocketBuildWidget::FogMode() const
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    return (ec ? static_cast<EC_Fog::FogMode>(ec->mode.Get()) : EC_Fog::None);
}

void RocketBuildWidget::SetSunlightColor(const QColor &color, bool undoable)
{
    Attribute<Color> *attr = 0;
    if (Component<EC_EnvironmentLight>(environmentLight))
    {
        attr = &Component<EC_EnvironmentLight>(environmentLight)->sunColor;
    }
    else if (Component<EC_SkyX>(environmentLight))
    {
        EC_SkyX *ec = Component<EC_SkyX>(environmentLight);
        if (ec->IsSunVisible())
            attr = &ec->sunlightDiffuseColor;
        else if (ec->IsMoonVisible())
            attr = &ec->moonlightDiffuseColor;
    }

    SetColorInternal(qobject_cast<QColorDialog *>(sender()), attr, color, undoable);
}

void RocketBuildWidget::SetAmbientLightColor(const QColor &color, bool undoable)
{
    Attribute<Color> *attr = 0;
    if (Component<EC_EnvironmentLight>(environmentLight))
        attr = &Component<EC_EnvironmentLight>(environmentLight)->ambientColor;
    else if (Component<EC_SkyX>(environmentLight))
        attr = &Component<EC_SkyX>(environmentLight)->ambientLightColor;

    SetColorInternal(qobject_cast<QColorDialog *>(sender()), attr, color, undoable);
}

void RocketBuildWidget::SetMeshmoonWaterColor(const QColor &color, bool undoable)
{
    SetColorInternal(qobject_cast<QColorDialog *>(sender()), (Component<EC_MeshmoonWater>(water) ? &Component<EC_MeshmoonWater>(water)->waterColor : 0), color, undoable);
}

void RocketBuildWidget::SetLightDiffuseColor(const QColor &color, bool undoable)
{
    SetColorInternal(qobject_cast<QColorDialog *>(sender()), (Component<EC_Light>(activeLight) ? &Component<EC_Light>(activeLight)->diffColor : 0), color, undoable);
}

void RocketBuildWidget::SetLightSpecularColor(const QColor &color, bool undoable)
{
    SetColorInternal(qobject_cast<QColorDialog *>(sender()), (Component<EC_Light>(activeLight) ? &Component<EC_Light>(activeLight)->specColor : 0), color, undoable);
}

void RocketBuildWidget::SetColorInternal(QColorDialog *colorDialog, Attribute<Color> *attr, const Color &color, bool undoable)
{
    if (!colorDialog || !attr)
        return;

    QString sourceButtonObjectName = colorDialog->property("colorButtonName").toString();
    if (!sourceButtonObjectName.trimmed().isEmpty())
        SetButtonColor(mainWidget.frameBuild->findChild<QAbstractButton*>(sourceButtonObjectName), color);

    if (undoable)
    {
        // Set the original value first as disconnected so that we'll have a proper undo action (we already have set the color as the color picking is applied immediately).
        attr->Set(originalColor, AttributeChange::Disconnected);
        owner->SetAttribute(*attr, Color(color));
    }
    else
        attr->Set(color, AttributeChange::Default);
}

void RocketBuildWidget::SetButtonColor(QAbstractButton *button, const Color &color)
{
    if (!button)
        return;

    QColor background = color.ToQColor();
    int test = background.toHsl().lightness();
    QColor foreground = background.alpha() > 100 ? (test <= 150 ? Qt::white : (test > 190 ? QColor(40,40,40) : Qt::black)) : Qt::black;
    button->setStyleSheet(QString("QPushButton { color: rgb(%1,%2,%3); background-color: rgba(%4,%5,%6,%7); }")
        .arg(foreground.red()).arg(foreground.green()).arg(foreground.blue())
        .arg(background.red()).arg(background.green()).arg(background.blue()).arg(background.alpha()));
}

void RocketBuildWidget::SetEnvironmentLightDirectionFromCamera()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    Entity *cameraEntity = scene->Subsystem<OgreWorld>()->Renderer()->MainCamera();
    shared_ptr<EC_Placeable> cameraPlaceable = (cameraEntity ? cameraEntity->Component<EC_Placeable>() : shared_ptr<EC_Placeable>());
    if (!cameraPlaceable)
        return;

    if (Component<EC_EnvironmentLight>(environmentLight))
        owner->SetAttribute(Component<EC_EnvironmentLight>(environmentLight)->sunDirection, cameraPlaceable->WorldOrientation() * scene->ForwardVector());
}

void RocketBuildWidget::SetLightDirectionFromCamera()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    Entity *cameraEntity = scene->Subsystem<OgreWorld>()->Renderer()->MainCamera();
    EC_Placeable *cameraPlaceable = (cameraEntity ? cameraEntity->Component<EC_Placeable>().get() : 0);
    if (!cameraPlaceable)
        return;

    EC_Placeable *lightPlaceable = (!activeLightEntity.expired() ?  activeLightEntity.lock()->Component<EC_Placeable>().get() : 0);
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec && lightPlaceable)
    {
        Frustum f = cameraEntity->Component<EC_Camera>()->ToFrustum();
        /// @todo rocket-BuildEditor()->SetAttribute
        lightPlaceable->SetOrientation(Quat::LookAt(float3::unitZ, f.front, float3::unitY, float3::unitY));
    }
}

void RocketBuildWidget::OpenColorPickerForFogColor()
{
    EC_Fog *ec = Component<EC_Fog>(fog);
    if (ec)
        OpenColorPicker(ec->color.Get(), SLOT(SetFogColorNoUndo(const QColor &)), SLOT(SetFogColorUndo(const QColor &)), SLOT(RestoreFogColor()), sender() ? sender()->objectName() : "");
}

void RocketBuildWidget::OpenColorPickerForSunlightColor()
{
    if (Component<EC_EnvironmentLight>(environmentLight))
        OpenColorPicker(Component<EC_EnvironmentLight>(environmentLight)->sunColor.Get(), SLOT(SetSunlightColorNoUndo(const QColor &)), SLOT(SetSunlightColorUndo(const QColor &)), SLOT(RestoreSunlightColor()), sender() ? sender()->objectName() : "");
    else if (Component<EC_SkyX>(environmentLight))
    {
        EC_SkyX *ec = Component<EC_SkyX>(environmentLight);
        if (ec->IsSunVisible())
            OpenColorPicker(ec->sunlightDiffuseColor.Get(), SLOT(SetSunlightColorNoUndo(const QColor &)), SLOT(SetSunlightColorUndo(const QColor &)), SLOT(RestoreSunlightColor()), sender() ? sender()->objectName() : "");
        else if (ec->IsMoonVisible())
            OpenColorPicker(ec->moonlightDiffuseColor.Get(), SLOT(SetSunlightColorNoUndo(const QColor &)), SLOT(SetSunlightColorUndo(const QColor &)), SLOT(RestoreSunlightColor()), sender() ? sender()->objectName() : "");
    }
}

void RocketBuildWidget::OpenColorPickerForAmbientLightColor()
{
    if (Component<EC_EnvironmentLight>(environmentLight))
        OpenColorPicker(Component<EC_EnvironmentLight>(environmentLight)->ambientColor.Get(), SLOT(SetAmbientLightColorNoUndo(const QColor &)), SLOT(SetAmbientLightColorUndo(const QColor &)), SLOT(RestoreAmbientLightColor()), sender() ? sender()->objectName() : "");
    else if (Component<EC_SkyX>(environmentLight))
        OpenColorPicker(Component<EC_SkyX>(environmentLight)->ambientLightColor.Get(), SLOT(SetAmbientLightColorNoUndo(const QColor &)), SLOT(SetAmbientLightColorUndo(const QColor &)), SLOT(RestoreAmbientLightColor()), sender() ? sender()->objectName() : "");
}

void RocketBuildWidget::OpenColorPickerForLightDiffuseColor()
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        OpenColorPicker(ec->diffColor.Get(), SLOT(SetLightDiffuseColorNoUndo(const QColor &)), SLOT(SetLightDiffuseColorUndo(const QColor &)), SLOT(RestoreLightDiffuseColor()), sender() ? sender()->objectName() : "");
}

void RocketBuildWidget::OpenColorPickerForLightSpecularColor()
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        OpenColorPicker(ec->specColor.Get(), SLOT(SetLightSpecularColorNoUndo(const QColor &)), SLOT(SetLightSpecularColorUndo(const QColor &)), SLOT(RestoreLightSpecularColor()), sender() ? sender()->objectName() : "");
}

void RocketBuildWidget::OpenColorPickerForMeshmoonWater()
{
    EC_MeshmoonWater *ec = Component<EC_MeshmoonWater>(water);
    if (ec)
        OpenColorPicker(ec->waterColor.Get(), SLOT(SetMeshmoonWaterColorNoUndo(const QColor &)), SLOT(SetMeshmoonWaterColorUndo(const QColor &)), SLOT(RestoreMeshmoonWaterColor()), sender() ? sender()->objectName() : "");
}

void RocketBuildWidget::RefreshTerrainSettings()
{
    /// @todo
}

void RocketBuildWidget::RefreshActiveTeleportSettings()
{
    EntityPtr selectedEntity = activeEntity.lock();
    EntityPtr entity = activeTeleportEntity.lock();
    mainWidget.addTeleportToEntityButton->setEnabled(entity.get() == 0 && selectedEntity.get() != 0);
    mainWidget.removeTeleportButton->setEnabled(entity.get() != 0);
    mainWidget.teleportSettingsPanel->setVisible(entity.get() != 0);
    if (!entity)
    {
        lastClickedListItem = 0;
        mainWidget.teleportsListWidget->clearSelection();
        return;
    }

    EC_MeshmoonTeleport *ec = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (!ec)
        return;
        
    ec->setProperty("buildModeUpdatingInterface", true);
        
    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.teleportTriggerEntityLineEdit << mainWidget.teleportDestinationLineEdit << mainWidget.teleportDestinationNameLineEdit
        << mainWidget.telePosXSpinBox << mainWidget.telePosYSpinBox << mainWidget.telePosZSpinBox
        << mainWidget.teleRotXSpinBox << mainWidget.teleRotYSpinBox << mainWidget.teleRotZSpinBox
        << mainWidget.teleportProximitySpinBox << mainWidget.confirmTeleportCheckBox 
        << mainWidget.teleportOnProximityCheckBox << mainWidget.teleportOnClickCheckBox;

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true);

    mainWidget.teleportProximitySpinBox->setEnabled(ec->teleportOnProximity.Get());
    mainWidget.teleportProximitySpinBox->setValue(ec->teleportProximity.Get());
    
    EntityReference triggerRef = ec->triggerEntity.Get();
    mainWidget.teleportTriggerEntityLineEdit->setText(triggerRef.ref);
    mainWidget.teleportDestinationLineEdit->setText(ec->destinationScene.Get());
    mainWidget.teleportDestinationNameLineEdit->setText(ec->destinationName.Get());
    
    mainWidget.confirmTeleportCheckBox->setChecked(ec->confirmTeleport.Get());
    mainWidget.teleportOnProximityCheckBox->setChecked(ec->teleportOnProximity.Get());
    mainWidget.teleportOnClickCheckBox->setChecked(ec->teleportOnLeftClick.Get());
    
    float3 pos = ec->destinationPos.Get();
    mainWidget.telePosXSpinBox->setValue(pos.x); mainWidget.telePosYSpinBox->setValue(pos.y); mainWidget.telePosZSpinBox->setValue(pos.z);
    float3 rot = ec->destinationRot.Get();
    mainWidget.teleRotXSpinBox->setValue(rot.x); mainWidget.teleRotYSpinBox->setValue(rot.y); mainWidget.teleRotZSpinBox->setValue(rot.z);
    
    // Validate trigger entity
    ScenePtr scene = Scene();
    bool valid = (!triggerRef.ref.isEmpty() && scene.get() ? (triggerRef.Lookup(scene.get()).get() != 0) : true);
    mainWidget.teleportTriggerEntityLineEdit->setStyleSheet(QString("color: %1;").arg((valid ? "black" : "red")));

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false); 
        
    ec->setProperty("buildModeUpdatingInterface", false);
}

void RocketBuildWidget::OnTeleportDestinationAttributeChanged(IAttribute *attribute, AttributeChange::Type)
{
    if (!attribute || attribute->Id() != "transform")
        return;

    // Update dest in teleport from manipulator placeable
    Attribute<Transform> *transform = dynamic_cast<Attribute<Transform>* >(attribute);
    EC_MeshmoonTeleport *ec = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (ec && transform)
    {
        float3 destPos = transform->Get().pos;
        if (!ec->destinationPos.Get().Equals(destPos))
            ec->destinationPos.Set(destPos, AttributeChange::Default);
        float3 destRot = transform->Get().rot;
        if (!ec->destinationRot.Get().Equals(destRot))
            ec->destinationRot.Set(destRot, AttributeChange::Default);
    }
}

void RocketBuildWidget::OnTeleportAttributeChanged(IAttribute *attribute, AttributeChange::Type)
{
    EC_MeshmoonTeleport *ec = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (!ec)
        return;

    // Update manipulator placeable from teleport dest
    if (attribute && (attribute->Id() == "destinationPos" || attribute->Id() == "destinationRot") && !activeTeleportDestinationEntity.expired())
    {
        Attribute<float3> *destPos = (attribute->Id() == "destinationPos" ? dynamic_cast<Attribute<float3>* >(attribute) : 0);
        Attribute<float3> *destRot = (attribute->Id() == "destinationRot" ? dynamic_cast<Attribute<float3>* >(attribute) : 0);
        EC_Placeable *destPlaceable = activeTeleportDestinationEntity.lock()->Component<EC_Placeable>().get();
        if (destPlaceable)
        {
            Transform t = destPlaceable->transform.Get();
            if (destPos && !t.pos.Equals(destPos->Get()))
                destPlaceable->SetPosition(destPos->Get());
            else if (destRot && !t.rot.Equals(destRot->Get()))
            {
                t.rot = destRot->Get();
                destPlaceable->transform.Set(t, AttributeChange::Default);
            }
        }
    }
        
    bool updating = ec->property("buildModeUpdatingInterface").toBool();
    if (!updating)
        RefreshActiveTeleportSettings();
}

void RocketBuildWidget::RefreshActiveLightSettings()
{
    EntityPtr entity = activeLightEntity.lock();
    bool lightSelected = (entity.get() != 0);
    mainWidget.removeLightButton->setEnabled(lightSelected);
    mainWidget.cloneLightButton->setEnabled(lightSelected);
    mainWidget.lightSettingsWidget->setVisible(lightSelected);
    mainWidget.lightSettingsTitle->setVisible(lightSelected);
    if (!lightSelected)
    {
        lastClickedListItem = 0;
        mainWidget.lightListWidget->clearSelection();
        return;
    }

    EC_Light *ec = Component<EC_Light>(activeLight);
    shared_ptr<EC_Placeable> placeable = entity->Component<EC_Placeable>();
    if (!ec)
    {
        LogError("RocketBuildEditor: Light " + entity->ToString() + " is missing the Light component!");
        return;
    }
    if (!placeable)
    {
        LogError("RocketBuildEditor: Light " + entity->ToString() +  " is missing the Placeable component!");
        return;
    }
    
    QList<QWidget*> suppressSignals;
    suppressSignals << mainWidget.lightTypeComboBox << mainWidget.lightCastShadowsButton << mainWidget.lightRangeSpinBox
                    << mainWidget.lightBrightnessSpinBox << mainWidget.lightInnerAngleSpinBox << mainWidget.lightOuterAngleSpinBox;

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(true); 

    SetButtonColor(mainWidget.lightDiffuseColorButton, ec->diffColor.Get());
    SetButtonColor(mainWidget.lightSpecularColorButton, ec->specColor.Get());
    
    EC_Light::Type lightType = static_cast<EC_Light::Type>(ec->type.Get());
    mainWidget.lightTypeComboBox->setCurrentIndex(lightType);
    mainWidget.lightCastShadowsButton->setVisible(lightType == EC_Light::DirectionalLight);
    mainWidget.labelCastShadows->setVisible(lightType == EC_Light::DirectionalLight);
    mainWidget.lightCastShadowsButton->setChecked(ec->castShadows.Get());
    mainWidget.lightCastShadowsButton->setText(ec->castShadows.Get() ? tr("Enabled") : tr("Disabled"));
    mainWidget.lightRangeSpinBox->setValue(ec->range.Get());
    mainWidget.lightBrightnessSpinBox->setValue(ec->brightness.Get());
    mainWidget.lightInnerAngleSpinBox->setValue(ec->innerAngle.Get());
    mainWidget.lightOuterAngleSpinBox->setValue(ec->outerAngle.Get());

    const bool isSpotlight = (lightType == EC_Light::Spotlight);
    const bool isPointLight = (lightType == EC_Light::PointLight);
    mainWidget.lightInnerAngleLabel->setVisible(isSpotlight);
    mainWidget.lightInnerAngleSpinBox->setVisible(isSpotlight);
    mainWidget.lightOuterAngleLabel->setVisible(isSpotlight);
    mainWidget.lightOuterAngleSpinBox->setVisible(isSpotlight);
    mainWidget.lightDirectionLabel->setVisible(!isPointLight);
    mainWidget.setLightDirectionFromCameraButton->setVisible(!isPointLight);

    foreach (QWidget *w, suppressSignals)
        w->blockSignals(false); 
}

void RocketBuildWidget::SetActiveTeleportFromSelection(QListWidgetItem *selected, QListWidgetItem *previous)
{
    if (!selected || !selected->isSelected() || selected == lastClickedListItem)
        return;

    // A new item has been selected without use clicking it first,
    // eg. when remove/add button is used.
    SetActiveTeleport(selected);
}

void RocketBuildWidget::SetActiveTeleport(QListWidgetItem *item)
{
    // QListWidget's selection modes suck as there's no single selection with toggling, so must hack it this way.
    if (item == lastClickedListItem)
    {
        mainWidget.teleportsListWidget->blockSignals(true);
        item->setSelected(false);
        mainWidget.teleportsListWidget->blockSignals(false);
        lastClickedListItem = 0;
    }
    else
        lastClickedListItem = item;

    EntityPtr select = item->isSelected() ? static_cast<EntityListWidgetItem *>(item)->Entity() : EntityPtr();
    bool wasSelected = (select.get() && select == owner->ActiveEntity());
    owner->SetSelection(select);
    if (wasSelected)
        SetActiveTeleportEntity(select);
}

void RocketBuildWidget::SetActiveTeleportEntity(const EntityPtr &entity)
{
    EC_MeshmoonTeleport *tele = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (tele)
    {
        disconnect(tele, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), 
            this, SLOT(OnTeleportAttributeChanged(IAttribute*, AttributeChange::Type)));
    }
    
    activeTeleportEntity = (entity && entity->Component<EC_MeshmoonTeleport>() ? entity : EntityPtr());
    activeTeleport = (entity ? entity->Component<EC_MeshmoonTeleport>() : ComponentWeakPtr());
    
    // Create teleport destination visualization entity
    tele = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (tele)
    {
        // Teleport dest entity
        if (activeTeleportDestinationEntity.expired())
        {
            activeTeleportDestinationEntity = Scene()->CreateLocalEntity(QStringList() << "Name" << "Placeable",
                AttributeChange::LocalOnly, false, true);
            activeTeleportDestinationEntity.lock()->SetName("BuildModeTeleportDestination");
        }
        // Placeable
        EntityPtr destEnt = activeTeleportDestinationEntity.lock();
        EC_Placeable *destPlaceable = destEnt->Component<EC_Placeable>().get();
        if (destPlaceable)
        {
            Transform t = destPlaceable->transform.Get();
            t.pos = tele->destinationPos.Get();
            t.rot = tele->destinationRot.Get();
            destPlaceable->transform.Set(t, AttributeChange::Default); 
            connect(destPlaceable, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), 
                this, SLOT(OnTeleportDestinationAttributeChanged(IAttribute*, AttributeChange::Type)), Qt::UniqueConnection);
        }
        // Transform editor (gizmo)
        if (!teleportTransformEditor)
        {
            teleportTransformEditor = MAKE_SHARED(TransformEditor, Scene());
            teleportTransformEditor->SetUserInterfaceEnabled(false);
            teleportTransformEditor->SetGizmoUsesLocalAxes(true);
            teleportTransformEditor->SetDragPlaceableAxisEnabled(false);
        }
        teleportTransformEditor->SetSelection(destEnt);
        teleportTransformEditor->FocusGizmoPivotToAabbCenter();
        teleportTransformEditor->SetGizmoVisible(true);
    }
    else
    {
        // Destroy teleport dest related entities/editors
        if (teleportTransformEditor.get())
            teleportTransformEditor.reset();
            
        EntityPtr destEnt = activeTeleportDestinationEntity.lock();
        if (destEnt.get())
        {
            EC_Placeable *destPlaceable = destEnt->Component<EC_Placeable>().get();
            disconnect(destPlaceable, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), 
                this, SLOT(OnTeleportDestinationAttributeChanged(IAttribute*, AttributeChange::Type)));
                
            entity_id_t destId = destEnt->Id();
            destEnt.reset();
            activeTeleportDestinationEntity.reset();
            Scene()->RemoveEntity(destId);
        }
    }
    
    // The selection of this entity might have been in the 3D view by mouse click, in this case
    // the list item for this teleport is not selected, do it here. Don't use SetActiveTeleport
    // or it can create a infinite loop.
    QList<QListWidgetItem*> selected = mainWidget.teleportsListWidget->selectedItems();
    if (!activeTeleportEntity.expired() && selected.isEmpty())
    {
        for(int i = 0; i < mainWidget.teleportsListWidget->count(); ++i)
        {
            EntityListWidgetItem *item = static_cast<EntityListWidgetItem *>(mainWidget.teleportsListWidget->item(i));
            if (item && item->Entity() == activeTeleportEntity.lock())
            {
                lastClickedListItem = 0;
                mainWidget.teleportsListWidget->setCurrentItem(item, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Clear);
                break;
            }
        }
    }

    RefreshActiveTeleportSettings();

    if (tele)
    {
        connect(tele, SIGNAL(AttributeChanged(IAttribute*, AttributeChange::Type)), 
            this, SLOT(OnTeleportAttributeChanged(IAttribute*, AttributeChange::Type)), Qt::UniqueConnection);
    }
}

void RocketBuildWidget::SetActiveLightFromSelection(QListWidgetItem *selected, QListWidgetItem *previous)
{
    if (!selected || !selected->isSelected() || selected == lastClickedListItem)
        return;

    // A new item has been selected without use clicking it first,
    // eg. when remove/add button is used.
    SetActiveLight(selected);
}

void RocketBuildWidget::SetActiveLight(QListWidgetItem *item)
{
    // QListWidget's selection modes suck as there's no single selection with toggling, so must hack it this way.
    if (item == lastClickedListItem)
    {
        mainWidget.lightListWidget->blockSignals(true);
        item->setSelected(false);
        mainWidget.lightListWidget->blockSignals(false);
        lastClickedListItem = 0;
    }
    else
        lastClickedListItem = item;

    owner->SetSelection(item->isSelected() ? static_cast<EntityListWidgetItem *>(item)->Entity() : EntityPtr());
}

void RocketBuildWidget::SetActiveLightEntity(const EntityPtr &entity)
{
    activeLightEntity = (entity && entity->Component<EC_Light>() ? entity : EntityPtr());
    activeLight = (entity ? entity->Component<EC_Light>() : ComponentWeakPtr());

    RefreshActiveLightSettings();
}

void RocketBuildWidget::FocusCameraToLight(QListWidgetItem *lightItem)
{
    FocusCameraToEntity(static_cast<EntityListWidgetItem *>(lightItem)->Entity());
}

void RocketBuildWidget::AddNewTeleportToSelectedEntity()
{
    EntityPtr ent = activeEntity.lock();
    if (!ent.get())
        return;
        
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);
    
    AddNewTeleport(scene, ent, false);
    
    ConnectToScene(scene);
}

void RocketBuildWidget::AddNewTeleport()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    EntityPtr ent = scene->CreateEntity();
    AddNewTeleport(scene, ent, true);

    ConnectToScene(scene);
}

void RocketBuildWidget::AddNewTeleport(ScenePtr &scene, EntityPtr &target, bool setupPlaceable)
{
    // Construct unique name, apply artificial limit of 10 000 to avoid voodoo bugs
    if (target->Name().isEmpty())
    {
        const QString lightBaseName = "Teleport";
        QString newLightName = lightBaseName;
        for(int i = 2; i < 10000; ++i)
        {
            if (scene->IsUniqueName(newLightName))
                break;
            newLightName = QString("%1 (%2)").arg(lightBaseName).arg(i);
        }
        target->SetName(newLightName);
    }

    shared_ptr<EC_Placeable> placeable = (setupPlaceable ? target->GetOrCreateComponent<EC_Placeable>() : target->Component<EC_Placeable>());

    shared_ptr<EC_MeshmoonTeleport> teleport = target->CreateComponent<EC_MeshmoonTeleport>();
    teleport->teleportOnProximity.Set(true, AttributeChange::Default);
    teleport->IgnoreCurrentProximity();
    
    // If this a newly created teleport, make the target different from the source.
    // For existing placeable, just the camera pos + some y for avatar to safely land on physics.
    float3 teleportToPos = CameraPosition(ogreWorld.lock()->Renderer()->MainCamera(), true);
    if (setupPlaceable)
        teleportToPos.Add(float3(0.f, static_cast<float>(teleport->teleportProximity.Get())*1.5f, 0.f)); /// @todo Make this in front of the teleport in the cam dir!
    else
        teleportToPos.Add(float3(0.0,2.0,0.0));
    teleport->destinationPos.Set(teleportToPos, AttributeChange::Default);

    if (setupPlaceable && placeable.get())
        placeable->SetPosition(CameraPosition(ogreWorld.lock()->Renderer()->MainCamera(), true));

    AddTeleport(teleport, true);
}

void RocketBuildWidget::CreateNewLight()
{
    ScenePtr scene = Scene();
    if (!scene)
        return;

    DisconnectFromScene(scene);

    EntityPtr newLightEntity = scene->CreateEntity();

    // Construct unique name, apply artificial limit of 10 000 to avoid voodoo bugs 
    const QString lightBaseName = "Light";
    QString newLightName = lightBaseName;
    for(int i = 2; i < 10000; ++i)
    {
        if (scene->IsUniqueName(newLightName))
            break;
        newLightName = QString("%1 (%2)").arg(lightBaseName).arg(i);
    }
    newLightEntity->SetName(newLightName);

    shared_ptr<EC_Placeable> lightPlaceable = newLightEntity->CreateComponent<EC_Placeable>();
    shared_ptr<EC_Light> light = newLightEntity->CreateComponent<EC_Light>();
    AddLight(light, true);

    lightPlaceable->SetPosition(ComputePositionInFrontOfCamera(ogreWorld.lock()->Renderer()->MainCamera(), owner->BlockPlacer()->MaxBlockDistance()));

    ConnectToScene(scene);
}

void RocketBuildWidget::RemoveCurrentlyActiveTeleport()
{
    EntityListWidgetItem *currentItem = static_cast<EntityListWidgetItem *>(mainWidget.teleportsListWidget->currentItem());
    if (currentItem)
    {
        // Only remove the parent entity if there are only teleport
        // related components: Name + Placeable + MeshmoonTeleport.
        RemoveTeleport(currentItem->Entity(), (currentItem->Entity()->NumComponents() <= 3));
    }
}

void RocketBuildWidget::ExecuteCurrentlyActiveTeleport()
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        teleport->TeleportNow();
}

void RocketBuildWidget::SetTelePosX(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 pos = teleport->destinationPos.Get();
        pos.x = static_cast<float>(v);
        SetTelePos(pos);
    }
}

void RocketBuildWidget::SetTelePosY(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 pos = teleport->destinationPos.Get();
        pos.y = static_cast<float>(v);
        SetTelePos(pos);
    }
}

void RocketBuildWidget::SetTelePosZ(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 pos = teleport->destinationPos.Get();
        pos.z = static_cast<float>(v);
        SetTelePos(pos);
    }
}

void RocketBuildWidget::SetTelePos(const float3 &pos)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->destinationPos, pos);
}

void RocketBuildWidget::SetTeleRotX(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 rot = teleport->destinationRot.Get();
        rot.x = static_cast<float>(v);
        SetTeleRot(rot);
    }
}

void RocketBuildWidget::SetTeleRotY(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 rot = teleport->destinationRot.Get();
        rot.y = static_cast<float>(v);
        SetTeleRot(rot);
    }
}

void RocketBuildWidget::SetTeleRotZ(double v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
    {
        float3 rot = teleport->destinationRot.Get();
        rot.z = static_cast<float>(v);
        SetTeleRot(rot);
    }
}

void RocketBuildWidget::SetTeleRot(const float3 &rot)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->destinationRot, rot);
}

void RocketBuildWidget::SetTeleProximity(int v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->teleportProximity, static_cast<uint>(v));
}

void RocketBuildWidget::SetTeleConfirm(bool v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->confirmTeleport, v);
}

void RocketBuildWidget::SetTeleOnProximity(bool v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->teleportOnProximity, v);
}

void RocketBuildWidget::SetTeleOnClick(bool v)
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->teleportOnLeftClick, v);
}

void RocketBuildWidget::SetTeleTriggerEntity()
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (!teleport)
        return;
    
    bool valid = true;
    ScenePtr scene = Scene();
    
    EntityReference ref(mainWidget.teleportTriggerEntityLineEdit->text());
    owner->SetAttribute(teleport->triggerEntity, ref);
    if (scene)
        valid = (!ref.ref.isEmpty() && scene.get() ? (ref.Lookup(scene.get()).get() != 0) : true);
    mainWidget.teleportTriggerEntityLineEdit->setStyleSheet(QString("color: %1;").arg((valid ? "black" : "red")));
}

void RocketBuildWidget::SetTeleDestinationSpace()
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->destinationScene, mainWidget.teleportDestinationLineEdit->text().trimmed());
}

void RocketBuildWidget::SetTeleDestinationName()
{
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    if (teleport)
        owner->SetAttribute(teleport->destinationName, mainWidget.teleportDestinationNameLineEdit->text());
}

void RocketBuildWidget::CloneCurrentlyActiveLight()
{
    EntityListWidgetItem *currentItem = static_cast<EntityListWidgetItem *>(mainWidget.lightListWidget->currentItem());
    if (currentItem && currentItem->Entity())
    {
        QString name = currentItem->Entity()->Name();
        if (name.isEmpty()) name = "Light";
        EntityPtr ent = currentItem->Entity()->Clone(false, false, name);
        AddLight(ent->Component(EC_Light::ComponentTypeId), true);
    }
}

void RocketBuildWidget::RemoveCurrentlyActiveLight()
{
    EntityListWidgetItem *currentItem = static_cast<EntityListWidgetItem *>(mainWidget.lightListWidget->currentItem());
    if (currentItem && currentItem->Entity())
    {
        // Only remove the parent entity if there are only light
        // related components: Name + Placeable + Light.
        RemoveLight(currentItem->Entity(), (currentItem->Entity()->NumComponents() <= 3));
    }
}

void RocketBuildWidget::RefreshCurrentObjectSelection()
{
    if (!owner->BuildTransformEditor())
        return;

    EntityList selection;
    if (buildMode == LightsMode && !mainWidget.lightListWidget->selectedItems().empty())
    {
        foreach(QListWidgetItem *selected, mainWidget.lightListWidget->selectedItems())
            selection.push_back(static_cast<EntityListWidgetItem *>(selected)->Entity());
    }
    else
        selection = owner->Selection();

    const bool hasSelection = !selection.empty();
    owner->BuildTransformEditor()->SetSelection(selection);
    owner->BuildTransformEditor()->FocusGizmoPivotToAabbCenter();
    owner->BuildTransformEditor()->SetGizmoVisible(hasSelection);

    if (contextWidget)
    {
        contextWidget->SetVisible(ShouldShowContextWidget() ? hasSelection : false);
        
        RepositionSceneAxisOverlay();

        QPointer<QWidget> gizmoSettings = qobject_cast<QWidget*>(owner->BuildTransformEditor()->EditorSettingsWidget());
        QComboBox *axisComboBox = (!gizmoSettings.isNull() ? gizmoSettings->findChild<QComboBox*>("axisComboBox") : 0);
        QComboBox *modeComboBox = (!gizmoSettings.isNull() ? gizmoSettings->findChild<QComboBox*>("modeComboBox") : 0);
        if (gizmoSettings) // Update context widget
        {
            if (axisComboBox && modeComboBox)
            {
                if (hasSelection)
                {
                    // Steal and customize the setting widgets.
                    QLabel *label = gizmoSettings->findChild<QLabel*>("axisLabel");
                    if (label)
                        label->setText("");
                    label = gizmoSettings->findChild<QLabel*>("modeLabel");
                    if (label)
                        label->setText("");
                    if (axisComboBox)
                        axisComboBox->setToolTip(tr("Manipulation axes"));
                    if (modeComboBox)
                        modeComboBox->setToolTip(tr("Manipulation mode"));

                    contextWidget->mainWidget.gizmoSettingsLayout->addWidget(axisComboBox);
                    contextWidget->mainWidget.gizmoSettingsLayout->addWidget(modeComboBox);
#ifdef __APPLE__
                    axisComboBox->setEditable(true);
                    modeComboBox->setEditable(true);
                    axisComboBox->setInsertPolicy(QComboBox::NoInsert);
                    modeComboBox->setInsertPolicy(QComboBox::NoInsert);
                    axisComboBox->lineEdit()->setStyleSheet("QLineEdit { background-color:  rgba(215, 215, 215, 150); }");
                    modeComboBox->lineEdit()->setStyleSheet("QLineEdit { background-color:  rgba(215, 215, 215, 150); }");
#endif
                }
                else
                {
                    contextWidget->mainWidget.gizmoSettingsLayout->removeWidget(axisComboBox);
                    contextWidget->mainWidget.gizmoSettingsLayout->removeWidget(modeComboBox);
                }

                axisComboBox->setVisible(hasSelection);
                modeComboBox->setVisible(hasSelection);
            }

            // Always hide gizmoSettings as we've stolen the widgets we're interest in.
            gizmoSettings->setVisible(false);
        }
    }
}

void RocketBuildWidget::UpdateEntityName(IAttribute *attr)
{
    EC_Name *nameComp = qobject_cast<EC_Name*>(sender());
    if (!nameComp || (attr != &nameComp->name) || (nameComp->ParentEntity() == 0))
        return;

    EntityPtr entity = nameComp->ParentEntity()->shared_from_this();
    
    // Light was renamed?
    for(int i = 0; i < mainWidget.lightListWidget->count(); ++i)
    {
        EntityListWidgetItem *item = static_cast<EntityListWidgetItem *>(mainWidget.lightListWidget->item(i));
        if (item->Entity() == entity)
            item->setText(entity->Name());
    }
    // Teleport was renamed?
    for(int i = 0; i < mainWidget.teleportsListWidget->count(); ++i)
    {
        EntityListWidgetItem *item = static_cast<EntityListWidgetItem *>(mainWidget.teleportsListWidget->item(i));
        if (item->Entity() == entity)
            item->setText(entity->Name());
    }
}

void RocketBuildWidget::Update(float frametime)
{
    OgreWorldPtr world = ogreWorld.lock();
    if (!world)
        return;

    if (sceneAxisRenderer && ogreWorld.lock()->Renderer())
    {
        Entity *cameraEnt = ogreWorld.lock()->Renderer()->MainCamera();
        EC_Placeable *placeable = (cameraEnt ? cameraEnt->Component<EC_Placeable>().get() : 0);
        if (placeable)
        {
            float3 pos = placeable->WorldOrientation() * float3(0,0,8.5f);
            sceneAxisRenderer->SetCameraPosition(pos);
            sceneAxisRenderer->SetCameraLookAt(float3::zero);
        }
    }

    // Selected object visualization
    EC_Light *light = Component<EC_Light>(activeLight);
    EC_Placeable *placeable = Component<EC_Placeable>(activePlaceable);
    EC_MeshmoonTeleport *teleport = Component<EC_MeshmoonTeleport>(activeTeleport);
    //EC_Mesh *mesh = Component<EC_Mesh>(activeMesh);
    
    /// @todo optimize (cache) world tm, obb et al.
    if (light && placeable)
        world->DebugDrawLight(placeable->WorldTransform(), light->type.Get(), light->range.Get(), light->outerAngle.Get(), light->diffColor.Get());
    if (teleport && placeable)
    {
        // Proximity trigger
        if (teleport->IsTriggeredWithProximity())
        {
            world->DebugDrawSphere(placeable->WorldPosition(), 
                teleport->IsTriggeredWithProximity() ? teleport->teleportProximity.Get() : 5, 384, Color::Green);
        }

        // Visualize the target pos/rot           
        if (teleport->IsParentSceneTeleport())
        {
            Transform t(teleport->destinationPos.Get(), teleport->destinationRot.Get(), float3(1,1,1));
            AABB aabb(float3::FromScalar(-5/2.f), float3::FromScalar(6/2.f));
            OBB obb = aabb.Transform(t.ToFloat3x4());
            world->DebugDrawOBB(obb, Color::Magenta);
        }
    }
    /*else if (mesh && mesh->HasMesh())
        world->DebugDrawOBB(mesh->WorldOBB(), Color::Red);
    else if (placeable)
        world->DebugDraw*/

    // Selection visualization
    for(std::list<weak_ptr<EC_Mesh> >::iterator it = activeGroup.begin(); it != activeGroup.end();)
    {
        EC_Mesh *mesh = it->lock().get();
        if (mesh)
        {
            if (mesh->HasMesh())
                world->DebugDrawOBB((*it).lock()->WorldOBB(), Color::Red);
            ++it;
        }
        else
            it = activeGroup.erase(it);
    }

    // Block placer
    if (buildMode == BlocksMode && owner->BlockPlacer())
    {
        owner->BlockPlacer()->Update();
        // Extra visualization
        if (mainWidget.blockExtraVisualizationButton->isChecked())
            owner->BlockPlacer()->DrawVisualHelp();
    }
}

void RocketBuildWidget::RefreshPlaceableOptions()
{
    EC_Placeable *p = (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock ?
        Component<EC_Placeable>(activePlaceable) : owner->BlockPlacer()->Placeable().get());
    if (p)
    {
        mainWidget.scaleXSpinBox->blockSignals(true);
        mainWidget.scaleYSpinBox->blockSignals(true);
        mainWidget.scaleZSpinBox->blockSignals(true);

        mainWidget.scaleXSpinBox->setValue(p->transform.Get().scale.x);
        mainWidget.scaleYSpinBox->setValue(p->transform.Get().scale.y);
        mainWidget.scaleZSpinBox->setValue(p->transform.Get().scale.z);

        mainWidget.scaleXSpinBox->blockSignals(false);
        mainWidget.scaleYSpinBox->blockSignals(false);
        mainWidget.scaleZSpinBox->blockSignals(false);
    }
}

void RocketBuildWidget::SetPlaceableScale(EC_Placeable *p, const float3 &scale)
{
    disconnect(p, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), this, SLOT(RefreshPlaceableOptions()));

    Transform t = p->transform.Get();
    t.scale = scale;
    owner->SetAttribute(p->transform, t);

    connect(p, SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)), SLOT(RefreshPlaceableOptions()), Qt::UniqueConnection);
}

void RocketBuildWidget::SetPlaceableScaleX(double x)
{
    EC_Placeable *p = (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock ?
        Component<EC_Placeable>(activePlaceable) : owner->BlockPlacer()->Placeable().get());
    if (p)
    {
        float3 scale = p->transform.Get().scale;
        scale.x = x;
        SetPlaceableScale(p, scale);
    }
}

void RocketBuildWidget::SetPlaceableScaleY(double y)
{
    EC_Placeable *p = (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock ?
        Component<EC_Placeable>(activePlaceable) : owner->BlockPlacer()->Placeable().get());
    if (p)
    {
        float3 scale = p->transform.Get().scale;
        scale.y = y;
        SetPlaceableScale(p, scale);
    }
}

void RocketBuildWidget::SetPlaceableScaleZ(double z)
{
    EC_Placeable *p = (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock ?
        Component<EC_Placeable>(activePlaceable) : owner->BlockPlacer()->Placeable().get());
    if (p)
    {
        float3 scale = p->transform.Get().scale;
        scale.z = z;
        SetPlaceableScale(p, scale);
    }
}

void RocketBuildWidget::SetLightType(int value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        owner->SetAttribute(ec->type, value);

    RefreshActiveLightSettings(); // different light types can have different options
}

void RocketBuildWidget::SetLightCastShadowsEnabled(bool value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
    {
        owner->SetAttribute(ec->castShadows, value);
        mainWidget.lightCastShadowsButton->setText(value ? tr("Enabled") : tr("Disabled"));
    }
}

void RocketBuildWidget::SetLightRange(double value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        owner->SetAttribute(ec->range, static_cast<float>(value));
}

void RocketBuildWidget::SetLightBrightness(double value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        owner->SetAttribute(ec->brightness, static_cast<float>(value));
}

void RocketBuildWidget::SetLightInnerAngle(double value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        owner->SetAttribute(ec->innerAngle, static_cast<float>(value));
}

void RocketBuildWidget::SetLightOuterAngle(double value)
{
    EC_Light *ec = Component<EC_Light>(activeLight);
    if (ec)
        owner->SetAttribute(ec->outerAngle, static_cast<float>(value));
}

void RocketBuildWidget::SetActiveEntity(const EntityList &entities)
{
    activeGroup.clear();
    foreach(const EntityPtr &e, entities)
    {
        shared_ptr<EC_Mesh> m = e->Component<EC_Mesh>();
        if (m)
            activeGroup.push_back(m);
    }

    activeEntity = entities.size() == 1 ? *entities.begin() : EntityPtr();
    EntityPtr entity = activeEntity.lock();

    // Store placeable and mesh for generic debug visualization.
    activePlaceable = (entity ? entity->Component<EC_Placeable>() : ComponentWeakPtr());
    activeMesh = (entity ? entity->Component<EC_Mesh>() : ComponentWeakPtr());

    // Page-specific behavior
    switch(buildMode)
    {
    case BlocksMode:
        if (!activePlaceable.expired())
        {
            RefreshPlaceableOptions();
            connect(activePlaceable.lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
                SLOT(RefreshPlaceableOptions()), Qt::UniqueConnection);
        }
        break;
    case PhysicsMode:
        SetActivePhysicsEntity(entity);
        break;
    case TeleportsMode:
        SetActiveTeleportEntity(entity);
        break;
    case LightsMode:
        SetActiveLightEntity(entity);
        break;
    default:
        break;
    }

    /// @todo Seems a bit wonky, clean up.
    if (buildMode != BlocksMode || (buildMode == BlocksMode && owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::EditBlock) || !entity)
    {
        RefreshCurrentObjectSelection();
        RefreshBlocksPage();
    }

    if (contextWidget)
        contextWidget->SetVisible(ShouldShowContextWidget() ? !entities.empty() : false);
}

bool RocketBuildWidget::ShouldShowContextWidget() const
{
    return !((buildMode == BlocksMode && owner->BlockPlacer()->BlockPlacerMode() != RocketBlockPlacer::EditBlock) || buildMode == ScenePackager);
}

void RocketBuildWidget::SetBlockPlacerMode(int mode)
{
    CloseComponentEditor();
    userSelectedComponents.clear();
    entitysExistingComponents.clear();

    owner->ClearSelection();
    owner->BlockPlacer()->SetBlockPlacerMode(static_cast<RocketBlockPlacer::PlacerMode>(mode));
}

void RocketBuildWidget::SetBlockPlacementMode(int mode)
{
    owner->BlockPlacer()->SetBlockPlacementMode(static_cast<RocketBlockPlacer::PlacementMode>(mode));
}

void RocketBuildWidget::OnPageClosing(BuildMode mode)
{
    const EntityPtr empty;

    switch(mode)
    {
    case EnvironmentMode:
        EnableShadowDebugPanels(false);
        break;
    case TeleportsMode:
        ClearTeleportItems();
        SetActiveTeleportEntity(empty);
        break;
    case LightsMode:
        ClearLightItems();
        SetActiveLightEntity(empty);
        break;
    case PhysicsMode:
        SetActivePhysicsEntity(empty);
        break;
    case BlocksMode:
        if (!activePlaceable.expired())
        {
            disconnect(activePlaceable.lock().get(), SIGNAL(AttributeChanged(IAttribute *, AttributeChange::Type)),
                this, SLOT(RefreshPlaceableOptions()));
        }
        CloseComponentEditor();
        userSelectedComponents.clear();
        entitysExistingComponents.clear();
        owner->BlockPlacer()->Hide();
        break;
    case ScenePackager:
        if (scenePackager)
            scenePackager->Hide();
        break;
    }

    owner->ClearSelection();
    //owner->BlockPlacer()->SetActiveBlock(empty);
}

void RocketBuildWidget::SetMaxBlockDistance(double distance)
{
    owner->BlockPlacer()->SetMaxBlockDistance(static_cast<float>(distance));
}

#if 0
void RocketBuildWidget::ShowBlockFunctionalitySelector()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    bool ok;
    uint submeshIndex = button->property("submeshIndex").toUInt(&ok);
    if (!ok)
        return;

    if (!blockFunctionalityMenu)
    {
        blockFunctionalityMenu = blockFunctionalityMenu = new QMenu(widget);
        QAction *webAction = new QAction(tr("Web Browser..."), blockFunctionalityMenu);
        QAction *mediaAction = new QAction(tr("Media Player..."), blockFunctionalityMenu);
        //QAction *slideShowAction = new QAction(tr("Slide Show..."), blockFunctionalityMenu);
        QAction *clearAction = new QAction(tr("Clear"), blockFunctionalityMenu);

        webAction->setProperty("submeshIndex", submeshIndex);
        mediaAction->setProperty("submeshIndex", submeshIndex);
        clearAction->setProperty("submeshIndex", submeshIndex);

        blockFunctionalityMenu->addAction(webAction);
        blockFunctionalityMenu->addAction(mediaAction);
        //blockFunctionalityMenu->addAction(slideShowAction);
        blockFunctionalityMenu->addSeparator();
        blockFunctionalityMenu->addAction(clearAction);

        connect(webAction, SIGNAL(triggered()), SLOT(AddWebBrowserFunctionality()));
        connect(mediaAction, SIGNAL(triggered()), SLOT(AddMediaBrowserFunctionality()));
        connect(clearAction, SIGNAL(triggered()), SLOT(ClearFunctionality()));
    }

    blockFunctionalityMenu->popup(Fw()->Input()->MousePos());
}

void RocketBuildWidget::AddWebBrowserFunctionality(/*int submeshIndex*/)
{
    /*
    if (submeshIndex < 0)
    {
        LogError("RocketBuildWidget::AddWebBrowserFunctionality: submeshIndex < 0.");
        return;
    }
    */
    QAction *action= qobject_cast<QAction *>(sender());
    if (!action)
        return;

    bool ok;
    uint submeshIndex = action->property("submeshIndex").toUInt(&ok);
    if (!ok)
        return;

    QString url = QInputDialog::getText(Fw()->Ui()->MainWindow(), tr("Enter URL"), tr("Web page URL:"), QLineEdit::Normal, "http://");
    mainWidget.blocksMediaLineEdit->setText(url);

    shared_ptr<EC_WebBrowser> browser = Fw()->Scene()->CreateComponent<EC_WebBrowser>(0);
    browser->url.Set(url, AttributeChange::Disconnected);

    browser->renderSubmeshIndex.Set(submeshIndex, AttributeChange::Disconnected);

    userSelectedComponents.push_back(browser);

    for(int i = 0; i < blockMaterialAssets.size(); ++i)
        if (blockMaterialAssets[i].index == submeshIndex && blockMaterialAssets[i].button2)
        {
            ///@todo placeholder icon
            blockMaterialAssets[i].button2->setIcon(QIcon(":/images/icon-world.png"));
            break;
        }
}

void RocketBuildWidget::AddMediaBrowserFunctionality(/*int submeshIndex*/)
{
    QAction *action= qobject_cast<QAction *>(sender());
    if (!action)
        return;

    bool ok;
    uint submeshIndex = action->property("submeshIndex").toUInt(&ok);
    if (!ok)
        return;
    /*
    if (submeshIndex < 0)
    {
        LogError("RocketBuildWidget::AddMediaBrowserFunctionality: submeshIndex < 0.");
        return;
    }
    */

    QString url = QInputDialog::getText(Fw()->Ui()->MainWindow(), tr("Enter URL"), tr("Media source URL:"), QLineEdit::Normal, "http://");
    mainWidget.blocksWebLineEdit->setText(url);
    /// @todo if (url.startsWith("https", Qt::CaseInsensitive))
    ///           ShowWarningDialog();

    shared_ptr<EC_MediaBrowser> browser = Fw()->Scene()->CreateComponent<EC_MediaBrowser>(0);
    browser->sourceRef.Set(url, AttributeChange::Disconnected);
    browser->renderSubmeshIndex.Set(submeshIndex, AttributeChange::Disconnected);
    userSelectedComponents.push_back(browser);

    for(int i = 0; i < blockMaterialAssets.size(); ++i)
        if (blockMaterialAssets[i].index == submeshIndex && blockMaterialAssets[i].button2)
        {
            ///@todo placeholder icon
            blockMaterialAssets[i].button2->setIcon(QIcon(":/images/icon-next-hover.png"));
            break;
        }
}

/*void RocketBuildWidget::AddSlideShowFunctionality(int submeshIndex)
{
    if (submeshIndex < 0)
    {
        LogError("RocketBuildWidget::AddSlideShowFunctionality: submeshIndex < 0.");
        return;

    ///@todo placeholder icon QIcon(":/images/icon-image.png")
}*/

void RocketBuildWidget::ClearFunctionality(/*int submeshIndex*/)
{
    QAction *action= qobject_cast<QAction *>(sender());
    if (!action)
        return;

    bool ok;
    uint submeshIndex = action->property("submeshIndex").toUInt(&ok);
    if (!ok)
        return;
    /*
    if (submeshIndex < 0)
    {
        LogError("RocketBuildWidget::ClearFunctionality: submeshIndex < 0.");
        return;
    }
    */
    for(int i = 0; i < blockMaterialAssets.size(); ++i)
        if (blockMaterialAssets[i].index == submeshIndex && blockMaterialAssets[i].button2)
        {
            ///@todo placeholder icon
            blockMaterialAssets[i].button2->setIcon(QIcon(":/images/icon-questionmark.png"));
            break;
        }
}

void RocketBuildWidget::OpenScriptPickerForBlocks()
{
    originalAssetReference = blocksScriptRef;
    OpenAssetPicker("Script", SLOT(SetBlocksScript(const AssetPtr &)), SLOT(SetBlocksScript(const AssetPtr&)), SLOT(RestoreOriginalBlocksScript()));
}

void RocketBuildWidget::SetBlocksScript(const AssetPtr &asset)
{
    blocksScriptRef = asset->Name();
    QString subAssetName;
    AssetAPI::ParseAssetRef(blocksScriptRef, 0, 0, 0, 0, 0, 0, &subAssetName);
//    mainWidget.blocksScriptButton->setText(subAssetName);
}

void RocketBuildWidget::RestoreOriginalBlocksScript()
{
//    mainWidget.blocksScriptButton->setText(originalAssetReference);
}
#endif

void RocketBuildWidget::ShowBlocksFunctionalityOptions(bool show)
{
    // Don't show the options in clone mode. They are not filled correctly atm.
    if (owner->BlockPlacer()->BlockPlacerMode() == RocketBlockPlacer::CloneBlock)
        show = false;

    mainWidget.blocksFunctionalityOptionsWidget->setVisible(show);
}

void RocketBuildWidget::CloseComponentEditor()
{
    // This widget will destroy itself on close.
    // Close and reset our QPointer.
    if (componentConfigurationWidget)
    {
        componentConfigurationWidget->close();
        componentConfigurationWidget = 0;
    }
    SAFE_DELETE(buildingComponentMenu);
}

void RocketBuildWidget::ShowComponentEditor(int componentTypeId, bool skipMenu)
{
    QAbstractButton *button = componentButtonGroup->button(componentTypeId);
    if (!button)
        return; 
        
    // If the component is enabled, show "Edit.." and "Remove" options.
    if (!button->isChecked() && !skipMenu)
    {
        CloseComponentEditor();

        buildingComponentMenu = new QMenu();
        
        QSignalMapper *mapper = new QSignalMapper(buildingComponentMenu);
        connect(mapper, SIGNAL(mapped(QObject*)), this, SLOT(AddOrRemoveComponent(QObject*)));

        // Edit..
        QAction *editAction = new QAction(tr("Edit..."), buildingComponentMenu);
        editAction->setProperty("operation", "edit");
        editAction->setProperty("tundraComponentTypeID", componentTypeId);
        buildingComponentMenu->addAction(editAction);
        mapper->setMapping(editAction, editAction);
        connect(editAction, SIGNAL(triggered()), mapper, SLOT(map()));

        // Remove
        QAction *addRemoveAction = new QAction(tr("Remove"), buildingComponentMenu);
        addRemoveAction->setProperty("operation", "remove");
        addRemoveAction->setProperty("tundraComponentTypeID", componentTypeId);
        buildingComponentMenu->addAction(addRemoveAction);
        mapper->setMapping(addRemoveAction, addRemoveAction);
        connect(addRemoveAction, SIGNAL(triggered()), mapper, SLOT(map()));
        
        // Don't accept this click, the menu will do that. Set back to checked.
        button->setChecked(true);
        buildingComponentMenu->popup(QCursor::pos());
        return;
    }

    ComponentPtr comp = AddOrRemoveComponent(button->isChecked(), componentTypeId);     
    if (comp.get())
    {
        if (!componentConfigurationWidget)
            componentConfigurationWidget = new RocketComponentConfigurationDialog(rocket);
        componentConfigurationWidget->SetComponentType(UserFriendlyComponentName(Fw()->Scene()->ComponentTypeNameForTypeId(componentTypeId)), componentTypeId);
        componentConfigurationWidget->SetComponent(comp);
        componentConfigurationWidget->Popup(QCursor::pos());
    }
    else
        CloseComponentEditor();
}

void RocketBuildWidget::AddOrRemoveComponent(QObject *sender)
{
    QString operation = sender->property("operation").toString();
    int componentTypeId = sender->property("tundraComponentTypeID").toInt();

    QAbstractButton *button = componentButtonGroup->button(componentTypeId);
    if (!button)
        return;

    if (operation == "edit")
        ShowComponentEditor(componentTypeId, true);
    else if (operation == "remove")
        AddOrRemoveComponent(false, componentTypeId);
}

ComponentPtr RocketBuildWidget::AddOrRemoveComponent(bool enabled, u32 componentTypeId)
{
    QAbstractButton *button = componentButtonGroup->button(componentTypeId);
    if (!button)
        return ComponentPtr();
        
    const RocketBlockPlacer::PlacerMode placerMode = owner->BlockPlacer()->BlockPlacerMode();

    ComponentPtr component;
    if (placerMode == RocketBlockPlacer::EditBlock)
    {
        Entity *ent = activeEntity.lock().get();
        if (!ent)
            return ComponentPtr();

        if (enabled)
        {
            component = ent->GetOrCreateComponent(componentTypeId);
            entitysExistingComponents[componentTypeId] = component;
        }
        else
        {
            ComponentPtr comp = ent->Component(componentTypeId);
            if (!comp.get())
                return ComponentPtr();

            // Confirmation as we are about to modify a live entity.
            QMessageBox::StandardButton result = QMessageBox::question(Fw()->Ui()->MainWindow(), tr("Component Removal"),
                tr("Are you sure you want to remove %1 component permanently?").arg(UserFriendlyComponentName(comp->TypeName())), 
                QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);

            if (result == QMessageBox::Yes)
            {
                // Clear shared ptr from internal state.
                ComponentMap::iterator it = entitysExistingComponents.find(componentTypeId);
                if (it != entitysExistingComponents.end())
                    entitysExistingComponents.erase(it);

                ent->RemoveComponent(comp);
                comp.reset();
                
                button->setChecked(false);
            }
        }
    }
    else
    {
        ComponentMap::iterator it = userSelectedComponents.find(componentTypeId);
        if (enabled)
        {
            if (it == userSelectedComponents.end())
            {
                component = Fw()->Scene()->CreateComponentById(0, componentTypeId);
                userSelectedComponents[componentTypeId] = component;
            }
            else
                component = it->second;

            button->setChecked(true);
        }
        else
        {
            if (it != userSelectedComponents.end())
                userSelectedComponents.erase(it);

            button->setChecked(false);
        }
    }

    if (!enabled)
        CloseComponentEditor();

    return component;
}

void RocketBuildWidget::EditComponent(int componentTypeId)
{
    ECEditorModule *editorModule = Fw()->Module<ECEditorModule>();
    ECEditorWindow *editor = editorModule->ActiveEditor();
    if (!editor)
    {
        editorModule->ShowEditorWindow();
        editor = editorModule->ActiveEditor();
    }

    QList<entity_id_t> selected(QList<entity_id_t>() << activeEntity.lock()->Id());
    editor->ClearEntities();
    editor->AddEntities(selected, true);
}

void RocketBuildWidget::AddWebBrowserFunctionality(const ComponentPtr &component)
{
    shared_ptr<EC_WebBrowser> browser = static_pointer_cast<EC_WebBrowser>(component);
    if (!browser)
        return;

    QString lastUrl = Fw()->Config()->Read(cBlockLastWebUrl).toString();
    uint lastSubmeshIndex = Fw()->Config()->Read(cBlockLastSubmeshIndex).toUInt();

    QString url = QInputDialog::getText(Fw()->Ui()->MainWindow(), tr("Enter URL"), tr("Web page URL:"), QLineEdit::Normal, lastUrl);
    browser->url.Set(url, AttributeChange::Disconnected);

    const uint numSubmeshes = 2; /**< @todo Proper max value. */
    lastSubmeshIndex = Clamp(lastSubmeshIndex, 0U, numSubmeshes);
    int submeshIndex = QInputDialog::getInt(Fw()->Ui()->MainWindow(), tr("Enter submesh index"), tr("Submesh index:"), 0, 0, numSubmeshes);
    browser->renderSubmeshIndex.Set(submeshIndex, AttributeChange::Disconnected);

    Fw()->Config()->Write(cBlockLastWebUrl, url);
    Fw()->Config()->Write(cBlockLastSubmeshIndex, submeshIndex);
}

void RocketBuildWidget::AddMediaBrowserFunctionality(const ComponentPtr &component)
{
    shared_ptr<EC_MediaBrowser> browser = static_pointer_cast<EC_MediaBrowser>(component);
    if (!browser)
        return;

    QString lastUrl = Fw()->Config()->Read(cBlockLastMediaUrl).toString();
    uint lastSubmeshIndex = Fw()->Config()->Read(cBlockLastSubmeshIndex).toUInt();

    QString url = QInputDialog::getText(Fw()->Ui()->MainWindow(), tr("Enter URL"), tr("Media source URL:"), QLineEdit::Normal, lastUrl);
    if (url.startsWith("https", Qt::CaseInsensitive))
        QMessageBox::information(Fw()->Ui()->MainWindow(), tr("HTTPS Media URL"), tr("Please note that HTTPS media URLs might not work properly."), QMessageBox::Ok);

    browser->sourceRef.Set(url, AttributeChange::Disconnected);

    const uint numSubmeshes = 2; /**< @todo Proper max value. */
    lastSubmeshIndex = Clamp(lastSubmeshIndex, 0U, numSubmeshes);
    int submeshIndex = QInputDialog::getInt(Fw()->Ui()->MainWindow(), tr("Enter submesh index"), tr("Submesh index:"), lastSubmeshIndex, 0, numSubmeshes);
    browser->renderSubmeshIndex.Set(submeshIndex, AttributeChange::Disconnected);

    Fw()->Config()->Write(cBlockLastMediaUrl, url);
    Fw()->Config()->Write(cBlockLastSubmeshIndex, submeshIndex);
}

void RocketBuildWidget::SetPinned(bool pinned)
{
    if (mainWidget.pinBuildMode->isChecked() != pinned)
    {
        mainWidget.pinBuildMode->blockSignals(true);
        mainWidget.pinBuildMode->setChecked(pinned);
        mainWidget.pinBuildMode->blockSignals(false);
    }
    mainWidget.pinBuildMode->setToolTip(pinned ? "Uncheck to enable automatic hiding " : "Check to disable automatic hiding ");
    Fw()->Config()->Write(cBuildModePinned, mainWidget.pinBuildMode->isChecked());
}

bool RocketBuildWidget::IsPinned() const
{
    return mainWidget.pinBuildMode->isChecked();
}
