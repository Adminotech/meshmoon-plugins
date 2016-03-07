/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketMeshEditor.h"
#include "RocketAssetSelectionWidget.h"
#include "RocketOgreSceneRenderer.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "FrameAPI.h"
#include "InputAPI.h"
#include "ConfigAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include "IRenderer.h"
#include "OgreWorld.h"
#include "OgreMeshAsset.h"
#include "OgreMaterialAsset.h"

#include "Math/MathFunc.h"

#include <OgreEntity.h>
#include <OgreSubEntity.h>
#include <OgreSceneNode.h>
#include <OgreSceneManager.h>
#include <OgreViewport.h>

#include <OgreMaterial.h>
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>

#include <QPixmap>
#include <QCursor>

RocketMeshEditor::RocketMeshEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource) :
    IRocketAssetEditor(plugin, resource, "Mesh Viewer", "Ogre Mesh")
{
    Initialize();
}

RocketMeshEditor::RocketMeshEditor(RocketPlugin *plugin, MeshmoonAsset *resource) :
    IRocketAssetEditor(plugin, resource, "Mesh Viewer", "Ogre Mesh")
{
    Initialize();
}

RocketMeshEditor::~RocketMeshEditor()
{
    WriteConfig();

    // Unload manually created asset
    if (!mesh_.expired())
        plugin_->GetFramework()->Asset()->ForgetAsset(mesh_.lock()->Name(), false);
    mesh_.reset();

    // Destroy scene renderer. This will destroy any scene nodes created with it.
    SAFE_DELETE(ogreSceneRenderer_);
    meshNode_ = 0;
    meshEntity_ = 0;
}

QStringList RocketMeshEditor::SupportedSuffixes()
{
    return QStringList() << "mesh";
}

QString RocketMeshEditor::EditorName()
{
    return "Mesh Viewer";
}

void RocketMeshEditor::Initialize()
{
    LC = "[RocketMeshEditor]: ";

    editor_ = 0;
    colorMenu_ = 0;

    ogreSceneRenderer_ = 0;
    meshNode_ = 0;
    meshEntity_ = 0;
    directionalLight_ = 0;
    
    mouseInteraction_ = false;
    statsUpdateT_ = 0.0f;

    InitializeUi();
}

void RocketMeshEditor::InitializeUi()
{
    SetSaveEnabled(false);

    // Create mesh editor ui
    editor_ = new QWidget(this);
    ui_.setupUi(editor_);

    // Connect signals
    connect(ui_.rotationXSlider, SIGNAL(valueChanged(int)), SLOT(OnRotationXChanged(int)));
    connect(ui_.rotationYSlider, SIGNAL(valueChanged(int)), SLOT(OnRotationYChanged(int)));
    connect(ui_.rotationZSlider, SIGNAL(valueChanged(int)), SLOT(OnRotationZChanged(int)));
    connect(ui_.directionalCheckBox, SIGNAL(toggled(bool)), SLOT(OnDirectionalEnabledChanged(bool)));
    connect(ui_.boundingBoxCheckBox, SIGNAL(toggled(bool)), SLOT(OnBoundingBoxEnabledChanged(bool)));
    connect(ui_.wireframeCheckBox, SIGNAL(toggled(bool)), SLOT(OnWireframeEnabledChanged(bool)));

    ui_.buttonBackground->setProperty("colorType", "Background");
    ui_.buttonAmbient->setProperty("colorType", "Ambient");
    ui_.buttonDirectional->setProperty("colorType", "Directional");

    connect(ui_.buttonBackground, SIGNAL(clicked()), SLOT(OnPickColor()));
    connect(ui_.buttonAmbient, SIGNAL(clicked()), SLOT(OnPickColor()));
    connect(ui_.buttonDirectional, SIGNAL(clicked()), SLOT(OnPickColor()));

    ui_.renderTargetWidget->installEventFilter(this);
    ui_.buttonBackground->installEventFilter(this);
    ui_.buttonAmbient->installEventFilter(this);
    ui_.buttonDirectional->installEventFilter(this);
    
    // Add to base editor ui
    ui.contentLayout->addWidget(editor_);
    
    if (!RestoreGeometryFromConfig("mesh-editor", true))
        resize(800, 600);

    resizeTimer_.setSingleShot(true);
    connect(&resizeTimer_, SIGNAL(timeout()), SLOT(RenderImage()));
}

QSize RocketMeshEditor::RenderSurfaceSize() const
{
    return ui_.renderTargetWidget->size();
}

QString RocketMeshEditor::UniqueMeshAssetRef()
{
    return QString::fromStdString(plugin_->GetFramework()->Renderer()->GetUniqueObjectName(
        QString("generated://" + this->filename + "_").toStdString())) + "_RocketMeshEditor.mesh";
}

void RocketMeshEditor::OnTransformChanged(Transform &value, bool apply)
{
    // Validate rotation to be inside 0-259 degrees
    if (value.rot.x < 0) value.rot.x += 360.f;
    if (value.rot.y < 0) value.rot.y += 360.f;
    if (value.rot.z < 0) value.rot.z += 360.f;
    if (value.rot.x >= 360.f) value.rot.x = fmod(value.rot.x, 360.f);
    if (value.rot.y >= 360.f) value.rot.y = fmod(value.rot.y, 360.f);
    if (value.rot.z >= 360.f) value.rot.z = fmod(value.rot.z, 360.f);
    
    OnRotationChanged(value.rot, false);

    if (apply && ogreSceneRenderer_ && meshNode_)
        ogreSceneRenderer_->SetNodeTransform(meshNode_, value);
}

void RocketMeshEditor::OnRotationChanged(const float3 &value, bool apply)
{
    OnRotationXChanged(RoundInt(value.x), false);
    OnRotationYChanged(RoundInt(value.y), false);
    OnRotationZChanged(RoundInt(value.z), false);

    if (apply && ogreSceneRenderer_ && meshNode_)
    {
        Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
        t.rot = value;
        ogreSceneRenderer_->SetNodeTransform(meshNode_, t);
    }
}

void RocketMeshEditor::OnRotationXChanged(int value, bool apply)
{
    ui_.rotationXSlider->blockSignals(true);
    ui_.rotationXSlider->setValue(value);
    ui_.rotationXSlider->blockSignals(false);

    ui_.rotationXSlider->setToolTip(QString("%1&deg;").arg(value));

    if (apply && ogreSceneRenderer_ && meshNode_)
    {
        Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
        t.rot.x = value;
        ogreSceneRenderer_->SetNodeTransform(meshNode_, t);
    }
}

void RocketMeshEditor::OnRotationYChanged(int value, bool apply)
{
    ui_.rotationYSlider->blockSignals(true);
    ui_.rotationYSlider->setValue(value);
    ui_.rotationYSlider->blockSignals(false);

    ui_.rotationYSlider->setToolTip(QString("%1&deg;").arg(value));

    if (apply && ogreSceneRenderer_ && meshNode_)
    {
        Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
        t.rot.y = value;
        ogreSceneRenderer_->SetNodeTransform(meshNode_, t);
    }
}

void RocketMeshEditor::OnRotationZChanged(int value, bool apply)
{
    ui_.rotationZSlider->blockSignals(true);
    ui_.rotationZSlider->setValue(value);
    ui_.rotationZSlider->blockSignals(false);

    ui_.rotationZSlider->setToolTip(QString("%1&deg;").arg(value));

    if (apply && ogreSceneRenderer_ && meshNode_)
    {
        Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
        t.rot.z = value;
        ogreSceneRenderer_->SetNodeTransform(meshNode_, t);
    }
}

void RocketMeshEditor::OnDirectionalEnabledChanged(bool enabled)
{
    if (!ogreSceneRenderer_)
        return;
        
    if (enabled && !directionalLight_)
    {
        directionalLight_ = ogreSceneRenderer_->CreateLight(Ogre::Light::LT_DIRECTIONAL, Color(QColor(200, 200, 200)));
        if (directionalLight_)
        {
            directionalLight_->setDirection(float3(0.0f, -0.5f, 1.0f));
            OnDirectionalChanged(ReadColorConfigOrDefault("directional-light-color", Color(directionalLight_->getDiffuseColour())), true);
        }
    }
    else if (!enabled && directionalLight_)
    {
        ogreSceneRenderer_->DestroyObject(directionalLight_);
        directionalLight_ = 0;

        OnDirectionalChanged(Color(0.f, 0.f, 0.f, 0.f), false);
    }
    
    ui_.buttonDirectional->setEnabled((directionalLight_ != 0));

    WriteConfig(true, false, false, true);
}

Color RocketMeshEditor::ReadColorConfigOrDefault(const QString &configKey, Color defaultColor)
{
    if (plugin_->GetFramework()->Config()->HasValue("rocket-asset-editors", "mesh-editor", configKey))
    {
        QVariant colorVariant = plugin_->GetFramework()->Config()->Read("rocket-asset-editors", "mesh-editor", configKey);
        QColor configColor = colorVariant.value<QColor>();
        if (configColor.isValid())
        {
            configColor.setAlpha(255);
            return Color(configColor);
        }
    }
    return defaultColor;
}

void RocketMeshEditor::OnRestoreDefaultColor()
{
    QString type = sender() ? sender()->property("colorType").toString() : "";
    if (type == "Background")
        OnBackgroundChanged(QColor(20,20,20));
    else if (type == "Ambient")
        OnAmbientChanged(OgreWorld::DefaultSceneAmbientLightColor());
    else if (type == "Directional")
        OnDirectionalChanged(QColor(200, 200, 200));
}

void RocketMeshEditor::OnPickColor(QString type)
{
    if (!ogreSceneRenderer_ || !ogreSceneRenderer_->IsValid())
        return;

    if (!colorPicker_.isNull())
    {
        if (colorPicker_->isMinimized())
            colorPicker_->showNormal();
        colorPicker_->setFocus(Qt::ActiveWindowFocusReason);
        colorPicker_->activateWindow();
        return;
    }

    if (type.isEmpty() && sender())
        type = sender()->property("colorType").toString();
    if (type != "Background" && type != "Ambient" && type != "Directional")
        return;

    colorPicker_ = new QColorDialog(this);
    colorPicker_->setOption(QColorDialog::ShowAlphaChannel, false);
    colorPicker_->setWindowTitle("Color Picker");
    colorPicker_->setOption(QColorDialog::NoButtons);
    colorPicker_->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef __APPLE__
    colorPicker_->setOption(QColorDialog::DontUseNativeDialog, true);
#endif
    Color color(1.0f, 1.0f, 1.0f, 1.0f);
    if (type == "Background")
    {
        if (ogreSceneRenderer_->OgreViewport())
        {
            color = ogreSceneRenderer_->OgreViewport()->getBackgroundColour();
            connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnBackgroundChanged(const QColor&)));
        }
        else
        {
            colorPicker_->deleteLater();
            return;
        }
    }
    else if (type == "Ambient")
    {
        color = ogreSceneRenderer_->SceneManager()->getAmbientLight();
        connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnAmbientChanged(const QColor&)));
    }
    else if (type == "Directional")
    {
        if (directionalLight_)
        {
            color = directionalLight_->getDiffuseColour();
            connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnDirectionalChanged(const QColor&)));
        }
        else
        {
            colorPicker_->deleteLater();
            return;
        }
    }
    
    connect(colorPicker_, SIGNAL(destroyed()), SLOT(OnColorPickerClosed()));

    color.a = 1.0f;
    colorPicker_->setCurrentColor(color);
    colorPicker_->show();
    
    plugin_->GetFramework()->Ui()->MainWindow()->EnsurePositionWithinDesktop(colorPicker_, frameGeometry().topRight());
}

void RocketMeshEditor::OnColorPickerClosed()
{
    WriteConfig();
}

void RocketMeshEditor::OnAmbientChanged(const QColor &color, bool apply)
{
    ui_.buttonAmbient->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(color.red()).arg(color.green()).arg(color.blue()));

    if (apply && ogreSceneRenderer_ && ogreSceneRenderer_->SceneManager())
        ogreSceneRenderer_->SceneManager()->setAmbientLight(Color(color));
}

void RocketMeshEditor::OnBackgroundChanged(const QColor &color, bool apply)
{
    ui_.buttonBackground->setStyleSheet(QString("background-color: rgb(%1,%2,%3);")
        .arg(color.red()).arg(color.green()).arg(color.blue()));

    if (apply && ogreSceneRenderer_ && ogreSceneRenderer_->OgreViewport())
        ogreSceneRenderer_->OgreViewport()->setBackgroundColour(Color(color));
}

void RocketMeshEditor::OnDirectionalChanged(const QColor &color, bool apply)
{
    ui_.buttonDirectional->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha()));

    if (apply && directionalLight_)
        directionalLight_->setDiffuseColour(Color(color));
}

void SetShowBoundingBoxRecursive(Ogre::SceneNode* node, bool enable)
{
    if (!node)
        return;

    node->showBoundingBox(enable);
    for(int i=0, num=node->numChildren(); i<num; ++i)
    {
        Ogre::SceneNode* childNode = dynamic_cast<Ogre::SceneNode*>(node->getChild(i));
        if (childNode)
            SetShowBoundingBoxRecursive(childNode, enable);
    }
}

void RocketMeshEditor::OnBoundingBoxEnabledChanged(bool enabled, bool apply)
{
    ui_.boundingBoxCheckBox->blockSignals(true);
    ui_.boundingBoxCheckBox->setChecked(enabled);
    ui_.boundingBoxCheckBox->blockSignals(false);

    if (apply && meshNode_)
        SetShowBoundingBoxRecursive(meshNode_, enabled);
}

void RocketMeshEditor::OnWireframeEnabledChanged(bool enabled, bool apply)
{
    ui_.wireframeCheckBox->blockSignals(true);
    ui_.wireframeCheckBox->setChecked(enabled);
    ui_.wireframeCheckBox->blockSignals(false);
    
    if (apply && meshEntity_)
    {
        if (enabled)
        {
            Ogre::MaterialPtr wireframe = Ogre::MaterialManager::getSingleton().getByName("RocketMeshEditor-wireframe.material");
            if (!wireframe.get())
            {
                wireframe = Ogre::MaterialManager::getSingleton().create("RocketMeshEditor-wireframe.material", RocketPlugin::MESHMOON_RESOURCE_GROUP);
                if (wireframe->getNumTechniques() == 0)
                    wireframe->createTechnique();
                Ogre::Technique *tech = wireframe->getTechnique(0);
                if (tech->getNumPasses() == 0)
                    tech->createPass();
                Ogre::Pass *pass = tech->getPass(0);
                pass->setPolygonMode(Ogre::PM_WIREFRAME);
                wireframe->load(false);
            }
            for(uint i=0, num=meshEntity_->getNumSubEntities(); i<num; ++i)
                if (meshEntity_->getSubEntity(i))
                    meshEntity_->getSubEntity(i)->setMaterial(wireframe);
        }
        else
        {
            QList<RocketAssetSelectionWidget*> materialSelectors = findChildren<RocketAssetSelectionWidget*>();
            foreach(RocketAssetSelectionWidget* materialSelector, materialSelectors)
                if (materialSelector)
                    materialSelector->EmitCurrentAssetRef();
        }
    }
}

void RocketMeshEditor::resizeEvent(QResizeEvent *e)
{
    IRocketAssetEditor::resizeEvent(e);
    resizeTimer_.start(500);
}

void RocketMeshEditor::SetOverrideCursor(Qt::CursorShape shape)
{
    QCursor *c = QApplication::overrideCursor();
    if (!c)
        QApplication::setOverrideCursor(QCursor(shape));
    else if (c->shape() != shape)
        c->setShape(shape);
}

void RocketMeshEditor::RemoveOverrideCursor()
{
    while(QApplication::overrideCursor())
        QApplication::restoreOverrideCursor();
}

bool RocketMeshEditor::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == ui_.renderTargetWidget && ogreSceneRenderer_)
    {    
        if (e->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent *me = dynamic_cast<QMouseEvent*>(e);
            if (me && me->button() == Qt::LeftButton)
            {
                mouseInteraction_ = true;
                mousePos_ = me->pos();
                
                //SetOverrideCursor(Qt::SizeAllCursor);
            }
            else if (me && me->button() == Qt::RightButton)
            {
                mouseInteraction_ = true;
                mousePos_ = me->pos();

                //SetOverrideCursor(Qt::SizeVerCursor);   
            }
        }
        else if (e->type() == QEvent::MouseButtonRelease)
        {
            QMouseEvent *me = dynamic_cast<QMouseEvent*>(e);
            if (me && mouseInteraction_)
            {
                mouseInteraction_ = false;
                //RemoveOverrideCursor();
            }
        }
        else if (e->type() == QEvent::MouseMove && meshNode_)
        {
            QMouseEvent *me = dynamic_cast<QMouseEvent*>(e);
            bool handled = false;
            if (!me || !mouseInteraction_)
                return handled;
            
            // Init mouse pos for relative movement tracking
            if (mousePos_.isNull())
            {
                mousePos_ = me->pos();
                return handled;
            }
            
            QPoint relative = mousePos_ - me->pos();
            if (me->buttons() == Qt::LeftButton && !relative.isNull())
            {            
                Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
                t.rot.x += static_cast<float>(relative.y());
                t.rot.y += static_cast<float>(relative.x());
                OnTransformChanged(t);

                handled = true;
                RenderImage();
            }
            else if (me->buttons() == Qt::RightButton && relative.y() != 0)
            {
                float distance = ogreSceneRenderer_->CameraDistance() + static_cast<float>(relative.y());
                ogreSceneRenderer_->SetCameraDistance(distance);

                handled = true;
                RenderImage();
            }

            mousePos_ = me->pos();
            return handled;
        }
        else if (e->type() == QEvent::Wheel)
        {
            QWheelEvent *we = dynamic_cast<QWheelEvent*>(e);
            if (!we)
                return false;

            float amount = (we->delta() > 0 ? -5.0f : 5.0f);
            float distance = ogreSceneRenderer_->CameraDistance() + amount;
            ogreSceneRenderer_->SetCameraDistance(distance);

            RenderImage();
            return true;
        }
    }
    else if (obj == ui_.buttonBackground || obj == ui_.buttonAmbient || obj == ui_.buttonDirectional)
    {
        if (e->type() == QEvent::MouseButtonRelease)
        {
            if (!colorPicker_.isNull())
                return false;

            QMouseEvent *me = dynamic_cast<QMouseEvent*>(e);
            if (me && me->button() == Qt::RightButton)
            {
                QAbstractButton *button = qobject_cast<QAbstractButton*>(obj);
                if (button && !button->isEnabled())
                    return false;

                QVariant colorType = button->property("colorType");
                if (colorType.isNull() || !colorType.isValid())
                    return false;

                // Init color menu if first show
                if (!colorMenu_)
                {
                    colorMenu_ = new QMenu(this);
                    QAction *reset = colorMenu_->addAction(QIcon(":/images/icon-unchecked.png"), "Reset to Default");
                    connect(reset, SIGNAL(triggered()), SLOT(OnRestoreDefaultColor()));
                }

                if (colorMenu_->actions().first())
                    colorMenu_->actions().first()->setProperty("colorType", colorType);

                colorMenu_->popup(QCursor::pos());
                return true;
            }
        }
    }
    return false;
}

void RocketMeshEditor::ResourceDownloaded(QByteArray data)
{
    AssetPtr asset = plugin_->GetFramework()->Asset()->CreateNewAsset("OgreMesh", UniqueMeshAssetRef());
    if (!asset.get())
    {
        ShowError("Mesh Loading Failed", "Failed to create new mesh asset");
        return;
    }
    if (!asset->LoadFromFileInMemory((const u8*)(data.constData()), data.size(), false))
    {
        ShowError("Mesh Loading Failed", "Failed to load mesh asset from raw data");
        return;
    }
    OgreMeshAssetPtr meshAsset = dynamic_pointer_cast<OgreMeshAsset>(asset);
    if (meshAsset.get())
    {
        SetMessage("Mesh Loaded");
        mesh_ = weak_ptr<OgreMeshAsset>(meshAsset);
    }
    else
    {
        ShowError("Failed Opening Resource", QString("The opened asset is of invalid type '%1' for this tool: ").arg(asset->Type()));
        return;
    }

    // Unload previous mesh
    SAFE_DELETE(ogreSceneRenderer_);
    
    ReadConfig();
    
    // Load mesh to scene
    ogreSceneRenderer_ = new RocketOgreSceneRenderer(plugin_, ui_.renderTargetWidget);
    meshEntity_ = ogreSceneRenderer_->CreateMesh(mesh_.lock().get());
    meshNode_ = (meshEntity_ ? meshEntity_->getParentSceneNode() : 0);
    if (!meshEntity_ || !meshNode_)
    {
        ShowError("Mesh Loading Failed", "Failed to load mesh to preview renderer");
        SAFE_DELETE(ogreSceneRenderer_);
        return;
    }

    // Show stats on live rendering mode    
    ui_.statsTextEdit->setVisible(ogreSceneRenderer_->GetRenderMode() == RocketOgreSceneRenderer::RM_RenderWidget);

    // Init camera
    ogreSceneRenderer_->SetCameraDistance(50.f);

    // Transform and lights
    Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
    OnTransformChanged(t, false);
    OnDirectionalEnabledChanged(ui_.directionalCheckBox->isChecked());
    
    // Colors
    if (ogreSceneRenderer_->SceneManager())
        OnAmbientChanged(ReadColorConfigOrDefault("ambient-light-color", Color(ogreSceneRenderer_->SceneManager()->getAmbientLight())), true);
    if (ogreSceneRenderer_->OgreViewport())
        OnBackgroundChanged(ReadColorConfigOrDefault("background-color", Color(ogreSceneRenderer_->OgreViewport()->getBackgroundColour())), true);
        
    // Rendering
    OnBoundingBoxEnabledChanged(ui_.boundingBoxCheckBox->isChecked(), true);
    OnWireframeEnabledChanged(ui_.wireframeCheckBox->isChecked(), true);
        
    // Submeshes
    for (uint i=0, num=meshEntity_->getNumSubEntities(); i<num; i++)
    {
        RocketAssetSelectionWidget *materialSelector = new RocketAssetSelectionWidget(plugin_, "OgreMaterial");
        connect(materialSelector, SIGNAL(AssetRefChanged(const QString&, AssetPtr)), SLOT(OnMaterialSelected(const QString&, AssetPtr)));

        materialSelector->setProperty("submeshIndex", i);
        materialSelector->SetConfigData(ConfigData("rocket-asset-editors", "mesh-editor", QString("material-%1").arg(i)));       

        ui_.materialsLayout->addWidget(materialSelector);
    }

    // Init rendering    
    RenderImage();
    
    // Frame updates
    connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), SLOT(OnUpdate(float)));
}

void RocketMeshEditor::OnUpdate(float frametime)
{
    if (!ogreSceneRenderer_ || !meshNode_)
        return;

    if (!mouseInteraction_ && (ui_.autoRotateX->isChecked() || ui_.autoRotateY->isChecked() || ui_.autoRotateZ->isChecked()))
    {
        float amount = frametime * 45.0f; // 45 degrees per second

        Transform t = ogreSceneRenderer_->NodeTransform(meshNode_);
        if (ui_.autoRotateX->isChecked())
            t.rot.x += amount;
        if (ui_.autoRotateY->isChecked())
            t.rot.y += amount;
        if (ui_.autoRotateZ->isChecked())
            t.rot.z += amount;
        OnTransformChanged(t);
    }
    
    if (ogreSceneRenderer_->GetRenderMode() == RocketOgreSceneRenderer::RM_RenderWidget)
    {
        statsUpdateT_ += frametime;
        if (statsUpdateT_ >= 1.0f)
        {
            statsUpdateT_ = 0.0f;

            Ogre::RenderTarget *renderTarget = ogreSceneRenderer_->RenderTarget();
            if (renderTarget)
            {
                const Ogre::RenderTarget::FrameStats &stats = renderTarget->getStatistics();
                QString tris = QString::number(stats.triangleCount);
                if (tris.length() == 7)      // 1000000 -> 1 000 000
                    tris = tris.left(1) + " " + tris.mid(1,3) + " " + tris.right(3);
                else if (tris.length() == 6) // 100000  -> 100 000
                    tris = tris.left(3) + " " + tris.right(3);
                else if (tris.length() == 5) // 10000   -> 10 000
                    tris = tris.left(2) + " " + tris.right(3);
                else if (tris.length() == 4) // 1000    -> 1 000
                    tris = tris.left(1) + " " + tris.right(3);
                ui_.statsTextEdit->setPlainText(QString("FPS        %1\nTriangles  %2\nBatches    %3")
                    .arg(stats.avgFPS, 0, 'f', 2).arg(tris).arg(stats.batchCount));
            }
        }
    }
}

void RocketMeshEditor::RenderImage()
{
    if (!ogreSceneRenderer_)
        return;

    if (ogreSceneRenderer_->GetRenderMode() == RocketOgreSceneRenderer::RM_RenderTexture)
    {
        ogreSceneRenderer_->Update();

        QImage img = ogreSceneRenderer_->ToQImage();
        ui_.renderTargetWidget->setPixmap(QPixmap::fromImage(img));
    }
}

IRocketAssetEditor::SerializationResult RocketMeshEditor::ResourceData()
{
    // Preview only editor, no saving
    IRocketAssetEditor::SerializationResult result(false, QByteArray());
    return result;
}

void RocketMeshEditor::OnMaterialSelected(const QString &ref, AssetPtr asset)
{
    // Wireframe rendering enabled. Don't apply custom materials.
    if (ui_.wireframeCheckBox->isChecked())
        return;

    RocketAssetSelectionWidget *selector = dynamic_cast<RocketAssetSelectionWidget*>(sender());
    if (!selector || !meshEntity_)
        return;

    uint submeshIndex = selector->property("submeshIndex").toUInt();
    QString materialName = "BaseWhite";

    if (asset.get() && asset->IsLoaded())
    {
        OgreMaterialAsset *materialAsset = dynamic_cast<OgreMaterialAsset*>(asset.get());
        if (materialAsset && !materialAsset->ogreAssetName.isEmpty())
            materialName = materialAsset->ogreAssetName;
    }
    
    if (submeshIndex < meshEntity_->getNumSubEntities())
    {
        Ogre::SubEntity *submesh = meshEntity_->getSubEntity(submeshIndex);
        if (submesh)
            submesh->setMaterialName(AssetAPI::SanitateAssetRef(materialName.toStdString()));
    }
}

void RocketMeshEditor::ReadConfig()
{
    if (!plugin_)
        return;

    ConfigData configData("rocket-asset-editors", "mesh-editor");

    ConfigAPI *config = plugin_->GetFramework()->Config();
    ui_.directionalCheckBox->setChecked(config->Read(configData, "directional-light-enabled", true).toBool());
    ui_.boundingBoxCheckBox->setChecked(config->Read(configData, "render-bounding-box", false).toBool());
    ui_.wireframeCheckBox->setChecked(config->Read(configData, "render-wireframe", false).toBool());
}

void RocketMeshEditor::WriteConfig(bool writeDirectional, bool writeAmbient, bool writeBackground, bool writeUiState)
{
    if (!plugin_)
        return;

    ConfigData configData("rocket-asset-editors", "mesh-editor");
    
    ConfigAPI *config = plugin_->GetFramework()->Config();
    if (writeDirectional && directionalLight_)
    {
        QColor c = Color(directionalLight_->getDiffuseColour());
        config->Write(configData, "directional-light-color", c);
    }
    if (writeAmbient && ogreSceneRenderer_ && ogreSceneRenderer_->SceneManager())
    {
        QColor c = Color(ogreSceneRenderer_->SceneManager()->getAmbientLight());
        config->Write(configData, "ambient-light-color", c);
    }
    if (writeBackground && ogreSceneRenderer_ && ogreSceneRenderer_->OgreViewport())
    {
        QColor c = Color(ogreSceneRenderer_->OgreViewport()->getBackgroundColour());
        config->Write(configData, "background-color", c);
    }
    if (writeUiState)
    {
        config->Write(configData, "directional-light-enabled", ui_.directionalCheckBox->isChecked());
        config->Write(configData, "render-bounding-box", ui_.boundingBoxCheckBox->isChecked());
        config->Write(configData, "render-wireframe", ui_.wireframeCheckBox->isChecked());
    }
}
