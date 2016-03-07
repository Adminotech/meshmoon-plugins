/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketScenePackager.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"

#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageHelpers.h"
#include "utils/RocketFileSystem.h"

#include "Framework.h"
#include "IRenderer.h"
#include "Application.h"
#include "ConfigAPI.h"
#include "LoggingFunctions.h"

#include "Math/MathFunc.h"

#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "AssetReference.h"

#include "UiAPI.h"
#include "UiMainWindow.h"

#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"

#include "EC_Mesh.h"
#include "EC_Material.h"

#include "OgreMaterialAsset.h"
#include "TextureAsset.h"
#include <OgreTexture.h>
#include <OgreTextureUnitState.h>

#include <QFileInfoList>
#include <QFile>
#include <QList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QUuid>
#include <QFileDialog>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QProcess>

#include "MemoryLeakCheck.h"

RocketScenePackager::RocketScenePackager(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    LC_("[RocketScenePackager]: "),
    contentToolsDataCompName_("AdminoContent"),
    widget_(new QWidget()),
    logWindow_(0),
    running_(false),
    stopping_(false)
{
    ui_.setupUi(widget_);

    QDir outdir(Application::UserDataDirectory());
    if (!outdir.exists("optimizer"))
        outdir.mkdir("optimizer");
    outdir.cd("optimizer");

    ReadConfig();
    
    // Read actual txml name and fill what we can
    QString txmlName = plugin_->Storage()->CurrentState().storageTxmlPath.split("/", QString::SkipEmptyParts).last().trimmed();
    txmlName = txmlName.replace(".txml", "", Qt::CaseInsensitive);
    if (!txmlName.isEmpty())
    {
        txmlName = txmlName.replace(" ", "-");

        ui_.outputDirectoryLineEdit->setText(QDir::toNativeSeparators(outdir.absoluteFilePath("temp-" + txmlName)));
        ui_.bundlePrefixLineEdit->setText(txmlName.toLower());
    }

    // Force asset ref rewrites for now. The other way is not well tested and bug prone.
    ui_.checkBoxRewriteAssetRefs->setChecked(true);
    ui_.checkBoxRewriteAssetRefs->hide();
    ui_.buttonStop->hide();

    connect(ui_.buttonStartProcessing, SIGNAL(clicked()), SLOT(Start()));
    connect(ui_.buttonStop, SIGNAL(clicked()), SLOT(RequestStop()));
    connect(ui_.buttonBrowseOutputDir, SIGNAL(clicked()), SLOT(OnBrowseOutputDir()));
    connect(ui_.checkBoxProcessTextures, SIGNAL(stateChanged(int)), SLOT(OnProcessTexturesChanged()));
    connect(ui_.comboBoxTextureMipmapGeneration, SIGNAL(currentIndexChanged(int)), SLOT(OnProcessMipmapGenerationChanged()));
    connect(ui_.pushButtonLogDock, SIGNAL(clicked()), SLOT(OnToggleLogDocking()));

    // Update other elements from ones value.
    OnProcessTexturesChanged();

    widget_->hide();
    
    Log("");
}

RocketScenePackager::~RocketScenePackager()
{
    widget_ = 0; // This is embedded to a parent widget that will handle deleting it
    SAFE_DELETE_LATER(logWindow_);
    
    state_.Reset();
}

void RocketScenePackager::Show()
{
    if (widget_ && !widget_->isVisible())
        widget_->show();
    
    if (!state_.storageAuthenticated)
        AuthenticateStorate();
}

void RocketScenePackager::AuthenticateStorate()
{
    MeshmoonStorageAuthenticationMonitor *auth = plugin_->Storage()->Authenticate();
    connect(auth, SIGNAL(Completed()), SLOT(StorageAuthCompleted()), Qt::UniqueConnection);
    connect(auth, SIGNAL(Canceled()), SLOT(StorageAuthCanceled()), Qt::UniqueConnection);
    connect(auth, SIGNAL(Failed(const QString&)), SLOT(StorageAuthCanceled()), Qt::UniqueConnection);
}

void RocketScenePackager::StorageAuthCompleted()
{
    state_.storageAuthenticated = true;
}

void RocketScenePackager::StorageAuthCanceled()
{
    state_.storageAuthenticated = false;
}

void RocketScenePackager::Hide()
{
    if (widget_ && widget_->isVisible())
        widget_->hide();
}

bool RocketScenePackager::IsRunning() const 
{
    return running_;
}

bool RocketScenePackager::IsStopping() const
{
    return stopping_;
}

bool RocketScenePackager::RequestStop()
{
    if (IsRunning() && !IsStopping())
    {
        // If we already uploading asset bundles the process cannot be stopped!
        if (state_.bundlesUploading)
        {
            plugin_->Notifications()->ShowSplashDialog("Optimization run is finalizing, it cannot be stopped at this point!", ":/images/server-anon.png");
            return false;
        }

        QMessageBox::StandardButton result = QMessageBox::question(framework_->Ui()->MainWindow(), "Stop confirmation", "Scene optimizer is still running. Are you sure you want to stop?", QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Cancel);
        if (result != QMessageBox::Yes)
            return false;

        stopping_ = true;

        // If we are done processing and waiting for the finalize command, we can stop right here.        
        if (state_.waitingForAssetBundleUploads && !state_.bundlesUploading)
            QTimer::singleShot(25, this, SLOT(CheckStopping()));

        return true;
    }
    return false;
}

bool RocketScenePackager::CheckStopping()
{
    if (stopping_)
    {
        // Restore already touched attributes to their original value.
        if (state_.changedAttributes.size() > 0) 
        {
            Log(" ");
            Log(QString("RESTORING %1 ALREADY OPTIMIZED REFS").arg(state_.changedAttributes.size()), true, false, false, "green");
        }
        for(int i=0; i<state_.changedAttributes.size(); ++i)
        {
            AttributeWeakPtr &container = state_.changedAttributes[i].first;
            QString originalValue = state_.changedAttributes[i].second;
            
            IAttribute *attribute = container.Get();
            if (attribute)
            {
                if (!originalValue.contains(";"))
                    TrimBaseStorageUrl(originalValue);
                Log(originalValue, true);
                attribute->FromString(originalValue, AttributeChange::Disconnected);
            }
            else
                Log("Failed to restore changed attribute to " + originalValue, true, true);
        }

        Log(" ");
        Log("Stopped by user request", true, true);
        Stop();
        return true;
    }
    return stopping_;
}

QWidget *RocketScenePackager::Widget() const 
{
    return widget_;
}

void RocketScenePackager::Log(const QString &message, bool logToConsole, bool errorChannel, bool warningChannel, const QString &color, bool logToFile)
{
    if (widget_ && ui_.log)
    {
        // Prepare potential file logging
        QTextStream logStream;
        if (logToFile)
        {
            if (!state_.logFile)
                state_.OpenLog();
            if (state_.logFile && state_.logFile->isOpen())
                logStream.setDevice(state_.logFile);
        }
        
        // UI logging
        QString uiLogLine;
        if (errorChannel || warningChannel)
        {
            if (!errorChannel && warningChannel)
            {
                uiLogLine = "<span style='color: blue;'>[WARNING]: " + message + "</span>";
                if (logStream.device()) logStream << "[WARNING]: " + message << endl;
            }
            else
            {
                uiLogLine = "<span style='color: red;'>[ERROR]: " + message + "</span>";
                if (logStream.device()) logStream << "[ERROR]: " + message << endl;
            }
        }
        else
        {
            if (color.isEmpty())
                uiLogLine = "<span style='color: black;'><pre>" + message + "</pre></span>";
            else
                uiLogLine = "<span style='color: " + color + ";'><pre>" + message + "</pre></span>";
            if (logStream.device()) logStream << message << endl;
        }

        if (!uiLogLine.isEmpty())
            ui_.log->appendHtml(uiLogLine);
    }
    
    // Console logging
    if (logToConsole)
    {
        if (!errorChannel && !warningChannel)
            LogInfo(LC_ + message);
        else if (!errorChannel && warningChannel)
            LogWarning(LC_ + message);
        else
            LogError(LC_ + message);
    }
}

void RocketScenePackager::LogProgress(const QString &action, int progress, bool updateButton)
{
    QString line = (progress >= 0 ? QString("%1% %2").arg((progress < 10 ? "  " : progress < 100 ? " " : "") + QString::number(progress)).arg(action) : action);

    if (ui_.logProgress)
        ui_.logProgress->setText(line);

    // Log window title
    if (logWindow_)
        logWindow_->setWindowTitle("Scene Optimizer Log" + (action.isEmpty() ? "" : " - " + line));
        
    // Main run button
    if (updateButton && ui_.buttonStartProcessing)
        ui_.buttonStartProcessing->setText((progress >= 0 ? QString("%1 %2%").arg(action).arg(progress) : action));
}

void RocketScenePackager::ReadConfig()
{
    if (!framework_ || !widget_)
        return;
        
    ConfigData config("rocket-scene-optimizer", "optimizer");
    if (framework_->Config()->HasValue(config, "outputdir"))
        ui_.outputDirectoryLineEdit->setText(QDir::toNativeSeparators(framework_->Config()->Read(config, "outputdir").toString()));
    if (framework_->Config()->HasValue(config, "bundleprefix"))
        ui_.bundlePrefixLineEdit->setText(framework_->Config()->Read(config, "bundleprefix").toString());
    if (framework_->Config()->HasValue(config, "bundlesplitsize"))
        ui_.spinBoxSplitSize->setValue(framework_->Config()->Read(config, "bundlesplitsize").toInt());
    if (framework_->Config()->HasValue(config, "bundlemeshes"))
        ui_.bundleMeshesCheckBox->setChecked(framework_->Config()->Read(config, "bundlemeshes").toBool());
    if (framework_->Config()->HasValue(config, "bundlematerials"))
        ui_.bundleMaterialsCheckBox->setChecked(framework_->Config()->Read(config, "bundlematerials").toBool());
    if (framework_->Config()->HasValue(config, "bundlegeneratedmaterials"))
        ui_.bundleEC_MaterialCheckBox->setChecked(framework_->Config()->Read(config, "bundlegeneratedmaterials").toBool());
    if (framework_->Config()->HasValue(config, "processtextures"))
        ui_.checkBoxProcessTextures->setChecked(framework_->Config()->Read(config, "processtextures").toBool());
    if (framework_->Config()->HasValue(config, "removeemptyents"))
        ui_.removeEmptyEntitiesCheckBox->setChecked(framework_->Config()->Read(config, "removeemptyents").toBool());
    if (framework_->Config()->HasValue(config, "logdebug"))
        ui_.checkBoxLogDebug->setChecked(framework_->Config()->Read(config, "logdebug").toBool());
    if (framework_->Config()->HasValue(config, "processscriptents"))
        ui_.checkBoxProcessScriptEnts->setChecked(framework_->Config()->Read(config, "processscriptents").toBool());
    if (framework_->Config()->HasValue(config, "processcontenttools"))
        ui_.chekBoxProcessContentToolsEnts->setChecked(framework_->Config()->Read(config, "processcontenttools").toBool());
    /*if (framework_->Config()->HasValue(config, "rewriteassetrefs"))
        ui_.checkBoxRewriteAssetRefs->setChecked(framework_->Config()->Read(config, "rewriteassetrefs").toBool());*/
    if (framework_->Config()->HasValue(config, "maxtexsizemode"))
        ui_.comboBoxMaxTexSize->setCurrentIndex(framework_->Config()->Read(config, "maxtexsizemode").toInt());
    if (framework_->Config()->HasValue(config, "rescaletexmode"))
        ui_.comboBoxRescaleMode->setCurrentIndex(framework_->Config()->Read(config, "rescaletexmode").toInt());
    if (framework_->Config()->HasValue(config, "textureformat"))
        ui_.comboBoxTextureFormat->setCurrentIndex(framework_->Config()->Read(config, "textureformat").toInt());
    if (framework_->Config()->HasValue(config, "texturequality"))
        ui_.spinBoxTextureQuality->setValue(framework_->Config()->Read(config, "texturequality").toInt());
    if (framework_->Config()->HasValue(config, "texturemipmapgeneration"))
        ui_.comboBoxTextureMipmapGeneration->setCurrentIndex(framework_->Config()->Read(config, "texturemipmapgeneration").toInt());
    if (framework_->Config()->HasValue(config, "texturemaxmipmaps"))
        ui_.spinBoxTextureMaxMipmaps->setValue(framework_->Config()->Read(config, "texturemaxmipmaps").toInt());
}

void RocketScenePackager::SaveConfig()
{
    if (!framework_ || !widget_)
        return;

    ConfigData config("rocket-scene-optimizer", "optimizer");
    framework_->Config()->Write(config, "outputdir", QDir::fromNativeSeparators(ui_.outputDirectoryLineEdit->text()));
    framework_->Config()->Write(config, "bundleprefix", ui_.bundlePrefixLineEdit->text());
    framework_->Config()->Write(config, "bundlesplitsize", ui_.spinBoxSplitSize->value());
    framework_->Config()->Write(config, "bundlemeshes", ui_.bundleMeshesCheckBox->isChecked());
    framework_->Config()->Write(config, "bundlematerials", ui_.bundleMaterialsCheckBox->isChecked());
    framework_->Config()->Write(config, "bundlegeneratedmaterials", ui_.bundleEC_MaterialCheckBox->isChecked());
    framework_->Config()->Write(config, "processtextures", ui_.checkBoxProcessTextures->isChecked());
    framework_->Config()->Write(config, "removeemptyents", ui_.removeEmptyEntitiesCheckBox->isChecked());
    framework_->Config()->Write(config, "logdebug", ui_.checkBoxLogDebug->isChecked());
    framework_->Config()->Write(config, "processscriptents", ui_.checkBoxProcessScriptEnts->isChecked());
    framework_->Config()->Write(config, "processcontenttools", ui_.chekBoxProcessContentToolsEnts->isChecked());
    //framework_->Config()->Write(config, "rewriteassetrefs", ui_.checkBoxRewriteAssetRefs->isChecked());
    framework_->Config()->Write(config, "maxtexsizemode", ui_.comboBoxMaxTexSize->currentIndex());
    framework_->Config()->Write(config, "rescaletexmode", ui_.comboBoxRescaleMode->currentIndex());
    framework_->Config()->Write(config, "textureformat", ui_.comboBoxTextureFormat->currentIndex());
    framework_->Config()->Write(config, "texturequality", ui_.spinBoxTextureQuality->value());
    framework_->Config()->Write(config, "texturemipmapgeneration", ui_.comboBoxTextureMipmapGeneration->currentIndex());
    framework_->Config()->Write(config, "texturemaxmipmaps", ui_.spinBoxTextureMaxMipmaps->value());
}

void RocketScenePackager::OnProcessTexturesChanged()
{
    if (!framework_ || !widget_ || !ui_.checkBoxProcessTextures)
        return;

    bool processing = ui_.checkBoxProcessTextures->isChecked();
    ui_.labelResizeTex->setEnabled(processing);
    ui_.comboBoxMaxTexSize->setEnabled(processing);
    ui_.labelRescaleTex->setEnabled(processing);
    ui_.comboBoxRescaleMode->setEnabled(processing);
    ui_.labelTexFormat->setEnabled(processing);
    ui_.comboBoxTextureFormat->setEnabled(processing);
    ui_.labelTexQuality->setEnabled(processing);
    ui_.spinBoxTextureQuality->setEnabled(processing);
    ui_.comboBoxTextureMipmapGeneration->setEnabled(processing);
    ui_.spinBoxTextureMaxMipmaps->setEnabled(processing);
    
    if (processing)
        OnProcessMipmapGenerationChanged();
}

void RocketScenePackager::OnProcessMipmapGenerationChanged()
{
    if (!framework_ || !widget_ || !ui_.checkBoxProcessTextures || !ui_.comboBoxTextureMipmapGeneration)
        return;

    int mipmapGenerationMode = ui_.comboBoxTextureMipmapGeneration->currentIndex();
    ui_.spinBoxTextureMaxMipmaps->setEnabled((ui_.checkBoxProcessTextures->isChecked() && mipmapGenerationMode == 0));
}

void RocketScenePackager::OnBrowseOutputDir()
{
    if (!ui_.outputDirectoryLineEdit->isEnabled())
        return;
    if (IsRunning())
        return;

    QString dir = QFileDialog::getExistingDirectory(framework_->Ui()->MainWindow(), "Select Optimizer Output Directory", ui_.outputDirectoryLineEdit->text());
    if (!dir.isEmpty())
        ui_.outputDirectoryLineEdit->setText(QDir::toNativeSeparators(dir));
}

void RocketScenePackager::OnToggleLogDocking()
{
    if (!ui_.log)
        return;

    if (!logWindow_)
    {
        QVBoxLayout *l = new QVBoxLayout();
        l->setSpacing(0);

        logWindow_ = new QWidget(0, Qt::Window|Qt::WindowStaysOnTopHint);
        logWindow_->setLayout(l);
        logWindow_->layout()->addWidget(ui_.log);
        logWindow_->layout()->addWidget(ui_.logProgress);
        logWindow_->setWindowIcon(framework_->Ui()->MainWindow()->windowIcon());
        logWindow_->setWindowTitle("Scene Optimizer Log");
        logWindow_->move(25,25);
        logWindow_->resize(framework_->Ui()->MainWindow()->width() * 0.75, framework_->Ui()->MainWindow()->height() * 0.75);
        logWindow_->show();
        
        logWindow_->installEventFilter(this);
        
        ui_.pushButtonLogDock->setText("Dock Log");
        ui_.verticalSpacer_4->changeSize(1,2000, QSizePolicy::Fixed, QSizePolicy::Expanding);
    }
    else
    {
        ui_.verticalLayoutLogging->addWidget(ui_.log);
        ui_.verticalLayoutLogging->addWidget(ui_.logProgress);
        ui_.pushButtonLogDock->setText("Undock Log");
        ui_.verticalSpacer_4->changeSize(1,10, QSizePolicy::Fixed, QSizePolicy::Fixed);
        SAFE_DELETE_LATER(logWindow_);
    }

    QTextCursor cursor = ui_.log->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui_.log->setTextCursor(cursor);
}

bool RocketScenePackager::eventFilter(QObject *obj, QEvent *e)
{
    if (logWindow_ && obj == logWindow_ && e->type() == QEvent::Close)
    {
        OnToggleLogDocking();
        return true;
    }
    return false;
}

void RocketScenePackager::Start()
{
    Log(" ", true);

    // Starting optimizations. Do txml backup and continue after it.
    if (!state_.waitingForAssetBundleUploads)
    {
        if (!state_.storageAuthenticated)
        {
            Log("Meshmoon Storage access is required for running this tool. Please authenticate to storage.", true, true);
            AuthenticateStorate();
            return;
        }
        
        if (!CreateOrClearWorkingDirectory())
            return;
        
        
        ui_.log->clear();
        Log(" ");
        LogProgress("Creating world backup");

        running_ = true;
        EnableInterface(false);
        
        emit Started();
        
        QS3CopyObjectResponse *backup = plugin_->Storage()->BackupTxml(true, "pre scene optimizations");
        if (!connect(backup, SIGNAL(finished(QS3CopyObjectResponse*)), SLOT(BackupCompleted(QS3CopyObjectResponse*))))
        {
            Log("Internal error while trying to backup current world state!", true, true);
            Stop();
            return;
        }
    }
    // User has given the command to start optimizations with current information. Upload asset bundles and replicate current state to server.
    else
    {
        StartAssetBundleUploads();
    }
}

void RocketScenePackager::BackupCompleted(QS3CopyObjectResponse *response)
{
    if (!response || !response->succeeded)
    {
        Stop();
        Log("Backup of current scene failed, cannot allow continue running the tool.", true, true);
        return;
    }
    
    Log("Backup of current scene complete to " + response->key, true);
    Log(" ", true);
    LogProgress("");

    Run();
}

void RocketScenePackager::Run()
{
    if (!IsRunning())
    {
        Log("Scene optimizers internal state is not valid. Aborting run...", true, true);
        Stop();
        return;
    }
    if (!state_.storageAuthenticated)
    {
        Log("Meshmoon Storage access is required for running this tool. Please authenticate to storage.", true, true);
        AuthenticateStorate();
        Stop();
        return;
    }
    
    Scene *activeScene = framework_->Renderer()->MainCameraScene();
    scene_ = SceneWeakPtr(activeScene != 0 ? framework_->Scene()->SceneByName(activeScene->Name()) : ScenePtr());
    if (!scene_.lock().get())
    {
        Log("Active scene is null, something is very wrong!", true, true);
        Stop();
        return;
    }

    state_.Reset();

    // Basic input data
    state_.txmlRef = plugin_->Storage()->CurrentState().storageTxmlPath.trimmed();
    state_.bundlePrefix = ui_.bundlePrefixLineEdit->text().trimmed();
    state_.txmlForcedBaseUrl = framework_->Asset()->DefaultAssetStorage()->BaseURL();
    state_.bundleSplitSize = (ui_.spinBoxSplitSize->value() * 1024 * 1024);
    
    // What to include in the bundle
    state_.processMeshes = ui_.bundleMeshesCheckBox->isChecked();
    state_.processMaterials = ui_.bundleMaterialsCheckBox->isChecked();
    state_.processGeneratedMaterials = ui_.bundleEC_MaterialCheckBox->isChecked();
    state_.processScriptEnts = ui_.checkBoxProcessScriptEnts->isChecked();
    state_.processContentToolsEnts = ui_.chekBoxProcessContentToolsEnts->isChecked();

    // Extra settings
    state_.removeEmptyEntities = ui_.removeEmptyEntitiesCheckBox->isChecked();
    state_.rewriteAssetReferences = ui_.checkBoxRewriteAssetRefs->isChecked();
    state_.logDebug = ui_.checkBoxLogDebug->isChecked();

    // Texture processing
    state_.texProcessing.process = ui_.checkBoxProcessTextures->isChecked();
    state_.texProcessing.convertToFormat = ui_.comboBoxTextureFormat->currentText().trimmed();
    state_.texProcessing.maxMipmaps = static_cast<uint>(ui_.spinBoxTextureMaxMipmaps->value());
    state_.texProcessing.rescaleMode = ui_.comboBoxRescaleMode->currentIndex();
    state_.texProcessing.quality = ui_.spinBoxTextureQuality->value();
    
    // Clamp certain values
    if (state_.texProcessing.maxMipmaps < 1) state_.texProcessing.maxMipmaps = 1;
    else if (state_.texProcessing.maxMipmaps > 16) state_.texProcessing.maxMipmaps = 16;
    if (state_.texProcessing.quality < 1) state_.texProcessing.quality = 0; /// @todo Make signed so this makes sense.
    else if (state_.texProcessing.quality > 255) state_.texProcessing.quality = 255;

    // Maximum texture size
    int maxSizeMode = ui_.comboBoxMaxTexSize->currentIndex();
    if (maxSizeMode == 0) state_.texProcessing.maxSize = 0;
    else if (maxSizeMode == 1) state_.texProcessing.maxSize = 4096;
    else if (maxSizeMode == 2) state_.texProcessing.maxSize = 2048;
    else if (maxSizeMode == 3) state_.texProcessing.maxSize = 1024;
    else if (maxSizeMode == 4) state_.texProcessing.maxSize = 512;
    else if (maxSizeMode == 5) state_.texProcessing.maxSize = 256;
    else if (maxSizeMode == 6) state_.texProcessing.maxSize = 128;
    else if (maxSizeMode == 7) state_.texProcessing.maxSize = 64;
    else if (maxSizeMode == 8) state_.texProcessing.maxSize = 32;
    else state_.texProcessing.maxSize = 0;

    // Mipmap generation
    int mipmapGenerationMode = ui_.comboBoxTextureMipmapGeneration->currentIndex();
    if (mipmapGenerationMode == 0)
        state_.texProcessing.mipmapGeneration = "Generate";
    else if (mipmapGenerationMode == 1)
        state_.texProcessing.mipmapGeneration = "UseSource";
    else if (mipmapGenerationMode == 2)
        state_.texProcessing.mipmapGeneration = "None";

    // Texture tool and its capabilities
    state_.textureTool = RocketFileSystem::InternalToolpath(RocketFileSystem::Crunch);
    state_.texProcessing.supportedInputFormats << "dds" << "png" << "jpg" << "jpeg" << "crn" << "bmp" << "tga";

    if (!state_.texProcessing.process)
        state_.texProcessing.Disable();
    if (state_.textureTool.isEmpty() || !QFile::exists(state_.textureTool))
    {
        if (state_.texProcessing.IsEnabled())
        {
            QMessageBox::StandardButton result = (QMessageBox::StandardButton)QMessageBox::warning(framework_->Ui()->MainWindow(), "Cannot find texture tool", 
                QString("Texture tool cannot be found from install directory.\n\nDo you want continue anyway with texture processing disabled?"),
                QMessageBox::Yes, QMessageBox::Abort);
            if (result != QMessageBox::Yes)
                return;
        }
        state_.texProcessing.Disable();
    }

    // Check for any errors in user input.
    bool errors = false;
    if (state_.txmlRef.isEmpty())
    {
        Log("Input txml is not defined.", true, true);
        errors = true;
    }
    else
    {
        if (!state_.txmlRef.contains(".txml"))
        {
            Log("Input txml is not a ref to a txml: " + state_.txmlRef, true, true);
            errors = true;
        }
    }
    if (!state_.bundlePrefix.isEmpty())
    {
        if (state_.bundlePrefix.endsWith(".zip"))
        {
            Log("Bundle prefix ends with .zip, cleaning it out!", true, false, true);
            state_.bundlePrefix = state_.bundlePrefix.left(state_.bundlePrefix.lastIndexOf(".zip"));
        }
        state_.bundlePrefix += "-";
    }
    QString outputDir = ui_.outputDirectoryLineEdit->text();
    if (outputDir.isEmpty())
    {
        Log("Output directory is not defined.", true, true);
        errors = true;
    }
    else
    {
        state_.outputDir = QDir(outputDir);
        if (!state_.outputDir.exists())
        {
            state_.outputDir.mkpath(outputDir);
            if (!state_.outputDir.exists())
            {
                Log("Output directory does not exist on disk. Please create it: " + outputDir, true, true);
                errors = true;
            }
        }
    }
    if (ui_.spinBoxSplitSize->value() <= 0)
    {
        Log("Texture split size cannot be 0 or smaller.", true, true);
        errors = true;
    }
    if (state_.txmlForcedBaseUrl.isEmpty())
    {
        Log("Forced base url could not be detected from scenes default storage.", true, true);
        errors = true;
    }
    else if (!state_.txmlForcedBaseUrl.endsWith("/"))
        state_.txmlForcedBaseUrl += "/";
    
    // Final error check, if ok clean the working dir if needed.   
    if (errors || !ClearDirectoryWithConfirmation(state_.outputDir))
    {
        Stop();
        return;
    }
    
    SaveConfig();

    Log("");
    LogProgress("Waiting for assets");

    state_.time.restart();
    
    // Remove entities that do not have any components
    if (state_.removeEmptyEntities)
    {
        Scene::EntityMap ents = scene_.lock()->Entities();
        for(Scene::EntityMap::iterator iter = ents.begin(); iter != ents.end(); ++iter)
        {
            EntityPtr ent = iter->second;
            if (!ent.get())
                continue;
            if (ent->Components().size() == 0)
            {
                entity_id_t entId = ent->Id();
                ent.reset();
                scene_.lock()->RemoveEntity(entId);
                state_.entitiesEmptyRemoved++;
            }
        }
        ents.clear();
    }

    // Start polling asset transfers
    QTimer::singleShot(10, this, SLOT(TryStartProcessing()));
}

void RocketScenePackager::Stop()
{
    scene_.reset();
    state_.Reset();

    EnableInterface(true);

    running_ = false;
    stopping_ = false;

    LogProgress("");

    emit Stopped();
}

void RocketScenePackager::EnableInterface(bool enabled)
{
    // Basic input data
    ui_.outputDirectoryLineEdit->setEnabled(enabled);
    ui_.bundlePrefixLineEdit->setEnabled(enabled);
    ui_.spinBoxSplitSize->setEnabled(enabled);

    // What to include in the bundle
    ui_.bundleMeshesCheckBox->setEnabled(enabled);
    ui_.bundleMaterialsCheckBox->setEnabled(enabled);
    ui_.bundleEC_MaterialCheckBox->setEnabled(enabled);
    ui_.checkBoxProcessScriptEnts->setEnabled(enabled);
    ui_.chekBoxProcessContentToolsEnts->setEnabled(enabled);

    // Extra settings
    ui_.removeEmptyEntitiesCheckBox->setEnabled(enabled);
    ui_.checkBoxRewriteAssetRefs->setEnabled(enabled);
    ui_.checkBoxLogDebug->setEnabled(enabled);

    // Texture processing
    ui_.checkBoxProcessTextures->setEnabled(enabled);
    ui_.comboBoxTextureFormat->setEnabled(enabled);
    ui_.comboBoxRescaleMode->setEnabled(enabled);
    ui_.spinBoxTextureQuality->setEnabled(enabled);
    ui_.comboBoxMaxTexSize->setEnabled(enabled);
    ui_.comboBoxTextureMipmapGeneration->setEnabled(enabled);
    ui_.spinBoxTextureMaxMipmaps->setEnabled(enabled);

    // Buttons
    ui_.buttonStartProcessing->setEnabled(enabled);
    ui_.buttonStop->setVisible(!enabled);

    if (enabled)
    {
        ui_.buttonStartProcessing->setText("Run");
        OnProcessTexturesChanged();
        
        if (logWindow_)
            logWindow_->setWindowTitle("Scene Optimizer Log");
    }
}

bool RocketScenePackager::CreateOrClearWorkingDirectory()
{
    if (!widget_ || !ui_.outputDirectoryLineEdit)
        return false;

    QString outputDir = ui_.outputDirectoryLineEdit->text();
    QDir dir;
    
    if (outputDir.isEmpty())
    {
        Log("Output directory is not defined.", true, true);
        return false;
    }
    else
    {
        dir = QDir(outputDir);
        if (!dir.exists())
        {
            dir.mkpath(outputDir);
            if (!dir.exists())
            {
                Log("Output directory does not exist on disk. Create it: " + outputDir, true, true);
                return false;
            }
        }
    }
    
    return ClearDirectoryWithConfirmation(dir);
}
    
bool RocketScenePackager::ClearDirectoryWithConfirmation(const QDir &dir)
{
    if (!dir.exists())
        return false;
    
    QFileInfoList files = dir.entryInfoList(QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot);
    if (!files.isEmpty())
    {
        QMessageBox::StandardButton result = (QMessageBox::StandardButton)QMessageBox::warning(framework_->Ui()->MainWindow(), "Cleaning working directory", 
            "All existing files will be deleted from working directory:<br><b>" + 
            QDir::toNativeSeparators(dir.absolutePath()) + 
            "</b><br><br>Are you sure you want to continue?",
            QMessageBox::Yes, QMessageBox::Abort);
        
        if (result != QMessageBox::Yes)
            return false;
        
        QString outputDirPath = dir.absolutePath();
        RemoveAllFilesRecursive(dir);
        dir.mkpath(outputDirPath);
    }
    return true;
}

void RocketScenePackager::RemoveAllFilesRecursive(const QDir &dir, bool onlyTxmlFromRoot)
{
    if (!dir.exists())
        return;

    QFileInfoList items = dir.entryInfoList(QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot);
    foreach(QFileInfo item, items)
    {
        if (item.isFile())
        {
            // From the root destination dir, only remove txml and zip files to not destroy existing data.
            if (onlyTxmlFromRoot && !item.fileName().endsWith(".txml") && !item.fileName().endsWith(".zip") && !item.fileName().endsWith(".txt"))
                continue;
            QFile::remove(item.absoluteFilePath());
            if (item.fileName().endsWith(".txml") || item.fileName().endsWith(".zip") || item.fileName().endsWith(".txt"))
                Log("Removed file   : " + item.absoluteFilePath());
        }
        else if (item.isDir())
        {
            QString dirName = item.fileName();
            QDir rmdir(item.absoluteFilePath());
            RemoveAllFilesRecursive(rmdir, false);

            if (item.absoluteFilePath() != state_.outputDir.absolutePath())
            {
                rmdir.cdUp();
                rmdir.rmpath(dirName);
                Log("Removed folder : " + item.absoluteFilePath());
            }
        }
    }
}

void RocketScenePackager::TryStartProcessing()
{
    if (scene_.expired())
    {
        Log("The scene being processed is no longer valid. Aborting...", true, true);
        Stop();
        return;
    }

    if (framework_->Asset()->NumCurrentTransfers() > 0)
    {
        bool shouldWait = false;

        // If we are only processing meshes, we can continue once all meshes are loaded etc.
        AssetTransferMap transfers = framework_->Asset()->GetCurrentTransfers();
        for(AssetTransferMap::const_iterator iter = transfers.begin(); iter != transfers.end(); ++iter)
        {
            if (!iter->second.get())
                continue;
            if (state_.processMeshes && iter->second->assetType == "OgreMesh")
                shouldWait = true;
            if ((state_.processMaterials || state_.processGeneratedMaterials) && (iter->second->assetType == "OgreMaterial" || iter->second->assetType == "Texture"))
                shouldWait = true;
            if (shouldWait)
                break;
        }

        if (shouldWait)
        {
            ui_.buttonStartProcessing->setText("Loading assets...");
            Log("Waiting for " + QString::number(framework_->Asset()->NumCurrentTransfers()) + " assets to finish loading...", true);
            if (framework_->Asset()->NumCurrentTransfers() < 100)
                QTimer::singleShot(1000, this, SLOT(TryStartProcessing()));
            else if (framework_->Asset()->NumCurrentTransfers() < 300)
                QTimer::singleShot(3000, this, SLOT(TryStartProcessing()));
            else
                QTimer::singleShot(5000, this, SLOT(TryStartProcessing()));
            return;
        }
    }
    
    Process();
}
    
void RocketScenePackager::Process()
{
    if (scene_.expired())
    {
        Log("The scene being processed is no longer valid. Aborting...", true, true);
        Stop();
        return;
    }

    Log("All assets ready. Processing scene...", true);
    Log(" ", true);
    
    QString processStepName;
    int processStep = 1;
    
    // EC_Material
    if (state_.processGeneratedMaterials)
    {
        std::vector<EC_Material*> materials = GetTargets(scene_.lock()->Components<EC_Material>());
        if (!materials.empty())
        {
            if (materials.size() > state_.entitiesProcessed)
                state_.entitiesProcessed += (uint)materials.size();
            
            processStep = 1;
            processStepName = "Processing Generated Materials";
            Log(processStepName, true);
            if (state_.logDebug) Log("==============================", true);
            for (std::vector<EC_Material*>::const_iterator iter = materials.begin(); iter != materials.end(); ++iter, ++processStep)
            {
                LogProgress(processStepName, static_cast<int>((processStep*100)/materials.size()), true);
                ProcessGeneratedMaterial((*iter)->ParentEntity(), (*iter));
                
                QApplication::processEvents();
                framework_->ProcessOneFrame();

                if (CheckStopping())
                    return;
            }
            if (state_.logDebug) Log(" ", true);
        }
    }

    // EC_Mesh
    if (state_.processMeshes || state_.processMaterials)
    {
        std::vector<EC_Mesh*> meshes = GetTargets(scene_.lock()->Components<EC_Mesh>());
        if (!meshes.empty())
        {
            if (meshes.size() > state_.entitiesProcessed)
                state_.entitiesProcessed = (uint)meshes.size();
            
            // meshRef
            if (state_.processMeshes)
            {
                processStep = 1;
                processStepName = "Processing Meshes";
                Log(processStepName, true);
                if (state_.logDebug) Log("=================", true);
                for (std::vector<EC_Mesh*>::const_iterator iter = meshes.begin(); iter != meshes.end(); ++iter, ++processStep)
                {
                    LogProgress(processStepName, static_cast<int>((processStep*100)/meshes.size()), true);
                    ProcessMeshRef((*iter)->ParentEntity(), (*iter));
                    
                    QApplication::processEvents();
                    framework_->ProcessOneFrame();
                    
                    if (CheckStopping())
                        return;
                }
                if (state_.logDebug) Log(" ", true);
            }
            
            // meshMaterial
            if (state_.processMaterials)
            {
                processStep = 1;
                processStepName = "Processing Materials";
                Log(processStepName, true);
                if (state_.logDebug) Log("====================", true);
                for (std::vector<EC_Mesh*>::const_iterator iter = meshes.begin(); iter != meshes.end(); ++iter, ++processStep)
                {
                    LogProgress(processStepName, static_cast<int>((processStep*100)/meshes.size()), true);
                    ProcessMaterialRefs((*iter)->ParentEntity(), (*iter));
                    
                    QApplication::processEvents();
                    framework_->ProcessOneFrame();
                    
                    if (CheckStopping())
                        return;
                }
                if (state_.logDebug) Log(" ", true);
            }
        }
    }

    Log(" ", true);
    if (CheckStopping())
        return;
    if (!IsRunning())
        return;
    if (scene_.expired())
        return;

    // Finalize
    if (state_.totalConvertedRefs > 0)
    {
        // Save txml
        QString outputTxmlFile = state_.txmlRef;
        if (outputTxmlFile.contains("\\"))
            outputTxmlFile = QDir::fromNativeSeparators(outputTxmlFile);
        if (outputTxmlFile.contains("/"))
            outputTxmlFile = outputTxmlFile.mid(outputTxmlFile.lastIndexOf("/")+1);
        if (outputTxmlFile.isEmpty())
            outputTxmlFile = "bundled-output-scene";
        if (!outputTxmlFile.endsWith(".txml", Qt::CaseInsensitive))
            outputTxmlFile += ".txml";
        state_.outputTxmlFile = state_.outputDir.absoluteFilePath(outputTxmlFile);
        if (!scene_.lock()->SaveSceneXML(state_.outputTxmlFile, false, false))
            Log("Failed to save txml to " + outputTxmlFile + " disk for inspection.", true, true);

        // If we have used CRN textures do Fastest compression, it will result 
        // in same size as Normal but will be faster to uncompress.
        RocketZipWorker::Compression textureCompression = RocketZipWorker::Normal;
        if (state_.texProcessing.process && state_.texProcessing.convertToFormat.toUpper() == "CRN")
            textureCompression = RocketZipWorker::Fastest;

        // Create zip packages and wait for their completion.
        if (state_.bundledMeshRefs > 0)
        {
            for (uint i=1; i<=state_.currentMeshBundleIndex; ++i)
            {
                QString folder = "models-" + QString::number(i);
                StartPackager(state_.outputDir.absoluteFilePath(state_.bundlePrefix + folder + ".zip"), state_.outputDir.absoluteFilePath(folder));
            }
        }
        if (state_.bundledMaterialRefs > 0)
        {
            StartPackager(state_.outputDir.absoluteFilePath(state_.bundlePrefix + "materials.zip"), state_.outputDir.absoluteFilePath("materials"));
        }
        if (state_.bundledTextureRefs > 0)
        {
            for(uint i=1; i<=state_.currentTextureBundleIndex; ++i)
            {
                QString folder = "textures-" + QString::number(i);
                StartPackager(state_.outputDir.absoluteFilePath(state_.bundlePrefix + folder +".zip"), state_.outputDir.absoluteFilePath(folder), textureCompression);
            }
        }
        if (state_.bundledTextureGeneratedRefs > 0)
        {
            for(uint i=1; i<=state_.currentTextureGeneratedBundleIndex; ++i)
            {
                QString folder = "textures-generated-" + QString::number(i);
                StartPackager(state_.outputDir.absoluteFilePath(state_.bundlePrefix + folder +".zip"), state_.outputDir.absoluteFilePath(folder), textureCompression);
            }
        }

        ui_.buttonStartProcessing->setText("Creating zip files...");
        LogProgress("Creating zip bundles", static_cast<int>(100.0f-(((float)PendingPackagers()/(float)state_.packagers.size())*100.0f)), true);
    }
    else
    {
        Log("Scene was processed without finding anything to convert to your asset bundle. Finished because there is nothing to do!", true, false, true);
        Stop();
    }
}

template <typename T>
std::vector<T*> RocketScenePackager::GetTargets(std::vector<shared_ptr<T> > candidates) const
{
    std::vector<T*> targets;
    for (typename std::vector<shared_ptr<T> >::const_iterator iter = candidates.begin(); iter != candidates.end(); ++iter)
    {
        // Component is null, temporary or local
        T *target = (*iter).get();
        if (!target || target->IsTemporary() || target->IsLocal())
            continue;
        // Parent entity is null, temporary or local
        Entity *parent = target->ParentEntity();
        if (!parent || parent->IsTemporary() || parent->IsLocal())
            continue;
        // We are not processing script entities and this one has a EC_Script
        if (!state_.processScriptEnts && parent->GetComponent("EC_Script").get())
            continue;
        // We are not processing content tools entities and this one has a spesific EC_DynamicComponent
        if (!state_.processContentToolsEnts && parent->GetComponent("EC_DynamicComponent", contentToolsDataCompName_).get())
            continue;

        targets.push_back(target);
    }
    return targets;
}

RocketZipWorker *RocketScenePackager::StartPackager(const QString &destinationFilePath, const QString &inputDirPath, RocketZipWorker::Compression compression)
{
    RocketZipWorker *packager = new RocketZipWorker(destinationFilePath, inputDirPath, compression);
    connect(packager, SIGNAL(AsynchPackageCompleted(bool)), SLOT(OnPackagerThreadCompleted()), Qt::QueuedConnection);

    // Start worker if there is room in the "pool"
    if (RunningPackagers() < state_.maxPackagerThreads)
    {
        Log(QString("Compressing %1 please wait...").arg(packager->DestinationFileName()), true);
        packager->start(QThread::HighPriority);
    }
    state_.packagers << packager;
    return packager;
}

uint RocketScenePackager::RunningPackagers() const
{
    uint running = 0;
    foreach(RocketZipWorker *packager, state_.packagers)
    {
        if (!packager)
            continue;
        if (packager->isRunning())
        {
            if (packager->Completed())
            {
                packager->exit();
                packager->wait();
            }
            else
                running++;
        }
    }
    return running;
}

uint RocketScenePackager::PendingPackagers() const
{
    uint pending = 0;
    foreach(RocketZipWorker *packager, state_.packagers)
        if (packager && !packager->isRunning() && !packager->Completed())
            pending++;
    return pending;
}

void RocketScenePackager::OnPackagerThreadCompleted()
{
    if (CheckStopping())
        return;

    // Check worker state, start new threads if we can.
    if (PendingPackagers() > 0)
    {
        LogProgress("Creating zip bundles", static_cast<int>(100.0f-(((float)PendingPackagers()/(float)state_.packagers.size())*100.0f)), true);
        foreach(RocketZipWorker *packager, state_.packagers)
        {
            if (!packager->isRunning() && !packager->Completed())
            {
                if (RunningPackagers() < state_.maxPackagerThreads)
                {
                    Log(QString("Compressing %1 please wait...").arg(packager->DestinationFileName()), true);
                    packager->start(QThread::HighPriority);
                }
                else
                    break;
            }
        }
        return;
    }

    LogProgress("Waiting for user confirmation to optimize...");
    
    QString totalCopied = QString::number((double)((state_.totalFileSize/1024.0)/1024.0), 'f', 2) + " mb";
    QString meshCopyMb = QString::number((double)((state_.totalFileSizeMesh/1024.0)/1024.0), 'f', 2) + " mb";
    QString materialCopyMb = QString::number((double)((state_.totalFileSizeMat/1024.0)/1024.0), 'f', 2) + " mb";
    QString textureCopyMb = QString::number((double)((state_.totalFileSizeTex/1024.0)/1024.0), 'f', 2) + " mb";
    QString textureGenCopyMb = QString::number((double)((state_.totalFileSizeTexGen/1024.0)/1024.0), 'f', 2) + " mb";

    int elapsedSec = state_.time.elapsed() / 1000;
    int min = elapsedSec / 60; int sec = elapsedSec % 60;

    // Final report
    Log(" ", true);
    Log("PATHS", true, false, false, "green");
    Log("-- Working dir  : " + state_.outputDir.absolutePath(), true);
    Log("-- Output scene : " + state_.outputTxmlFile, true);
    Log("-- Base URL     : " + state_.txmlForcedBaseUrl, true);

    Log(" ", true);
    Log("SCENE", true, false, false, "green");
    Log("-- Processed entities          : " + QString::number(state_.entitiesProcessed), true);
    if (state_.entitiesEmptyRemoved > 0)
        Log("-- Removed empty entities      : " + QString::number(state_.entitiesEmptyRemoved) + " (no components)", true);
    
    Log(" ", true);
    Log("BUNDLING", true, false, false, "green");
    Log("-- Bundle output spec          : " + state_.bundlePrefix + "&lt;type&gt;.zip#&lt;subasset&gt;", true);
    Log("-- Bundle split size           : " + QString::number((double)((state_.bundleSplitSize/1024.0)/1024.0), 'f', 2) + " mb", true);
    Log("-- Total refs converted        : " + QString::number(state_.totalConvertedRefs), true);
    
    Log(" ", true);
    Log("TEXTURES", true, false, false, "green");
    Log("-- Largest encountered width   : " + QString("%1").arg(state_.foundMaxTexWidth, 4) + " pixels", true);
    Log("-- Largest encountered height  : " + QString("%1").arg(state_.foundMaxTexHeight, 4) + " pixels", true);
    if (state_.texProcessing.process)
    {
        Log("-- Textures processed          : " + QString::number(state_.texProcessing.totalProcessed), true);
        if (!state_.texProcessing.convertToFormat.isEmpty())
            Log(QString("-- Converted to %1            : ").arg(state_.texProcessing.convertToFormat) + QString::number(state_.texProcessing.totalConverted), true);
        if (state_.texProcessing.maxSize > 0)
            Log("-- Textures resized            : " + QString::number(state_.texProcessing.totalResized) + " to " + QString("%1x%2").arg(state_.texProcessing.maxSize).arg(state_.texProcessing.maxSize), true);
        else
            Log("-- Texture resizing            : Disabled", true);
        if (state_.texProcessing.rescaleMode > 0)
            Log("-- Textures rescaled           : " + QString::number(state_.texProcessing.totalRescaled) + " to " + ui_.comboBoxRescaleMode->currentText(), true);
        else
            Log("-- Texture rescaling           : Disabled", true);
        Log(QString("-- Mipmap generation           : %1").arg(ui_.comboBoxTextureMipmapGeneration->currentText()) , true);
        if (state_.texProcessing.mipmapGeneration == "Generate")
            Log(QString("-- Max mipmap levels           : %1").arg(state_.texProcessing.maxMipmaps) , true);
    }
    else
        Log("-- Texture processing disabled", true);

    Log(" ", true);
    Log("OPERATIONS", true, false, false, "green");
    Log("-- Meshes copied               : " + QString("%1 %2").arg(state_.bundledMeshRefs, 4).arg(meshCopyMb, 13), true);
    Log("-- Materials copied            : " + QString("%1 %2").arg(state_.bundledMaterialRefs, 4).arg(materialCopyMb, 13), true);
    Log("-- Textures copied             : " + QString("%1 %2").arg(state_.bundledTextureRefs, 4).arg(textureCopyMb, 13), true);
    Log("-- EC_Material textures copied : " + QString("%1 %2").arg(state_.bundledTextureGeneratedRefs, 4).arg(textureGenCopyMb, 13), true);
    Log("-- Total copied file size      : " + QString("%1 %2").arg("", 4).arg(totalCopied, 13), true);
    Log("-- Total time spent            : " + QString("%1 minutes %2 seconds").arg(min).arg(sec));
    
    // Report created zip files
    Log(" ", true);
    Log("CREATED ZIP BUNDLES", true, false, false, "green");

    bool allZipFilesCreated = true;
    qint64 totalZipFilesize = 0;
    foreach(RocketZipWorker *packager, state_.packagers)
    {
        if (packager->isRunning())
        {
            packager->exit();
            packager->wait();
        }
        if (!packager->Succeeded())
        {
            Log("Failed to create " + QDir::toNativeSeparators(packager->DestinationFilePath()), true, true);
            allZipFilesCreated = false;
        }
        else
        {
            Log("-- " + QDir::toNativeSeparators(packager->DestinationFilePath()), true);
            totalZipFilesize += QFileInfo(packager->DestinationFilePath()).size();
        }
    }
    Log("-- Total compressed file size " + QString::number((double)((totalZipFilesize/1024.0)/1024.0), 'f', 2) + " mb", true);

    // Log files
    if (QFile::exists(state_.outputDir.absoluteFilePath("packager_texture-processing-log.txt")) || QFile::exists(state_.outputDir.absoluteFilePath("packager_main-log.txt")))
    {
        Log(" ", true);
        Log("LOG FILES", true, false, false, "green");
        if (QFile::exists(state_.outputDir.absoluteFilePath("packager_texture-processing-log.txt")))
            Log("-- Full texture processing log : " + QDir::toNativeSeparators(state_.outputDir.absoluteFilePath("packager_texture-processing-log.txt")), true);
        if (QFile::exists(state_.outputDir.absoluteFilePath("packager_main-log.txt")))
            Log("-- Full main processing log    : " + QDir::toNativeSeparators(state_.outputDir.absoluteFilePath("packager_main-log.txt")), true);
    }
    
    if (!allZipFilesCreated)
    {
        Log("All zip files were not created successfully, aborting optimization run!", true, true);
        Stop();
        return;
    }

    QStringList zipFileNames;
    foreach(RocketZipWorker *packager, state_.packagers)
        zipFileNames << packager->DestinationFileName();

    Log(" ", true);      
    QString finalizeNote = QString("<b>Everything is ready for uploading asset bundles and applying the scene optimizations</b><br><br>") +
        "Please read the log for any errors/warning, if everything seems to be in order click the <b>Optimize Scene</b> button to finish the process.<br><br>" +
        "<b>WARNING:</b> Uploading the zip bundles to storage is a permanent change. Existing zip files with the same name will be overwritten and will not be automatically backed up. "
        "Backup any existing zip bundles now if you think it's necessary. You might need them for example if you want to restore a previous backup that was optimized with the currently existing storage zip bundles."
        "<br><br>The following zip bundles are going to be overwritten<pre>  " + zipFileNames.join("<br>  ") + "</pre>";
    ui_.log->appendHtml(finalizeNote);
    
    plugin_->Notifications()->ShowSplashDialog(finalizeNote, ":/images/server-anon.png");

    state_.waitingForAssetBundleUploads = true;
    ui_.buttonStartProcessing->setText("Optimize Scene");
    ui_.buttonStartProcessing->setEnabled(true);
    
    // Open the storage to indicate to the user he can backup what he wishes at this stage.
    plugin_->Storage()->Open();
}

void RocketScenePackager::StartAssetBundleUploads()
{
    if (state_.bundlesUploading)
        return;
    if (CheckStopping())
        return;
    if (!IsRunning())
        return;

    // Upload zip bundles
    Log(" ", true);
    Log("UPLOADING ASSET BUNDLES", true, false, false, "green");
    LogProgress("Uploading asset bundles...", -1, true);
    ui_.buttonStartProcessing->setEnabled(false);  

    QStringList zipFiles;
    QStringList zipFileNames;
    foreach(RocketZipWorker *packager, state_.packagers)
    {
        zipFileNames << packager->DestinationFileName();
        zipFiles << packager->DestinationFilePath();
    }

    state_.bundlesUploading = true;
    state_.bundleStorageUploadsOk = true;

    // Open storage and start uploads
    plugin_->Storage()->Open();

    MeshmoonStorageOperationMonitor *monitor = plugin_->Storage()->UploadFiles(zipFiles, false, plugin_->Storage()->RootDirectory());
    connect(monitor, SIGNAL(Progress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)), SLOT(OnAssetBundleUploadOperationProgress(QS3PutObjectResponse*, qint64, qint64, MeshmoonStorageOperationMonitor*)));
    connect(monitor, SIGNAL(Finished(MeshmoonStorageOperationMonitor*)), SLOT(OnAssetBundlesUploaded(MeshmoonStorageOperationMonitor*)));
}

void RocketScenePackager::OnAssetBundleUploadOperationProgress(QS3PutObjectResponse *response, qint64 completed, qint64 total, MeshmoonStorageOperationMonitor * /*operation*/)
{
    if (CheckStopping())
        return;
    if (!IsRunning())
        return;

    if (total <= 0)
    {
        completed = 0;
        total = 1;
    }
    int progress = int(((qreal)completed / (qreal)total) * 100.0);
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;

    if (response)
    {
        if (response->succeeded)     
            Log(QString("[100%] Upload completed for %1").arg(response->key), true);                
        else
        {
            state_.bundleStorageUploadsOk = false;
            Log(QString("[???%] %1 upload failed: %2").arg(response->key).arg(response->error.toString()), true, true);
        }
    }
    else
        LogProgress("Uploading asset bundles...", progress, true);
}

void RocketScenePackager::OnAssetBundlesUploaded(MeshmoonStorageOperationMonitor * /*monitor*/)
{
    if (CheckStopping())
        return;
    if (!IsRunning())
        return;

    if (state_.bundleStorageUploadsOk)
        ReplicateChangesToServer();
    else
    {
        Log("All bundles were not uploaded successfully, cannot continue with scene optimizations!", true, true);
        Stop();
    }
}

bool QStringCaseInsensitiveLessThan(const QString &s1, const QString &s2)
{
    return (s1.toLower() < s2.toLower());
}

void RocketScenePackager::TrimBaseStorageUrl(QString &ref)
{
    if (ref.startsWith(state_.txmlForcedBaseUrl))
        ref = ref.right(ref.length()-state_.txmlForcedBaseUrl.length());
}

void RocketScenePackager::ReplicateChangesToServer()
{
    if (CheckStopping())
        return;
    if (!IsRunning())
        return;

    Log(" ", false);
    LogProgress("Replicating optimizations to server...", -1, true);

    if (!state_.optimizedAssetRefs.isEmpty())
    {
        Log(QString("Forgetting %1 old assets that are going to be optimized").arg(state_.optimizedAssetRefs.size()), false, false, false, "green");
        Log(" ", false);

        QStringList optimizedMeshRefs, optimizedMaterialRefs, optimizedOtherRefs, assetBundleRefs;
        foreach(const QString &ref, state_.optimizedAssetRefs)
        {
            AssetPtr optimizedAsset = framework_->Asset()->GetAsset(ref);
            if (!optimizedAsset.get())
                continue;

            QString assetRef = optimizedAsset->Name();
            QString subAssetPart, mainAssetPart;
            AssetAPI::ParseAssetRef(assetRef, 0, 0, 0, 0, 0, 0, 0, &subAssetPart, 0, &mainAssetPart);

            if (!subAssetPart.isEmpty() && !mainAssetPart.isEmpty())
            {
                if (!assetBundleRefs.contains(mainAssetPart))
                    assetBundleRefs << mainAssetPart;
            }
            if (optimizedAsset->Type() == "OgreMesh")
            {
                if (!optimizedMeshRefs.contains(assetRef))
                    optimizedMeshRefs << assetRef;
            }
            else if (optimizedAsset->Type() == "OgreMaterial")
            {
                if (!optimizedMaterialRefs.contains(assetRef))
                    optimizedMaterialRefs << assetRef;
            }
            else
            {
                if (!optimizedOtherRefs.contains(assetRef))
                    optimizedOtherRefs << assetRef;
            }
        }
        qSort(assetBundleRefs.begin(), assetBundleRefs.end(), QStringCaseInsensitiveLessThan);
        qSort(optimizedMeshRefs.begin(), optimizedMeshRefs.end(), QStringCaseInsensitiveLessThan);
        qSort(optimizedMaterialRefs.begin(), optimizedMaterialRefs.end(), QStringCaseInsensitiveLessThan);
        qSort(optimizedOtherRefs.begin(), optimizedOtherRefs.end(), QStringCaseInsensitiveLessThan);

        if (!assetBundleRefs.isEmpty()) Log("ASSET BUNDLES", false, false, false, "green");
        foreach(QString ref, assetBundleRefs)
        {
            framework_->Asset()->ForgetBundle(ref, false);
            TrimBaseStorageUrl(ref);
            Log(QString("  %1").arg(ref));
        }
        if (!optimizedMeshRefs.isEmpty()) Log("MESHES", false, false, false, "green");
        foreach(QString ref, optimizedMeshRefs)
        {
            framework_->Asset()->ForgetAsset(ref, false);
            TrimBaseStorageUrl(ref);
            Log(QString("  %1").arg(ref));
        }
        if (!optimizedMeshRefs.isEmpty()) Log("MATERIALS", false, false, false, "green");
        foreach(QString ref, optimizedMaterialRefs)
        {
            framework_->Asset()->ForgetAsset(ref, false);
            TrimBaseStorageUrl(ref);
            Log(QString("  %1").arg(ref));
        }
        if (!optimizedMeshRefs.isEmpty()) Log("OTHER", false, false, false, "green");
        foreach(QString ref, optimizedOtherRefs)
        {
            framework_->Asset()->ForgetAsset(ref, false);
            TrimBaseStorageUrl(ref);
            Log(QString("  %1").arg(ref));
        }

        Log(" ", false);
    }

    Log(QString("Replicating %1 scene optimization changes to the server, please wait...").arg(state_.changedAttributes.size()), false, false, false, "green");
    Log(" ", false);

    // This will take a while and print lots of things to the log. Process main loop now to get the above title to show.
    QApplication::processEvents();
    framework_->ProcessOneFrame();
    
    for(int i=0; i<state_.changedAttributes.size(); ++i)
    {
        AttributeWeakPtr &container = state_.changedAttributes[i].first;
        IAttribute *attribute = container.Get();

        if (!attribute || !attribute->Owner())
        {
            Log("  An optimized attribute does not exist anymore, the parent entity/component has probably been removed while processing!", true, false, true);
            continue;
        }

        const QString &compName = attribute->Owner()->TypeName();
        const QString &attributeTypeName = attribute->TypeName();
        const QString &attributeName = attribute->Name();
        const QStringList attributeValues = attribute->ToString().split(";", QString::SkipEmptyParts);

        QString fillerCompName, fillerAttributeTypeName, fillerName;
        while((fillerCompName.length() + compName.length()) < 15)
            fillerCompName.append(" ");
        while((fillerAttributeTypeName.length() + attributeTypeName.length()) < 21)
            fillerAttributeTypeName.append(" ");
        while((fillerName.length() + attributeName.length()) < 16)
            fillerName.append(" ");

        Log(QString("  <span style='color:blue;'>%1</span>%2%3%4%5%6<span style='color:green;'>%7</span>")
            .arg(compName).arg(fillerCompName)
            .arg(attributeTypeName).arg(fillerAttributeTypeName)
            .arg(attributeName).arg(fillerName)
            .arg(attributeValues.first()), false, false, false, "", false);

        for(int iVal=1; iVal<attributeValues.size(); ++iVal)
            Log(QString("                                                      <span style='color:green;'>%1</span>").arg(attributeValues[iVal]), false, false, false, "", false);

        attribute->Owner()->EmitAttributeChanged(attribute, AttributeChange::Replicate);
    }

    Log(" ", false);
    Log(QString("Unloaded %1 old assets that were optimized").arg(state_.optimizedAssetRefs.size()), false, false, false, "green");
    Log(QString("Replicated %1 scene optimization changes to the server").arg(state_.changedAttributes.size()), false, false, false, "green");
    
    int elapsedSec = state_.time.elapsed() / 1000;
    int min = elapsedSec / 60; int sec = elapsedSec % 60;
    
    Log(" ", false);
    Log("Scene optimization finished successfully in " + QString("%1 minutes %2 seconds").arg(min).arg(sec), false, false, false, "green");

    Stop();
}

void RocketScenePackager::ProcessGeneratedMaterial(Entity *ent, EC_Material *comp)
{
    if (!ent || !comp)
        return;
        
    // outputMat: generated material name
    QString outputMaterial = comp->outputMat.Get();
    if (!outputMaterial.trimmed().isEmpty())
    {
        QString subAssetName, basePath, fileName;
        AssetAPI::ParseAssetRef(outputMaterial, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);
        
        QString generatedOutputMaterial = "generated://" + (!fileName.isEmpty() ? fileName : QUuid::createUuid().toString().replace("{", "").replace("}", ""));
        if (!generatedOutputMaterial.toLower().endsWith(".material"))
            generatedOutputMaterial += ".material";
        if (generatedOutputMaterial.contains(" ")) 
            generatedOutputMaterial = generatedOutputMaterial.replace(" ", "_");
            
        state_.oldToNewGeneraterMaterial[outputMaterial] = generatedOutputMaterial;
        comp->outputMat.Set(generatedOutputMaterial, AttributeChange::Disconnected);
        
        state_.changedAttributes << qMakePair(AttributeWeakPtr(comp->shared_from_this(), &comp->outputMat), outputMaterial);
        state_.optimizedAssetRefs << outputMaterial;
    }
    
    // parameters: texture = <texRef>
    bool changedParams = false;
    const QString originalParams = comp->parameters.ToString();
    QVariantList params = comp->parameters.Get();
    for(int mi=0; mi<params.size(); ++mi)
    {
        QString paramString = params[mi].toString().trimmed();
        if (paramString.startsWith("texture = "))
        {
            QString originalTextureRef = paramString.mid(paramString.lastIndexOf("= ") + 2).trimmed();
            AssetReference ref(originalTextureRef, "TextureGenerated");
            
            QString subAssetName, basePath, fileName;
            AssetAPI::AssetRefType refType = AssetAPI::ParseAssetRef(ref.ref, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);

            AssetFileInfo *assetInfo = GetOrCreateAssetFileInfo(ref.ref, ref.type, refType, basePath, fileName, subAssetName);
            if (!assetInfo || assetInfo->dontProcess)
                return;

            // Original file is now copied. Convert and resize if enabled.
            if (!assetInfo->texturePostProcessed)
            {
                PostProcessTexture(assetInfo, "TextureGenerated");
                assetInfo->texturePostProcessed = true;
            }
            
            ref.ref = assetInfo->bundleAssetRef;
            params[mi] = "texture = " + ref.ref;

            state_.optimizedAssetRefs << originalTextureRef;
            state_.totalConvertedRefs++;
            changedParams = true;
        }
    }
    if (changedParams)
    {
        comp->parameters.Set(params, AttributeChange::Disconnected);
        state_.changedAttributes << qMakePair(AttributeWeakPtr(comp->shared_from_this(), &comp->parameters), originalParams);
    }
    
    // inputMat: process last as has returns.
    QString inputMaterial = comp->inputMat.Get();
    if (!inputMaterial.trimmed().isEmpty())
    {
        AssetReference ref(inputMaterial);
        ref.ref = ref.ref.trimmed();
        if (ref.ref.isEmpty())
            return;
        ref.type = ref.type.trimmed();
        if (ref.type.isEmpty())
            ref.type = framework_->Asset()->GetResourceTypeFromAssetRef(ref);

        QString subAssetName, basePath, fileName;
        AssetAPI::AssetRefType refType = AssetAPI::ParseAssetRef(ref.ref, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);

        AssetFileInfo *assetInfo = GetOrCreateAssetFileInfo(ref.ref, ref.type, refType, basePath, fileName, subAssetName);
        if (!assetInfo || assetInfo->dontProcess)
            return;

        // Process the materials texture dependencies.
        // This is only done once per material and involves
        // us writing a new file on disk if textures are changed.
        if (!assetInfo->texturesProcessed)
        {
            TextureList textures = GetTextures(ref.ref);
            if (!textures.isEmpty())
                ProcessTextures(assetInfo, textures);
            assetInfo->texturesProcessed = true;
        }

        // Update the material asset reference.
        comp->inputMat.Set(assetInfo->bundleAssetRef, AttributeChange::Disconnected);
        
        state_.changedAttributes << qMakePair(AttributeWeakPtr(comp->shared_from_this(), &comp->inputMat), inputMaterial);
        state_.optimizedAssetRefs << inputMaterial;
        state_.totalConvertedRefs++;
    }
}

void RocketScenePackager::ProcessMeshRef(Entity *ent, EC_Mesh *comp)
{
    if (!ent || !comp)
        return;

    QString originalMeshRef = comp->meshRef.Get().ref;

    AssetReference ref = comp->meshRef.Get();
    ref.ref = ref.ref.trimmed();
    if (ref.ref.isEmpty())
        return;
    ref.type = ref.type.trimmed();
    if (ref.type.isEmpty())
        ref.type = framework_->Asset()->GetResourceTypeFromAssetRef(ref);

    QString subAssetName, basePath, fileName;
    AssetAPI::AssetRefType refType = AssetAPI::ParseAssetRef(ref.ref, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);

    AssetFileInfo *assetInfo = GetOrCreateAssetFileInfo(ref.ref, ref.type, refType, basePath, fileName, subAssetName);
    if (!assetInfo || assetInfo->dontProcess)
        return;

    // Update the mesh asset reference
    ref.ref = assetInfo->bundleAssetRef;
    comp->meshRef.Set(ref, AttributeChange::Disconnected);
    
    state_.changedAttributes << qMakePair(AttributeWeakPtr(comp->shared_from_this(), &comp->meshRef), originalMeshRef);
    state_.optimizedAssetRefs << originalMeshRef;
    state_.totalConvertedRefs++;
}

void RocketScenePackager::ProcessMaterialRefs(Entity *ent, EC_Mesh *comp)
{
    if (!ent || !comp)
        return;

    bool changed = false;
    
    QString originalRefs = comp->materialRefs.ToString();
    AssetReferenceList refs = comp->materialRefs.Get();
    for(int i=0; i<refs.Size(); ++i)
    {
        if (IsStopping())
            return;

        QString originalMaterialRef = refs[i].ref;

        AssetReference ref = refs[i];
        ref.ref = ref.ref.trimmed();
        if (ref.ref.isEmpty())
            continue;
        ref.type = ref.type.trimmed();
        if (ref.type.isEmpty())
            ref.type = framework_->Asset()->GetResourceTypeFromAssetRef(ref);
            
        // Check if this is a EC_Material generated material reference.
        // If found set it as the new ref and continue.
        if (state_.oldToNewGeneraterMaterial.contains(ref.ref))
        {
            ref.ref = state_.oldToNewGeneraterMaterial[ref.ref];
            refs.Set(i, ref);
            changed = true;
            continue;
        }

        QString subAssetName, basePath, fileName;
        AssetAPI::AssetRefType refType = AssetAPI::ParseAssetRef(ref.ref, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);

        AssetFileInfo *assetInfo = GetOrCreateAssetFileInfo(ref.ref, ref.type, refType, basePath, fileName, subAssetName);
        if (!assetInfo || assetInfo->dontProcess)
            continue;

        // Process the materials texture dependencies.
        // This is only done once per material and involves
        // us writing a new file on disk if textures are changed.
        if (!assetInfo->texturesProcessed)
        {
            TextureList textures = GetTextures(ref.ref);
            if (!textures.isEmpty())
                ProcessTextures(assetInfo, textures);
            assetInfo->texturesProcessed = true;
        }

        // Update the material asset reference.
        ref.ref = assetInfo->bundleAssetRef;
        refs.Set(i, ref);

        state_.optimizedAssetRefs << originalMaterialRef;
        state_.totalConvertedRefs++;
        changed = true;
    }

    // If changes were done, update the material list.
    if (changed)
    {
        comp->materialRefs.Set(refs, AttributeChange::Disconnected);
        state_.changedAttributes << qMakePair(AttributeWeakPtr(comp->shared_from_this(), &comp->materialRefs), originalRefs);
    }
}

void RocketScenePackager::ProcessTextures(AssetFileInfo *materialAssetInfo, const TextureList &textures)
{
    if (textures.isEmpty())
        return;
    if (!materialAssetInfo || materialAssetInfo->dontProcess)
        return;

    AssetPtr asset = framework_->Asset()->GetAsset(materialAssetInfo->ref.ref);
    if (!asset.get())
    {
        Log("Cannot process textures for null asset with ref " + materialAssetInfo->ref.ref, true, true);
        return;
    }
    if (asset->Type() != "OgreMaterial")
    {
        Log("Cannot process textures for non OgreMaterial ref " + materialAssetInfo->ref.ref, true, true);
        return;
    }   
    
    OgreMaterialAsset *originalMaterialAsset = dynamic_cast<OgreMaterialAsset*>(asset.get());
    if (!originalMaterialAsset)
    {
        Log("Cannot process textures for non OgreMaterialAsset typed asset ptr " + materialAssetInfo->ref.ref, true, true);
        return;
    }
    OgreMaterialAsset *materialAsset = dynamic_cast<OgreMaterialAsset*>(originalMaterialAsset->Clone(materialAssetInfo->bundleAssetRef).get());
    if (!materialAsset)
    {
        Log("Failed to clone OgreMaterialAsset for " + materialAssetInfo->ref.ref, true, true);
        return;
    }

    bool changes = false;

    foreach(const TextureInfo &texture, textures)
    {
        if (IsStopping())
        {
            framework_->Asset()->ForgetAsset(materialAsset->Name(), false);
            return;
        }

        QString originalTextureRef = texture.ref;
        
        AssetReference ref(texture.ref);
        ref.ref = ref.ref.trimmed();
        if (ref.ref.isEmpty())
            continue;
        ref.type = framework_->Asset()->GetResourceTypeFromAssetRef(texture.ref);

        QString subAssetName, basePath, fileName;
        AssetAPI::AssetRefType refType = AssetAPI::ParseAssetRef(ref.ref, 0, 0, 0, 0, 0, &basePath, &fileName, &subAssetName);

        AssetFileInfo *assetInfo = GetOrCreateAssetFileInfo(ref.ref, ref.type, refType, basePath, fileName, subAssetName);
        if (!assetInfo || assetInfo->dontProcess)
            continue;

        // Original file is now copied. Convert and resize if enabled.
        if (!assetInfo->texturePostProcessed)
        {
            PostProcessTexture(assetInfo, "Texture");
            assetInfo->texturePostProcessed = true;
        }

        // This will print a shit ton of errors to console, but this is the only direct way of doing this.
        // If we use OgreMaterialAsset::SetTexture it wont set it really to Ogre until the ref is fetched from the web,
        // and in our case the asset is not yet in the web so we cannot fetch it.
        Ogre::TextureUnitState *textureUnit = materialAsset->GetTextureUnit(texture.iTech, texture.iPass, texture.iTexUnit);
        if (textureUnit)
        {
            textureUnit->setTextureName(AssetAPI::SanitateAssetRef(assetInfo->bundleAssetRef.toStdString()));
            state_.optimizedAssetRefs << originalTextureRef;
            changes = true;
        }
    }

    // Overwrite the material file to destination dir
    if (changes)
    {
        QString materialFilePath = materialAssetInfo->diskSource.absoluteFilePath();
        if (!materialAsset->SaveToFile(materialFilePath))
            Log("Failed to rewrite material file after texture changes: " + materialFilePath);
    }

    framework_->Asset()->ForgetAsset(materialAsset->Name(), false);
}

RocketScenePackager::TextureList RocketScenePackager::GetTextures(const QString &materialRef)
{
    TextureList textureRefs;
    if (materialRef.isEmpty())
    {
        if (state_.logDebug)
            Log("Cannot get textures for empty material ref", true, true);
        return textureRefs;
    }
    AssetPtr asset = framework_->Asset()->GetAsset(materialRef);
    if (!asset)
    {
        Log("Cannot get textures for null asset with ref " + materialRef, true, true);
        return textureRefs;
    }
    if (asset->Type() != "OgreMaterial")
    {
        Log("Cannot get textures for non OgreMaterial ref " + materialRef, true, true);
        return textureRefs;
    }

    OgreMaterialAsset *materialAsset = asset.get() ? dynamic_cast<OgreMaterialAsset*>(asset.get()) : 0;
    if (materialAsset)
    {
        std::vector<AssetReference> textureDependencies = materialAsset->FindReferences();
        
        for(int iTech=0; iTech<materialAsset->GetNumTechniques(); ++iTech)
        {
            int numPasses = materialAsset->GetNumPasses(iTech);
            for (int iPass=0; iPass<numPasses; ++iPass)
            {
                int numTexUnit = materialAsset->GetNumTextureUnits(iTech, iPass);
                for (int iTexUnit=0; iTexUnit<numTexUnit; ++iTexUnit)
                {
                    Ogre::TextureUnitState *textureUnitState = materialAsset->GetTextureUnit(iTech, iPass, iTexUnit);
                    QString textureRef = textureUnitState ? AssetAPI::DesanitateAssetRef(QString::fromStdString(textureUnitState->getTextureName())) : "";
                    if (!textureRef.isEmpty())
                    {
                        QString originalTextureRef = textureRef;

                        // Make sure this ref is found from the parsed texture dependencies.
                        // If not it can be a .crn texture that was internally renamed to .dds for Ogre.
                        bool found = false;
                        while (!found)
                        {
                            for (std::vector<AssetReference>::const_iterator iter = textureDependencies.begin(); iter != textureDependencies.end(); ++iter)
                            {
                                if ((*iter).ref.compare(textureRef, Qt::CaseInsensitive) == 0)
                                {
                                    found = true;
                                    break;
                                }
                            }
                            
                            if (!found)
                            {
                                if (textureRef.endsWith(".dds", Qt::CaseInsensitive))
                                    textureRef = textureRef.left(textureRef.lastIndexOf(".")+1) + "crn";
                                else
                                    break;
                            }
                        }
                        
                        if (found)
                        {
                            TextureInfo texture;
                            texture.iTech = iTech;
                            texture.iPass = iPass;
                            texture.iTexUnit = iTexUnit;
                            texture.ref = textureRef;
                            textureRefs << texture;
                        }
                        else
                            Log("Failed to find a texture from materials dependency list: " + originalTextureRef, true, false, true);
                    }
                }
            }
        }
    }
    else
        Log("Failed to cast AssetPtr to OgreMaterialAsset: " + materialRef, true, true);

    return textureRefs;
}

void RocketScenePackager::PostProcessTexture(AssetFileInfo *textureInfo, const QString &type)
{
    IAsset *textureAsset = framework_->Asset()->GetAsset(textureInfo->ref.ref).get();
    TextureAsset *texture = (textureAsset != 0 ? dynamic_cast<TextureAsset*>(textureAsset) : 0);
    if (texture)
    {
        if (!texture->ogreTexture.get())
        {
            Log("Failed to find '" + texture->Name() + "' texture from the rendering engine.", true, true);
            return;
        }

        // Source texture info
        QString src = textureInfo->diskSource.absoluteFilePath();
        QString srcSuffix = textureInfo->diskSource.suffix().toUpper();
        QSize srcSize((int)texture->ogreTexture->getWidth(), (int)texture->ogreTexture->getHeight());
        qint64 srcFileSize = textureInfo->diskSource.size();

        // Track biggest encountered sizes
        if (state_.foundMaxTexWidth < (uint)srcSize.width()) state_.foundMaxTexWidth = srcSize.width();
        if (state_.foundMaxTexHeight < (uint)srcSize.height()) state_.foundMaxTexHeight = srcSize.height();

        // Check if any processing is even enabled and if we support the input format.
        if (!state_.texProcessing.IsEnabled())
            return;
        if (!state_.texProcessing.supportedInputFormats.contains(srcSuffix, Qt::CaseInsensitive))
        {
            Log("Texture processing with input format " + srcSuffix + " is not supported!", true, true);
            return;
        }

        QStringList params;
        params << "-noprogress";
        params << "-logfile" << QDir::toNativeSeparators(state_.outputDir.absoluteFilePath("packager_texture-processing-log.txt"));
        params << "-file" << QDir::toNativeSeparators(src);

        // Quality
        params << "-quality" << QString::number(state_.texProcessing.quality);

        // Format for conversion.
        QString destSuffix = state_.texProcessing.convertToFormat.toUpper();
        params << "-fileformat" << destSuffix.toLower();

        // Destination file
        QString dest = textureInfo->diskSource.absoluteDir().absoluteFilePath(textureInfo->diskSource.baseName() + "." + destSuffix.toLower());       
        params << "-out" << QDir::toNativeSeparators(dest);

        // Resizing
        QSize destSize;
        if (state_.texProcessing.maxSize > 0)
        {
            if ((uint)srcSize.width() > state_.texProcessing.maxSize || (uint)srcSize.height() > state_.texProcessing.maxSize)
            {
                QString maxSizeStr = QString::number(state_.texProcessing.maxSize);
                params << "-clampscale" << maxSizeStr << maxSizeStr;
                destSize = QSize(state_.texProcessing.maxSize, state_.texProcessing.maxSize);

                state_.texProcessing.totalResized++;
            }
        }

        // Rescaling
        if (state_.texProcessing.rescaleMode > 0)
        {
            if (!IsPow2((uint)srcSize.width()) || !IsPow2((uint)srcSize.height()))
            {
                params << "-rescalemode";
                if (state_.texProcessing.rescaleMode == 1)
                    params << "nearest";
                else if (state_.texProcessing.rescaleMode == 2)
                    params << "lo";
                else if (state_.texProcessing.rescaleMode == 3)
                    params << "hi";
                else
                    params << "nearest";

                state_.texProcessing.totalRescaled++;
            }
        }
        
        // Mipmap generation
        if (!state_.texProcessing.mipmapGeneration.isEmpty())
        {
            params << "-mipMode" << state_.texProcessing.mipmapGeneration;

            // Max mipmap levels when generating
            if (state_.texProcessing.mipmapGeneration == "Generate")
                params << "-maxmips" << QString::number(state_.texProcessing.maxMipmaps);
        }

        // Bail out if we have nothing to do for this texture:
        // 1. Format is the same as destination
        // 2. Width and height is not over max size (-clampscale)
        // 3. Width and height are both power of 2 (-rescalemode)
        // 4. Mipmap generation is disabled, saying to use source mipmaps.
        if (srcSuffix == destSuffix && !params.contains("-clampscale") && !params.contains("-rescalemode") && state_.texProcessing.mipmapGeneration == "UseSource")
            return;

        // Run the process
        QProcess *p = new QProcess();
        p->start(QDir::toNativeSeparators(state_.textureTool), params);

        int exitCode = 0;
        if (!p->waitForStarted(30000))
            exitCode = 1;
        if (exitCode == 0)
        {
            // Wait max 2 minutes for this to complete, it should not take
            // that long even for the biggest textures. At the same time keep
            // our main thread responsive.
            int totaWaitTime = 0;
            while (p->state() == QProcess::Running)
            {
                if (!p->waitForFinished(40))
                {
                    totaWaitTime += 40;
                    if (totaWaitTime > (1000 * 120))
                    {
                        exitCode = 2;
                        Log("Texture tool did no finish in 120 seconds, stopping processing of: " + QDir::toNativeSeparators(src), true, true);
                        break;
                    }
                    // Try our best to keep Rocket responsive
                    QApplication::processEvents();
                    framework_->ProcessOneFrame();
                }
            }
            if (exitCode == 0)
            {
                exitCode = p->exitCode();
                if (exitCode == 0 && state_.logDebug)
                {
                    QByteArray output = p->readAllStandardOutput();
                    QTextStream outputStream(output);
                    while(!outputStream.atEnd())
                    {
                        QString logLine = outputStream.readLine().trimmed();
                        if (logLine.isEmpty() || logLine.startsWith("crunch:") || logLine.startsWith("copyright", Qt::CaseInsensitive) || logLine.startsWith("crnlib version") ||
                            logLine.startsWith("Appending output") || logLine.startsWith("Texture successfully loaded") || logLine.startsWith("Compressing using quality level") ||
                            logLine.startsWith("Texture successfully written") || logLine.startsWith("Texture successfully processed") || logLine.startsWith("Source texture:") ||
                            logLine.startsWith("1 total file(s)") || logLine.startsWith("Exit status:") || logLine.startsWith("Apparent type:"))
                            continue;
                        Log("    " + logLine, true, false, false, "green");
                    }
                }
            }
        }
        p->close();
        SAFE_DELETE(p);
        
        // Check exit code
        if (exitCode != 0)
        {
            if (exitCode == 1)
                Log("Texture tool execution failed, check " + QDir::toNativeSeparators(state_.outputDir.absoluteFilePath("packager_texture-processing-log.txt")) + " for more information.", true, true);
            return;
        }

        qint64 destFileSize = QFileInfo(dest).size();
        state_.texProcessing.totalProcessed++;

        // Only change bundle reference if the file changed.
        if (srcSuffix != destSuffix)
        {
            if (QFile::exists(dest))
            {
                // Modify bundle ref to the new .<format> spec.
                QString srcFilename = textureInfo->diskSource.fileName();
                QString destFileName = textureInfo->diskSource.baseName() + "." + destSuffix.toLower();
                textureInfo->bundleAssetRef = textureInfo->bundleAssetRef.replace(srcFilename, destFileName);

                // Copy original for inspection and remove it from being zipped.
                QString formatDir = "converted-to-" + destSuffix.toLower() + "/";
                if (!state_.outputDir.exists(formatDir))
                    state_.outputDir.mkdir(formatDir);
                QFile::copy(src, state_.outputDir.absoluteFilePath(formatDir + textureInfo->diskSource.fileName()));
                bool srcRemoved = QFile::remove(src);
                if (!srcRemoved)
                    Log("Failed to remove conversion source, please remove by hand from the zip: " + textureInfo->bundleAssetRef, true, true);

                textureInfo->SetDiskSource(dest);
                state_.texProcessing.totalConverted++;
            }
            else
                Log("Destination file does not exist after conversion: " + dest, true, true);
        }

        if (srcFileSize != destFileSize)
        {
            if (type == "Texture")
            {
                // Current size for splitting.
                if (state_.nowFileSizeTex >= srcFileSize)
                    state_.nowFileSizeTex -= srcFileSize;
                else
                    state_.nowFileSizeTex = 0;
                state_.nowFileSizeTex += destFileSize;

                // Total sizes
                state_.totalFileSizeTex -= srcFileSize;
                state_.totalFileSizeTex += destFileSize;
                state_.totalFileSize -= srcFileSize;
                state_.totalFileSize += destFileSize;
            }
            else if (type == "TextureGenerated")
            {
                // Current size for splitting.
                if (state_.nowFileSizeTexGen >= srcFileSize)
                    state_.nowFileSizeTexGen -= srcFileSize;
                else
                    state_.nowFileSizeTexGen = 0;
                state_.nowFileSizeTexGen += destFileSize;

                // Total size
                state_.totalFileSizeTexGen -= srcFileSize;
                state_.totalFileSizeTexGen += destFileSize;
                state_.totalFileSize -= srcFileSize;
                state_.totalFileSize += destFileSize;
            }
        }
    }
    else
        Log("Failed to find '" + textureInfo->ref.ref + "' texture from the asset system or type is not Texture.", true, true);
}

RocketScenePackager::AssetFileInfo *RocketScenePackager::GetOrCreateAssetFileInfo(const QString &ref, const QString &type, AssetAPI::AssetRefType refType, 
                                                                                  const QString &basePath, const QString &fileName, const QString &subAssetName)
{
    AssetFileInfo *existingAssetInfo = GetAssetFileInfo(ref);
    if (existingAssetInfo)
        return existingAssetInfo;

    // Asset needs to be loaded into AssetAPI.
    AssetPtr asset = framework_->Asset()->GetAsset(ref);
    if (!asset.get())
    {
        Log("Cannot process " + ref + ", not loaded to asset system.", true, true);
        return 0;
    }

    // This is a in memory generated asset, there is no disk source.
    // Make a new asset info entry and mark it a failed, even if its not.
    // This will make all sequential calls with this just be ignored.
    if (asset->DiskSourceType() == IAsset::Programmatic)
    {
        AssetFileInfo *memoryAssetInfo = new AssetFileInfo(ref, type);
        memoryAssetInfo->dontProcess = true;
        state_.assetsInfo << memoryAssetInfo;
        return memoryAssetInfo;
    }

    // Check if disk source exists.
    QString assetDiskSource = asset->DiskSource();
    if (assetDiskSource.isEmpty())
    {
        Log("Cannot process " + ref + ", no valid disk source.", true, true);
        return 0;
    }
    if (!QFile::exists(assetDiskSource))
    {
        Log("Cannot process " + ref + ", disk source file does not exist.", true, true);
        return 0;
    }

    // Decide which asset subdir (eventually zip file) this asset will go
    QString destinationSubdir;
    if (type == "OgreMesh")
        destinationSubdir = "models-" + QString::number(state_.currentMeshBundleIndex);
    else if (type == "OgreMaterial")
        destinationSubdir = "materials";
    else if (type == "Texture")
        destinationSubdir = "textures-" + QString::number(state_.currentTextureBundleIndex);
    else if (type == "TextureGenerated")
        destinationSubdir = "textures-generated-" + QString::number(state_.currentTextureGeneratedBundleIndex);
    else
    {
        Log("Asset type '" + type + "' not currently supported for zip bundling.", true, true);
        return 0;
    }

    // Clean path separators and spaces from filename
    QString filenameClean = subAssetName.trimmed().isEmpty() ? fileName.trimmed() : subAssetName.trimmed();
    if (filenameClean.contains("/"))
        filenameClean = filenameClean.split("/", QString::SkipEmptyParts).last();
    if (filenameClean.contains(" "))
        filenameClean = filenameClean.replace(" ", "_");
    
    // Additional url path before the filename.
    QString baseUrlAdditionalPath;
    
    // If we are rewriting refs, create a UUID as file name.
    // Additionally reset any path that was resolved inside the zip.
    if (state_.rewriteAssetReferences)
    {
        QFileInfo inputFileInfo(assetDiskSource);
        filenameClean = QUuid::createUuid().toString().replace("{","").replace("}","") + "." + inputFileInfo.suffix().toLower();

        // UUID collisions with file suffix will be extremely rare/implausible, but be sure
        // that we never get > 1 of the same UUID from QUuid as it would break scenes.
        while (state_.rewriteUuidFilenames.contains(filenameClean))
        {
            Log("UUID collision detected for " + filenameClean + ", generating new UUID.", true, false, true);
            filenameClean = QUuid::createUuid().toString().replace("{","").replace("}","") + "." + inputFileInfo.suffix().toLower();
        }
        state_.rewriteUuidFilenames << filenameClean;
    }
    else
    {
        if (refType == AssetAPI::AssetRefExternalUrl && !basePath.isEmpty())
        {
            QUrl baseUrl(ref);
            if (baseUrl.isValid())
            {
                // Get the UUID for this particular url host, not with path/query!
                QString cleanUrl = baseUrl.toString(QUrl::RemovePath|QUrl::RemoveQuery|QUrl::RemoveFragment|QUrl::StripTrailingSlash);
                if (!state_.assetBaseToUuid.contains(cleanUrl))
                {
                    QString uuid = QUuid::createUuid().toString().replace("{","").replace("}","");
                    state_.assetBaseToUuid[cleanUrl] = uuid;
                    if (state_.logDebug)
                        Log(cleanUrl + " -> " + uuid, true);
                }
                baseUrlAdditionalPath = state_.assetBaseToUuid[cleanUrl];

                // Get asset path, remove filename.
                QString urlPath = baseUrl.path();
                if (!urlPath.isEmpty())
                {
                    if (!urlPath.startsWith("/"))
                        urlPath = "/" + urlPath;
                    baseUrlAdditionalPath += urlPath.left(urlPath.lastIndexOf("/"));
                }
                if (!baseUrlAdditionalPath.endsWith("/"))
                    baseUrlAdditionalPath += "/";
            }
        }
        else if (refType == AssetAPI::AssetRefRelativePath && !basePath.isEmpty())
        {
            baseUrlAdditionalPath = basePath;
            if (!baseUrlAdditionalPath.endsWith("/"))
                baseUrlAdditionalPath += "/";
        }
    }
    
    // Remove any spacer from additional path
    if (baseUrlAdditionalPath.contains(" "))
        baseUrlAdditionalPath = baseUrlAdditionalPath.replace(" ", "_");
    
    // 1) URL ref: models/<URL_BASE_UUID>/url/path/
    // 2) Relative ref: models/<SUB_DIR_IF_RELATIVE_HAD_IT>
    destinationSubdir += "/" + baseUrlAdditionalPath;

    // If creating the target dir fails, the copy may 
    // create it or if it fails, well handle it there.
    if (!state_.outputDir.exists(destinationSubdir))
        state_.outputDir.mkpath(destinationSubdir);

    // Absolute destination path.
    QString destinationAbsoluteFilePath = state_.outputDir.absoluteFilePath(destinationSubdir + filenameClean);

    // Copy file to destination
    AssetFileInfo *assetInfo = new AssetFileInfo(ref, type);
    if (QFile::copy(assetDiskSource, destinationAbsoluteFilePath))
    {
        state_.filesCopied++;

        // Set file information
        assetInfo->SetDiskSource(destinationAbsoluteFilePath);

        // Resolve final asset reference for this asset
        QString logMessage;
        if (type == "OgreMesh")
        {
            state_.bundledMeshRefs++;
            state_.totalFileSizeMesh += assetInfo->diskSource.size();
            state_.nowFileSizeMesh += assetInfo->diskSource.size();

            assetInfo->bundleAssetRef = state_.bundlePrefix + "models-" +
                QString::number(state_.currentMeshBundleIndex) + ".zip#" + baseUrlAdditionalPath + filenameClean;
            logMessage = ref;
            TrimBaseStorageUrl(logMessage);
            
            if (state_.nowFileSizeMesh > state_.bundleSplitSize)
            {
                state_.currentMeshBundleIndex++;
                state_.nowFileSizeMesh = 0;
            }
        }
        else if (type == "OgreMaterial")
        {
            state_.bundledMaterialRefs++;
            state_.totalFileSizeMat += assetInfo->diskSource.size();

            assetInfo->bundleAssetRef = state_.bundlePrefix + "materials.zip#" + baseUrlAdditionalPath + filenameClean;
            logMessage = ref;
            TrimBaseStorageUrl(logMessage);
        }
        else if (type == "Texture")
        {
            state_.bundledTextureRefs++;
            state_.totalFileSizeTex += assetInfo->diskSource.size();
            state_.nowFileSizeTex += assetInfo->diskSource.size();

            // Material textures always use full urls as they reside in a different asset bundle.
            assetInfo->bundleAssetRef = state_.txmlForcedBaseUrl + state_.bundlePrefix + "textures-" + 
                QString::number(state_.currentTextureBundleIndex) +".zip#" + baseUrlAdditionalPath + filenameClean;
            logMessage = asset->Name();
            TrimBaseStorageUrl(logMessage);
            logMessage = "  " + logMessage;

            if (state_.nowFileSizeTex > state_.bundleSplitSize)
            {
                state_.currentTextureBundleIndex++;
                state_.nowFileSizeTex = 0;
            }
        }
        else if (type == "TextureGenerated")
        {
            state_.bundledTextureGeneratedRefs++;
            state_.totalFileSizeTexGen += assetInfo->diskSource.size();
            state_.nowFileSizeTexGen += assetInfo->diskSource.size();

            // EC_Material 'texture = <ref>' parameter textures always use full urls.
            assetInfo->bundleAssetRef = state_.txmlForcedBaseUrl + state_.bundlePrefix + "textures-generated-" + 
                QString::number(state_.currentTextureGeneratedBundleIndex) +".zip#" + baseUrlAdditionalPath + filenameClean;
            logMessage = asset->Name();
            TrimBaseStorageUrl(logMessage);
            logMessage = "  " + logMessage;

            if (state_.nowFileSizeTexGen > state_.bundleSplitSize)
            {
                state_.currentTextureGeneratedBundleIndex++;
                state_.nowFileSizeTexGen = 0;
            }
        }

        state_.totalFileSize += assetInfo->diskSource.size();

        if (state_.logDebug)
            Log(logMessage, true);

        // To keep the UI responsive while copying ton of files.
        QApplication::processEvents();
        framework_->ProcessOneFrame();
    }
    else
    {
        Log("Failed to copy " + assetDiskSource + " to " + destinationAbsoluteFilePath + ". Reference will not be processed: " + ref, true, true);
        assetInfo->dontProcess = true;
    }

    // This should be the only place where we 
    // add asset information to our state.
    state_.assetsInfo << assetInfo;

    return assetInfo;
}

RocketScenePackager::AssetFileInfo *RocketScenePackager::GetAssetFileInfo(const QString &ref)
{
    foreach(AssetFileInfo *assetInfo, state_.assetsInfo)
        if (assetInfo->ref.ref == ref)
            return assetInfo;
    return 0;   
}

void RocketScenePackager::State::Reset()
{
    maxPackagerThreads = 1;

    waitingForAssetBundleUploads = false;
    bundleStorageUploadsOk = false;
    bundlesUploading = false;

    entitiesProcessed = 0;
    entitiesEmptyRemoved = 0;
    totalConvertedRefs = 0;
    bundledMeshRefs = 0;
    bundledMaterialRefs = 0;
    bundledTextureRefs = 0;
    bundledTextureGeneratedRefs = 0;

    currentMeshBundleIndex = 1;
    currentTextureBundleIndex = 1;
    currentTextureGeneratedBundleIndex = 1;

    totalFileSize = 0;
    totalFileSizeMesh = 0;
    totalFileSizeMat = 0;
    totalFileSizeTex = 0;
    totalFileSizeTexGen = 0;
    nowFileSizeMesh = 0;
    nowFileSizeTex = 0;
    nowFileSizeTexGen = 0;
    
    bundleSplitSize = (10 * 1024 * 1024);
    filesCopied = 0;

    foundMaxTexWidth = 0;
    foundMaxTexHeight = 0;

    processMeshes = true;
    processMaterials = true;
    processGeneratedMaterials = true;
    processScriptEnts = false;
    processContentToolsEnts = false;

    removeEmptyEntities = true;
    rewriteAssetReferences = false;

    if (logFile && logFile->isOpen())
        logFile->close();
    SAFE_DELETE(logFile);
    logDebug = false;

    txmlRef = "";
    txmlForcedBaseUrl = "";
    outputTxmlFile = "";
    bundlePrefix = "";
    outputDir = QDir();

    assetBaseToUuid.clear();
    rewriteUuidFilenames.clear();
    oldToNewGeneraterMaterial.clear();

    foreach(AssetFileInfo *assetInfo, assetsInfo)
        if (assetInfo) SAFE_DELETE(assetInfo);
    assetsInfo.clear();

    foreach(RocketZipWorker *packager, packagers)
        SAFE_DELETE_LATER(packager);
    packagers.clear();
    
    changedAttributes.clear();
    optimizedAssetRefs.clear();

    time = QTime();

    texProcessing.Reset();
}

QFile *RocketScenePackager::State::OpenLog()
{
    if (!logFile)
        logFile = new QFile(outputDir.absoluteFilePath("packager_main-log.txt"));
    if (!logFile->isOpen())
    {
        if (logFile->open(QIODevice::ReadWrite))
            logFile->resize(0);
        else
            return 0;
    }

    return logFile;
}
