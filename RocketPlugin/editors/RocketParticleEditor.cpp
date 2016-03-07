/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketParticleEditor.h"
#include "RocketMaterialEditor.h"

#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "MeshmoonAsset.h"
#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "Framework.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "IAssetTypeFactory.h"
#include "IRenderer.h"
#include "Scene.h"
#include "Entity.h"
#include "EC_Placeable.h"

#include "Math/MathFunc.h"

#include "OgreWorld.h"
#include "Renderer.h"
#include "OgreParticleAsset.h"
#include "OgreMaterialAsset.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
#include <OgreParticleSystemManager.h>
#include <OgreParticleSystem.h>
#include <OgreParticleEmitter.h>
#include <OgreParticleEmitterFactory.h>
#include <OgreParticleAffector.h>
#include <OgreParticleAffectorFactory.h>
#include <OgreBillboardParticleRenderer.h>
#include <OgreStringConverter.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <QDesktopServices>

namespace
{
    // Root attribute names

    const QString AttrQuota                    = "quota";
    const QString AttrMaterial                 = "material";
    const QString AttrParticleWidth            = "particle_width";
    const QString AttrParticleHeight           = "particle_height";
    const QString AttrCullEach                 = "cull_each";
    const QString AttrRenderer                 = "renderer";
    const QString AttrSorted                   = "sorted";
    const QString AttrLocalSpace               = "local_space";
    const QString AttrBillboardType            = "billboard_type";
    const QString AttrBillboardOrigin          = "billboard_origin";
    const QString AttrBillboardRotationType    = "billboard_rotation_type";
    const QString AttrCommonDirection          = "common_direction";
    const QString AttrCommonUpVector           = "common_up_vector";
    const QString AttrPointRendering           = "point_rendering";
    const QString AttrAccurateFacing           = "accurate_facing";
    const QString AttrIterationInterval        = "iteration_interval";
    const QString AttrNonVisibleUpdateTimeout  = "nonvisible_update_timeout";

    // Emitter attribute names

    const QString AttrName                     = "name";
    const QString AttrAngle                    = "angle";
    const QString AttrColour                   = "colour";
    const QString AttrColourRangeStart         = "colour_range_start";
    const QString AttrColourRangeEnd           = "colour_range_end";
    const QString AttrDirection                = "direction";
    const QString AttrDirectionReference       = "direction_position_reference";
    const QString AttrEmitEmitter              = "emit_emitter";
    const QString AttrEmitEmitterQuota         = "emit_emitter_quota";
    const QString AttrEmissionRate             = "emission_rate";
    const QString AttrPosition                 = "position";
    const QString AttrVelocity                 = "velocity";
    const QString AttrVelocityMin              = "velocity_min";
    const QString AttrVelocityMax              = "velocity_max";
    const QString AttrTimeToLive               = "time_to_live";
    const QString AttrTimeToLiveMin            = "time_to_live_min";
    const QString AttrTimeToLiveMax            = "time_to_live_max";
    const QString AttrDuration                 = "duration";
    const QString AttrDurationMin              = "duration_min";
    const QString AttrDurationMax              = "duration_max";
    const QString AttrRepeatDelay              = "repeat_delay";
    const QString AttrRepeatDelayMin           = "repeat_delay_min";
    const QString AttrRepeatDelayMax           = "repeat_delay_max";
    const QString AttrUp                       = "up";
    const QString AttrWidth                    = "width";
    const QString AttrHeight                   = "height";
    const QString AttrDepth                    = "depth";
    const QString AttrInnerWidth               = "inner_width";
    const QString AttrInnerHeight              = "inner_height";
    const QString AttrInnerDepth               = "inner_depth";
    
    // Affector attribute names
    /// @todo DeflectorPlane and DirectionRandomiser affector attributes

    const QString AttrForceVector              = "force_vector";
    const QString AttrForceApplication         = "force_application";
    const QString AttrRed                      = "red";
    const QString AttrGreen                    = "green";
    const QString AttrBlue                     = "blue";
    const QString AttrAlpha                    = "alpha";
    const QString AttrStateChange              = "state_change";
    const QString AttrRate                     = "rate";
    const QString AttrTime                     = "time";
    const QString AttrRotateRangeStart         = "rotation_range_start";
    const QString AttrRotateRangeEnd           = "rotation_range_end";
    const QString AttrRotateSpeedRangeStart    = "rotation_speed_range_start";
    const QString AttrRotateSpeedRangeEnd      = "rotation_speed_range_end";

    // Default values

    const QString AttrDefaultMaterial          = "BaseWhite";
    const QString AttrDefaultRenderer          = "billboard";
    const QString AttrDefaultBillboardType     = "point";
    const QString AttrDefaultBillboardOrigin   = "center";
    const QString AttrDefaultBillboardRotationType = "texcoord";
    const QString AttrDefaultColor             = "1 1 1 1";
    const QString AttrDefaultColorIterpolator1 = "0.5 0.5 0.5 0.0";
    const QString AttrDefaultColorIterpolator2 = "0.5 0.5 0.5 0";
    const QString AttrDefaultVector4Zero       = "0 0 0 0";
    const QString AttrDefaultVectorZero        = "0 0 0";
    const QString AttrDefaultVectorX           = "1 0 0";
    const QString AttrDefaultVectorY           = "0 1 0";
    const QString AttrDefaultVectorZ           = "0 0 1";
    const QString AttrDefaultForceVector       = "0 -100 0";
    const QString AttrDefaultForceApplication  = "add";
    const QString AttrDefaultZero              = "0";
    const QString AttrDefaultZeroPointFive     = "0.5";
    const QString AttrDefaultOne               = "1";
    const QString AttrDefaultFive              = "5";
    const QString AttrDefaultTen               = "10";
    const QString AttrDefaultHundred           = "100";
    const QString AttrDefaultTrue              = "true";
    const QString AttrDefaultFalse             = "false";
    const QString AttrDefaultOn                = "on";
    const QString AttrDefaultOff               = "off";

    bool IsAffectorColourChannel(const QString &name)
    {
        if (name == AttrRed   || name == AttrGreen || name == AttrBlue  || name == AttrAlpha)
            return true;

        for (int i=1, num=5; i<=num; i++)
            if (name == QString("%1%2").arg(AttrRed).arg(i)   || 
                name == QString("%1%2").arg(AttrGreen).arg(i) ||
                name == QString("%1%2").arg(AttrBlue).arg(i) ||
                name == QString("%1%2").arg(AttrAlpha).arg(i))
                return true;

        return false;
    }

    bool IsAffectorColour(const QString &name)
    {
        if (name == AttrColour + "0" || name == AttrColour + "1" || name == AttrColour + "2")
            return true;
        return false;
    }

    bool IsAffectorTime(const QString &name)
    {
        if (name == AttrTime + "0" || name == AttrTime + "1" || name == AttrTime + "2")
            return true;
        return false;
    }
}

// RocketParticleEditor

RocketParticleEditor::RocketParticleEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource) :
    IRocketAssetEditor(plugin, resource, "Particle Editor", "Ogre Particle Script"),
    LC("[RocketParticleEditor]: "),
    materialState_(IRocketAssetEditor::DS_Verifying),
    menu_(0)
{
    /** @todo Handle situation where the editor is opened with generated particle
        but then a ref is added to the scene aka. its loaded to the system. At
        this point we need to update our particle weak ptr to point to it! */

    editor_ = new QWidget(this);
    ui_.setupUi(editor_);

    ui.contentLayout->addWidget(editor_);
    resize(550, 600);
    
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
    
    SetMaterialState(IRocketAssetEditor::DS_OK);
    
    // Ogre::Billboard::BillboardType
    QStringList billboardTypes;
    billboardTypes << "Point" << "Oriented Common" << "Oriented Self" << "Perpendicular Common" << "Perpendicular Self";
    ui_.bbTypeComboBox->addItems(billboardTypes);
    
    // Ogre::Billboard::BillboardOrigin
    QStringList billboarOriginTypes;
    billboarOriginTypes << "Top Left" << "Top Center" << "Top Right" << "Center Left" << "Center" << "Center Right" << "Bottom Left" << "Bottom Center" << "Bottom Right";
    ui_.bbOriginComboBox->addItems(billboarOriginTypes);
    
    // Ogre::Billboard::BillboardRotationType
    QStringList billboardRotationTypes;
    billboardRotationTypes << "Vertex" << "Texture Coordinate";
    ui_.bbRotationComboBox->addItems(billboardRotationTypes);

    ui_.emittersTabWidget->clear();
    ui_.affectorsTabWidget->clear();
    
    connect(ui_.helpConfiguration, SIGNAL(clicked()), SLOT(OnHelpConfiguration()));
    connect(ui_.helpEmitters, SIGNAL(clicked()), SLOT(OnHelpEmitters()));
    connect(ui_.helpAffectors, SIGNAL(clicked()), SLOT(OnHelpAffectors()));
    
    connect(ui_.addEmitterButton, SIGNAL(clicked()), SLOT(OnAddNewEmitter()));
    connect(ui_.addAffectorButton, SIGNAL(clicked()), SLOT(OnAddNewAffector()));

    connect(ui_.emittersTabWidget, SIGNAL(tabCloseRequested(int)), SLOT(OnRemoveEmitterRequest(int)));
    connect(ui_.affectorsTabWidget, SIGNAL(tabCloseRequested(int)), SLOT(OnRemoveAffectorRequest(int)));

    connect(ui_.materialLineEdit, SIGNAL(editingFinished()), SLOT(OnMaterialChanged()));
    connect(ui_.buttonStorageSelection, SIGNAL(clicked()), SLOT(OnSelectMaterialFromStorage()));
    
    connect(ui_.bbTypeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnBillboardTypeChanged(int)));
    connect(ui_.bbOriginComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnBillboardOriginChanged(int)));
    connect(ui_.bbRotationComboBox, SIGNAL(currentIndexChanged(int)), SLOT(OnBillboardRotationTypeChanged(int)));
    
    connect(ui_.quotaSpinBox, SIGNAL(valueChanged(int)), SLOT(OnQuotaChanged(int)));
    connect(ui_.sizeWidthSpinBox, SIGNAL(valueChanged(double)), SLOT(OnParticleWidthChanged(double)));
    connect(ui_.sizeHeightSpinBox, SIGNAL(valueChanged(double)), SLOT(OnParticleHeightChanged(double)));
    connect(ui_.framerateSpinBox, SIGNAL(valueChanged(double)), SLOT(OnIterationIntervalChanged(double)));
    connect(ui_.updateTimeoutSpinBox, SIGNAL(valueChanged(double)), SLOT(OnNonVisibleUpdateTimeoutChanged(double)));
    
    connect(ui_.sortedCheckBox, SIGNAL(clicked(bool)), SLOT(OnSortedChanged(bool)));
    connect(ui_.cullEachCheckBox, SIGNAL(clicked(bool)), SLOT(OnCullEachChanged(bool)));
    connect(ui_.localSpaceCheckBox, SIGNAL(clicked(bool)), SLOT(OnLocalSpaceChanged(bool)));
    connect(ui_.accurateFacingCheckBox, SIGNAL(clicked(bool)), SLOT(OnAccurateFacingChanged(bool)));
}

RocketParticleEditor::~RocketParticleEditor()
{
    ClearEmitterTabs();
    ClearAffectorTabs();
    OnCloseMaterialEditor();
    
    SAFE_DELETE(menu_);
    
    if (!plugin_)
        return;

    // Forget in-mem generated asset
    if (!particle_.expired() && particle_.lock()->DiskSourceType() == IAsset::Programmatic)
        plugin_->GetFramework()->Asset()->ForgetAsset(particle_.lock()->Name(), false);
        
    AssetPtr existing = plugin_->GetFramework()->Asset()->GetAsset(AssetRef() + "_RocketParticleEditor.particle");
    if (existing && existing->DiskSourceType() == IAsset::Programmatic)
        plugin_->GetFramework()->Asset()->ForgetAsset(existing->Name(), false);
}

QStringList RocketParticleEditor::SupportedSuffixes()
{
    QStringList suffixes;
    suffixes << "particle";
    return suffixes;
}

QString RocketParticleEditor::EditorName()
{
    return "Particle Editor";
}

void RocketParticleEditor::ResourceDownloaded(QByteArray data)
{
    if (!plugin_)
        return;

    particle_.reset();

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
            LoadParticle(loaded);
            break;
        }
    }

    // Previous step did nothing: Get or create a in memory asset from the input data.
    if (particle_.expired())
    {
        // Generated asset should have correct url base so relative texture refs will be resolved correctly.
        QString generatedName = (!storageKey.isEmpty() ? plugin_->Storage()->UrlForItem(storageKey) : AssetRef()) + "_RocketParticleEditor.particle";
        AssetPtr created = plugin_->GetFramework()->Asset()->GetAsset(generatedName);
        if (!created.get())
        {
            // Create in memory material.
            created = plugin_->GetFramework()->Asset()->CreateNewAsset("OgreParticle", generatedName);
            if (!created.get())
            {
                ShowError("Particle Loading Failed", "Failed to generate an in memory particle for raw input data.");
                return;
            }

            // Hook to loaded signal that is emitted either in LoadFromFileInMemory or when all dependencies are loaded.
            connect(created.get(), SIGNAL(Loaded(AssetPtr)), SLOT(LoadParticle(AssetPtr)));

            // Start loading (also reads and requests deps)
            if (!created->LoadFromFileInMemory((const u8*)data.data(), (size_t)data.size(), false))
            {
                ShowError("Particle Loading Failed", "Failed to load a particle from raw input data. See console for load errors.");
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
            LoadParticle(created);
    }
}

IRocketAssetEditor::SerializationResult RocketParticleEditor::ResourceData()
{
    IRocketAssetEditor::SerializationResult result(false, QByteArray());
    
    Ogre::ParticleSystem *p = GetSystem();
    Ogre::BillboardParticleRenderer *r = GetSystemRenderer();
    if (p && r)
    {
        QByteArray data;
        {
            QTextStream s(&data);

            // Start
            int indentLevel = 0;
            ushort numEmitters = p->getNumEmitters();
            ushort numAffectors = p->getNumAffectors();

            WriteParameter(s, "particle_system", AssetAPI::SanitateAssetRef(particle_.lock()->Name()), indentLevel, true);
            WriteParametersSection(s, p, "Main attributes", ++indentLevel);
            WriteParametersSection(s, r, "Billboard renderer attributes", indentLevel);
      
            // Emitters
            if (numEmitters > 0)
                WriteParameter(s, QString("//"), "Emitters", indentLevel);
            for (ushort i=0; i<numEmitters; i++)
            {
                Ogre::ParticleEmitter *emitter = p->getEmitter(i);
                if (!emitter)
                {
                    LogError(LC + "ResourceData() Encountered a null Ogre::ParticleEmitter*!");
                    continue;
                }
                WriteParameter(s, "emitter", emitter->getType(), indentLevel, true);
                WriteParameters(s, emitter, ++indentLevel);
                WriteEndPracet(s, --indentLevel);

                if (i<numEmitters-1 || numAffectors > 0)
                    s << endl;
            }

            // Affectors
            if (numAffectors > 0)
                WriteParameter(s, QString("//"), "Affectors", indentLevel);
            for (ushort i=0; i<numAffectors; i++)
            {
                Ogre::ParticleAffector *affector = p->getAffector(i);
                if (!affector)
                {
                    LogError(LC + "ResourceData() Encountered a null Ogre::ParticleAffector*!");
                    continue;
                }
                WriteParameter(s, "affector", affector->getType(), indentLevel, true);
                WriteParameters(s, affector, ++indentLevel);
                WriteEndPracet(s, --indentLevel);

                if (i<numAffectors-1)
                    s << endl;
            }

            // End
            indentLevel = 0;
            WriteEndPracet(s, indentLevel);
            s.flush();
        }
        
        // Uncomment to see the file contents in stdout
        //qDebug() << endl << data << endl;
        
        result.first = true;
        result.second = data;
    }
    return result;
}

void RocketParticleEditor::WriteStartPracet(QTextStream &s, int indentLevel)
{
    WriteIndent(s, indentLevel);
    s << "{" << endl;
}

void RocketParticleEditor::WriteEndPracet(QTextStream &s, int indentLevel)
{
    WriteIndent(s, indentLevel);
    s << "}" << endl;
}

void RocketParticleEditor::WriteParametersSection(QTextStream &s, Ogre::StringInterface *si, const QString &sectionName, int indentLevel)
{
    QByteArray dataTemp;
    {
        QTextStream sTemp(&dataTemp);
        WriteParameters(sTemp, si, indentLevel);
        sTemp.flush();
    }
    if (!dataTemp.isEmpty())
    {
        if (!sectionName.isEmpty())
            WriteParameter(s, "//", sectionName, indentLevel);
        s << dataTemp;
        s << endl;
    }
}

void RocketParticleEditor::WriteParameters(QTextStream &s, Ogre::StringInterface *si, int indentLevel)
{
    bool isParticleSystem = (dynamic_cast<Ogre::ParticleSystem*>(si) != 0);

    Ogre::ParameterList params = si->getParameters();           
    for (uint i=0; i<params.size(); i++)
    {
        if (EqualsDefaultValue(si, &params[i]))
        {
            //qDebug() << "SKIP  " << params[i].name.c_str() << si->getParameter(params[i].name).c_str();
            continue;
        }

        std::string name = params[i].name;
        std::string value = si->getParameter(name);
        //qDebug() << "WRITE " << name.c_str() << value.c_str();

        if (isParticleSystem && name == "material")
            WriteParameter(s, name, OgreMaterialToAssetReference(value), indentLevel);
        else
            WriteParameter(s, name, value, indentLevel);
    }
    //qDebug() << "";
}

void RocketParticleEditor::OnHelpConfiguration()
{
    QDesktopServices::openUrl(QUrl("http://www.ogre3d.org/docs/manual/manual_35.html#Particle-System-Attributes"));
}

void RocketParticleEditor::OnHelpEmitters()
{
    QDesktopServices::openUrl(QUrl("http://www.ogre3d.org/docs/manual/manual_36.html#Particle-Emitters"));
}

void RocketParticleEditor::OnHelpAffectors()
{
    QDesktopServices::openUrl(QUrl("http://www.ogre3d.org/docs/manual/manual_40.html#Standard-Particle-Affectors"));
}

void RocketParticleEditor::ClearEmitterTabs()
{
    for (int i=0, num=ui_.emittersTabWidget->count(); i<num; i++)
    {
        QWidget *w = ui_.emittersTabWidget->widget(i);
        RocketParticleEmitterWidget *ew = w ? dynamic_cast<RocketParticleEmitterWidget*>(w) : 0;
        if (ew) ew->Reset();
        if (w)  w->deleteLater();
    }
    ui_.emittersTabWidget->clear();
}

void RocketParticleEditor::ClearAffectorTabs()
{
    for (int i=0, num=ui_.affectorsTabWidget->count(); i<num; i++)
    {
        QWidget *w = ui_.affectorsTabWidget->widget(i);
        RocketParticleAffectorWidget *aw = w ? dynamic_cast<RocketParticleAffectorWidget*>(w) : 0;
        if (aw) aw->Reset();
        if (w)  w->deleteLater();
    }
    ui_.affectorsTabWidget->clear();
}

void RocketParticleEditor::OnAddNewEmitter()
{
    if (!GetSystem())
        return;

    SAFE_DELETE(menu_);
    menu_ = new QMenu();
    
    Ogre::ParticleSystemManager::ParticleEmitterFactoryIterator iter = Ogre::ParticleSystemManager::getSingleton().getEmitterFactoryIterator();
    while(iter.hasMoreElements())
    {
        Ogre::ParticleEmitterFactory *f = iter.getNext();
        if (f) menu_->addAction(QString::fromStdString(f->getName()));
    }
    
    QAction *act = menu_->exec(QCursor::pos());
    QString emitterType = act ? act->text() : "";
    if (emitterType.isEmpty())
        return;

    Ogre::ParticleEmitter *emitter = GetSystem()->addEmitter(emitterType.toStdString());
    if (!emitter)
        return;

    RocketParticleEmitterWidget *emitterEditor = new RocketParticleEmitterWidget(plugin_, emitter);
    connect(emitterEditor, SIGNAL(Touched()), SLOT(OnEmitterOrAffectorTouched()));

    int index = ui_.emittersTabWidget->addTab(emitterEditor, QString::fromStdString(emitter->getType()));
    ui_.emittersTabWidget->setCurrentIndex(index);
}

void RocketParticleEditor::OnAddNewAffector()
{
    if (!GetSystem())
        return;

    SAFE_DELETE(menu_);
    menu_ = new QMenu();

    Ogre::ParticleSystemManager::ParticleAffectorFactoryIterator iter = Ogre::ParticleSystemManager::getSingleton().getAffectorFactoryIterator();
    while(iter.hasMoreElements())
    {
        // ColourImage is currently not supported by OgreParticleAsset
        Ogre::ParticleAffectorFactory *a = iter.getNext();
        if (a && a->getName() != "ColourImage") menu_->addAction(QString::fromStdString(a->getName()));
    }

    QAction *act = menu_->exec(QCursor::pos());
    QString affectorType = act ? act->text() : "";
    if (affectorType.isEmpty())
        return;

    Ogre::ParticleAffector *affector = GetSystem()->addAffector(affectorType.toStdString());
    if (!affector)
        return;
        
    // Setup reasonable default values
    if (affector->getType() == "ColourInterpolator")
    {
        // t0.5 red -> t1.0 transparent
        affector->setParameter("time0", "0.5");
        affector->setParameter("colour0", "1 0 0 1");
        affector->setParameter("time1", "1.0");
        affector->setParameter("colour1", "0 0 0 0");
    }
    else if (affector->getType() == "LinearForce")
    {
        // Small gravity effect
        affector->setParameter("force_vector", "0 -0.25 0");
    }
    else if (affector->getType() == "ColourFader")
    {
        // Fade slowly to transparent
        affector->setParameter("alpha", "-0.2");
    }
    else if (affector->getType() == "ColourFader2")
    {
        // Fade to red and last 2 seconds to transparent
        affector->setParameter("green1", "-0.2");
        affector->setParameter("blue1", "-0.2");
        affector->setParameter("alpha2", "-0.5");
        affector->setParameter("state_change", "2");
    }
    else if (affector->getType() == "Rotator")
    {
        affector->setParameter("rotation_speed_range_start", "45");
        affector->setParameter("rotation_speed_range_end", "90");
    }
    else if (affector->getType() == "Scaler")
    {
        // Scale slowly to bigger size
        affector->setParameter("rate", "-0.2");
    }

    RocketParticleAffectorWidget *affectorEditor = new RocketParticleAffectorWidget(plugin_, affector);
    connect(affectorEditor, SIGNAL(Touched()), SLOT(OnEmitterOrAffectorTouched()));

    int index = ui_.affectorsTabWidget->addTab(affectorEditor, QString::fromStdString(affector->getType()));
    ui_.affectorsTabWidget->setCurrentIndex(index);
}

void RocketParticleEditor::OnRemoveEmitterRequest(int tabIndex)
{
    if (!confirmDialog_.isNull())
        return;

    RocketParticleEmitterWidget *emitterEditor = dynamic_cast<RocketParticleEmitterWidget*>(ui_.emittersTabWidget->widget(tabIndex));
    if (!emitterEditor || !emitterEditor->Emitter())
        return;

    confirmDialog_ = plugin_->Notifications()->ShowSplashDialog(QString("Remove emitter %1 permanently?").arg(emitterEditor->Emitter()->getType().c_str()),
        ":/images/icon-recycle.png", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes, this);
    confirmDialog_->setProperty("tabIndex", tabIndex);
    connect(confirmDialog_, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(OnRemoveEmitterConfirmed(QAbstractButton*)));
}

void RocketParticleEditor::OnRemoveEmitterConfirmed(QAbstractButton *button)
{
    if (confirmDialog_.isNull())
        return;
    if (confirmDialog_->standardButton(button) != QMessageBox::Yes)
        return;
    int tabIndex = sender() ? sender()->property("tabIndex").toInt() : -1;
    if (tabIndex < 0)
        return;

    Ogre::ParticleSystem *p = GetSystem();
    if (!p)
        return;

    RocketParticleEmitterWidget *emitterEditor = dynamic_cast<RocketParticleEmitterWidget*>(ui_.emittersTabWidget->widget(tabIndex));
    if (!emitterEditor || !emitterEditor->Emitter())
        return;
        
    for(ushort i=0, num=p->getNumEmitters(); i<num; i++)
    {
        if (p->getEmitter(i) == emitterEditor->Emitter())
        {
            p->removeEmitter(i);
            break;
        }
    }

    ui_.emittersTabWidget->removeTab(tabIndex);
    emitterEditor->deleteLater();

    Touched();
}

void RocketParticleEditor::OnRemoveAffectorRequest(int tabIndex)
{
    if (!confirmDialog_.isNull())
        return;

    RocketParticleAffectorWidget *affectirEditor = dynamic_cast<RocketParticleAffectorWidget*>(ui_.affectorsTabWidget->widget(tabIndex));
    if (!affectirEditor || !affectirEditor->Affector())
        return;

    confirmDialog_ = plugin_->Notifications()->ShowSplashDialog(QString("Remove affector %1 permanently?").arg(affectirEditor->Affector()->getType().c_str()),
        ":/images/icon-recycle.png", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes, this);
    confirmDialog_->setProperty("tabIndex", tabIndex);
    connect(confirmDialog_, SIGNAL(buttonClicked(QAbstractButton*)), SLOT(OnRemoveAffectorConfirmed(QAbstractButton*)));
}

void RocketParticleEditor::OnRemoveAffectorConfirmed(QAbstractButton *button)
{
    if (confirmDialog_.isNull())
        return;
    if (confirmDialog_->standardButton(button) != QMessageBox::Yes)
        return;
    int tabIndex = sender() ? sender()->property("tabIndex").toInt() : -1;
    if (tabIndex < 0)
        return;

    Ogre::ParticleSystem *p = GetSystem();
    if (!p)
        return;

    RocketParticleAffectorWidget *affectorEditor = dynamic_cast<RocketParticleAffectorWidget*>(ui_.affectorsTabWidget->widget(tabIndex));
    if (!affectorEditor || !affectorEditor->Affector())
        return;

    for(ushort i=0, num=p->getNumAffectors(); i<num; i++)
    {
        if (p->getAffector(i) == affectorEditor->Affector())
        {
            p->removeAffector(i);
            break;
        }
    }

    ui_.affectorsTabWidget->removeTab(tabIndex);
    affectorEditor->deleteLater();

    Touched();
}

bool RocketParticleEditor::EqualsDefaultValue(Ogre::StringInterface *si, Ogre::ParameterDef *def)
{
    if (!si || !def)
        return false;

    bool isParticleSystem   = (dynamic_cast<Ogre::ParticleSystem*>(si) != 0);
    bool isBbRenderer       = (!isParticleSystem && dynamic_cast<Ogre::BillboardParticleRenderer*>(si) != 0);
    bool isParticleEmitter  = (!isBbRenderer && !isParticleSystem && dynamic_cast<Ogre::ParticleEmitter*>(si) != 0);
    bool isParticleAffector = (!isBbRenderer && !isParticleSystem && !isParticleEmitter && dynamic_cast<Ogre::ParticleAffector*>(si) != 0);
    
    QString name  = QString::fromStdString(def->name);
    QString value = QString::fromStdString(si->getParameter(name.toStdString()));
    
    if (isParticleSystem)
    {
        // Never write certain attributes
        if (name == AttrEmitEmitterQuota)
            return true;

        if (name == AttrQuota && value == AttrDefaultTen)
            return true;
        else if (name == AttrMaterial && (value.isEmpty() || value == AttrDefaultMaterial))
            return true;
        else if ((name == AttrParticleWidth || name == AttrParticleHeight) && value == AttrDefaultHundred)
            return true;
        else if ((name == AttrCullEach || name == AttrSorted || name == AttrLocalSpace) && value == AttrDefaultFalse)
            return true;
        else if (name == AttrRenderer && value == AttrDefaultRenderer)
            return true;          
        else if ((name == AttrIterationInterval || name == AttrNonVisibleUpdateTimeout) && value == AttrDefaultZero)
            return true;
    }
    else if (isBbRenderer)
    {
        if (name == AttrBillboardType && value == AttrDefaultBillboardType)
            return true;
        else if (name == AttrBillboardOrigin && value == AttrDefaultBillboardOrigin)
            return true;
        else if (name == AttrBillboardRotationType && value == AttrDefaultBillboardRotationType)
            return true;
        else if (name == AttrCommonDirection && value == AttrDefaultVectorZ)
            return true;
        else if (name == AttrCommonUpVector && value == AttrDefaultVectorY)
            return true;
        else if (name == AttrPointRendering && value == AttrDefaultFalse)
            return true; 
        else if (name == AttrAccurateFacing && (value == AttrDefaultFalse || value == AttrDefaultOff || value == AttrDefaultZero))
            return true;
    }
    else if (isParticleEmitter)
    {
        //QString affectorType = dynamic_cast<Ogre::ParticleEmitter*>(si)->getType();

        if (name == AttrAngle && value == AttrDefaultZero)
            return true;
        else if ((name == AttrColour || name == AttrColourRangeStart || name == AttrColourRangeEnd) && value == AttrDefaultColor)
            return true;
        else if (name == AttrDirection && value == AttrDefaultVectorX) // This might be dangerous, different emitters seem to have different defaults
            return true;
        else if (name == AttrEmissionRate && value == AttrDefaultTen)
            return true;
        else if (name == AttrPosition && value == AttrDefaultVectorZero)
            return true;
        else if ((name == AttrWidth || name == AttrHeight || name == AttrDepth) && value == AttrDefaultHundred)
            return true;
        else if ((name == AttrInnerWidth || name == AttrInnerHeight || name == AttrInnerDepth) && value == AttrDefaultZeroPointFive)
            return true;
        else if (name == AttrDirectionReference && (value == AttrDefaultVector4Zero || value == AttrDefaultVectorZero))
            return true;
        else if ((name == AttrVelocity || name == AttrVelocityMin || name == AttrVelocityMax) && value == AttrDefaultOne)
            return true;
        else if ((name == AttrTimeToLive || name == AttrTimeToLiveMin || name == AttrTimeToLiveMax) && value == AttrDefaultFive)
            return true;
        else if ((name == AttrDuration || name == AttrDurationMin || name == AttrDurationMax) && value == AttrDefaultZero)
            return true;
        else if ((name == AttrRepeatDelay || name == AttrRepeatDelayMin || name == AttrRepeatDelayMax) && value == AttrDefaultZero)
            return true;
        else if ((name == AttrName || name == AttrEmitEmitter) && value.isEmpty())
            return true;
    }
    else if (isParticleAffector)
    {
        if (name == AttrForceVector && value == AttrDefaultForceVector)
            return true;
        else if (name == AttrForceApplication && value == AttrDefaultForceApplication)
            return true;
        else if (name == AttrStateChange && value == AttrDefaultOne)
            return true;
        else if (name == AttrRate && value == AttrDefaultZero)
            return true;
        else if ((name == AttrRotateRangeStart || name == AttrRotateRangeEnd || name == AttrRotateSpeedRangeStart || name == AttrRotateSpeedRangeEnd) && value == AttrDefaultZero)
            return true;
        else if (IsAffectorTime(name) && value == AttrDefaultOne)
            return true;
        else if (IsAffectorColour(name) && (value == AttrDefaultColorIterpolator1 || value == AttrDefaultColorIterpolator2))
            return true;
        else if (IsAffectorColourChannel(name) && value == AttrDefaultZero)
            return true;
    }
    return false;
}

void RocketParticleEditor::WriteParameter(QTextStream &s, const std::string &name, const std::string &value, int indentLevel, bool writePostStartPracet)
{
    WriteParameter(s, QString::fromStdString(name), QString::fromStdString(value), indentLevel, writePostStartPracet);
}

void RocketParticleEditor::WriteParameter(QTextStream &s, const QString &name, const QString &value, int indentLevel, bool writePostStartPracet)
{
    WriteIndent(s, indentLevel);
    s << QString("%1 %2").arg(name).arg(value) << endl;
    if (writePostStartPracet)
        WriteStartPracet(s, indentLevel);
}

void RocketParticleEditor::WriteIndent(QTextStream &s, int indentLevel)
{
    if (indentLevel <= 0)
        return;
    for(int i=0; i<indentLevel; i++)
        s << "    ";
}

Ogre::ParticleSystem *RocketParticleEditor::GetSystem() const
{
    if (particle_.expired() || !particle_.lock()->IsLoaded())
        return 0;

    QString mainTemplateName = particle_.lock()->GetTemplateName(0);
    if (mainTemplateName.isEmpty())
        return 0;

    try
    {
        return Ogre::ParticleSystemManager::getSingleton().getTemplate(mainTemplateName.toStdString());
    }
    catch(Ogre::Exception& /*e*/) { }
    return 0;
}

Ogre::BillboardParticleRenderer *RocketParticleEditor::GetSystemRenderer() const
{
    Ogre::ParticleSystem *p = GetSystem();
    if (!p || !p->getRenderer())
        return 0;
    return dynamic_cast<Ogre::BillboardParticleRenderer*>(p->getRenderer());
}

void RocketParticleEditor::OnGeneratedMaterialDependencyFailed(IAssetTransfer *transfer, QString reason)
{
    ShowError("Particle Loading Failed", QString("The opened particle failed to load dependency %1. You need to fix it manually with the Text Editor.<br><br>This tool can not open malformed particles.").arg(transfer->SourceUrl()));
}

void RocketParticleEditor::LoadParticle(AssetPtr asset)
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
                ShowError("Particle Loading Failed", QString("The opened particle failed to load dependency %1. You need to fix it manually with the Text Editor.<br><br>This tool can not open malformed particles.").arg(ref.ref));
                return;
            }
        }

        ShowError("Failed Opening Particle", "Particle could not be loaded, it might have syntax errors or invalid texture references. If the particle is broken you need to edit it manually with the Text Editor.<br><br>This tool can not open malformed particles.");
        return;
    }

    OgreParticleAssetPtr particle = dynamic_pointer_cast<OgreParticleAsset>(asset);
    if (particle.get())
    {
        if (particle->DiskSourceType() == IAsset::Programmatic)
            SetMessage("Particle Loaded [Generated]");
        else
            SetMessage("Particle Loaded [Live Preview]");
        
        // Disconnect old asset (might change)
        if (particle_.lock().get())
            disconnect(particle_.lock().get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(LoadParticle(AssetPtr)));
            
        particle_ = weak_ptr<OgreParticleAsset>(particle);
        
        // Connect new asset for reloads
        if (particle_.lock().get())
            connect(particle_.lock().get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(LoadParticle(AssetPtr)), Qt::UniqueConnection);
    }
    else
    {
        ShowError("Failed Opening Resource", QString("The opened asset is of invalid type '%1' for this tool: ").arg(asset->Type()));
        return;
    }
    
    Ogre::ParticleSystem *p = GetSystem();
    Ogre::BillboardParticleRenderer *r = GetSystemRenderer();
    if (!p || !r)
    {
        ShowError("Failed Opening Particle", QString("Particle was seemingly loaded but it's not in a valid state (system or system renderer is null)."));
        particle_.reset();
        return;
    }

    // Main material
    ui_.materialLineEdit->blockSignals(true);
    ui_.materialLineEdit->setText(OgreMaterialToAssetReference(QString::fromStdString(p->getMaterialName())));
    ui_.materialLineEdit->blockSignals(false);
    
    // Root attributes
    OnQuotaChanged(static_cast<int>(p->getParticleQuota()), false);
    OnParticleWidthChanged(p->getDefaultWidth(), false);
    OnParticleHeightChanged(p->getDefaultHeight(), false);
    OnIterationIntervalChanged(p->getIterationInterval(), false);
    OnNonVisibleUpdateTimeoutChanged(p->getNonVisibleUpdateTimeout(), false);
    OnCullEachChanged(p->getCullIndividually(), false);
    OnSortedChanged(p->getSortingEnabled(), false);
    OnLocalSpaceChanged(p->getKeepParticlesInLocalSpace(), false);
    
    // Renderer root attributes
    OnAccurateFacingChanged(r->getUseAccurateFacing(), false);
    OnBillboardTypeChanged(static_cast<int>(r->getBillboardType()), false);
    OnBillboardOriginChanged(static_cast<int>(r->getBillboardOrigin()), false);
    OnBillboardRotationTypeChanged(static_cast<int>(r->getBillboardRotationType()), false);
   
    // Emitters
    int currentTabIndex = ui_.emittersTabWidget->currentIndex();
    ClearEmitterTabs();
    for(ushort i=0, num=p->getNumEmitters(); i<num; i++)
    {
        Ogre::ParticleEmitter *emitter = p->getEmitter(i);
        if (!emitter)
            continue;

        RocketParticleEmitterWidget *emitterEditor = new RocketParticleEmitterWidget(plugin_, emitter);
        connect(emitterEditor, SIGNAL(Touched()), SLOT(OnEmitterOrAffectorTouched()));

        ui_.emittersTabWidget->addTab(emitterEditor, QString::fromStdString(emitter->getType()));
    }
    if (currentTabIndex != -1 && currentTabIndex < ui_.emittersTabWidget->count())
        ui_.emittersTabWidget->setCurrentIndex(currentTabIndex);
    
    // Affectors
    currentTabIndex = ui_.affectorsTabWidget->currentIndex();
    ClearAffectorTabs();
    for(ushort i=0, num=p->getNumAffectors(); i<num; i++)
    {
        Ogre::ParticleAffector *affector = p->getAffector(i);
        if (!affector)
            continue;

        RocketParticleAffectorWidget *affectirEditor = new RocketParticleAffectorWidget(plugin_, affector);
        connect(affectirEditor, SIGNAL(Touched()), SLOT(OnEmitterOrAffectorTouched()));

        ui_.affectorsTabWidget->addTab(affectirEditor, QString::fromStdString(affector->getType()));
    }
    if (currentTabIndex != -1 && currentTabIndex < ui_.affectorsTabWidget->count())
        ui_.affectorsTabWidget->setCurrentIndex(currentTabIndex);
}

QString RocketParticleEditor::OgreMaterialToAssetReference(QString materialRef)
{
    if (particle_.expired() || materialRef.isEmpty())
        return "";

    // Resolve context
    QString context = particle_.lock()->Name();
    if (!context.endsWith("/"))
        context = context.mid(0, context.lastIndexOf("/")+1);

    // 1. Full URL ref.
    materialRef = AssetAPI::DesanitateAssetRef(materialRef);

    // 2. Relative ref to particle.
    if (materialRef.startsWith(context, Qt::CaseInsensitive))
        materialRef = materialRef.mid(context.length());
    else
    {
        // 3. Check if relative to S3 variation URL.
        QString variationContext = MeshmoonAsset::S3RegionVariationURL(particle_.lock()->Name());
        if (!variationContext.endsWith("/"))
            variationContext = variationContext.mid(0, variationContext.lastIndexOf("/")+1);
        if (materialRef.startsWith(variationContext, Qt::CaseInsensitive))
            materialRef = materialRef.mid(variationContext.length());
    }

    // 'BaseWhite' is the default Ogre material, we don't show this in the ui.
    if (materialRef.compare("BaseWhite", Qt::CaseSensitive) == 0)
        return "";
    return materialRef; 
}

std::string RocketParticleEditor::OgreMaterialToAssetReference(std::string materialRef)
{
    return OgreMaterialToAssetReference(QString::fromStdString(materialRef)).toStdString();
}

void RocketParticleEditor::SetMaterial(const QString &assetRef)
{
    if (Material().compare(assetRef.trimmed(), Qt::CaseSensitive) == 0)
        return;

    ui_.materialLineEdit->blockSignals(true);
    ui_.materialLineEdit->setText(OgreMaterialToAssetReference(assetRef.trimmed()));
    ui_.materialLineEdit->blockSignals(false);

    OnMaterialChanged();
}

void RocketParticleEditor::OnMaterialChanged()
{
    if (particle_.expired())
        return;

    QString materialRef = Material();
    if (lastVerifiedMaterialRef_.compare(materialRef, Qt::CaseSensitive) == 0)
        return;
    lastVerifiedMaterialRef_ = materialRef;

    if (!materialRef.isEmpty())
    {
        // Resolve this ref with the particle as a context
        AssetAPI *assetAPI = plugin_->GetFramework()->Asset();
        QString resolvedRef = assetAPI->ResolveAssetRef(particle_.lock()->Name(), materialRef);

        // Already loaded asset.
        AssetPtr existing = assetAPI->GetAsset(resolvedRef);
        if (existing.get())
        {
            // Check that this is a material
            if (existing->Type() == "OgreMaterial")
                SetMaterialState(IRocketAssetEditor::DS_OK);
            else
            {
                SetMaterialState(IRocketAssetEditor::DS_Error);
                LogError(LC + "Proposed material is loaded to the asset system but is not of material type: " + existing->Name() + " Resolved type: " + existing->Type());
            }   
            return;
        }

        AssetTransferPtr transfer = assetAPI->RequestAsset(resolvedRef, "OgreMaterial", true);
        connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnMaterialTransferCompleted(AssetPtr)));
        connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(OnMaterialTransferFailed(IAssetTransfer*, QString)));

        SetMaterialState(IRocketAssetEditor::DS_Verifying);
    }
    else
        SetMaterialState(IRocketAssetEditor::DS_OK);

    // If the ref was changed close down open editor that points to the old ref.
    OnCloseMaterialEditor();
}

QString RocketParticleEditor::Material() const
{
    return ui_.materialLineEdit->text().trimmed();
}

QString RocketParticleEditor::MaterialUrl() const
{
    return (!particle_.expired() ? plugin_->GetFramework()->Asset()->ResolveAssetRef(particle_.lock()->Name(), Material()) : "");
}

void RocketParticleEditor::SetMaterialState(IRocketAssetEditor::DependencyState state)
{
    // @todo Enable open editor button if we have a valid material set from storage
    //ui_.buttonMaterialViewer->setEnabled((state == DS_OK && !Material().isEmpty()));
    ui_.buttonMaterialViewer->setEnabled(false);

    // Setup ui
    if (materialState_ != state)
    {
        materialState_ = state;

        if (state == IRocketAssetEditor::DS_Verifying)
        {
            if (movieLoading_->isValid())
            {
                ui_.labelIcon->setMovie(movieLoading_);
                movieLoading_->start();
            }
            else
                ui_.labelIcon->clear();
        }
        else if (state == IRocketAssetEditor::DS_Error)
        {
            ui_.labelIcon->setPixmap(pixmapError_);
            if (movieLoading_->isValid()) movieLoading_->stop();
        }
        else if (state == IRocketAssetEditor::DS_OK)
        {
            ui_.labelIcon->setPixmap(pixmapOK_);
            if (movieLoading_->isValid()) movieLoading_->stop();
        }

        ui_.materialLineEdit->setStyleSheet(QString("color: %1;").arg((state == DS_OK ? "black" : (state == DS_Error ? "red" : "blue"))));
    }
    
    // Don't update to ogre while still verifying
    if (materialState_ == IRocketAssetEditor::DS_Verifying)
        return;

    Ogre::ParticleSystem *p = GetSystem();
    if (!p)
        return;

    QString currentMaterialName = QString::fromStdString(p->getMaterialName());
    QString futureMaterialName = "BaseWhite"; // The default material name for new material templates without a material set, stick with it.
    
    if (materialState_ == IRocketAssetEditor::DS_OK)
    {
        QString materialRef = Material();
        if (!materialRef.isEmpty())
        {
            // Must be loaded to the system to apply
            IAsset *asset = plugin_->GetFramework()->Asset()->GetAsset(materialRef).get();
            if (!asset)
                asset = plugin_->GetFramework()->Asset()->GetAsset(MaterialUrl()).get();
            OgreMaterialAsset *materialAsset = asset ? dynamic_cast<OgreMaterialAsset*>(asset) : 0;
            if (materialAsset && materialAsset->IsLoaded())
                futureMaterialName = AssetAPI::SanitateAssetRef(materialAsset->Name());
        }
    }

    if (currentMaterialName.compare(futureMaterialName, Qt::CaseSensitive) != 0)
    {
        p->setMaterialName(futureMaterialName.toStdString());
        Touched();
    }
}

void RocketParticleEditor::OnMaterialTransferCompleted(AssetPtr asset)
{
    QString assetRef = asset->Name();
    QString assetType = asset->Type();
    QString resolvedAssetType = plugin_->GetFramework()->Asset()->ResourceTypeForAssetRef(assetRef);
    asset.reset();

    // Unload if not a texture (binary fetch of a texture or a non texture asset to begin with)
    if (assetType != "OgreMaterial" || resolvedAssetType != "OgreMaterial")
        plugin_->GetFramework()->Asset()->ForgetAsset(assetRef, false);

    // Send this asset ref forward for material live preview and later saving.
    if (resolvedAssetType == "OgreMaterial")
        SetMaterialState(IRocketAssetEditor::DS_OK);
    else
    {
        SetMaterialState(IRocketAssetEditor::DS_Error);
        LogError(LC + "Proposed material is not of material type: " + assetRef + " Resolved type: " + resolvedAssetType);
    }
}

void RocketParticleEditor::OnMaterialTransferFailed(IAssetTransfer* /*transfer*/, QString /*reason*/)
{
    SetMaterialState(IRocketAssetEditor::DS_Error);
}

void RocketParticleEditor::OnOpenMaterialEditor()
{
    /// @todo Enable opening editor if the asset is from the current storage.
/*
    // Do we even have a ref?
    if (Material().isEmpty())
        return;
    // Is the ref valid?
    if (materialState_ != IRocketAssetEditor::DS_OK)
        return;

    if (materialEditor_)
    {
        materialEditor_->setFocus(Qt::ActiveWindowFocusReason);
        materialEditor_->activateWindow();
        return;
    }

    QWidget *topMostParent = RocketNotifications::TopMostParent(this);

    // We want to be able to open an supported images, not just storage refs.
    // Lets go around MeshmoonStorage::OpenVisualEditor(textureItem); and directly show the editor here.
    QString suffix = QFileInfo(Material()).suffix();
    if (RocketMaterialEditor::SupportedSuffixes().contains(suffix, Qt::CaseInsensitive))
    {
        materialEditor_ = new RocketMaterialEditor(new MeshmoonAsset(Url(), MeshmoonAsset::Material, this));
        materialEditor_->setAttribute(Qt::WA_DeleteOnClose, true);
        materialEditor_->Start(plugin_);
        materialEditor_->Show();

        plugin_->Notifications()->CenterToWindow(topMostParent, materialEditor_);
    }
    else
        plugin_->Notifications()->ShowSplashDialog("Material Editor does not support opening " + suffix.toUpper() + " file format.", "", QMessageBox::Ok, QMessageBox::Ok, topMostParent);
*/
}

void RocketParticleEditor::OnCloseMaterialEditor()
{
    if (materialEditor_)
        materialEditor_->close();
}

void RocketParticleEditor::OnSelectMaterialFromStorage()
{
    if (!plugin_ || !plugin_->Storage())
        return;

    QStringList suffixes;
    AssetTypeFactoryPtr textureFactory = plugin_->GetFramework()->Asset()->GetAssetTypeFactory("OgreMaterial");
    if (textureFactory.get())
        suffixes = textureFactory->TypeExtensions();
    else
        suffixes << ".material";

    QWidget *topMostParent = RocketNotifications::TopMostParent(this);
    RocketStorageSelectionDialog *dialog = plugin_->Storage()->ShowSelectionDialog(suffixes, true, lastStorageDir_, topMostParent);
    if (dialog)
    {
        connect(dialog, SIGNAL(StorageItemSelected(const MeshmoonStorageItem&)), SLOT(OnMaterialSelectedFromStorage(const MeshmoonStorageItem&)), Qt::UniqueConnection);
        connect(dialog, SIGNAL(Canceled()), SLOT(OnStorageSelectionClosed()), Qt::UniqueConnection);
    }
}

void RocketParticleEditor::OnMaterialSelectedFromStorage(const MeshmoonStorageItem &item)
{
    OnStorageSelectionClosed();

    if (!item.IsNull())
        SetMaterial(item.fullAssetRef);
}

void RocketParticleEditor::OnStorageSelectionClosed()
{
    // Store the last storage location into memory for the next dialog open
    RocketStorageSelectionDialog *dialog = dynamic_cast<RocketStorageSelectionDialog*>(sender());
    if (dialog)
        lastStorageDir_ = dialog->CurrentDirectory();
}

// Ogre::ParticleSystem attributes

void RocketParticleEditor::OnQuotaChanged(int quota, bool apply)
{
    if (quota < 1) quota = 1;
    
    ui_.quotaSpinBox->blockSignals(true);
    ui_.quotaSpinBox->setValue(quota);
    ui_.quotaSpinBox->blockSignals(false);

    if (apply && GetSystem())
    {
        GetSystem()->setParticleQuota(quota);
        Touched();
    }
}

void RocketParticleEditor::OnParticleWidthChanged(double width, bool apply)
{
    if (width < 0.01) width = 0.01;
    
    ui_.sizeWidthSpinBox->blockSignals(true);
    ui_.sizeWidthSpinBox->setValue(width);
    ui_.sizeWidthSpinBox->blockSignals(false);
    
    if (apply && GetSystem())
    {
        GetSystem()->setDefaultWidth(static_cast<Ogre::Real>(width));
        Touched();
    }
}

void RocketParticleEditor::OnParticleHeightChanged(double height, bool apply)
{
    if (height < 0.01) height = 0.01;

    ui_.sizeHeightSpinBox->blockSignals(true);
    ui_.sizeHeightSpinBox->setValue(height);
    ui_.sizeHeightSpinBox->blockSignals(false);
    
    if (apply && GetSystem())
    {
        GetSystem()->setDefaultHeight(static_cast<Ogre::Real>(height));
        Touched();
    }
}

void RocketParticleEditor::OnIterationIntervalChanged(double interval, bool apply)
{
    if (interval < 0.0) interval = 0.0;
    
    ui_.framerateSpinBox->blockSignals(true);
    ui_.framerateSpinBox->setValue(interval);
    ui_.framerateSpinBox->blockSignals(false);
    
    ui_.framerateSpinBox->setStyleSheet((interval == 0.0 ? "color: grey;" : ""));
    
    if (apply && GetSystem())
    {
        GetSystem()->setIterationInterval(interval);
        Touched();
    }
}

void RocketParticleEditor::OnNonVisibleUpdateTimeoutChanged(double timeout, bool apply)
{
    if (timeout < 0.0) timeout = 0.0;
    
    ui_.updateTimeoutSpinBox->blockSignals(true);
    ui_.updateTimeoutSpinBox->setValue(timeout);
    ui_.updateTimeoutSpinBox->blockSignals(false);
    
    ui_.updateTimeoutSpinBox->setStyleSheet((timeout == 0.0 ? "color: grey;" : ""));
    
    if (apply && GetSystem())
    {
        GetSystem()->setNonVisibleUpdateTimeout(timeout);
        Touched();
    }
}

void RocketParticleEditor::OnCullEachChanged(bool cullEach, bool apply)
{
    ui_.cullEachCheckBox->blockSignals(true);
    ui_.cullEachCheckBox->setChecked(cullEach);
    ui_.cullEachCheckBox->blockSignals(false);

    if (apply && GetSystem())
    {
        GetSystem()->setCullIndividually(cullEach);
        Touched();
    }
}

void RocketParticleEditor::OnSortedChanged(bool sorted, bool apply)
{
    ui_.sortedCheckBox->blockSignals(true);
    ui_.sortedCheckBox->setChecked(sorted);
    ui_.sortedCheckBox->blockSignals(false);
    
    if (apply && GetSystem())
    {
        GetSystem()->setSortingEnabled(sorted);
        Touched();
    }
}

void RocketParticleEditor::OnLocalSpaceChanged(bool localSpace, bool apply)
{
    ui_.localSpaceCheckBox->blockSignals(true);
    ui_.localSpaceCheckBox->setChecked(localSpace);
    ui_.localSpaceCheckBox->blockSignals(false);
    
    if (apply && GetSystem())
    {
        GetSystem()->setKeepParticlesInLocalSpace(localSpace);
        Touched();
    }
}

// Ogre::BillboardParticleRenderer attributes

void RocketParticleEditor::OnAccurateFacingChanged(bool accurateFacing, bool apply)
{
    ui_.accurateFacingCheckBox->blockSignals(true);
    ui_.accurateFacingCheckBox->setChecked(accurateFacing);
    ui_.accurateFacingCheckBox->blockSignals(false);
    
    if (apply && GetSystemRenderer())
    {
        GetSystemRenderer()->setUseAccurateFacing(accurateFacing);
        Touched();
    }
}

void RocketParticleEditor::OnBillboardTypeChanged(int type, bool apply)
{
    if (type < 0 || type > Ogre::BBT_PERPENDICULAR_SELF)
    {
        LogError(LC + "Out of Ogre::BillboardType range: " + QString::number(type));
        return;
    }
    
    ui_.bbTypeComboBox->blockSignals(true);
    ui_.bbTypeComboBox->setCurrentIndex(type);
    ui_.bbTypeComboBox->blockSignals(false);
    
    if (apply && GetSystemRenderer())
    {
        GetSystemRenderer()->setBillboardType(static_cast<Ogre::BillboardType>(type));
        Touched();
    }
}

void RocketParticleEditor::OnBillboardOriginChanged(int type, bool apply)
{
    if (type < 0 || type > Ogre::BBO_BOTTOM_RIGHT)
    {
        LogError(LC + "Out of Ogre::BillboardOrigin range: " + QString::number(type));
        return;
    }

    ui_.bbOriginComboBox->blockSignals(true);
    ui_.bbOriginComboBox->setCurrentIndex(type);
    ui_.bbOriginComboBox->blockSignals(false);
    
    if (apply && GetSystemRenderer())
    {
        GetSystemRenderer()->setBillboardOrigin(static_cast<Ogre::BillboardOrigin>(type));
        Touched();
    }
}

void RocketParticleEditor::OnBillboardRotationTypeChanged(int type, bool apply)
{
    if (type < 0 || type > Ogre::BBR_TEXCOORD)
    {
        LogError(LC + "Out of Ogre::BillboardRotationType range: " + QString::number(type));
        return;
    }

    ui_.bbRotationComboBox->blockSignals(true);
    ui_.bbRotationComboBox->setCurrentIndex(type);
    ui_.bbRotationComboBox->blockSignals(false);

    if (apply && GetSystemRenderer())
    {
        GetSystemRenderer()->setBillboardRotationType(static_cast<Ogre::BillboardRotationType>(type));
        Touched();
    }
}

// RocketParticleEmitterWidget

RocketParticleEmitterWidget::RocketParticleEmitterWidget(RocketPlugin *plugin, Ogre::ParticleEmitter *emitter, QWidget *parent) :
    QWidget(parent),
    plugin_(plugin),
    emitter_(0),
    colorMenu_(0),
    increaseColorMenu_(0),
    decreaseColorMenu_(0)
{
    ui_.setupUi(this);
    
    ui_.sizeFrame->hide();
    ui_.innerSizeFrame->hide();
    
    ui_.buttonColorStart->installEventFilter(this);
    ui_.buttonColorEnd->installEventFilter(this);
    ui_.buttonColorStart->setProperty("colorType", "RangeStart");
    ui_.buttonColorEnd->setProperty("colorType", "RangeEnd");
    
    connect(ui_.buttonColorStart, SIGNAL(clicked()), SLOT(OnPickColor()));
    connect(ui_.buttonColorEnd, SIGNAL(clicked()), SLOT(OnPickColor()));
    connect(ui_.directionFromCameraButton, SIGNAL(clicked()), SLOT(SetDirectionFromCamera()));
    
    connect(ui_.directionX, SIGNAL(valueChanged(double)), SLOT(OnDirectionXChanged(double)));
    connect(ui_.directionY, SIGNAL(valueChanged(double)), SLOT(OnDirectionYChanged(double)));
    connect(ui_.directionZ, SIGNAL(valueChanged(double)), SLOT(OnDirectionZChanged(double)));
    
    connect(ui_.positionX, SIGNAL(valueChanged(double)), SLOT(OnPositionXChanged(double)));
    connect(ui_.positionY, SIGNAL(valueChanged(double)), SLOT(OnPositionYChanged(double)));
    connect(ui_.positionZ, SIGNAL(valueChanged(double)), SLOT(OnPositionZChanged(double)));
    
    connect(ui_.emissionRateSpinBox, SIGNAL(valueChanged(double)), SLOT(OnEmissionRateChanged(double)));
    connect(ui_.angleHorizontalSlider, SIGNAL(valueChanged(int)), SLOT(OnAngleChanged(int)));

    connect(ui_.ttlMin, SIGNAL(valueChanged(double)), SLOT(OnTtlMinChanged(double)));
    connect(ui_.ttlMax, SIGNAL(valueChanged(double)), SLOT(OnTtlMaxChanged(double)));
    
    connect(ui_.velocityMin, SIGNAL(valueChanged(double)), SLOT(OnVelocityMinChanged(double)));
    connect(ui_.velocityMax, SIGNAL(valueChanged(double)), SLOT(OnVelocityMaxChanged(double)));
    
    connect(ui_.durationMin, SIGNAL(valueChanged(double)), SLOT(OnDurationMinChanged(double)));
    connect(ui_.durationMax, SIGNAL(valueChanged(double)), SLOT(OnDurationMaxChanged(double)));
    
    connect(ui_.repeatDelayMin, SIGNAL(valueChanged(double)), SLOT(OnRepeatDelayMinChanged(double)));
    connect(ui_.repeatDelayMax, SIGNAL(valueChanged(double)), SLOT(OnRepeatDelayMaxChanged(double)));
    
    connect(ui_.sizeWidth, SIGNAL(valueChanged(double)), SLOT(OnSizeWidthChanged(double)));
    connect(ui_.sizeHeight, SIGNAL(valueChanged(double)), SLOT(OnSizeHeightChanged(double)));
    connect(ui_.sizeDepth, SIGNAL(valueChanged(double)), SLOT(OnSizeDepthChanged(double)));

    connect(ui_.innerSizeWidth, SIGNAL(valueChanged(double)), SLOT(OnInnerSizeWidthChanged(double)));
    connect(ui_.innerSizeHeight, SIGNAL(valueChanged(double)), SLOT(OnInnerSizeHeightChanged(double)));
    connect(ui_.innerSizeDepth, SIGNAL(valueChanged(double)), SLOT(OnInnerSizeDepthChanged(double)));
    
    if (emitter)
        LoadEmitter(emitter);
}

RocketParticleEmitterWidget::~RocketParticleEmitterWidget()
{
    Reset();
}

void RocketParticleEmitterWidget::Reset()
{
    emitter_ = 0;
    if (colorPicker_)
    {
        colorPicker_->close();
        colorPicker_ = 0;
    }
    SAFE_DELETE(colorMenu_);
}

void RocketParticleEmitterWidget::LoadEmitter(Ogre::ParticleEmitter* emitter)
{
    emitter_ = emitter;
    if (!emitter_)
        return;
    
    OnColorRangeStartChanged(Color(emitter->getColourRangeStart()), false);
    OnColorRangeEndChanged(Color(emitter->getColourRangeEnd()), false);
    
    OnDirectionChanged(emitter->getDirection(), false);
    OnPositionChanged(emitter->getPosition(), false);
    
    OnEmissionRateChanged(emitter->getEmissionRate(), false);
    OnAngleChanged(Floor(static_cast<float>(emitter->getAngle().valueDegrees())), false);
    
    OnTtlMinChanged(emitter->getMinTimeToLive(), false);
    OnTtlMaxChanged(emitter->getMaxTimeToLive(), false);

    OnVelocityMinChanged(emitter->getMinParticleVelocity(), false);
    OnVelocityMaxChanged(emitter->getMaxParticleVelocity(), false);

    OnDurationMinChanged(emitter->getMinDuration(), false);
    OnDurationMaxChanged(emitter->getMaxDuration(), false);

    OnRepeatDelayMinChanged(emitter->getMinRepeatDelay(), false);
    OnRepeatDelayMaxChanged(emitter->getMaxRepeatDelay(), false);
    
    ReadSizeParameters();
    ReadInnerSizeParameters();
}

void RocketParticleEmitterWidget::ReadSizeParameters()
{
    if (!HasSizeParameters())
        return;

    if (!ui_.sizeFrame->isVisible())
        ui_.sizeFrame->show();
    
    if (HasParameter(AttrWidth))
        OnSizeWidthChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrWidth.toStdString())), false);
    if (HasParameter(AttrHeight))
        OnSizeHeightChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrHeight.toStdString())), false);
    if (HasParameter(AttrDepth))
        OnSizeDepthChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrDepth.toStdString())), false);
}

void RocketParticleEmitterWidget::OnSizeWidthChanged(double value, bool apply)
{
    ui_.sizeWidth->blockSignals(true);
    ui_.sizeWidth->setValue(value);
    ui_.sizeWidth->blockSignals(false);
    
    if (apply && emitter_)
    {
        emitter_->setParameter(AttrWidth.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnSizeHeightChanged(double value, bool apply)
{
    ui_.sizeHeight->blockSignals(true);
    ui_.sizeHeight->setValue(value);
    ui_.sizeHeight->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setParameter(AttrHeight.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnSizeDepthChanged(double value, bool apply)
{
    ui_.sizeDepth->blockSignals(true);
    ui_.sizeDepth->setValue(value);
    ui_.sizeDepth->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setParameter(AttrDepth.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::ReadInnerSizeParameters()
{
    if (!HasInnerSizeParameters())
        return;

    if (!ui_.innerSizeFrame->isVisible())
        ui_.innerSizeFrame->show();

    if (HasParameter(AttrInnerWidth))
        OnInnerSizeWidthChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrInnerWidth.toStdString())), false);
    if (HasParameter(AttrInnerHeight))
        OnInnerSizeHeightChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrInnerHeight.toStdString())), false);
    if (HasParameter(AttrInnerDepth))
        OnInnerSizeDepthChanged(Ogre::StringConverter::parseReal(emitter_->getParameter(AttrInnerDepth.toStdString())), false);
    else
        ui_.innerSizeDepth->hide();
}

void RocketParticleEmitterWidget::OnInnerSizeWidthChanged(double value, bool apply)
{
    ui_.innerSizeWidth->blockSignals(true);
    ui_.innerSizeWidth->setValue(value);
    ui_.innerSizeWidth->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setParameter(AttrInnerWidth.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnInnerSizeHeightChanged(double value, bool apply)
{
    ui_.innerSizeHeight->blockSignals(true);
    ui_.innerSizeHeight->setValue(value);
    ui_.innerSizeHeight->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setParameter(AttrInnerHeight.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnInnerSizeDepthChanged(double value, bool apply)
{
    ui_.innerSizeDepth->blockSignals(true);
    ui_.innerSizeDepth->setValue(value);
    ui_.innerSizeDepth->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setParameter(AttrInnerDepth.toStdString(), Ogre::StringConverter::toString(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}


bool RocketParticleEmitterWidget::HasParameter(const QString &name) const
{
    if (!emitter_)
        return false;

    const std::string nameStr = name.toStdString();
    const Ogre::ParameterList &params = emitter_->getParameters();
    for(size_t i=0; i<params.size(); i++)
        if (params[i].name == nameStr)
            return true;
    return false;
}

// Direction

void RocketParticleEmitterWidget::SetDirectionFromCamera()
{
    Scene *scene = plugin_ ? plugin_->GetFramework()->Renderer()->MainCameraScene() : 0;
    if (!scene)
        return;

    Entity *cameraEntity = scene->Subsystem<OgreWorld>()->Renderer()->MainCamera();
    shared_ptr<EC_Placeable> cameraPlaceable = (cameraEntity ? cameraEntity->Component<EC_Placeable>() : shared_ptr<EC_Placeable>());
    if (!cameraPlaceable)
        return;

    OnDirectionChanged(cameraPlaceable->WorldOrientation() * scene->ForwardVector());
}

void RocketParticleEmitterWidget::OnDirectionChanged(float3 dir, bool apply)
{
    OnDirectionXChanged(dir.x, apply);
    OnDirectionYChanged(dir.y, apply);
    OnDirectionZChanged(dir.z, apply);
}

void RocketParticleEmitterWidget::OnDirectionXChanged(double value, bool apply)
{
    ui_.directionX->blockSignals(true);
    ui_.directionX->setValue(value);
    ui_.directionX->blockSignals(false);
    
    if (apply && emitter_)
    {
        float3 dir = emitter_->getDirection();
        dir.x = value;
        emitter_->setDirection(dir);

        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnDirectionYChanged(double value, bool apply)
{
    ui_.directionY->blockSignals(true);
    ui_.directionY->setValue(value);
    ui_.directionY->blockSignals(false);

    if (apply && emitter_)
    {
        float3 dir = emitter_->getDirection();
        dir.y = value;
        emitter_->setDirection(dir);

        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnDirectionZChanged(double value, bool apply)
{
    ui_.directionZ->blockSignals(true);
    ui_.directionZ->setValue(value);
    ui_.directionZ->blockSignals(false);

    if (apply && emitter_)
    {
        float3 dir = emitter_->getDirection();
        dir.z = value;
        emitter_->setDirection(dir);

        emit Touched();
    }
}

// Position

void RocketParticleEmitterWidget::OnPositionChanged(float3 pos, bool apply)
{
    OnPositionXChanged(pos.x, apply);
    OnPositionYChanged(pos.y, apply);
    OnPositionZChanged(pos.z, apply);
}

void RocketParticleEmitterWidget::OnPositionXChanged(double value, bool apply)
{
    ui_.positionX->blockSignals(true);
    ui_.positionX->setValue(value);
    ui_.positionX->blockSignals(false);

    if (apply && emitter_)
    {
        float3 pos = emitter_->getPosition();
        pos.x = value;
        emitter_->setPosition(pos);

        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnPositionYChanged(double value, bool apply)
{
    ui_.positionY->blockSignals(true);
    ui_.positionY->setValue(value);
    ui_.positionY->blockSignals(false);

    if (apply && emitter_)
    {
        float3 pos = emitter_->getPosition();
        pos.y = value;
        emitter_->setPosition(pos);

        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnPositionZChanged(double value, bool apply)
{
    ui_.positionZ->blockSignals(true);
    ui_.positionZ->setValue(value);
    ui_.positionZ->blockSignals(false);

    if (apply && emitter_)
    {
        float3 pos = emitter_->getPosition();
        pos.z = value;
        emitter_->setPosition(pos);

        emit Touched();
    }
}

// Single value ones

void RocketParticleEmitterWidget::OnEmissionRateChanged(double value, bool apply)
{
    ui_.emissionRateSpinBox->blockSignals(true);
    ui_.emissionRateSpinBox->setValue(value);
    ui_.emissionRateSpinBox->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setEmissionRate(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnAngleChanged(int value, bool apply)
{
    ui_.angleHorizontalSlider->blockSignals(true);
    ui_.angleHorizontalSlider->setValue(value);
    ui_.angleValueLabel->setText(QString("%1&deg;").arg(value));
    ui_.angleHorizontalSlider->blockSignals(false);

    if (apply && emitter_)
    {
        emitter_->setAngle(Ogre::Degree(static_cast<Ogre::Real>(value)));
        emit Touched();
    }
}

// Colors

bool RocketParticleEmitterWidget::eventFilter(QObject *watched, QEvent *e)
{
    if (watched == ui_.buttonColorStart || watched == ui_.buttonColorEnd)
    {
        if (!colorPicker_.isNull())
            return false;

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
                    increase->setProperty("colorType", colorType);
                foreach(QAction *decrease, decreaseColorMenu_->actions())
                    decrease->setProperty("colorType", colorType);
                if (colorMenu_->actions().first())
                    colorMenu_->actions().first()->setProperty("colorType", colorType);

                colorMenu_->popup(QCursor::pos());
                return true;
            }
        }
    }
    return false;
}

void RocketParticleEmitterWidget::OnPickColor(QString type)
{
    if (!emitter_)
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
    if (type != "RangeStart" && type != "RangeEnd")
        return;

    colorPicker_ = new QColorDialog(this);
    colorPicker_->setOption(QColorDialog::ShowAlphaChannel);
    colorPicker_->setWindowTitle("Color Picker");
    colorPicker_->setOption(QColorDialog::NoButtons);
    colorPicker_->setAttribute(Qt::WA_DeleteOnClose, true);
    
#ifdef __APPLE__
    colorPicker_->setOption(QColorDialog::DontUseNativeDialog, true);
#endif

    Color color(1.0f, 1.0f, 1.0f, 1.0f);
    if (type == "RangeStart")
    {
        color = emitter_->getColourRangeStart();
        connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnColorRangeStartChanged(const QColor&)));
    }
    else if (type == "RangeEnd")
    {
        color = emitter_->getColourRangeEnd();
        connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnColorRangeEndChanged(const QColor&)));
    }

    colorPicker_->setCurrentColor(color);
    colorPicker_->show();
}

void RocketParticleEmitterWidget::OnRestoreDefaultColor()
{
    if (!emitter_)
        return;

    QString type = sender() ? sender()->property("colorType").toString() : "";
    if (type == "RangeStart")
        OnColorRangeStartChanged(QColor(255, 255, 255, 255));
    else if (type == "RangeEnd")
        OnColorRangeEndChanged(QColor(255, 255, 255, 255));
}

void RocketParticleEmitterWidget::OnIncreaseOrDecreaseColor()
{
    QObject *s = sender();
    if (!s)
        return;
    QVariant type = s->property("colorType");
    QVariant multiplier = s->property("multiplier");
    if (!type.isValid() || type.isNull() || !multiplier.isValid() || multiplier.isNull())
        return;
    OnMultiplyColor(type.toString(), multiplier.toFloat());
}

void RocketParticleEmitterWidget::OnMultiplyColor(const QString &type, float scalar)
{
    if (!emitter_)
        return;

    if (type == "RangeStart")
        OnColorRangeStartChanged(Color(emitter_->getColourRangeStart()) * scalar);
    else if (type == "RangeEnd")
        OnColorRangeEndChanged(Color(emitter_->getColourRangeEnd()) * scalar);
}

void RocketParticleEmitterWidget::OnColorRangeStartChanged(const QColor &color, bool apply)
{       
    if (color.red() == 255 && color.green() == 255 && color.blue() == 255 && color.alpha() == 255)
    {
        ui_.buttonColorStart->setStyleSheet("background-color: transparent;");
        ui_.buttonColorStart->setText("No Color");
        ui_.buttonColorStart->setToolTip("Color Range Start");
    }
    else
    {
        QString c = QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
        ui_.buttonColorStart->setStyleSheet(QString("background-color: %1;").arg(c));
        ui_.buttonColorStart->setText("");
        ui_.buttonColorStart->setToolTip("Color Range Start " + c);
    }

    if (apply && emitter_)
    {
        emitter_->setColourRangeStart(Color(color));
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnColorRangeEndChanged(const QColor &color, bool apply)
{
    if (color.red() == 255 && color.green() == 255 && color.blue() == 255 && color.alpha() == 255)
    {
        ui_.buttonColorEnd->setStyleSheet("background-color: transparent;");
        ui_.buttonColorEnd->setText("No Color");
        ui_.buttonColorEnd->setToolTip("Color Range End");
    }
    else
    {
        QString c = QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
        ui_.buttonColorEnd->setStyleSheet(QString("background-color: %1;").arg(c));
        ui_.buttonColorEnd->setText("");
        ui_.buttonColorEnd->setToolTip("Color Range End " + c);
    }

    if (apply && emitter_)
    {
        emitter_->setColourRangeEnd(Color(color));
        emit Touched();
    }
}

// Min/max attributes

void RocketParticleEmitterWidget::OnTtlMinChanged(double value, bool apply)
{
    ui_.ttlMin->blockSignals(true);
    ui_.ttlMin->setValue(value);
    ui_.ttlMin->blockSignals(false);
    
    ui_.ttlMin->setStyleSheet(value == 5.0 ? "color: grey;" : "");
    if (ui_.ttlMax->value() != 5.0)
        ui_.ttlMax->setStyleSheet(ui_.ttlMax->value() < value ? "color: red;" : "");
    
    if (apply && emitter_)
    {
        emitter_->setMinTimeToLive(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnTtlMaxChanged(double value, bool apply)
{
    ui_.ttlMax->blockSignals(true);
    ui_.ttlMax->setValue(value);
    ui_.ttlMax->blockSignals(false);
    
    if (value == 5.0)
        ui_.ttlMax->setStyleSheet("color: grey;");
    else
        ui_.ttlMax->setStyleSheet(ui_.ttlMin->value() > value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMaxTimeToLive(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnVelocityMinChanged(double value, bool apply)
{
    ui_.velocityMin->blockSignals(true);
    ui_.velocityMin->setValue(value);
    ui_.velocityMin->blockSignals(false);

    ui_.velocityMin->setStyleSheet(value == 1.0 ? "color: grey;" : "");
    if (ui_.velocityMax->value() != 1.0)
        ui_.velocityMax->setStyleSheet(ui_.velocityMax->value() < value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMinParticleVelocity(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnVelocityMaxChanged(double value, bool apply)
{
    ui_.velocityMax->blockSignals(true);
    ui_.velocityMax->setValue(value);
    ui_.velocityMax->blockSignals(false);

    if (value == 1.0)
        ui_.velocityMax->setStyleSheet("color: grey;");
    else
        ui_.velocityMax->setStyleSheet(ui_.velocityMin->value() > value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMaxParticleVelocity(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnDurationMinChanged(double value, bool apply)
{
    ui_.durationMin->blockSignals(true);
    ui_.durationMin->setValue(value);
    ui_.durationMin->blockSignals(false);

    ui_.durationMin->setStyleSheet(value == 0.0 ? "color: grey;" : "");
    if (ui_.durationMax->value() != 0.0)
        ui_.durationMax->setStyleSheet(ui_.durationMax->value() < value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMinDuration(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnDurationMaxChanged(double value, bool apply)
{
    ui_.durationMax->blockSignals(true);
    ui_.durationMax->setValue(value);
    ui_.durationMax->blockSignals(false);

    if (value == 0.0)
        ui_.durationMax->setStyleSheet("color: grey;");
    else
        ui_.durationMax->setStyleSheet(ui_.durationMin->value() > value ? "color: red;" : "");
        
    if (apply && emitter_)
    {
        emitter_->setMaxDuration(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnRepeatDelayMinChanged(double value, bool apply)
{
    ui_.repeatDelayMin->blockSignals(true);
    ui_.repeatDelayMin->setValue(value);
    ui_.repeatDelayMin->blockSignals(false);

    ui_.repeatDelayMin->setStyleSheet(value == 0.0 ? "color: grey;" : "");
    if (ui_.repeatDelayMax->value() != 0.0)
        ui_.repeatDelayMax->setStyleSheet(ui_.repeatDelayMax->value() < value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMinRepeatDelay(value);
        emit Touched();
    }
}

void RocketParticleEmitterWidget::OnRepeatDelayMaxChanged(double value, bool apply)
{
    ui_.repeatDelayMax->blockSignals(true);
    ui_.repeatDelayMax->setValue(value);
    ui_.repeatDelayMax->blockSignals(false);

    if (value == 0.0)
        ui_.repeatDelayMax->setStyleSheet("color: grey;");
    else
        ui_.repeatDelayMax->setStyleSheet(ui_.repeatDelayMin->value() > value ? "color: red;" : "");

    if (apply && emitter_)
    {
        emitter_->setMaxRepeatDelay(value);
        emit Touched();
    }
}

Ogre::ParticleEmitter *RocketParticleEmitterWidget::Emitter() const
{
    return emitter_;
}

bool RocketParticleEmitterWidget::HasSizeParameters() const
{
    return (HasParameter(AttrWidth) && HasParameter(AttrHeight) && HasParameter(AttrDepth));
}

bool RocketParticleEmitterWidget::HasInnerSizeParameters() const
{
    return (HasParameter(AttrInnerWidth) && HasParameter(AttrInnerHeight));
}

bool RocketParticleEmitterWidget::HasInnerDepthParameter() const
{
    return HasParameter(AttrInnerDepth);
}

// RocketParticleAffectorWidget

RocketParticleAffectorWidget::RocketParticleAffectorWidget(RocketPlugin *plugin, Ogre::ParticleAffector *affector, QWidget *parent) :
    QWidget(parent),
    plugin_(plugin),
    affector_(0)
{
    ui_.setupUi(this);
    
    LoadAffector(affector);
}

RocketParticleAffectorWidget::~RocketParticleAffectorWidget()
{
    Reset();
}

void RocketParticleAffectorWidget::Reset()
{
    affector_ = 0;
    if (colorPicker_)
    {
        colorPicker_->close();
        colorPicker_ = 0;
    }
}

void RocketParticleAffectorWidget::LoadAffector(Ogre::ParticleAffector* affector)
{
    affector_ = affector;
    if (!affector_)
        return;
        
    CreateDescription();
        
    const Ogre::ParameterList &params = affector->getParameters();
    for (size_t i=0, num=params.size(); i<num; i++)
    {
        const Ogre::ParameterDef &def = params[i];
        const QString name = QString::fromStdString(def.name);
        const QString value = QString::fromStdString(affector_->getParameter(def.name));
        CreateParameterWidget(name, value, def.paramType);
    }
}

Ogre::ParticleAffector *RocketParticleAffectorWidget::Affector() const
{
    return affector_;
}

void RocketParticleAffectorWidget::CreateDescription()
{
    QString affectorType = QString::fromStdString(affector_->getType());
    QString desc = "";
    
    if (affectorType == "LinearForce")
        desc = "Applies a force vector to all particles to modify their trajectory. Can be used for gravity, wind, or any other linear force.";
    else if (affectorType == "ColourFader")
        desc = "This affector modifies the colour of particles in flight. Adjusts the color components by the set values per second.";
    else if (affectorType == "ColourFader2")
        desc = "This affector modifies the colour of particles in flight. Adjusts the color components by the set values per second. The second colour change state is activated once the set amount of time remains in the particles life.";
    else if (affectorType == "Scaler")
        desc = "This affector scales particles in flight. Rate is the amount by which to scale the particles in both the x and y direction per second, it can be negative or positive.";
    else if (affectorType == "Rotator")
        desc = "This affector rotates particles in flight. This is done by rotating the texture.";
    else if (affectorType == "ColourInterpolator")
        desc = "This affector modifies the colour of particles in flight, except it has a variable number of defined stages. It swaps the particle colour for several stages in the life of a particle and interpolates between them. The time values define the start time as life time position from 0.0 to 1.0.";
    else if (affectorType == "DeflectorPlane")
        desc = "This affector defines a plane which deflects particles which collide with it.";
    else if (affectorType == "DirectionRandomiser")
        desc = "This affector applies randomness to the movement of the particles.";
            
    if (!desc.isEmpty())
        ui_.descriptionLabel->setText(desc);
    else
        ui_.descriptionLabel->hide();
}

QDoubleSpinBox *RocketParticleAffectorWidget::CreateRealWidget(double min, double max, double step, int decimals)
{
    QDoubleSpinBox *w = new QDoubleSpinBox();
    w->setMinimum(min);
    w->setMaximum(max);
    w->setSingleStep(step);
    w->setDecimals(decimals);
    return w;
}

QSpinBox *RocketParticleAffectorWidget::CreateIntWidget(int min, int max, int step)
{
    QSpinBox *w = new QSpinBox();
    w->setMinimum(min);
    w->setMaximum(max);
    w->setSingleStep(step);
    return w;
}

void RocketParticleAffectorWidget::CreateParameterWidget(const QString &name, const QString &value, Ogre::ParameterType type)
{
    if (!affector_)
        return;
        
    QString affectorType = QString::fromStdString(affector_->getType());
    QString tooltip = "";
    QString labelText = name;
    labelText = labelText.replace("_", " ");
    labelText = labelText.left(1).toUpper() + labelText.mid(1);

    QWidget *widget = 0;
    
    switch(type)
    {
        case Ogre::PT_BOOL:
        {
            QCheckBox *w = new QCheckBox();
            w->setChecked(Ogre::StringConverter::parseBool(value.toStdString()));
            connect(w, SIGNAL(toggled(bool)), SLOT(OnBoolParameterChanged(bool)));
            
            widget = w;
            break;
        }
        case Ogre::PT_REAL:
        {
            double min = 0.0;
            double max = 1000000.0;
            bool negateBeforeSet = false;
            QString suffix = "";

            if (affectorType == "Scaler" && name == "rate")
            {
                min = -1000000.0;
                labelText = "Scaling rate";
                tooltip = "The amount by which to scale the particles in both the x and y direction per second.";
                suffix = " x/per sec";
            }
            else if ((affectorType == "ColourFader" || affectorType == "ColourFader2"))
            {
                if (IsAffectorColourChannel(name))
                {
                    negateBeforeSet = true;
                    tooltip = "The amount by which to adjust the color component per second.";
                    suffix = " x/per sec";
                }
            }
            else if (affectorType == "ColourInterpolator")
            {
                if (IsAffectorTime(name))
                {
                    max = 1.0;
                    tooltip = "The point in life time, from 0 to 1, to start stage " + name.right(1);
                }
            }
            else if (affectorType == "Rotator")
            {
                if (name == "rotation_speed_range_start")
                {
                    tooltip = "The start of a range of rotation speeds to be assigned to emitted particles as degrees per second.";
                    suffix = " deg/sec";
                }
                else if (name == "rotation_speed_range_end")
                {
                    tooltip = "The end of a range of rotation speeds to be assigned to emitted particles as degrees per second.";
                    suffix = " deg/sec";
                }
                else if (name == "rotation_range_start")
                {
                    tooltip = "The start of a range of rotation angles to be assigned to emitted particles in degrees.";
                    suffix = " deg";
                }
                else if (name == "rotation_range_end")
                {
                    tooltip = "The end of a range of rotation angles to be assigned to emitted particles in degrees.";
                    suffix = " deg";
                }
            }

            QDoubleSpinBox *w = CreateRealWidget(min, max);
            if (!suffix.isEmpty())
                w->setSuffix(suffix);
            w->setProperty("negateBeforeSet", negateBeforeSet);
            w->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            Ogre::Real valueReal = Ogre::StringConverter::parseReal(value.toStdString());
            if (negateBeforeSet)
                valueReal = -valueReal;
            w->setValue(valueReal);
            connect(w, SIGNAL(valueChanged(double)), SLOT(OnRealParameterChanged(double)));

            widget = w;
            break;
        }
        case Ogre::PT_STRING:
        {
            QStringList options = StringParameterOptions(name);
            if (options.isEmpty())
            {
                LogError(QString("[RocketParticleAffectorWidget]: Could not get options on type %1 for attribute '%2' in %3 affector")
                    .arg(static_cast<int>(type)).arg(name).arg(QString::fromStdString(affector_->getType())));
                return;
            }

            QComboBox *w = new QComboBox();
            w->addItems(options);
            w->setCurrentIndex(options.indexOf(value));
            connect(w, SIGNAL(currentIndexChanged(QString)), SLOT(OnStringParameterChanged(QString)));

            widget = w;
            break;
        }
        case Ogre::PT_VECTOR3:
        {
            widget = new QWidget();
            QHBoxLayout *l = new QHBoxLayout();
            l->setContentsMargins(0,0,0,0);
            widget->setLayout(l);

            Ogre::Vector3 vec = Ogre::StringConverter::parseVector3(value.toStdString());

            QDoubleSpinBox *spinBox = CreateRealWidget(-1000000.0);
            spinBox->setObjectName("vectorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "x");
            spinBox->setPrefix("X ");
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            spinBox->setValue(vec.x);
            connect(spinBox, SIGNAL(valueChanged(double)), SLOT(OnRealParameterChanged(double)));
            widget->layout()->addWidget(spinBox);

            spinBox = CreateRealWidget(-1000000.0);
            spinBox->setObjectName("vectorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "y");
            spinBox->setPrefix("Y ");
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            spinBox->setValue(vec.y);
            connect(spinBox, SIGNAL(valueChanged(double)), SLOT(OnRealParameterChanged(double)));
            widget->layout()->addWidget(spinBox);

            spinBox = CreateRealWidget(-1000000.0);
            spinBox->setObjectName("vectorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "z");
            spinBox->setPrefix("Z ");
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            spinBox->setValue(vec.z);
            connect(spinBox, SIGNAL(valueChanged(double)), SLOT(OnRealParameterChanged(double)));
            widget->layout()->addWidget(spinBox);
            
            QPushButton *button = new QPushButton("From Camera");
            button->setProperty("parameterName", name);
            button->setProperty("parameterType", static_cast<int>(type));
            connect(button, SIGNAL(clicked()), SLOT(OnCameraDirToParameter()));
            widget->layout()->addWidget(button);

            break;
        }
        case Ogre::PT_INT:
        {
            QString suffix = "";
            if (affectorType == "ColourFader2" && name == "state_change")
            {
                labelText = "Change when time to live";
                suffix = " sec";
                tooltip = "When a particle has this much time left to live, it will switch to state 2.";
            }

            QSpinBox *w = CreateIntWidget();
            if (!suffix.isEmpty())
                w->setSuffix(suffix);
            w->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            w->setValue(Ogre::StringConverter::parseInt(value.toStdString()));
            connect(w, SIGNAL(valueChanged(int)), SLOT(OnIntParameterChanged(int)));

            widget = w;
            break;
        }
        case Ogre::PT_COLOURVALUE:
        {
            widget = new QWidget();
            QHBoxLayout *l = new QHBoxLayout();
            l->setContentsMargins(0,0,0,0);
            widget->setLayout(l);

            Ogre::ColourValue color = Ogre::StringConverter::parseColourValue(value.toStdString());

            QSpinBox *spinBox = CreateIntWidget(0, 255);
            spinBox->setObjectName("colorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "r");
            spinBox->setPrefix("R ");
            spinBox->setToolTip("Red color channel 0-255");
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            spinBox->setValue(FloorInt(color.r * 255.0f));
            connect(spinBox, SIGNAL(valueChanged(int)), SLOT(OnIntParameterChanged(int)));
            widget->layout()->addWidget(spinBox);

            spinBox = CreateIntWidget(0, 255);
            spinBox->setObjectName("colorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "g");
            spinBox->setPrefix("G ");
            spinBox->setToolTip("Green color channel 0-255");
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            spinBox->setValue(FloorInt(color.g * 255.0f));
            connect(spinBox, SIGNAL(valueChanged(int)), SLOT(OnIntParameterChanged(int)));
            widget->layout()->addWidget(spinBox);

            spinBox = CreateIntWidget(0, 255);
            spinBox->setObjectName("colorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "b");
            spinBox->setPrefix("B ");
            spinBox->setToolTip("Blue color channel 0-255");
            spinBox->setValue(FloorInt(color.b * 255.0f));
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            connect(spinBox, SIGNAL(valueChanged(int)), SLOT(OnIntParameterChanged(int)));
            widget->layout()->addWidget(spinBox);

            spinBox = CreateIntWidget(0, 255);
            spinBox->setObjectName("colorWidget");
            spinBox->setProperty("parameterName", name);
            spinBox->setProperty("parameterType", static_cast<int>(type));
            spinBox->setProperty("parameterSubComponent", "a");
            spinBox->setPrefix("A ");
            spinBox->setToolTip("Alpha color channel 0-255");
            spinBox->setValue(FloorInt(color.a * 255.0f));
            spinBox->setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
            connect(spinBox, SIGNAL(valueChanged(int)), SLOT(OnIntParameterChanged(int)));
            widget->layout()->addWidget(spinBox);
            
            QPushButton *button = new QPushButton("Pick Color...");
            button->setProperty("parameterName", name);
            button->setProperty("parameterType", static_cast<int>(type));
            connect(button, SIGNAL(clicked()), SLOT(OnPickColorParameter()));
            widget->layout()->addWidget(button);

            break;
        }
        default:
        {
            LogWarning(QString("[RocketParticleAffectorWidget]: Unsupported type %1 for attribute '%2' in %3 affector")
                .arg(static_cast<int>(type)).arg(name).arg(QString::fromStdString(affector_->getType())));
            return;
        }
    }
    
    if (widget)
    {
        if (!tooltip.isEmpty())
            widget->setToolTip(tooltip);
        widget->setProperty("parameterName", name);
        widget->setProperty("parameterType", static_cast<int>(type));
        ui_.parametersLayout->addRow(labelText, widget);
    }
}

QStringList RocketParticleAffectorWidget::StringParameterOptions(const QString &name)
{
    QStringList options;
    if (name == "force_application")
        options << "add" << "average";
    return options;
}

Ogre::ParameterDef RocketParticleAffectorWidget::GetSenderDef()
{
    Ogre::ParameterDef def("", "", Ogre::PT_BOOL);
    
    QObject *s = sender();
    if (!affector_ || !s)
        return def;
        
    std::string name = s->property("parameterName").toString().toStdString();
    if (name.empty())
    {
        LogError("[RocketParticleAffectorWidget]: Sending object is missing 'parameterName' property.");
        return def;
    }
    def.name = name;
    def.paramType = static_cast<Ogre::ParameterType>(s->property("parameterType").toInt());
    return def;
}

void RocketParticleAffectorWidget::OnBoolParameterChanged(bool value)
{
    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
        
    bool result = false;
    if (def.paramType == Ogre::PT_BOOL)
        result = affector_->setParameter(def.name, Ogre::StringConverter::toString(value));
    
    if (!result)    
        LogError("[RocketParticleAffectorWidget]: Failed to set parameter " + QString::fromStdString(def.name) + " to " + (value ? "true" : "false"));
}

void RocketParticleAffectorWidget::OnIntParameterChanged(int value)
{
    QObject *s = sender();
    if (!affector_ || !s)
        return;

    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
        
    bool result = false;
    if (def.paramType == Ogre::PT_INT)
        result = affector_->setParameter(def.name, Ogre::StringConverter::toString(value));
    else if (def.paramType == Ogre::PT_REAL)
    {
        if (s->property("convertToRealColor").toBool())
        {
            float colorFloat = static_cast<float>(value) / 255.0f;
            result = affector_->setParameter(def.name, Ogre::StringConverter::toString(colorFloat));
        }
        else
            result = affector_->setParameter(def.name, Ogre::StringConverter::toString(static_cast<float>(value)));
    }
    else if (def.paramType == Ogre::PT_COLOURVALUE)
    {
        QString subComponent = s->property("parameterSubComponent").toString();
        if (subComponent.isEmpty())
        {
            LogError("[RocketParticleAffectorWidget]: OnRealParameterChanged() Sending object is missing 'parameterSubComponent' property for Ogre::PT_VECTOR3 parameter.");
            return;
        }
        Ogre::ColourValue color = Ogre::StringConverter::parseColourValue(affector_->getParameter(def.name));
        if (subComponent == "r")
            color.r = static_cast<float>(value) / 255.0f;
        else if (subComponent == "g")
            color.g = static_cast<float>(value) / 255.0f;
        else if (subComponent == "b")
            color.b = static_cast<float>(value) / 255.0f;
        else if (subComponent == "a")
            color.a = static_cast<float>(value) / 255.0f;

        result = affector_->setParameter(def.name, Ogre::StringConverter::toString(color));
    }
    
    if (!result)
        LogError("[RocketParticleAffectorWidget]: Failed to set parameter PT_INT " + QString::fromStdString(def.name) + " to " + QString::number(value));
}

void RocketParticleAffectorWidget::OnRealParameterChanged(double value)
{
    QObject *s = sender();
    if (!affector_ || !s)
        return;

    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
        
    bool result = false;
    if (def.paramType == Ogre::PT_REAL)
    {
        if (s->property("negateBeforeSet").toBool() && value != 0.0)
            value = -value;
        result = affector_->setParameter(def.name, Ogre::StringConverter::toString(value));
    }
    else if (def.paramType == Ogre::PT_VECTOR3)
    {
        QString subComponent = s->property("parameterSubComponent").toString();
        if (subComponent.isEmpty())
        {
            LogError("[RocketParticleAffectorWidget]: OnRealParameterChanged() Sending object is missing 'parameterSubComponent' property for Ogre::PT_VECTOR3 parameter.");
            return;
        }
        Ogre::Vector3 vec = Ogre::StringConverter::parseVector3(affector_->getParameter(def.name));
        if (subComponent == "x")
            vec.x = value;
        else if (subComponent == "y")
            vec.y = value;
        else if (subComponent == "z")
            vec.z = value;
        result = affector_->setParameter(def.name, Ogre::StringConverter::toString(vec));
    }
    
    if (!result)
        LogError("[RocketParticleAffectorWidget]: Failed to set PT_REAL parameter " + QString::fromStdString(def.name) + " to " + QString::number(value));
}

void RocketParticleAffectorWidget::OnStringParameterChanged(QString value)
{
    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;

    if (!affector_->setParameter(def.name, value.toStdString()))
        LogError("[RocketParticleAffectorWidget]: Failed to set PT_STRING parameter " + QString::fromStdString(def.name) + " to " + value);
}

void RocketParticleAffectorWidget::OnCameraDirToParameter()
{
    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
        
    float3 dir = DirFromCamera();
    
    // Update vector spinbox widgets from parent layout
    QList<QDoubleSpinBox*> vectorWidgets = findChildren<QDoubleSpinBox*>("vectorWidget");
    foreach(QDoubleSpinBox *spinBox, vectorWidgets)
    {
        if (!spinBox)
            continue;

        QString paramName = spinBox->property("parameterName").toString();
        if (paramName != QString::fromStdString(def.name))
            continue;

        QString subComponent = spinBox->property("parameterSubComponent").toString();
        spinBox->blockSignals(true);
        if (subComponent == "x")
            spinBox->setValue(dir.x);
        else if (subComponent == "y")
            spinBox->setValue(dir.y);
        else if (subComponent == "z")
            spinBox->setValue(dir.z);
        spinBox->blockSignals(false);
    }

    if (def.paramType == Ogre::PT_VECTOR3 && !affector_->setParameter(def.name, Ogre::StringConverter::toString(dir)))
        LogError("[RocketParticleAffectorWidget]: Failed to set PT_VECTOR3 parameter " + QString::fromStdString(def.name) + " to camera directional vector.");
}

void RocketParticleAffectorWidget::OnPickColorParameter()
{
    if (!colorPicker_.isNull())
    {        
        if (colorPicker_->isMinimized())
            colorPicker_->showNormal();
        colorPicker_->setFocus(Qt::ActiveWindowFocusReason);
        colorPicker_->activateWindow();
        return;
    }
        
    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
        
    if (def.paramType != Ogre::PT_COLOURVALUE)
        return;
        
    colorPicker_ = new QColorDialog(this);
    colorPicker_->setProperty("parameterName", QString::fromStdString(def.name));
    colorPicker_->setProperty("parameterType", static_cast<int>(def.paramType));
    colorPicker_->setOption(QColorDialog::ShowAlphaChannel);
    colorPicker_->setWindowTitle("Color Picker");
    colorPicker_->setOption(QColorDialog::NoButtons);
    colorPicker_->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(colorPicker_, SIGNAL(currentColorChanged(const QColor&)), SLOT(OnColorParameterChanged(const QColor&)));

    Color currentColor = Ogre::StringConverter::parseColourValue(affector_->getParameter(def.name));
    colorPicker_->setCurrentColor(currentColor);
    colorPicker_->show();
}

void RocketParticleAffectorWidget::OnColorParameterChanged(const QColor &color)
{
    Ogre::ParameterDef def = GetSenderDef();
    if (def.name.empty())
        return;
    if (def.paramType != Ogre::PT_COLOURVALUE)
        return;

    // Update color spinbox widgets from parent widget
    QList<QSpinBox*> colorWidgets = findChildren<QSpinBox*>("colorWidget");
    foreach(QSpinBox *spinBox, colorWidgets)
    {
        if (!spinBox)
            continue;

        QString paramName = spinBox->property("parameterName").toString();
        if (paramName != QString::fromStdString(def.name))
            continue;

        QString subComponent = spinBox->property("parameterSubComponent").toString();
        spinBox->blockSignals(true);
        if (subComponent == "r")
            spinBox->setValue(color.red());
        else if (subComponent == "g")
            spinBox->setValue(color.green());
        else if (subComponent == "b")
            spinBox->setValue(color.blue());
        else if (subComponent == "a")
            spinBox->setValue(color.alpha());
        spinBox->blockSignals(false);
    }

    Color currentColor(color);
    if (!affector_->setParameter(def.name, Ogre::StringConverter::toString(Ogre::ColourValue(currentColor))))
        LogError("[RocketParticleAffectorWidget]: Failed to set PT_COLOURVALUE parameter " + QString::fromStdString(def.name) + " to picked color " + currentColor.toString());
}

float3 RocketParticleAffectorWidget::DirFromCamera()
{
    Scene *scene = plugin_ ? plugin_->GetFramework()->Renderer()->MainCameraScene() : 0;
    if (!scene)
        return float3(0.f, 0.f, 0.f);

    Entity *cameraEntity = scene->Subsystem<OgreWorld>()->Renderer()->MainCamera();
    shared_ptr<EC_Placeable> cameraPlaceable = (cameraEntity ? cameraEntity->Component<EC_Placeable>() : shared_ptr<EC_Placeable>());
    if (!cameraPlaceable)
        return float3(0.f, 0.f, 0.f);

    return (cameraPlaceable->WorldOrientation() * scene->ForwardVector());
}
