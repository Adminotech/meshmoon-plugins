/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketMaterialEditor.h"
#include "RocketTextureEditor.h"
#include "RocketOgreSceneRenderer.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "common/MeshmoonCommon.h"
#include "MeshmoonAsset.h"
#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "Framework.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "IAssetTypeFactory.h"
#include "FrameAPI.h"

#include "OgreMaterialAsset.h"
#include "TextureAsset.h"

#include <OgreEntity.h>
#include <OgreViewport.h>
#include <OgreLight.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreResourceManager.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>

#include <QTextStream>
#include <QVector2D>
#include <QVector4D>
#include <QSignalMapper>
#include <QDesktopServices>

namespace
{
    const std::string cNamedParameterKey("namedParameter");
    const std::string cShaderTypeKey("shaderType");

    const QString cShaderTypeVertex("vertexShader");
    const QString cShaderTypePixel("pixelShader");
}

RocketMaterialEditor::RocketMaterialEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource) :
    IRocketAssetEditor(plugin, resource, "Material Editor", "Ogre Material Script")
{
    Initialize();
}

void RocketMaterialEditor::Initialize()
{
    sceneRenderer_ = 0;
    previewMeshEntity_ = 0;

    colorMenu_ = 0;
    increaseColorMenu_ = 0;
    decreaseColorMenu_ = 0;

    InitializeUi();
}

void RocketMaterialEditor::InitializeUi()
{
    /** @todo Handle situation where the editor is opened with generated material
        but then a ref is added to the scene aka. its loaded to the system. At
        this point we need to update our material weak ptr to point to it! */

    editor_ = new QWidget(this);
    ui_.setupUi(editor_);

    connect(ui_.prefabCube, SIGNAL(clicked()), SLOT(CreateCubePreview()));
    connect(ui_.prefabSphere, SIGNAL(clicked()), SLOT(CreateSpherePreview()));
    connect(ui_.prefabPlane, SIGNAL(clicked()), SLOT(CreatePlanePreview()));

    /// @todo This prefab plane is one sided, get a better mesh and enable back.
    ui_.prefabPlane->hide();

    // Help button
    connect(ui_.helpMaterials, SIGNAL(clicked()), SLOT(OnHelpMaterials()));

    // Pass selection
    connect(ui_.passComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnPassChanged(int)));

    // Colors
    QSignalMapper *colorMapper = new QSignalMapper(this);
    colorMapper->setMapping(ui_.buttonAmbient, static_cast<int>(AmbientColor));
    colorMapper->setMapping(ui_.buttonDiffuse, static_cast<int>(DiffuseColor));
    colorMapper->setMapping(ui_.buttonSpecular, static_cast<int>(SpecularColor));
    colorMapper->setMapping(ui_.buttonEmissive, static_cast<int>(EmissiveColor));
    connect(colorMapper, SIGNAL(mapped(int)), SLOT(OnPickColor(int)));

    ui_.buttonAmbient->setProperty("colorType", AmbientColor);
    ui_.buttonDiffuse->setProperty("colorType", DiffuseColor);
    ui_.buttonSpecular->setProperty("colorType", SpecularColor);
    ui_.buttonEmissive->setProperty("colorType", EmissiveColor);
    ui_.buttonAmbient->installEventFilter(this);
    ui_.buttonDiffuse->installEventFilter(this);
    ui_.buttonSpecular->installEventFilter(this);
    ui_.buttonEmissive->installEventFilter(this);
    connect(ui_.buttonAmbient, SIGNAL(clicked()), colorMapper, SLOT(map()));
    connect(ui_.buttonDiffuse, SIGNAL(clicked()), colorMapper, SLOT(map()));
    connect(ui_.buttonSpecular, SIGNAL(clicked()), colorMapper, SLOT(map()));
    connect(ui_.buttonEmissive, SIGNAL(clicked()), colorMapper, SLOT(map()));
        
    // Blending
    QStringList blendModes;
    blendModes << "One" << "Zero" << "Destination Color" << "Source Color" 
               << "One Minus Destination Color" << "One Minus Source Color"
               << "Destination Alpha" << "Source Alpha" 
               << "One Minus Destination Alpha" << "One Minus Source Alpha";
    ui_.blendSourceComboBox->addItems(blendModes);
    ui_.blendDestComboBox->addItems(blendModes);
    
    QStringList cullingModes;
    cullingModes << "None" << "Clockwise" << "Anticlockwise";
    ui_.cullingComboBox->addItems(cullingModes);

    QStringList compareFunctions;
    compareFunctions << "Always fail" << "Always pass" << "Less than" << "Less than or equal to"
                     << "Equals" << "Not equal to" << "Greater than or equal to" << "Greater than";
    ui_.alphaRejectionComboBox->addItems(compareFunctions);

    connect(ui_.blendSourceComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnSourceBlendingChanged(int)));
    connect(ui_.blendDestComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnDestinationBlendingChanged(int)));
    connect(ui_.cullingComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnCullingChanged(int)));
    connect(ui_.lightingCheckBox, SIGNAL(clicked(bool)), SLOT(OnLightingChanged(bool)));
    connect(ui_.depthCheckCheckBox, SIGNAL(clicked(bool)), SLOT(OnDepthCheckChanged(bool)));
    connect(ui_.depthWriteCheckBox, SIGNAL(clicked(bool)), SLOT(OnDepthWriteChanged(bool)));
    connect(ui_.depthBiasSpinBox, SIGNAL(valueChanged(double)), SLOT(OnDepthBiasChanged(double)));
    connect(ui_.alphaRejectionComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnAlphaRejectionFunctionChanged(int)));
    connect(ui_.alphaRejectionSpinBox, SIGNAL(valueChanged(int)), SLOT(OnAlphaRejectionValueChanged(int)));
    
    QButtonGroup *diffuseGroup = new QButtonGroup(this);
    ui_.radioButtonDiffuseTexture->setChecked(true);
    diffuseGroup->addButton(ui_.radioButtonDiffuseTexture);
    diffuseGroup->addButton(ui_.radioButtonDiffuseWeighted);
    //diffuseGroup->addButton(ui_.radioButtonDiffuseSolidColor);
    
    // Shaders - Associate Meshmoon shader compile arguments to UI elements
    ui_.radioButtonDiffuseTexture->setProperty("compileArgument", "DIFFUSE_MAPPING");
    ui_.radioButtonDiffuseWeighted->setProperty("compileArgument", "WEIGHTED_DIFFUSE");
    //ui_.radioButtonDiffuseSolidColor->setProperty("compileArgument", "SOLID_COLOR");
    
    ui_.checkBoxShadingSpecLight->setProperty("compileArgument", "SPECULAR_LIGHTING");
    ui_.checkBoxShadingSpecMap->setProperty("compileArgument", "SPECULAR_MAPPING");
    ui_.checkBoxShadingMultiDiffuse->setProperty("compileArgument", "MULTI_DIFFUSE");
    ui_.checkBoxShadingNormalMap->setProperty("compileArgument", "NORMAL_MAPPING");
    ui_.checkBoxShadingLightMap->setProperty("compileArgument", "LIGHT_MAPPING");
    ui_.checkBoxShadingShadows->setProperty("compileArgument", "SHADOW_MAPPING");
    
    // @todo Enable when implemented.
    ui_.radioButtonDiffuseSolidColor->setEnabled(false);
    ui_.radioButtonDiffuseSolidColor->hide();

    ui_.checkBoxShadingSpecLight->setChecked(true);
    ui_.checkBoxShadingSpecLight->setEnabled(false);

    connect(ui_.radioButtonDiffuseTexture, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.radioButtonDiffuseWeighted, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    //connect(ui_.radioButtonDiffuseSolidColor, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));

    connect(ui_.checkBoxShadingSpecLight, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.checkBoxShadingSpecMap, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.checkBoxShadingMultiDiffuse, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.checkBoxShadingNormalMap, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.checkBoxShadingLightMap, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    connect(ui_.checkBoxShadingShadows, SIGNAL(toggled(bool)), SLOT(LoadShaderFromSelection()));
    
    ui.contentLayout->addWidget(editor_);
    resize(550, 600);
}

void RocketMaterialEditor::OnUpdate(float frametime)
{
    if (sceneRenderer_ && previewMeshEntity_ && previewMeshEntity_->getParentSceneNode())
    {
        Transform t = sceneRenderer_->NodeTransform(previewMeshEntity_->getParentSceneNode());
        t.rot.y = fmod(t.rot.y + (frametime * 15.0f), 360.f); // 15 deg/sec rotation, 0-259 degree range
        sceneRenderer_->SetNodeTransform(previewMeshEntity_->getParentSceneNode(), t);
    }
}

RocketMaterialEditor::~RocketMaterialEditor()
{
    if (!plugin_)
        return;

    // Forget in-mem generated asset
    if (!material_.expired() && material_.lock()->DiskSourceType() == IAsset::Programmatic)
        plugin_->GetFramework()->Asset()->ForgetAsset(material_.lock()->Name(), false);

    AssetPtr existing = plugin_->GetFramework()->Asset()->GetAsset(AssetRef() + "_RocketMaterialEditor.material");
    if (existing && existing->DiskSourceType() == IAsset::Programmatic)
        plugin_->GetFramework()->Asset()->ForgetAsset(existing->Name(), false);
        
    SAFE_DELETE(sceneRenderer_);
    previewMeshEntity_ = 0;
}

QStringList RocketMaterialEditor::SupportedSuffixes()
{
    QStringList suffixes;
    suffixes << "material";
    return suffixes;
}

QString RocketMaterialEditor::EditorName()
{
    return "Material Editor";
}

void RocketMaterialEditor::ResourceDownloaded(QByteArray data)
{
    if (!plugin_)
        return;

    material_.reset();

    // Now we need to resolve if the material we are about to edit
    // is loaded to the system (aka referenced in the scene) or not.
    // 1) Absolute storage URL
    // 2) Absolute storage S3 variation URL
    // 3) Absolute URL resolved from relative ref and the default storage
    QStringList refs;
    
    QString storageKey = StorageKey();
    if (!storageKey.isEmpty())
    {
        refs << plugin_->Storage()->UrlForItem(storageKey)
             << MeshmoonAsset::S3RegionVariationURL(plugin_->Storage()->UrlForItem(storageKey))
             << plugin_->GetFramework()->Asset()->ResolveAssetRef("", plugin_->Storage()->RelativeRefForKey(storageKey));
    }
    else
        refs << AssetRef();

    foreach(const QString &ref, refs)
    {
        AssetPtr loaded = plugin_->GetFramework()->Asset()->GetAsset(ref);
        if (loaded.get())
        {
            LoadMaterial(loaded);
            break;
        }
    }

    // Previous step did nothing: Get or create a in memory asset from the input data.
    if (material_.expired())
    {
        // Generated asset should have correct url base so relative texture refs will be resolved correctly.
        QString generatedName = (!storageKey.isEmpty() ? plugin_->Storage()->UrlForItem(storageKey) : AssetRef()) + "_RocketMaterialEditor.material";
        AssetPtr created = plugin_->GetFramework()->Asset()->GetAsset(generatedName);
        if (!created.get())
        {
            // Create in memory material.
            created = plugin_->GetFramework()->Asset()->CreateNewAsset("OgreMaterial", generatedName);
            if (!created.get())
            {
                ShowError("Material Loading Failed", "Failed to generate an in memory material for raw input data.");
                return;
            }

            // Hook to loaded signal that is emitted either in LoadFromFileInMemory or when all dependencies are loaded.
            connect(created.get(), SIGNAL(Loaded(AssetPtr)), SLOT(LoadMaterial(AssetPtr)));
            
            // Start loading (also reads and requests deps)
            if (!created->LoadFromFileInMemory((const u8*)data.data(), (size_t)data.size(), false))
            {
                ShowError("Material Loading Failed", "Failed to load a material from raw input data. See console for load errors.");
                return;
            }
            
            // In memory generated assets do not have a signal currently that tells that deps loading failed, so go around this.
            std::vector<AssetReference> deps = created->FindReferences();
            for(size_t i=0; i<deps.size(); ++i)
            {
                AssetReference ref = deps[i];
                if (ref.ref.isEmpty())
                    continue;

                AssetPtr existing = plugin_->GetFramework()->Asset()->GetAsset(ref.ref);
                if (!existing || !existing->IsLoaded())
                {
                    AssetTransferPtr depTransfer = plugin_->GetFramework()->Asset()->RequestAsset(ref);
                    connect(depTransfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnGeneratedMaterialDependencyFailed(IAssetTransfer*, QString)));
                }
            }
        }
        else
            LoadMaterial(created);
    }
}

void RocketMaterialEditor::OnGeneratedMaterialDependencyFailed(IAssetTransfer *transfer, QString reason)
{
    ShowError("Material Loading Failed", QString("The opened material failed to load texture dependency %1. You need to fix it manually with the Text Editor.<br><br>This tool can not open malformed materials.").arg(transfer->SourceUrl()));
}

void RocketMaterialEditor::LoadMaterial(AssetPtr asset)
{
    if (!plugin_ || !asset.get())
        return;

    if (!asset->IsLoaded())
    {
        // Try to resolve which dependency failed to load.
        std::vector<AssetReference> deps = asset->FindReferences();
        for(size_t i=0; i<deps.size(); ++i)
        {
            AssetReference ref = deps[i];
            if (ref.ref.isEmpty())
                continue;

            AssetPtr dep = plugin_->GetFramework()->Asset()->GetAsset(ref.ref);
            if (!dep || !dep->IsLoaded())
            {
                ShowError("Material Loading Failed", QString("The opened material failed to load texture dependency %1. You need to fix it manually with the Text Editor.<br><br>This tool can not open malformed materials.").arg(ref.ref));
                return;
            }
        }

        ShowError("Failed Opening Material", "Material could not be loaded, it might have syntax errors or invalid texture references. If the material is broken you need to edit it manually with the Text Editor.<br><br>This tool can not open malformed materials.");
        return;
    }

    OgreMaterialAssetPtr material = dynamic_pointer_cast<OgreMaterialAsset>(asset);
    if (material.get())
    {
        if (material->DiskSourceType() == IAsset::Programmatic)
            SetMessage("Material Loaded [Generated]");
        else
            SetMessage("Material Loaded [Live Preview]");
        material_ = weak_ptr<OgreMaterialAsset>(material);
    }
    else
    {
        ShowError("Failed Opening Resource", QString("The opened asset is of invalid type '%1' for this tool: ").arg(asset->Type()));
        return;
    }
    
    // Initialize renderer
    SAFE_DELETE(sceneRenderer_);
    previewMeshEntity_ = 0;
    
    sceneRenderer_ = new RocketOgreSceneRenderer(plugin_, ui_.renderTargetWidget);
    if (sceneRenderer_->OgreViewport())
        sceneRenderer_->OgreViewport()->setBackgroundColour(Color(QColor(200,200,200)));
    sceneRenderer_->CreateLight(Ogre::Light::LT_DIRECTIONAL);
    CreateCubePreview();
        
    connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), SLOT(OnUpdate(float)), Qt::UniqueConnection);
    
    /// @todo Should we support more than one technique?   
    int numTech = 0;
    int numPass = material->GetNumPasses(numTech);
    if (numPass <= 0)
    {
        ShowError("Failed Opening Material", "The opened material does not have any valid passes. If the material is broken you need to edit it manually with the Text Editor.<br><br>This tool can not open malformed materials.");
        material_.reset();
        return;
    }
    
    // Load known Meshmoon shader information.
    LoadShaders();

    // Load all material passes to the pass selection widget.
    int comboIndex = 0;
    ui_.passComboBox->clear();
    for (int iPass=0; iPass<numPass; ++iPass)
    {
        Ogre::Pass *pass = material->GetPass(numTech, iPass);
        if (pass)
        {
            // Ogre gives the index as a string if no name is set. Ignore this in the UI.
            QString passName = QString::fromStdString(pass->getName()).trimmed();
            if (passName == QString::number(iPass))
                passName = "";

            ui_.passComboBox->addItem(QString("%1%2").arg(iPass+1).arg((!passName.isEmpty() ? " " + passName : "")));
            ui_.passComboBox->setItemData(comboIndex, numTech, TechniqueRole);
            ui_.passComboBox->setItemData(comboIndex, iPass, PassRole);
            comboIndex++;
        }
    }

    // Triggers LoadPass with the first pass.
    ui_.passComboBox->setCurrentIndex(0);
}

void RocketMaterialEditor::CreateCubePreview()
{
    CreatePreviewMesh(Ogre::SceneManager::PT_CUBE);
}

void RocketMaterialEditor::CreateSpherePreview()
{
    CreatePreviewMesh(Ogre::SceneManager::PT_SPHERE);
}

void RocketMaterialEditor::CreatePlanePreview()
{
    CreatePreviewMesh(Ogre::SceneManager::PT_PLANE);
}

void RocketMaterialEditor::CreatePreviewMesh(Ogre::SceneManager::PrefabType type)
{
    if (!sceneRenderer_)
        return;

    Transform t;
    if (previewMeshEntity_)
    {
        t = sceneRenderer_->NodeTransform(previewMeshEntity_->getParentSceneNode());
        sceneRenderer_->DestroyObject(previewMeshEntity_);
    }

    previewMeshEntity_ = sceneRenderer_->CreateMesh(type);
    if (!previewMeshEntity_)
        return;

    sceneRenderer_->SetCameraDistance((type == Ogre::SceneManager::PT_CUBE ? 140.f : 
        (Ogre::SceneManager::PT_SPHERE ? 110.f : 200.f)));

    sceneRenderer_->SetNodeTransform(previewMeshEntity_->getParentSceneNode(), t);
    if (!material_.expired())
        previewMeshEntity_->setMaterialName(material_.lock()->ogreAssetName.toStdString());
}

void RocketMaterialEditor::LoadPass(int techIndex, int passIndex)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;

    if (!material->GetPass(techIndex, passIndex))
        return;

    iTech_ = techIndex;
    iPass_ = passIndex;

    // Colors
    OnAmbientChanged(material->AmbientColor(techIndex, passIndex), false);
    OnDiffuseChanged(material->DiffuseColor(techIndex, passIndex), false);
    OnSpecularChanged(material->SpecularColor(techIndex, passIndex), false);
    OnEmissiveChanged(material->EmissiveColor(techIndex, passIndex), false);

    // Blending
    OnSourceBlendingChanged(material->SourceSceneBlendFactor(techIndex, passIndex), false);
    OnDestinationBlendingChanged(material->DestinationSceneBlendFactor(techIndex, passIndex), false);
    OnCullingChanged(material->HardwareCullingMode(techIndex, passIndex)-1, false);
    OnLightingChanged(material->IsLightingEnabled(techIndex, passIndex), false);
    OnDepthCheckChanged(material->IsDepthCheckEnabled(techIndex, passIndex), false);
    OnDepthWriteChanged(material->IsDepthWriteEnabled(techIndex, passIndex), false);
    OnDepthBiasChanged(static_cast<double>(material->DepthBias(techIndex, passIndex)), false);
    OnAlphaRejectionFunctionChanged(static_cast<int>(material->AlphaRejectionFunction(techIndex, passIndex)), false);
    OnAlphaRejectionValueChanged(static_cast<int>(material->AlphaRejection(techIndex, passIndex)), false);
    
    // Shaders
    OnShaderChanged(material->VertexShader(techIndex, passIndex));
}

void RocketMaterialEditor::LoadShaders()
{
    shaders_ = RocketGPUProgramGenerator::MeshmoonShaders(plugin_->GetFramework());
}

QVariant RocketMaterialEditor::ReadShaderNamedParameterCache(const QString &namedParameter) const
{
    return shaderNamedParameterCache_.value(namedParameter, QVariant());
}

void RocketMaterialEditor::WriteShaderNamedParameterCache(const QString &namedParameter, QVariant value, bool overwriteExisting)
{
    if (!overwriteExisting && shaderNamedParameterCache_.contains(namedParameter))
        return;
    shaderNamedParameterCache_[namedParameter] = value;
}

void RocketMaterialEditor::OnHelpMaterials()
{
    QDesktopServices::openUrl(QUrl("http://www.ogre3d.org/docs/manual/manual_14.html#Material-Scripts"));
}

void RocketMaterialEditor::OnPassChanged(int index)
{
    int techIndex = ui_.passComboBox->itemData(index, TechniqueRole).toInt();
    int passIndex = ui_.passComboBox->itemData(index, PassRole).toInt();
    LoadPass(techIndex, passIndex);
}

QString RocketMaterialEditor::ColorTypeToString(int type)
{
    ColorType colorType = (ColorType)type;
    switch(colorType)
    {
        case AmbientColor:
            return "Ambient";
        case DiffuseColor:
            return "Diffuse";
        case SpecularColor:
            return "Specular";
        case EmissiveColor:
            return "Emissive";
    }
    return "";
}

void RocketMaterialEditor::OnPickColor(int type)
{
    if (ActivateColorDialog(type))
        return;

    ColorType colorType = (ColorType)type;
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;

    QPointer<QColorDialog> dialog = new QColorDialog(this);
    dialog->setWindowTitle(ColorTypeToString(colorType) + " color");
    dialog->setOption(QColorDialog::NoButtons);
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setProperty("colorType", type);
    
#ifdef __APPLE__
    dialog->setOption(QColorDialog::DontUseNativeDialog, true);
#endif

    Color color(1.0f, 1.0f, 1.0f, 1.0f);
    if (colorType == AmbientColor)
    {
        color = material->AmbientColor(iTech_, iPass_);
        connect(dialog, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnAmbientChanged(const QColor&)));
    }
    else if (colorType == DiffuseColor)
    {
        color = material->DiffuseColor(iTech_, iPass_);
        connect(dialog, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnDiffuseChanged(const QColor&)));
    }
    else if (type == SpecularColor)
    {
        color = material->SpecularColor(iTech_, iPass_);
        connect(dialog, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnSpecularChanged(const QColor&)));
    }
    else if (type == EmissiveColor)
    {
        color = material->EmissiveColor(iTech_, iPass_);
        connect(dialog, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnEmissiveChanged(const QColor&)));
    }

    // At this moment we don't want to confuse people with alpha.
    /// @todo Enable this when the alpha masking shaders are enabled to this editor.
    color.a = 1.0f;
    dialog->setCurrentColor(color);
    dialog->show();
    
    colorDialogs_ << dialog;
}

void RocketMaterialEditor::OnRestoreDefaultColor()
{
    if (material_.expired())
        return;

    QObject *s = sender();
    if (!s)
        return;
    QVariant type = s->property("colorType");
    if (!type.isValid() || type.isNull())
        return;

    ColorType colorType = (ColorType)type.toInt();
    if (colorType == AmbientColor)
        OnAmbientChanged(QColor(255, 255, 255, 255));
    else if (colorType == DiffuseColor)
        OnDiffuseChanged(QColor(255, 255, 255, 255));
    else if (colorType == SpecularColor)
        OnSpecularChanged(QColor(0, 0, 0, 255));
    else if (colorType == EmissiveColor)
        OnEmissiveChanged(QColor(0, 0, 0, 255));
}

void RocketMaterialEditor::OnIncreaseOrDecreaseColor()
{
    QObject *s = sender();
    if (!s)
        return;
    QVariant type = s->property("colorType");
    QVariant multiplier = s->property("multiplier");
    if (!type.isValid() || type.isNull() || !multiplier.isValid() || multiplier.isNull())
        return;
    OnMultiplyColor(type.toInt(), multiplier.toFloat());
}

void RocketMaterialEditor::OnMultiplyColor(int type, float scalar)
{
    if (material_.expired())
        return;

    ColorType colorType = (ColorType)type;
    if (colorType == AmbientColor)
    {
        Color color = material_.lock()->AmbientColor(iTech_, iPass_) * scalar;
        color.a = 1.0f;
        OnAmbientChanged(color);
    }
    else if (colorType == DiffuseColor)
    {
        Color color = material_.lock()->DiffuseColor(iTech_, iPass_) * scalar;
        color.a = 1.0f;
        OnDiffuseChanged(color);
    }
    else if (colorType == SpecularColor)
    {
        Color color = material_.lock()->SpecularColor(iTech_, iPass_) * scalar;
        color.a = 1.0f;
        OnSpecularChanged(color);
    }
    else if (colorType == EmissiveColor)
    {
        Color color = material_.lock()->EmissiveColor(iTech_, iPass_) * scalar;
        color.a = 1.0f;
        OnEmissiveChanged(color);
    }
}

void RocketMaterialEditor::OnAmbientChanged(const QColor &color, bool apply)
{
    ui_.buttonAmbient->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(255));
            
    if (apply && !material_.expired())
    {
        material_.lock()->SetAmbientColor(iTech_, iPass_, color);
        Touched();
    }
}

void RocketMaterialEditor::OnDiffuseChanged(const QColor &color, bool apply)
{
    ui_.buttonDiffuse->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(255));

    if (apply && !material_.expired())
    {
        material_.lock()->SetDiffuseColor(iTech_, iPass_, color);
        Touched();
    }
}

void RocketMaterialEditor::OnSpecularChanged(const QColor &color, bool apply)
{
    ui_.buttonSpecular->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(255));

    if (apply && !material_.expired())
    {
        material_.lock()->SetSpecularColor(iTech_, iPass_, color);
        Touched();
    }
}

void RocketMaterialEditor::OnEmissiveChanged(const QColor &color, bool apply)
{
    ui_.buttonEmissive->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(255));

    if (apply && !material_.expired())
    {
        material_.lock()->SetEmissiveColor(iTech_, iPass_, color);
        Touched();
    }
}

void RocketMaterialEditor::OnSourceBlendingChanged(int mode, bool apply)
{
    ui_.blendSourceComboBox->blockSignals(true);
    ui_.blendSourceComboBox->setCurrentIndex(mode);
    ui_.blendSourceComboBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetSceneBlend(iTech_, iPass_,
            static_cast<uint>(ui_.blendSourceComboBox->currentIndex()),
            static_cast<uint>(ui_.blendDestComboBox->currentIndex()));
        Touched();
    }
}

void RocketMaterialEditor::OnDestinationBlendingChanged(int mode, bool apply)
{
    ui_.blendDestComboBox->blockSignals(true);
    ui_.blendDestComboBox->setCurrentIndex(mode);
    ui_.blendDestComboBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetSceneBlend(iTech_, iPass_,
            static_cast<uint>(ui_.blendSourceComboBox->currentIndex()),
            static_cast<uint>(ui_.blendDestComboBox->currentIndex()));
        Touched();
    }
}

void RocketMaterialEditor::OnCullingChanged(int mode, bool apply)
{
    ui_.cullingComboBox->blockSignals(true);
    ui_.cullingComboBox->setCurrentIndex(mode);
    ui_.cullingComboBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetHardwareCullingMode(iTech_, iPass_, static_cast<uint>(mode+1)); // Ogre::CullingMode starts from 1
        Touched();
    }
}

void RocketMaterialEditor::OnDepthCheckChanged(bool enabled, bool apply)
{
    ui_.depthCheckCheckBox->blockSignals(true);
    ui_.depthCheckCheckBox->setChecked(enabled);
    ui_.depthCheckCheckBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetDepthCheck(iTech_, iPass_, enabled);
        Touched();
    }
}

void RocketMaterialEditor::OnDepthWriteChanged(bool enabled, bool apply)
{
    ui_.depthWriteCheckBox->blockSignals(true);
    ui_.depthWriteCheckBox->setChecked(enabled);
    ui_.depthWriteCheckBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetDepthWrite(iTech_, iPass_, enabled);
        Touched();
    }
}

void RocketMaterialEditor::OnDepthBiasChanged(double value, bool apply)
{
    ui_.depthBiasSpinBox->blockSignals(true);
    ui_.depthBiasSpinBox->setValue(value);
    ui_.depthBiasSpinBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetDepthBias(iTech_, iPass_, static_cast<float>(value));
        Touched();
    }
}

void RocketMaterialEditor::OnAlphaRejectionFunctionChanged(int value, bool apply)
{
    ui_.alphaRejectionComboBox->blockSignals(true);
    ui_.alphaRejectionComboBox->setCurrentIndex(value);
    ui_.alphaRejectionComboBox->blockSignals(false);
    
    ui_.alphaRejectionSpinBox->setEnabled((value != static_cast<int>(Ogre::CMPF_ALWAYS_FAIL) && value != static_cast<int>(Ogre::CMPF_ALWAYS_PASS)));

    if (apply && !material_.expired())
    {
        material_.lock()->SetAlphaRejectionFunction(iTech_, iPass_, static_cast<Ogre::CompareFunction>(value));
        Touched();
    }
}

void RocketMaterialEditor::OnAlphaRejectionValueChanged(int value, bool apply)
{
    if (value > 255) value = 255;
    if (value < 0) value = 0;

    ui_.alphaRejectionSpinBox->blockSignals(true);
    ui_.alphaRejectionSpinBox->setValue(value);
    ui_.alphaRejectionSpinBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetAlphaRejection(iTech_, iPass_, static_cast<u8>(value));
        Touched();
    }
}

void RocketMaterialEditor::OnLightingChanged(bool enabled, bool apply)
{
    ui_.lightingCheckBox->blockSignals(true);
    ui_.lightingCheckBox->setChecked(enabled);
    ui_.lightingCheckBox->blockSignals(false);

    if (apply && !material_.expired())
    {
        material_.lock()->SetLighting(iTech_, iPass_, enabled);
        Touched();
    }
    
    if (!material_.expired())
    {       
        // When lighting is enabled update the UI.
        if (enabled)
        {
            if (colorCache_.size() >= 4)
            {
                OnAmbientChanged(colorCache_[0], true);
                OnDiffuseChanged(colorCache_[1], true);
                OnSpecularChanged(colorCache_[2], true);
                OnEmissiveChanged(colorCache_[3], true);
            }
            colorCache_.clear();
        }
        // When lighting is disabled update UI to reflect that colors are now not applied.
        else
        {
            // Keep a editor specific memory of the color values before lighting was turned off.
            // This gives us the ability to restore your previous colors when you turn lighting back on.
            colorCache_.clear();
            colorCache_ << material_.lock()->AmbientColor(iTech_, iPass_)  << material_.lock()->DiffuseColor(iTech_, iPass_)
                         << material_.lock()->SpecularColor(iTech_, iPass_) << material_.lock()->EmissiveColor(iTech_, iPass_);
            
            // Reset alpha
            for (int i=0; i<colorCache_.size(); ++i)
                colorCache_[i].a = 1.0f;

            OnAmbientChanged(QColor(0, 0, 0, 255), false);
            OnDiffuseChanged(QColor(0, 0, 0, 255), false);
            OnSpecularChanged(QColor(0, 0, 0, 255), false);
            OnEmissiveChanged(QColor(0, 0, 0, 255), false);
        }
    }

    // Disable buttons when lighting is off as they have no effect.
    ui_.buttonAmbient->setEnabled(enabled);
    ui_.buttonDiffuse->setEnabled(enabled);
    ui_.buttonSpecular->setEnabled(enabled);
    ui_.buttonEmissive->setEnabled(enabled);

    QString buttonText = enabled ? "" : "Lighting Disabled";
    ui_.buttonAmbient->setText(buttonText);
    ui_.buttonDiffuse->setText(buttonText);
    ui_.buttonSpecular->setText(buttonText);
    ui_.buttonEmissive->setText(buttonText);
}

void RocketMaterialEditor::SelectShadingCheckbox(QCheckBox *target, bool checked, bool enabled, bool apply)
{
    if (!target)
        return;

    if (!apply)
        target->blockSignals(true);

    if (target->isChecked() != checked)
        target->setChecked(checked);
    if (target->isEnabled() != enabled)
        target->setEnabled(enabled);

    if (!apply)
        target->blockSignals(false);
}

void RocketMaterialEditor::SelectShadingSpecLighting(bool checked, bool enabled, bool apply)
{
    //SelectShadingCheckbox(ui_.checkBoxShadingSpecLight, checked, enabled, apply);
}

void RocketMaterialEditor::SelectShadingSpecMap(bool checked, bool enabled, bool apply)
{
    SelectShadingCheckbox(ui_.checkBoxShadingSpecMap, checked, enabled, apply);
}

void RocketMaterialEditor::SelectShadingMultiDiffuse(bool checked, bool enabled, bool apply)
{
    SelectShadingCheckbox(ui_.checkBoxShadingMultiDiffuse, checked, enabled, apply);
}

void RocketMaterialEditor::SelectShadingNormalMap(bool checked, bool enabled, bool apply)
{
    SelectShadingCheckbox(ui_.checkBoxShadingNormalMap, checked, enabled, apply);
}

void RocketMaterialEditor::SelectShadingLightMap(bool checked, bool enabled, bool apply)
{
    SelectShadingCheckbox(ui_.checkBoxShadingLightMap, checked, enabled, apply);
}

void RocketMaterialEditor::SelectShadingShadows(bool checked, bool enabled, bool apply)
{
    //SelectShadingCheckbox(ui_.checkBoxShadingShadows, checked, enabled, apply);
}

void RocketMaterialEditor::LoadShaderAsSelection(const MeshmoonShaderInfo &shader, bool readOnly)
{
    ui_.radioButtonDiffuseTexture->blockSignals(true);
    ui_.radioButtonDiffuseWeighted->blockSignals(true);
    ui_.radioButtonDiffuseSolidColor->blockSignals(true);

    // Diffuse
    if (shader.compileArguments.contains(ui_.radioButtonDiffuseTexture->property("compileArgument").toString(), Qt::CaseSensitive))
        ui_.radioButtonDiffuseTexture->setChecked(true);
    else if (shader.compileArguments.contains(ui_.radioButtonDiffuseWeighted->property("compileArgument").toString(), Qt::CaseSensitive))
        ui_.radioButtonDiffuseWeighted->setChecked(true);
    /*else if (shader.compileArguments.contains(ui_.radioButtonDiffuseSolidColor->property("compileArgument").toString(), Qt::CaseSensitive))
        ui_.radioButtonDiffuseSolidColor->setChecked(true);*/

    ui_.radioButtonDiffuseTexture->blockSignals(false);
    ui_.radioButtonDiffuseWeighted->blockSignals(false);
    ui_.radioButtonDiffuseSolidColor->blockSignals(false);

    bool diffuseTextureSelected = ui_.radioButtonDiffuseTexture->isChecked();
    bool weightedDiffuseTextureSelected = ui_.radioButtonDiffuseWeighted->isChecked();

    SelectShadingSpecLighting(ui_.checkBoxShadingSpecLight->isChecked(), diffuseTextureSelected);
    SelectShadingSpecMap(ui_.checkBoxShadingSpecMap->isChecked(), diffuseTextureSelected);
    SelectShadingMultiDiffuse(ui_.checkBoxShadingMultiDiffuse->isChecked(), diffuseTextureSelected);
    SelectShadingNormalMap(ui_.checkBoxShadingNormalMap->isChecked(), diffuseTextureSelected);
    SelectShadingLightMap(ui_.checkBoxShadingLightMap->isChecked(), diffuseTextureSelected || weightedDiffuseTextureSelected);
    SelectShadingShadows(ui_.checkBoxShadingShadows->isChecked(), true);

    // Shading
    QList<QCheckBox*> shadingWidgets;
    shadingWidgets << ui_.checkBoxShadingSpecLight << ui_.checkBoxShadingSpecMap << ui_.checkBoxShadingMultiDiffuse
        << ui_.checkBoxShadingNormalMap << ui_.checkBoxShadingLightMap << ui_.checkBoxShadingShadows;

    foreach(QCheckBox *shadingWidget, shadingWidgets)
    {
        if (!shadingWidget)
            continue;
        bool checked = shader.compileArguments.contains(shadingWidget->property("compileArgument").toString(), Qt::CaseSensitive);
        SelectShadingCheckbox(shadingWidget, checked, shadingWidget->isEnabled());
    }
    
    LoadShaderFromSelection(readOnly);
}

void RocketMaterialEditor::LoadShaderFromSelection(bool readOnly)
{
    QStringList selectedShaderDefines;
    
    // These are defaults that everything will have
    selectedShaderDefines << "DIFFUSE_LIGHTING";

    /** Force one of the diffuse types to be checked
        @note Block all signals as enabling one of the exclusive radio
        buttons can trigger a deselect on the other ones in the group. */
    if (!ui_.radioButtonDiffuseTexture->isChecked() && !ui_.radioButtonDiffuseWeighted->isChecked() && !ui_.radioButtonDiffuseSolidColor->isChecked())
    {
        ui_.radioButtonDiffuseTexture->blockSignals(true);
        ui_.radioButtonDiffuseWeighted->blockSignals(true);
        ui_.radioButtonDiffuseSolidColor->blockSignals(true);
        
        ui_.radioButtonDiffuseTexture->setChecked(true);
        
        ui_.radioButtonDiffuseTexture->blockSignals(false);
        ui_.radioButtonDiffuseWeighted->blockSignals(false);
        ui_.radioButtonDiffuseSolidColor->blockSignals(false);
    }

    // Diffuse
    if (ui_.radioButtonDiffuseTexture->isChecked())
    {
        selectedShaderDefines << "SPECULAR_LIGHTING";
        selectedShaderDefines << ui_.radioButtonDiffuseTexture->property("compileArgument").toString();
    }
    else if (ui_.radioButtonDiffuseWeighted->isChecked())
    {
        selectedShaderDefines << ui_.radioButtonDiffuseWeighted->property("compileArgument").toString();
    }
    else if (ui_.radioButtonDiffuseSolidColor->isChecked())
    {
        //selectedShaderDefines << ui_.radioButtonDiffuseSolidColor->property("compileArgument").toString();
        LogError("Diffuse solid color not supported!");
        return;
    }
    
    bool diffuseTextureSelected = ui_.radioButtonDiffuseTexture->isChecked();
    bool weightedDiffuseTextureSelected = ui_.radioButtonDiffuseWeighted->isChecked();

    SelectShadingSpecLighting(ui_.checkBoxShadingSpecLight->isChecked(), diffuseTextureSelected);
    SelectShadingSpecMap(ui_.checkBoxShadingSpecMap->isChecked(), diffuseTextureSelected);
    SelectShadingMultiDiffuse(ui_.checkBoxShadingMultiDiffuse->isChecked(), diffuseTextureSelected);
    SelectShadingNormalMap(ui_.checkBoxShadingNormalMap->isChecked(), diffuseTextureSelected);
    SelectShadingLightMap(ui_.checkBoxShadingLightMap->isChecked(), diffuseTextureSelected || weightedDiffuseTextureSelected);
    SelectShadingShadows(ui_.checkBoxShadingShadows->isChecked(), true);
    
    // Shading
    QList<QCheckBox*> shadingWidgets;
    shadingWidgets << ui_.checkBoxShadingSpecLight << ui_.checkBoxShadingSpecMap << ui_.checkBoxShadingMultiDiffuse
                   << ui_.checkBoxShadingNormalMap << ui_.checkBoxShadingLightMap << ui_.checkBoxShadingShadows;

    foreach(const QCheckBox *shadingWidget, shadingWidgets)
        if (shadingWidget && shadingWidget->isEnabled() && shadingWidget->isChecked())
            selectedShaderDefines << shadingWidget->property("compileArgument").toString();

    selectedShaderDefines.removeDuplicates();

    // Find the shader that has all and exactly our required defines.
    foreach(const MeshmoonShaderInfo &info, shaders_)
    {
        if (info.compileArguments.size() != selectedShaderDefines.size())
            continue;

        bool match = false;
        foreach(const QString &requiredDefine, selectedShaderDefines)
        {
            match = info.compileArguments.contains(requiredDefine, Qt::CaseSensitive);
            if (!match)
                break;
        }
        if (!match)
            continue;

        LoadShader(info.name, readOnly);
        return;
    }

    ShowError("Shader not supported", "Failed to find a matchin shader for the UI selection<br><br>" + selectedShaderDefines.join(", "));
}

void RocketMaterialEditor::LoadShader(const QString &shaderName, bool readOnly)
{
    // Remove all current texture editors and cache the texture values.
    while(ui_.texturesLayout->count() > 0)
    {
        QLayoutItem *item = ui_.texturesLayout->takeAt(0);
        if (!item)
            break;
        QWidget *w = item->widget();
        QLayout *l = item->layout();
        
        // Cache value so we can load it back to the next shader selection.
        RocketMaterialTextureWidget *editor = dynamic_cast<RocketMaterialTextureWidget*>(w);
        if (editor && !editor->Texture().isEmpty())
            textureCache_[editor->TextureType()] = editor->Texture();

        SAFE_DELETE(item);
        if (w) SAFE_DELETE(w);
        if (l) SAFE_DELETE(l);
    }

    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    if (!material->GetPass(iTech_, iPass_))
        return;

    // Reset shaders if errors occur during texture unit processing, or no shader is selected.
    if (!readOnly)
    {
        material->SetVertexShader(iTech_, iPass_, "");
        material->SetPixelShader(iTech_, iPass_, "");
        Touched();
    }
    
    // No shader means diffuse only.
    if (shaderName.isEmpty())
    {
        // Crate diffuse editor and texture unit.
        RocketMaterialTextureWidget *editor = new RocketMaterialTextureWidget(plugin_, material->Name(), MeshmoonShaderInfo(), "Diffuse", 0);

        // Hook texture editor now. PrepareTextureUnit etc. can setup the material if readOnly == false.
        HookTextureEditorSignals(editor);

        if (!PrepareTextureUnit(0, QStringList(), "DIFFUSE_MAPPING", material, editor, readOnly))
        {
            SetError("Failed to create dummy Diffuse texture unit when no shader was selected");
            SAFE_DELETE(editor);
            return;
        }

        ui_.texturesLayout->addWidget(editor);
        return;
    }

    // Find the shader
    foreach(const MeshmoonShaderInfo &info, shaders_)
    {
        if (info.name != shaderName)
            continue;

        // Set shaders. This must be done before manipulating texture units and named parameters.
        if (!readOnly)
        {
            material->SetVertexShader(iTech_, iPass_, info.VertexProgramName());
            material->SetPixelShader(iTech_, iPass_, info.FragmentProgramName());
        }

        int shaderTextureIndex = 0;
        foreach(const QString &compileArgument, info.compileArguments)
        {
            QString textureType = info.TextureType(compileArgument, shaderTextureIndex);
            if (textureType.isEmpty())
                continue;

            if (compileArgument != "SHADOW_MAPPING" && compileArgument != "WEIGHTED_DIFFUSE")
            {
                // Create or read texture unit
                RocketMaterialTextureWidget *editor = new RocketMaterialTextureWidget(plugin_, material->Name(), info, textureType, shaderTextureIndex);
                editor->setProperty("initializing", true);
                
                // Hook texture editor now. PrepareTextureUnit etc. can setup the material if readOnly == false.
                HookTextureEditorSignals(editor);

                if (!PrepareTextureUnit(shaderTextureIndex, info.compileArguments, compileArgument, material, editor, readOnly))
                {
                    SetError(QString("Failed to create texture unit %1 for selected shader").arg(textureType));
                    SAFE_DELETE(editor);
                    return;
                }
                shaderTextureIndex += 1;
                
                QString tilingParam = info.TilingNamedParameterName(compileArgument);
                if (!tilingParam.isEmpty())
                {
                    // Tiling
                    if (readOnly)
                    {
                        QVariant shaderVariant = ReadShaderNamedParameter(tilingParam, cShaderTypePixel, QVariant::Vector2D);
                        if (!shaderVariant.isNull() && shaderVariant.isValid() && shaderVariant.type() == QVariant::Vector2D)
                            WriteShaderNamedParameterCache(tilingParam, shaderVariant.value<QVector2D>(), true);
                        else
                            WriteShaderNamedParameterCache(tilingParam, QVector2D(1.0f, 1.0f), false);
                    }
                    else
                        WriteShaderNamedParameterCache(tilingParam, QVector2D(1.0f, 1.0f), false);
                        
                    QVariant value = ReadShaderNamedParameterCache(tilingParam);
                    QVector2D tiling = value.value<QVector2D>();
                    
                    editor->setProperty("tilingNamedParameter", tilingParam);
                    editor->SetTiling(float2(tiling.x(), tiling.y()), false);
                    
                    if (!readOnly)
                        WriteShaderNamedParameter(tilingParam, cShaderTypePixel, value);
                }

                ui_.texturesLayout->addWidget(editor);
                editor->setProperty("initializing", false);
            }
            else if (compileArgument == "WEIGHTED_DIFFUSE")
            {
                /// @todo Check the define for weighted normal maps when supported!
                /// @todo Weight and diffuse map tiling!

                // Generate weight + diffuse maps. Creates four texture units instead of one.
                for (int wdi=0; wdi<5; ++wdi)
                {
                    textureType = (wdi == 0 ? "Weight map" : QString("Diffuse %1").arg(wdi));

                    // Create or read texture unit
                    RocketMaterialTextureWidget *editor = new RocketMaterialTextureWidget(plugin_, material->Name(), info, textureType, shaderTextureIndex);
                    editor->setProperty("initializing", true);
                    
                    // Hook texture editor now. PrepareTextureUnit etc. can setup the material if readOnly == false.
                    HookTextureEditorSignals(editor);

                    if (!PrepareTextureUnit(shaderTextureIndex, info.compileArguments, compileArgument, material, editor, readOnly))
                    {
                        SetError(QString("Failed to create texture unit %1 for selected shader").arg(textureType));
                        SAFE_DELETE(editor);
                        return;
                    }
                    shaderTextureIndex += 1;

                    QString tilingParam = (wdi == 0 ? "weightMapTiling" : "diffuseMapTilings");
                    if (!tilingParam.isEmpty())
                    {
                        QVariant value;

                        // Weighted tiling
                        if (wdi == 0)
                        {
                            if (readOnly)
                            {
                                QVariant shaderVariant = ReadShaderNamedParameter(tilingParam, cShaderTypePixel, QVariant::Vector2D);
                                if (!shaderVariant.isNull() && shaderVariant.isValid() && shaderVariant.type() == QVariant::Vector2D)
                                    WriteShaderNamedParameterCache(tilingParam, shaderVariant.value<QVector2D>(), true);
                                else
                                    WriteShaderNamedParameterCache(tilingParam, QVector2D(1.0f, 1.0f), false);
                            }
                            else
                                WriteShaderNamedParameterCache(tilingParam, QVector2D(1.0f, 1.0f), false);

                            value = ReadShaderNamedParameterCache(tilingParam);
                            QVector2D tiling = value.value<QVector2D>();

                            editor->setProperty("tilingNamedParameter", tilingParam);
                            editor->SetTiling(float2(tiling.x(), tiling.y()), false);
                        }
                        else
                        {
                            // Only read or initialize cache on the first diffuse.
                            if (wdi == 1)
                            {
                                if (readOnly)
                                {
                                    QVariant shaderVariant = ReadShaderNamedParameter(tilingParam, cShaderTypePixel, QVariant::List, 8);
                                    if (!shaderVariant.isNull() && shaderVariant.isValid() && shaderVariant.type() == QVariant::List && shaderVariant.toList().size() == 8)
                                        WriteShaderNamedParameterCache(tilingParam, shaderVariant, true);
                                    else
                                        WriteShaderNamedParameterCache(tilingParam, QVariantList() << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0, false);
                                }
                                else
                                    WriteShaderNamedParameterCache(tilingParam, QVariantList() << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0 << 1.0, false);
                            }

                            value = ReadShaderNamedParameterCache(tilingParam);
                            if (value.type() == QVariant::List)
                            {
                                QVector2D tiling;
                                int offset = (wdi-1) * 2;

                                QVariantList valueList = value.toList();
                                if (valueList.size() >= offset)
                                    tiling.setX(valueList.value(offset, 0.0).toDouble());
                                if (valueList.size() >= offset+1)
                                    tiling.setY(valueList.value(offset+1, 0.0).toDouble());

                                editor->setProperty("tilingNamedParameter", tilingParam);
                                editor->setProperty("tilingNamedParameterOffset", offset);
                                editor->SetTiling(float2(tiling.x(), tiling.y()), false);
                            }
                        }

                        if (!readOnly && !value.isNull() && value.isValid())
                            WriteShaderNamedParameter(tilingParam, cShaderTypePixel, value);
                    }

                    ui_.texturesLayout->addWidget(editor);
                    editor->setProperty("initializing", false);
                }
            }
            else if (compileArgument == "SHADOW_MAPPING")
            {               
                // Generate shadow maps. Creates three texture units instead of one.
                if (!PrepareShadowMaps(shaderTextureIndex, compileArgument, material, readOnly))
                {
                    SetError("Failed to create shadow map texture units for selected shader");
                    return;
                }
                shaderTextureIndex += 5;
            }
        }

        if (!readOnly)
        {
            // Remove any extra texture units.
            /// @todo Figure out what to do with Meshmoon shader generated shadow maps
            while (material->GetNumTextureUnits(iTech_, iPass_) > shaderTextureIndex)
            {
                if (!material->RemoveTextureUnit(iTech_, iPass_, shaderTextureIndex))
                {
                    LogError("[RocketMaterialEditor]: Failed to remove texture unit from index " + QString::number(shaderTextureIndex));
                    break;
                }
            }
        }

        // Create widgets for float typed named parameters for this shader.
        foreach(const QString &floatNamedParameter, info.FloatTypeNamedParameters())
        {
            float defaultValue = info.FloatTypeNamedParameterDefaultValue(floatNamedParameter);

            // Init cache from current shader or a sensible default.
            if (readOnly)
            {
                QVariant shaderVariant = ReadShaderNamedParameter(floatNamedParameter, cShaderTypePixel, QVariant::Double);
                if (!shaderVariant.isNull() && shaderVariant.isValid() && shaderVariant.type() == QVariant::Double)
                    WriteShaderNamedParameterCache(floatNamedParameter, shaderVariant.toDouble(), true);
                else
                    WriteShaderNamedParameterCache(floatNamedParameter, defaultValue, false);
            }
            else
                WriteShaderNamedParameterCache(floatNamedParameter, defaultValue, false);

            // Read from the cache. It has the value from loaded shader, default value or the value that the editor has now.
            QVariant value = ReadShaderNamedParameterCache(floatNamedParameter);

            QWidget *w = new QWidget();
            QLabel *label = new QLabel(MeshmoonShaderInfo::UserFriendlyNamedParameterName(floatNamedParameter), w);
            QDoubleSpinBox *editor = new QDoubleSpinBox(w);
            
            editor->setProperty(cNamedParameterKey.c_str(), floatNamedParameter);
            editor->setProperty(cShaderTypeKey.c_str(), cShaderTypePixel);
            editor->setDecimals(4);
            editor->setSingleStep(0.25);
            editor->setMinimum(-100000.0);
            editor->setMaximum(100000.0);
            editor->setValue(value.toDouble());
            connect(editor, SIGNAL(valueChanged(double)), SLOT(OnShaderNamedParameterChanged(double)));

            w->setLayout(new QHBoxLayout());
            w->layout()->setContentsMargins(4,0,4,0);
            w->layout()->setSpacing(10);
            w->layout()->addWidget(label);
            w->layout()->addWidget(editor);
            ui_.texturesLayout->addWidget(w);

            if (!readOnly)
                WriteShaderNamedParameter(floatNamedParameter, cShaderTypePixel, value);
        }

        return;
    }
    
    ShowError("Unknown Shader", QString("Shader '%1' found in material is unknown to this editor.<br><br>You can still edit the other properties or change the shader.").arg(shaderName));
}

void RocketMaterialEditor::HookTextureEditorSignals(RocketMaterialTextureWidget *editor)
{
    // texture editor -> material editor
    connect(editor, SIGNAL(TextureChanged(int, const QString&)), this, SLOT(OnTextureChanged(int, const QString&)), Qt::UniqueConnection);
    connect(editor, SIGNAL(UVChanged(int, int)), SLOT(OnTextureUVChanged(int, int)), Qt::UniqueConnection);
    connect(editor, SIGNAL(TilingChanged(int, float2)), SLOT(OnTextureTilingChanged(int, float2)), Qt::UniqueConnection);
    connect(editor, SIGNAL(AddressingChanged(int, int)), SLOT(OnTextureAddressingChanged(int, int)), Qt::UniqueConnection);
    connect(editor, SIGNAL(FilteringChanged(int, int)), SLOT(OnTextureFilteringChanged(int, int)), Qt::UniqueConnection);
    connect(editor, SIGNAL(MaxAnisotropyChanged(int, uint)), SLOT(OnTextureMaxAnisotropyChanged(int, uint)), Qt::UniqueConnection);
    
    /// @todo Deprecated, remove.
    connect(editor, SIGNAL(ColorOperationChanged(int, int)), SLOT(OnTextureColorOperationChanged(int, int)), Qt::UniqueConnection);
    
    // material editor -> texture editor
    connect(this, SIGNAL(CloseTextureEditors()), editor, SLOT(OnCloseTextureEditors()), Qt::UniqueConnection);
}

bool RocketMaterialEditor::PrepareTextureUnit(int textureIndex, const QStringList &compileArguments, const QString &compileArgument,
                                              OgreMaterialAsset *material, RocketMaterialTextureWidget *editor, bool readOnly)
{
    if (compileArgument == "SHADOW_MAPPING")
        return PrepareShadowMaps(textureIndex, compileArgument, material, readOnly);

    Ogre::TextureUnitState *unit = 0;
    
    // If we are writing nuke all texture units with a higher index.
    // This will ensure we wont pick up wrong type of textures from existing
    // units but can instead remember texture by type from the texture cache.
    if (!readOnly)
    {
        while (true)
        {
            int num = material->GetNumTextureUnits(iTech_, iPass_);
            if (num <= textureIndex+1)
                break;
            if (num == -1 || !material->RemoveTextureUnit(iTech_, iPass_, num-1))
                return false;
        }
    }

    do
    {
        unit = material->GetTextureUnit(iTech_, iPass_, textureIndex);

        // Texture unit does not exits. If read only we are done.
        if (!unit && readOnly)
            break;
            
        // If we have write permission. Try to create the needed texture unit.
        if (!unit && (material->CreateTextureUnit(iTech_, iPass_) == -1))
        {
            LogError("[RocketMaterialEditor]: Failed to create texture unit " + QString::number(textureIndex));
            return false;
        }
    } 
    while (unit == 0);

    if (unit)
    {
        // Read current values to editor.
        editor->SetAddressing(static_cast<int>(unit->getTextureAddressingMode().u), false);
        editor->SetColorOperation(static_cast<int>(unit->getColourBlendMode().operation), false);
        editor->SetMaxAnisotropy(unit->getTextureAnisotropy(), false);

        editor->SetFilteringComplex(
            static_cast<int>(unit->getTextureFiltering(Ogre::FT_MIN)),
            static_cast<int>(unit->getTextureFiltering(Ogre::FT_MAG)),
            static_cast<int>(unit->getTextureFiltering(Ogre::FT_MIP)), false);

        // UVs for particular texture units are hardcoded to the Meshmoon shaders and cannot be changed.
        if (editor->ShaderInfo().name.isEmpty())
            editor->SetUV(static_cast<int>(unit->getTextureCoordSet()), false);
        else
        {
            // Disable certain UI elements as we are working with a Meshmoon Shader.
            int meshmoonShaderUV = (compileArgument == "LIGHT_MAPPING" ? 1 : 0);
            editor->SetUV(meshmoonShaderUV, false);
            if (editor->Ui().labelUvTitle)
                editor->Ui().labelUvTitle->hide();
            if (editor->Ui().uvSpinBox)
            {
                editor->Ui().uvSpinBox->setEnabled(false);
                editor->Ui().uvSpinBox->hide();
            }
            if (editor->Ui().colorOperationComboBox)
                editor->Ui().colorOperationComboBox->hide();
        }
        
        // First try the existing texture unit value from the material. Second try our cache that remembers textures between shader swaps.
        // Note that reading the material value can only work if this is a readOnly invocation, otherwise the texture unit is already gone
        // by now. Even if it were not removed we could not use it as the shader can have different texture order. The cache instead remembers by type.
        QString existingRef = OgreTextureToAssetReference(material->Name(), QString::fromStdString(unit->getTextureName())).trimmed();
        if (!existingRef.isEmpty())
            editor->SetTexture(existingRef, false);
        else
        {
            QString cachedRef = textureCache_.value(editor->TextureType(), "");
            if (!cachedRef.isEmpty())
                editor->SetTexture(cachedRef, !readOnly);
        }

        // Prepare texture unit
        if (!readOnly)
        {
            unit->setName(MeshmoonShaderInfo::TextureUnitName(compileArguments, compileArgument, textureIndex).toStdString());
            unit->setTextureNameAlias("");
            unit->setContentType(Ogre::TextureUnitState::CONTENT_NAMED);
            Touched();
        }
    }
    return true;
}

bool RocketMaterialEditor::PrepareShadowMaps(int textureIndex, const QString &compileArgument, OgreMaterialAsset *material, bool readOnly)
{
    if (readOnly)
        return true;

    for (int shadowMapIndex=textureIndex; shadowMapIndex<textureIndex+5; ++shadowMapIndex)
    {
        // If there is a existing texture unit. This is the time to nuke it.
        material->RemoveTextureUnit(iTech_, iPass_, shadowMapIndex);

        Ogre::TextureUnitState *unit = 0;

        do
        {
            unit = material->GetTextureUnit(iTech_, iPass_, shadowMapIndex);
            if (!unit && (material->CreateTextureUnit(iTech_, iPass_) == -1))
            {
                LogError("[RocketMaterialEditor]: Failed to create shadow map texture unit " + QString::number(textureIndex));
                return false;
            }
        } 
        while (unit == 0);

        if (unit)
        {
            if (shadowMapIndex == textureIndex)
            {
                // texture_unit shadowKernelRotation
                // {
                //     texture KernelRotation.png
                //     filtering point point none
                // }

                unit->setName("shadowKernelRotation");
                unit->setTextureNameAlias("");
                unit->setTextureName("KernelRotation.png");
                unit->setContentType(Ogre::TextureUnitState::CONTENT_NAMED);
                unit->setTextureFiltering(Ogre::TFO_NONE);
            }    
            else
            {
                // texture_unit shadowMap<shadowMapIndex>
                // {
                //     content_type shadow
                //     tex_address_mode clamp
                // }

                unit->setName(MeshmoonShaderInfo::TextureUnitName(QStringList(), compileArgument, shadowMapIndex-textureIndex).toStdString());
                unit->setTextureNameAlias("");
                unit->setContentType(Ogre::TextureUnitState::CONTENT_SHADOW);
                unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);
            }

            Touched();
        }
    }

    return true;
}

void RocketMaterialEditor::OnTextureChanged(int textureUnitIndex, const QString &textureRef)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    /// @todo Hook to the texture ref transfer that SetTexture makes so we can verify when its
    /// safe to serialize material. Before the transfer is completed the serialization wont have the texture applied!

    if (!textureRef.isEmpty())
        material->SetTexture(iTech_, iPass_, textureUnitIndex, textureRef);
    else
        unit->setBlank(); // This will clear the whole unit, but if there is not texture ref all other params are a waste of space too.
        
    RocketMaterialTextureWidget *source = dynamic_cast<RocketMaterialTextureWidget*>(sender());
    bool editorLoading = (source && source->property("initializing").isValid() ? source->property("initializing").toBool() : false);
    if (!editorLoading)
        Touched();
}

void RocketMaterialEditor::OnTextureUVChanged(int textureUnitIndex, int index)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    unit->setTextureCoordSet(static_cast<uint>(index));
    Touched();
}

void RocketMaterialEditor::OnTextureTilingChanged(int textureUnitIndex, float2 tiling)
{
    RocketMaterialTextureWidget *editor = dynamic_cast<RocketMaterialTextureWidget*>(sender());
    if (!editor)
        return;

    QString tilingParam = editor->property("tilingNamedParameter").toString();
    if (!tilingParam.isEmpty())
    {
        if (editor->property("tilingNamedParameterOffset").isValid())
        {
            int offset = editor->property("tilingNamedParameterOffset").toInt();
                       
            QVariant current = ReadShaderNamedParameterCache(tilingParam);
            if (current.type() == QVariant::List)
            {
                QVariantList valueList = current.toList();
                if (valueList.size() >= offset)
                    valueList[offset] = tiling.x;
                else
                {
                    LogError(QString("RocketMaterialEditor::OnTextureTilingChanged: Error writing %1 to offset %2").arg(tilingParam).arg(offset));
                    return;
                }
                if (valueList.size() >= offset+1)
                    valueList[offset+1] = tiling.y;
                else
                {
                    LogError(QString("RocketMaterialEditor::OnTextureTilingChanged: Error writing %1 to offset %2").arg(tilingParam).arg(offset+1));
                    return;
                }

                WriteShaderNamedParameter(tilingParam, cShaderTypePixel, valueList);
            }
            else
                LogError(QString("RocketMaterialEditor::OnTextureTilingChanged: Non QVariant::List type for writing %1 to offset %2").arg(tilingParam).arg(offset));
        }
        else        
            WriteShaderNamedParameter(tilingParam, cShaderTypePixel, QVector2D(tiling.x, tiling.y));
        Touched();
    }
}

void RocketMaterialEditor::OnTextureAddressingChanged(int textureUnitIndex, int addressing)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    material->SetTextureAddressingMode(iTech_, iPass_, textureUnitIndex, static_cast<uint>(addressing));
    Touched();
}

void RocketMaterialEditor::OnTextureFilteringChanged(int textureUnitIndex, int filtering)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    unit->setTextureFiltering(static_cast<Ogre::TextureFilterOptions>(filtering));
    Touched();
}

void RocketMaterialEditor::OnTextureMaxAnisotropyChanged(int textureUnitIndex, uint maxAnisotropy)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    unit->setTextureAnisotropy(maxAnisotropy);
    Touched();
}

void RocketMaterialEditor::OnTextureColorOperationChanged(int textureUnitIndex, int operation)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;
    Ogre::TextureUnitState *unit = material->GetTextureUnit(iTech_, iPass_, textureUnitIndex);
    if (!unit)
        return;

    unit->setColourOperationEx((Ogre::LayerBlendOperationEx)operation);
    Touched();
}

QString RocketMaterialEditor::OgreTextureToAssetReference(const QString &materialRef, QString textureRef)
{
    if (textureRef.isEmpty())
        return "";

    // Resolve context
    QString context = materialRef;
    if (!context.endsWith("/"))
        context = context.mid(0, context.lastIndexOf("/")+1);
    
    // 1. Full URL ref.
    textureRef = AssetAPI::DesanitateAssetRef(textureRef);

    /** If this is a .dds. It is possible this is a trickery ref by us to set a valid texture alias for a .crn.
        There is no 100% correct solution here, the best we can do is to fetch see if the .dds is not loaded but
        a .crn is loaded. Then swap it here to give a proper ref to the UI. If both .dds and .crn variants are
        loaded to the system from the same base, this will select the .dds even if the original material had .crn! */
    if (textureRef.endsWith(".dds", Qt::CaseInsensitive))
    {
        shared_ptr<TextureAsset> texDDS = plugin_->Fw()->Asset()->FindAsset<TextureAsset>(textureRef);
        if (!texDDS.get() || !texDDS->IsLoaded())
        {
            shared_ptr<TextureAsset> texCRN = plugin_->Fw()->Asset()->FindAsset<TextureAsset>(textureRef.left(textureRef.length()-3) + "crn");
            if (texCRN.get() && texCRN->IsLoaded())
                textureRef = texCRN->Name();
        }
    }
        
    // 2. Relative ref to material.
    if (textureRef.startsWith(context, Qt::CaseInsensitive))
        textureRef = textureRef.mid(context.length());

    return textureRef; 
}

void RocketMaterialEditor::OnShaderChanged(const QString &shaderName)
{
    QString nameNoPostFix = shaderName;
    if (nameNoPostFix.endsWith("VP") || nameNoPostFix.endsWith("FP"))
        nameNoPostFix = nameNoPostFix.left(nameNoPostFix.length()-2);

    // Find shader info for this name to check matches against main name and aliases.
    for(int k=0; k<shaders_.size(); ++k)
    {
        MeshmoonShaderInfo &shaderInfo = shaders_[k];
        
        bool useShader = false;
        bool readOnly = true;

        // If no shader in material but texture units are present. Suggest upgrading to basic diffuse shader.
        if (nameNoPostFix.isEmpty() && shaderInfo.name == "meshmoon/Diffuse")
        {
            OgreMaterialAsset *material = material_.lock().get();
            if (!material)
                break;

            Ogre::Pass *pass = material->GetPass(iTech_, iPass_);
            if (!pass || pass->getNumTextureUnitStates() == 0)
                break;

            QMessageBox::StandardButton result = QMessageBox::question(this, "No shader", 
                "No shader set in this material, but material seems to have texturing.<br><br>Would you like to upgrade to a basic Diffuse shader?",
                    QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);

            useShader = (result == QMessageBox::Yes);
            readOnly = false;
            if (!useShader)
                break;
        }
        else if (shaderInfo.MatchesAlias(nameNoPostFix))
        {
            QMessageBox::StandardButton result = QMessageBox::question(this, "Shader can be upgraded", 
                "The shader in this material can be upgraded to a new Meshmoon shader<br><br>Would you like to upgrade?",
                    QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes);

            useShader = (result == QMessageBox::Yes);
            readOnly = false;
            
            // We are upgrading from old rex/Meshmoon shader to new Mesmoon shader.
            // Cache all texture units or anything above index 0 will be forgotten.
            if (useShader)
                CacheCompatibleTextureUnits();
        }
        else if (shaderInfo.Matches(nameNoPostFix))
            useShader = true;
            
        if (useShader)
        {
            LoadShaderAsSelection(shaderInfo, readOnly);
            return;
        }
    }

    if (!nameNoPostFix.isEmpty())
    {
        ShowError("Shader not supported", 
            QString("Shader '%1' in this material is not supported by this editor.<br><br>It is recommended you upgrade the material by using the provided user interface.")
                .arg(nameNoPostFix));
    }
    else
    {
        ShowError("Shader not defined", 
            QString("There is no shader in this material.<br><br>It is recommended you upgrade the material by using the provided user interface. Reopen the material editor to upgrade."));
    }
    ui_.radioButtonDiffuseTexture->setChecked(false);
}

void RocketMaterialEditor::CacheCompatibleTextureUnits()
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return;

    Ogre::Pass *pass = material->GetPass(iTech_, iPass_);
    if (!pass)
        return;
        
    for (u16 i=0, len=pass->getNumTextureUnitStates(); i<len; ++i)
    {
        Ogre::TextureUnitState *unit = pass->getTextureUnitState(i);
        if (!unit)
            continue;

        /** @todo We can make MeshmoonShaderInfo::TextureTypeFromTextureUnit fancier in that it would get the current shader for the texture unit set
            and detect the exactly correct texture type from the index. Trivial implementation will be a big messy if/else tree.
            This cache will now fail if the texture unit does not have a name or alias. Detection is done with name/alias maching eg. "baseMap" -> "Diffuse". */
        QString textureRef = OgreTextureToAssetReference(material->Name(), QString::fromStdString(unit->getTextureName())).trimmed();
        QString textureType = (textureRef.isEmpty() ? "" : MeshmoonShaderInfo::TextureTypeFromTextureUnit(QString::fromStdString(unit->getName()), QString::fromStdString(unit->getTextureNameAlias())));
        if (!textureType.isEmpty())
            textureCache_[textureType] = textureRef;
    }
}

void RocketMaterialEditor::OnShaderNamedParameterChanged(double value, bool apply)
{
    if (!apply)
        return;
    
    QObject *s = sender();
    if (!s)
        return;

    QVariant namedVariant = s->property(cNamedParameterKey.c_str());
    if (!namedVariant.isValid() || namedVariant.isNull() || namedVariant.type() != QVariant::String)
        return;

    QVariant typeVariant = s->property(cShaderTypeKey.c_str());
    if (!typeVariant.isValid() || typeVariant.isNull() || typeVariant.type() != QVariant::String)
        return;

    WriteShaderNamedParameter(namedVariant.toString(), typeVariant.toString(), value);
}

bool RocketMaterialEditor::WriteShaderNamedParameter(const QString &namedParameter, const QString &shaderType, QVariant value)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return false;

    if (!material->GetPass(iTech_, iPass_))
        return false;

    QVariantList out;
    if (value.type() == QVariant::Vector2D)
    {
        QVector2D temp = value.value<QVector2D>();
        out << temp.x() << temp.y() << 0.0 << 0.0;
    }
    else if (value.type() == QVariant::List)
        out = value.toList();
    else
        out << value;

    if (shaderType.compare(cShaderTypePixel, Qt::CaseInsensitive) == 0)
    {
        if (!material->SetPixelShaderParameter(iTech_, iPass_, namedParameter, out))
        {
            SetError(QString("Failed to update pixel shader named parameter '%1' to %2").arg(namedParameter).arg(value.toString()));
            return false;
        }
    }
    else if (shaderType.compare(cShaderTypeVertex, Qt::CaseInsensitive) == 0)
    {
        if (!material->SetVertexShaderParameter(iTech_, iPass_, namedParameter, out))
        {
            SetError(QString("Failed to update vertex shader named parameter '%1' to %2").arg(namedParameter).arg(value.toString()));
            return false;
        }
    }
    else
        return false;

    WriteShaderNamedParameterCache(namedParameter, value);
    Touched();
    return true;
}

QVariant RocketMaterialEditor::ReadShaderNamedParameter(const QString &namedParameter, const QString &shaderType, QVariant::Type type, size_t size)
{
    OgreMaterialAsset *material = material_.lock().get();
    if (!material)
        return QVariant();

    Ogre::Pass* pass = material->GetPass(iTech_, iPass_);
    if (!pass)
        return QVariant();

    const Ogre::GpuProgramPtr &fragProg = (shaderType == cShaderTypeVertex ? pass->getVertexProgram() : pass->getFragmentProgram());
    if (fragProg.isNull())
    {
        LogError("RocketMaterialEditor::ReadShaderNamedParameter: Pass does not contain the program you are looking for.");
        return QVariant();
    }
    
    Ogre::GpuProgramParametersSharedPtr fragPtr = (shaderType == cShaderTypeVertex ? pass->getVertexProgramParameters() : pass->getFragmentProgramParameters());
    if (!fragPtr->hasNamedParameters())
    {
        LogError("RocketMaterialEditor::ReadShaderNamedParameter: Program has no named parameters.");
        return QVariant();
    }

    Ogre::GpuConstantDefinitionIterator mapIter = fragPtr->getConstantDefinitionIterator();
    while(mapIter.hasMoreElements())
    {
        QString paramName(mapIter.peekNextKey().c_str());
        const Ogre::GpuConstantDefinition &paramDef = mapIter.getNext();
        if (paramName.lastIndexOf("[0]") != -1) // Filter names that end with '[0]'
            continue;
        if (paramName != namedParameter)
            continue;

        bool isFloat = paramDef.isFloat();
        if (!isFloat)
        {
            LogError("RocketMaterialEditor::ReadShaderNamedParameter: Only float based parameters can be read.");
            return QVariant();
        }

        size_t size = paramDef.elementSize * paramDef.arraySize;
        if (type == QVariant::Double)
        {
            if (size >= 1)
            {
                float value = 0.0f;
                fragPtr->_readRawConstants(paramDef.physicalIndex, 1, &value);
                return QVariant(static_cast<double>(value));
            }
            else
                LogError(QString("RocketMaterialEditor::ReadShaderNamedParameter: Parameter size %1 not enough for QVariant::Double").arg(size));
        }
        else if (type == QVariant::List)
        {
            QVariantList retList;
            float *ret = new float[size];
            fragPtr->_readRawConstants(paramDef.physicalIndex, size, ret);
            for (size_t i=0; i<size; ++i)
                retList << ret[i];
            delete[] ret;
            return retList;
        }
        else if (type == QVariant::Vector2D)
        {
            if (size >= 2)
            {
                float2 value(0.0f, 0.0f);
                fragPtr->_readRawConstants(paramDef.physicalIndex, 2, &value.x);
                return QVariant(QVector2D(value.x, value.y));
            }
            else
                LogError(QString("RocketMaterialEditor::ReadShaderNamedParameter: Parameter size %1 not enough for QVariant::Double").arg(size));
        }
    }
    return QVariant();
}

IRocketAssetEditor::SerializationResult RocketMaterialEditor::ResourceData()
{
    IRocketAssetEditor::SerializationResult result;
    result.first = false;

    if (material_.expired())
    {
        ShowError("Saving Aborted", "Material has been unloaded during editing. Close the editor and try again. If the material is broken you need to edit it manually with the Text Editor.<br><br>This tool can not open malformed materials.");
        return result;
    }
    
    // Validate textures
    for(int i=0; i<ui_.texturesLayout->count(); ++i)
    {
        QLayoutItem *item = ui_.texturesLayout->itemAt(i);
        if (!item || !item->widget())
            continue;

        RocketMaterialTextureWidget *editor = dynamic_cast<RocketMaterialTextureWidget*>(item->widget());
        if (editor && editor->State() != RocketMaterialTextureWidget::TS_OK)
        {
            QString message;
            if (editor->State() == RocketMaterialTextureWidget::TS_Error)
                message = QString("Texture %1 at index %2 is invalid").arg(editor->Texture().split("/").last()).arg(editor->TextureUnitIndex()+1);
            else if (editor->State() == RocketMaterialTextureWidget::TS_VerifyingTexture)
                message = QString("Texture %1 at index %2 is still being verified").arg(editor->Texture().split("/").last()).arg(editor->TextureUnitIndex()+1);
            ShowError("Saving Aborted", message);
            return result;
        }
    }

    std::vector<u8> data;
    if (!material_.lock()->SerializeTo(data, ""))
    {
        ShowError("Saving Failed", "Material generation failed. Close the editor and try again. If the material is broken you need to edit it manually with the Text Editor.<br><br>This tool can not open malformed materials.");
        SetError("Material generation failed");
        return result;
    }

    QByteArray script = QByteArray((const char*)&data[0], static_cast<int>(data.size()));
    QByteArray processedScript = PostProcessMaterialScript(script);

    result.first = true;
    result.second = (!processedScript.isEmpty() ? processedScript : script);
    return result;
}

QByteArray RocketMaterialEditor::PostProcessMaterialScript(QByteArray &script)
{
    QStringList linesToRemove;
    linesToRemove << "param_named texMatrix0"
                  << "param_named invShadowMapSize"
                  << "param_named shadowMapSize"
                  << "param_named texMatrixScaleBias1"
                  << "param_named texMatrixScaleBias2"
                  << "param_named texMatrixScaleBias3"
                  << "param_named fixedDepthBias"
                  << "param_named gradientScaleBias"
                  << "param_named shadowFadeDist"
                  << "param_named shadowMaxDist";

    /// @todo This is kind of dangerous if the user has a texture with this name!
    QHash<QString, QString> occurancesToReplace;
    occurancesToReplace["texture KernelRotation.png"] = "texture Ogre Media:KernelRotation.png";

    /** Find texture that have a different Name() to NameInternal()
        meaning there was some trickery being done to Ogre to allow
        loading eg. crn textures. */
    for(int i=0, len=ui_.texturesLayout->count(); i<len; ++i)
    {
        QLayoutItem *item = ui_.texturesLayout->itemAt(i);
        if (!item)
            continue;

        RocketMaterialTextureWidget *editor = dynamic_cast<RocketMaterialTextureWidget*>(item->widget());
        if (editor && !editor->Texture().isEmpty())
        {
            QString assetRef = editor->Url();
            shared_ptr<TextureAsset> texPtr = plugin_->Fw()->Asset()->FindAsset<TextureAsset>(assetRef);
            if (texPtr.get() && texPtr->NameInternal().compare(assetRef, Qt::CaseSensitive) != 0)
                occurancesToReplace[texPtr->NameInternal()] = assetRef;
        }
    }

    QStringList occurancesToReplaceKeys = occurancesToReplace.keys();
    
    // Tabs to spaces
    script.replace('\t', "    ");

    QByteArray processedScript;
    QTextStream writer(&processedScript, QIODevice::ReadWrite);
    QTextStream reader(&script, QIODevice::ReadWrite);

    writer.setCodec("UTF-8");
    reader.setCodec("UTF-8");

    while(!reader.atEnd())
    {
        QString line = reader.readLine();

        bool skipLine = false;
        foreach(const QString &toRemove, linesToRemove)
        {
            if (line.indexOf(toRemove, 0, Qt::CaseSensitive) != -1)
            {
                skipLine = true;
                break;
            }
        }

        if (!skipLine)
        {
            foreach(const QString &toReplace, occurancesToReplaceKeys)
            {
                if (line.contains(toReplace, Qt::CaseSensitive))
                {
                    //qDebug() << "Fixing" << toReplace << "->" << occurancesToReplace[toReplace];
                    line.replace(toReplace, occurancesToReplace[toReplace], Qt::CaseSensitive);
                }
            }
            writer << line << endl;
        }
    }

    writer.flush();
    return processedScript;
}

void RocketMaterialEditor::AboutToClose()
{
    emit CloseTextureEditors();
}

bool RocketMaterialEditor::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == ui_.buttonAmbient || watched == ui_.buttonDiffuse ||
        watched == ui_.buttonSpecular || watched == ui_.buttonEmissive)
    {
        if (e->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *me = dynamic_cast<QMouseEvent*>(e);
            if (me && me->button() == Qt::RightButton)
            {
                QAbstractButton *button = qobject_cast<QAbstractButton*>(watched);
                if (button && !button->isEnabled())
                    return false;
                    
                QVariant colorType = button->property("colorType");
                if (colorType.isNull() || !colorType.isValid())
                    return false;

                // Don't show context menu if there is a open editor for this color
                if (ActivateColorDialog(colorType.toInt()))
                    return false;

                // Init color menu if first show
                if (!colorMenu_)
                {
                    colorMenu_ = new QMenu(this);
                    QAction *reset = colorMenu_->addAction(QIcon(":/images/icon-unchecked.png"), "Reset to Default");
                    connect(reset, SIGNAL(triggered()), SLOT(OnRestoreDefaultColor()));

                    increaseColorMenu_ = colorMenu_->addMenu(QIcon(":/images/icon-plus-32x32.png"), "Increase Intensity");
                    decreaseColorMenu_ = colorMenu_->addMenu(QIcon(":/images/icon-minus-32x32.png"), "Decrease Intensity");

                    int step = 10;
                    for(int i=10; i<100; i+=step)
                    {
                        QAction *increase = increaseColorMenu_->addAction(QString("+ %1%").arg(i));
                        increase->setProperty("multiplier", static_cast<qreal>(i)/100.0+1.0);
                        connect(increase, SIGNAL(triggered()), SLOT(OnIncreaseOrDecreaseColor()));
                        
                        QAction *decrease = decreaseColorMenu_->addAction(QString("- %1%").arg(i));
                        decrease->setProperty("multiplier", 1.0-static_cast<qreal>(i)/100.0);
                        connect(decrease, SIGNAL(triggered()), SLOT(OnIncreaseOrDecreaseColor()));

                        if (i == 50)
                            step = 25;
                    }
                }
                
                // Set the color type to actions for slot inspection
                foreach(QAction *increase, increaseColorMenu_->actions())
                    increase->setProperty("colorType", colorType.toInt());
                foreach(QAction *decrease, decreaseColorMenu_->actions())
                    decrease->setProperty("colorType", colorType.toInt());
                if (colorMenu_->actions().first())
                    colorMenu_->actions().first()->setProperty("colorType", colorType.toInt());
    
                colorMenu_->popup(QCursor::pos());
                return true;
            }
        }
    }
    return false;
}

bool RocketMaterialEditor::ActivateColorDialog(int type)
{
    // Check if we already have a editor open for this color
    for (int i=0; i<colorDialogs_.size(); ++i)
    {
        if (!colorDialogs_[i])
        {
            colorDialogs_.removeAt(i);
            --i;
            continue;
        }

        QVariant typeVariant = colorDialogs_[i]->property("colorType");
        if (!typeVariant.isValid() || typeVariant.isNull())
            continue;
        if (type == typeVariant.toInt())
        {
            // Activate the existing color dialog
            if (colorDialogs_[i]->isMinimized())
                colorDialogs_[i]->showNormal();
            colorDialogs_[i]->setFocus(Qt::ActiveWindowFocusReason);
            colorDialogs_[i]->activateWindow();
            return true;
        }
    }
    return false;
}

// RocketMaterialTextureWidget

RocketMaterialTextureWidget::RocketMaterialTextureWidget(RocketPlugin *plugin, const QString &materialRef, const MeshmoonShaderInfo &info,
                                                         const QString &textureType, int textureUnitIndex, QWidget *parent) :
    QWidget(parent),
    plugin_(plugin),
    info_(info),
    materialRef_(materialRef),
    materialBase_(""),
    textureType_(textureType),
    textureUnitIndex_(textureUnitIndex),
    state_(TS_VerifyingTexture)
{
    int separatorIndex = materialRef_.lastIndexOf("/");
    if (separatorIndex != -1)
        materialBase_ = materialRef_.left(separatorIndex+1);

    setAcceptDrops(true);

    ui_.setupUi(this);
    ui_.titleTiling->hide();
    ui_.separatorTiling->hide();
    ui_.doubleSpinBoxTilingX->hide();
    ui_.doubleSpinBoxTilingY->hide();
    ui_.maxAnisotropyComboBox->hide();

    pixmapOK_ = QPixmap(":/images/icon-checked-ball-32x32.png");
    pixmapOK_ = pixmapOK_.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    pixmapError_ = QPixmap(":/images/icon-x-24x24.png");
    pixmapError_ = pixmapError_.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    movieLoading_ = new QMovie(":/images/loader.gif", "GIF");
    if (movieLoading_->isValid())
    {
        movieLoading_->setCacheMode(QMovie::CacheAll);
        movieLoading_->setScaledSize(QSize(16,16));
    }
    
    /// Ogre::TextureUnitState::TextureAddressingMode
    QStringList addressingModes;
    addressingModes << "Wrap" << "Mirror" << "Clamp" << "Border";
    ui_.addressingComboBox->addItems(addressingModes);
    
    QStringList layerBlendOperation;
    layerBlendOperation << "Source 1" << "Source 2" 
                        << "Modulate" << "Modulate x2"
                        << "Modulate x4" << "Add" << "Add Signed"
                        << "Add Smooth" << "Subtract"
                        << "Blend Diffuse Alpha"
                        << "Blend Texture Alpha"
                        << "Blend Current Alpha";
                        // LBX_BLEND_MANUAL not supported atm. Needs blend float to be defined in UI.
                        // LBX_DOTPRODUCT not supported atm. Needs two colors to the UI.
                        // LBX_BLEND_DIFFUSE_COLOUR not supported atm.

    ui_.colorOperationComboBox->addItems(layerBlendOperation);
    
    QStringList filteringModes;
    filteringModes << "None" << "Bilinear" << "Trilinear" << "Anisotropic";
                   
    ui_.filteringComboBox->addItems(filteringModes);

    QStringList maxAnisotropyModes;
    maxAnisotropyModes << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9";
    ui_.maxAnisotropyComboBox->addItems(maxAnisotropyModes);

    ui_.textureTypeTitle->setText(textureType);

    connect(ui_.textureLineEdit, SIGNAL(editingFinished()), SLOT(OnTextureChanged()));

    connect(ui_.addressingComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetAddressing(int)));
    connect(ui_.colorOperationComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetColorOperation(int)));
    connect(ui_.filteringComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetFilteringFromComboBoxIndex(int)));
    connect(ui_.maxAnisotropyComboBox, SIGNAL(currentIndexChanged(int)), SLOT(SetMaxAnisotropyFromComboBoxIndex(int)));
    
    connect(ui_.buttonStorageSelection, SIGNAL(clicked()), SLOT(OnSelectTextureFromStorage()));
    connect(ui_.buttonTextureViewer, SIGNAL(clicked()), SLOT(OnOpenTextureViewer()));
    
    connect(ui_.uvSpinBox, SIGNAL(valueChanged(int)), SLOT(SetUV(int)));
    connect(ui_.doubleSpinBoxTilingX, SIGNAL(valueChanged(double)), SLOT(SetTilingX(double)));
    connect(ui_.doubleSpinBoxTilingY, SIGNAL(valueChanged(double)), SLOT(SetTilingY(double)));

    SetState(TS_OK);
}

RocketMaterialTextureWidget::~RocketMaterialTextureWidget()
{
    OnCloseTextureEditors();
}

void RocketMaterialTextureWidget::dragEnterEvent(QDragEnterEvent *e)
{
    if (!e || !e->mimeData())
        return;
        
    QStringList urls = MeshmoonStorage::ParseStorageSchemaReferences(e->mimeData());
    if (urls.size() != 1)
        return;
        
    QString absoluteRef = e->mimeData()->property("absoluteRef").toString();
    QString assetType = plugin_->Fw()->Asset()->ResourceTypeForAssetRef(absoluteRef);
    if (assetType == "Texture")
    {
        ui_.frameTexMain->setStyleSheet(QString("QFrame#frameTexMain { background-color: %1; } QWidget { color: white; }").arg(Meshmoon::Theme::BlueStr));
        e->acceptProposedAction();
    }
}

void RocketMaterialTextureWidget::dragLeaveEvent(QDragLeaveEvent *e)
{
    dropEvent(0);
    if (e)
        e->accept();
}

void RocketMaterialTextureWidget::dropEvent(QDropEvent *e)
{
    ui_.frameTexMain->setStyleSheet("QFrame#frameTexMain { background-color: rgba(198, 198, 198, 100); }");
    if (!e || !e->mimeData())
        return;

    QStringList urls = MeshmoonStorage::ParseStorageSchemaReferences(e->mimeData());
    if (urls.size() != 1)
        return;

    QString absoluteRef = e->mimeData()->property("absoluteRef").toString();
    QString assetType = plugin_->Fw()->Asset()->ResourceTypeForAssetRef(absoluteRef);
    if (assetType == "Texture")
    {
        SetTexture(absoluteRef);
        e->acceptProposedAction();
    }
}

RocketMaterialTextureWidget::TextureState RocketMaterialTextureWidget::State() const
{
    return state_;
}

QString RocketMaterialTextureWidget::TextureType() const
{
    return ui_.textureTypeTitle->text();
}

int RocketMaterialTextureWidget::TextureUnitIndex() const
{
    return textureUnitIndex_;
}

void RocketMaterialTextureWidget::SetTexture(const QString &textureRef, bool apply)
{
    // Convert to relative reference if the ref starts with the editors material ref base
    ui_.textureLineEdit->blockSignals(true);
    ui_.textureLineEdit->setText((materialBase_.isEmpty() ? textureRef : (!textureRef.startsWith(materialBase_, Qt::CaseSensitive) ? textureRef : textureRef.mid(materialBase_.length()))));
    ui_.textureLineEdit->blockSignals(false);

    OnTextureChanged();
}

QString RocketMaterialTextureWidget::Texture() const
{
    return ui_.textureLineEdit->text().trimmed();
}

QString RocketMaterialTextureWidget::Url() const
{
    return plugin_->GetFramework()->Asset()->ResolveAssetRef(materialRef_, Texture());
}

void RocketMaterialTextureWidget::SetUV(int index, bool apply)
{
    ui_.uvSpinBox->blockSignals(true);
    ui_.uvSpinBox->setValue(index);
    ui_.uvSpinBox->blockSignals(false);
    
    if (apply)
        emit UVChanged(textureUnitIndex_, index);
}

void RocketMaterialTextureWidget::SetTilingX(double tiling, bool apply)
{
    SetTiling(float2(static_cast<float>(tiling), static_cast<float>(ui_.doubleSpinBoxTilingY->value())), apply);
}

void RocketMaterialTextureWidget::SetTilingY(double tiling, bool apply)
{
    SetTiling(float2(static_cast<float>(ui_.doubleSpinBoxTilingX->value()), static_cast<float>(tiling)), apply);
}

void RocketMaterialTextureWidget::SetTiling(float2 tiling, bool apply)
{
    ui_.doubleSpinBoxTilingX->blockSignals(true);
    ui_.doubleSpinBoxTilingY->blockSignals(true);
    
    ui_.doubleSpinBoxTilingX->setValue(static_cast<double>(tiling.x));
    ui_.doubleSpinBoxTilingY->setValue(static_cast<double>(tiling.y));

    ui_.titleTiling->show();
    ui_.separatorTiling->show();
    ui_.doubleSpinBoxTilingX->show();
    ui_.doubleSpinBoxTilingY->show();

    ui_.doubleSpinBoxTilingX->blockSignals(false);
    ui_.doubleSpinBoxTilingY->blockSignals(false);

    if (apply)
        emit TilingChanged(textureUnitIndex_, tiling);
}

int RocketMaterialTextureWidget::UV() const
{
    return ui_.uvSpinBox->value();
}

void RocketMaterialTextureWidget::SetFilteringFromComboBoxIndex(int index, bool apply)
{   
    SetFiltering(index, apply);
}

Ogre::TextureFilterOptions RocketMaterialTextureWidget::FilteringFromComplex(int min, int mag, int mip)
{
    // TFO_NONE         equal to min=FO_POINT, mag=FO_POINT, mip=FO_NONE
    // TFO_BILINEAR     equal to min=FO_LINEAR, mag=FO_LINEAR, mip=FO_POINT
    // TFO_TRILINEAR    equal to min=FO_LINEAR, mag=FO_LINEAR, mip=FO_LINEAR
    // TFO_ANISOTROPIC  equal to min=FO_ANISOTROPIC, max=FO_ANISOTROPIC, mip=FO_LINEAR

    Ogre::FilterOptions oMin = static_cast<Ogre::FilterOptions>(min);
    Ogre::FilterOptions oMag = static_cast<Ogre::FilterOptions>(mag);
    Ogre::FilterOptions oMip = static_cast<Ogre::FilterOptions>(mip);

    Ogre::TextureFilterOptions filtering = Ogre::TFO_NONE;
    if (oMin == Ogre::FO_LINEAR && oMag == Ogre::FO_LINEAR && oMip == Ogre::FO_POINT)
        filtering = Ogre::TFO_BILINEAR;
    else if (oMin == Ogre::FO_LINEAR && oMag == Ogre::FO_LINEAR && oMip == Ogre::FO_LINEAR)
        filtering = Ogre::TFO_TRILINEAR;
    else if (oMin == Ogre::FO_ANISOTROPIC && oMag == Ogre::FO_ANISOTROPIC && oMip == Ogre::FO_LINEAR)
        filtering = Ogre::TFO_ANISOTROPIC;
    return filtering;
}

void RocketMaterialTextureWidget::SetFilteringComplex(int min, int mag, int mip, bool apply)
{
    SetFiltering(static_cast<int>(FilteringFromComplex(min, mag, mip)), apply);
}

void RocketMaterialTextureWidget::SetFiltering(int ogreFiltering, bool apply)
{
    ui_.filteringComboBox->blockSignals(true);
    ui_.maxAnisotropyComboBox->blockSignals(true);
    
    ui_.filteringComboBox->setCurrentIndex(ogreFiltering);
    ui_.maxAnisotropyComboBox->setVisible(static_cast<Ogre::TextureFilterOptions>(ogreFiltering) == Ogre::TFO_ANISOTROPIC);

    ui_.filteringComboBox->blockSignals(false);
    ui_.maxAnisotropyComboBox->blockSignals(false);

    if (apply)
        emit FilteringChanged(textureUnitIndex_, static_cast<int>(ogreFiltering));
}

int RocketMaterialTextureWidget::Filtering() const
{
    return ui_.filteringComboBox->currentIndex();
}

void RocketMaterialTextureWidget::SetMaxAnisotropyFromComboBoxIndex(int index, bool apply)
{
    SetMaxAnisotropy(index + 1, apply);
}

void RocketMaterialTextureWidget::SetMaxAnisotropy(uint maxAnisotropy, bool apply)
{
    ui_.maxAnisotropyComboBox->blockSignals(true);
    
    if (maxAnisotropy < 1)
        maxAnisotropy = 1;   
    else if (maxAnisotropy > 9)
        maxAnisotropy = 9;
    ui_.maxAnisotropyComboBox->setCurrentIndex(maxAnisotropy - 1);
                
    ui_.maxAnisotropyComboBox->blockSignals(false);

    if (apply)
        emit MaxAnisotropyChanged(textureUnitIndex_, maxAnisotropy);
}

uint RocketMaterialTextureWidget::MaxAnisotropy() const
{
    uint maxAnisotropy = (ui_.maxAnisotropyComboBox->currentIndex() + 1);
    return (maxAnisotropy <= 9 ? maxAnisotropy : 9);
}

void RocketMaterialTextureWidget::SetAddressing(int addressing, bool apply)
{
    if (addressing >= ui_.addressingComboBox->count())
        addressing = 0;

    ui_.addressingComboBox->blockSignals(true);
    ui_.addressingComboBox->setCurrentIndex(addressing);
    ui_.addressingComboBox->blockSignals(false);
    
    if (apply)
        emit AddressingChanged(textureUnitIndex_, addressing);
}

int RocketMaterialTextureWidget::Addressing() const
{
    return ui_.addressingComboBox->currentIndex();
}

void RocketMaterialTextureWidget::SetColorOperation(int operation, bool apply)
{
    if (operation >= ui_.colorOperationComboBox->count())
    {
        LogWarning("[RocketMaterialEditor]: Found color operation from material that is not supported by this editor: Ogre::LayerBlendOperationEx " + QString::number(operation));
        operation = 0;
    }

    ui_.colorOperationComboBox->blockSignals(true);
    ui_.colorOperationComboBox->setCurrentIndex(operation);    
    ui_.colorOperationComboBox->blockSignals(false);
    
    if (apply)
        emit ColorOperationChanged(textureUnitIndex_, operation);
}

int RocketMaterialTextureWidget::ColorOperation() const
{
    return ui_.colorOperationComboBox->currentIndex();
}

float2 RocketMaterialTextureWidget::Tiling() const
{
    return float2(ui_.doubleSpinBoxTilingX->value(), ui_.doubleSpinBoxTilingY->value());
}

void RocketMaterialTextureWidget::SetState(TextureState state)
{
    // Enable open editor button if we have a valid texture set
    ui_.buttonTextureViewer->setEnabled((state == TS_OK && !Texture().isEmpty()));

    if (state_ != state)
    {
        state_ = state;   

        if (state == TS_VerifyingTexture)
        {
            if (movieLoading_->isValid())
            {
                ui_.labelIcon->setMovie(movieLoading_);
                movieLoading_->start();
            }
            else
                ui_.labelIcon->clear();
        }
        else if (state == TS_Error)
        {
            ui_.labelIcon->setPixmap(pixmapError_);
            if (movieLoading_->isValid()) movieLoading_->stop();
        }
        else if (state == TS_OK)
        {
            ui_.labelIcon->setPixmap(pixmapOK_);
            if (movieLoading_->isValid()) movieLoading_->stop();
        }
    }

    ui_.textureLineEdit->setStyleSheet(QString("color: %1;").arg((state == TS_OK ? "black" : (state == TS_Error ? "red" : "blue"))));
    if (Texture().isEmpty() && textureType_ == "Light map")
    {
        ui_.textureTypeTitle->setStyleSheet("color: red;");
        ui_.textureTypeTitle->setToolTip("Leaving the light map empty will result in black rendering"); 
    }
    else
    {
        ui_.textureTypeTitle->setStyleSheet(QString("color: %1;").arg((state == TS_Error ? "red" : "black")));
        ui_.textureTypeTitle->setToolTip(state == TS_Error ? "Given texture reference is not valid" : "");
    }
}

void RocketMaterialTextureWidget::OnOpenTextureViewer()
{
    // Do we even have a ref?
    if (Texture().isEmpty())
        return;
    // Is the ref valid?
    if (state_ != TS_OK)
        return;
    
    if (textureViewer_)
    {
        textureViewer_->setFocus(Qt::ActiveWindowFocusReason);
        textureViewer_->activateWindow();
        return;
    }

    QWidget *topMostParent = RocketNotifications::TopMostParent(this);

    // We want to be able to open an supported images, not just storage refs.
    // Lets go around MeshmoonStorage::OpenVisualEditor(textureItem); and directly show the editor here.
    QString suffix = QFileInfo(Texture()).suffix();
    if (RocketTextureEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive))
    {
        textureViewer_ = new RocketTextureEditor(plugin_, new MeshmoonAsset(Url(), MeshmoonAsset::Texture, this));
        textureViewer_->setAttribute(Qt::WA_DeleteOnClose, true);
        textureViewer_->Start();
        textureViewer_->Show();

        plugin_->Notifications()->CenterToWindow(topMostParent, textureViewer_);
    }
    else
        plugin_->Notifications()->ShowSplashDialog("Texture Viewer does not support opening " + suffix.toUpper() + " file format.", "", QMessageBox::Ok, QMessageBox::Ok, topMostParent);
}

void RocketMaterialTextureWidget::OnSelectTextureFromStorage()
{
    if (!plugin_ || !plugin_->Storage())
        return;

    // Get the texture suffixes from the asset factory if possible or default to a comprehensive enough list that all clients should handle well.
    QStringList suffixes;
    AssetTypeFactoryPtr textureFactory = plugin_->GetFramework()->Asset()->GetAssetTypeFactory("Texture");
    if (textureFactory.get())
        suffixes = textureFactory->TypeExtensions();
    else
        suffixes << ".dds" << ".jpg" << ".jpeg" << ".png" << ".crn" << ".gif" << ".bmp" << ".tga" << ".targa" << ".tiff" << ".tif";
    
    QWidget *topMostParent = RocketNotifications::TopMostParent(this);
    RocketStorageSelectionDialog *dialog = plugin_->Storage()->ShowSelectionDialog(suffixes, true, lastStorageDir_, topMostParent);
    if (dialog)
    {
        connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), SLOT(OnTextureSelectedFromStorage(const MeshmoonStorageItem&)), Qt::UniqueConnection);
        connect(dialog, SIGNAL(Canceled()), SLOT(OnStorageSelectionClosed()), Qt::UniqueConnection);
    }
}

void RocketMaterialTextureWidget::OnTextureSelectedFromStorage(const MeshmoonStorageItem &item)
{
    OnStorageSelectionClosed();

    if (!item.IsNull())
        SetTexture(item.fullAssetRef);
}

void RocketMaterialTextureWidget::OnStorageSelectionClosed()
{
    // Store the last storage location into memory for the next dialog open
    RocketStorageSelectionDialog *dialog = dynamic_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageDir_ = dialog->CurrentDirectory();
}

void RocketMaterialTextureWidget::OnTextureChanged()
{
    QString textureRef = Texture();
    if (lastVerifiedTextureRef_.compare(textureRef, Qt::CaseSensitive) == 0)
        return;
    lastVerifiedTextureRef_ = textureRef;

    if (!textureRef.isEmpty())
    {
        // Resolve this ref with the material as a context
        AssetAPI *assetAPI = plugin_->GetFramework()->Asset();
        QString resolvedRef = assetAPI->ResolveAssetRef(materialRef_, textureRef);
        
        // Already loaded asset.
        AssetPtr existing = assetAPI->GetAsset(resolvedRef);
        if (existing.get())
        {
            // Check that this is a texture
            if (existing->Type() == "Texture")
            {
                SetState(TS_OK);
                emit TextureChanged(textureUnitIndex_, textureRef);
            }
            else
            {
                SetState(TS_Error);
                LogError("[RocketMaterialEditor]: Proposed texture is loaded to the asset system but is not with texture type: " + existing->Name() + " Resolved type: " + existing->Type());
            }   
            return;
        }

        AssetTransferPtr transfer = assetAPI->RequestAsset(resolvedRef, "Binary", true);
        connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnTextureTransferCompleted(AssetPtr)));
        connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnTextureTransferFailed(IAssetTransfer*, QString)));

        SetState(TS_VerifyingTexture);
    }
    else
    {
        SetState(TS_OK);
        emit TextureChanged(textureUnitIndex_, textureRef);
    }
    
    // If the ref was changed close down open editor that points to the old ref.
    OnCloseTextureEditors();
}

void RocketMaterialTextureWidget::OnTextureTransferCompleted(AssetPtr asset)
{
    QString assetRef = asset->Name();
    QString assetType = asset->Type();
    QString resolvedAssetType = plugin_->GetFramework()->Asset()->ResourceTypeForAssetRef(assetRef);
    asset.reset();

    // Unload if not a texture (binary fetch of a texture or a non texture asset to begin with)
    if (assetType != "Texture" || resolvedAssetType != "Texture")
        plugin_->GetFramework()->Asset()->ForgetAsset(assetRef, false);

    // Send this asset ref forward for material live preview and later saving.
    if (resolvedAssetType == "Texture")
    {
        SetState(TS_OK);
        emit TextureChanged(textureUnitIndex_, Texture());
    }
    else
    {
        SetState(TS_Error);
        LogError("[RocketMaterialEditor]: Proposed texture is not of texture type: " + assetRef + " Resolved type: " + resolvedAssetType);
    }
}

void RocketMaterialTextureWidget::OnTextureTransferFailed(IAssetTransfer *transfer, QString error)
{
    SetState(TS_Error);
}

void RocketMaterialTextureWidget::OnCloseTextureEditors()
{
    if (textureViewer_)
        textureViewer_->close();
}

const Ui::RocketMaterialTextureWidget &RocketMaterialTextureWidget::Ui() const
{
    return ui_;
}

const MeshmoonShaderInfo &RocketMaterialTextureWidget::ShaderInfo() const
{
    return info_;
}
