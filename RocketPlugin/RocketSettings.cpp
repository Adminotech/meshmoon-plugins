/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketSettings.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "oculus/RocketOculusManager.h"

#include "Framework.h"
#include "Math/MathFunc.h"
#include "LoggingFunctions.h"
#include "Application.h"
#include "CoreDefines.h"
#include "ConfigAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <OgreRoot.h>
#include <OgreRenderSystem.h>
#include <OgreConfigOptionMap.h>

#include <QGraphicsScene>
#include <QMessageBox>
#include <QTimer>
#include <QDesktopWidget>
#include <QNetworkProxyFactory>
#include <QNetworkProxy>

#include "MemoryLeakCheck.h"

namespace
{
    QString GraphicsModeToString(ClientSettings::GraphicsMode val)
    {
        switch(val)
        {
        case ClientSettings::GraphicsNone: return "None";
        case ClientSettings::GraphicsCustom: return "Custom";
        case ClientSettings::GraphicsUltra: return "Ultra";
        case ClientSettings::GraphicsHigh: return "High";
        case ClientSettings::GraphicsMedium: return "Medium";
        case ClientSettings::GraphicsLow: return "Low";
        default: return "Unknown";
        }
    }

    QString ShadowQualityToString(OgreRenderer::Renderer::ShadowQualitySetting val)
    {
        switch(val)
        {
        case OgreRenderer::Renderer::Shadows_Off: return "Off";
        case OgreRenderer::Renderer::Shadows_Low: return "Low";
        case OgreRenderer::Renderer::Shadows_High: return "High";
        default: return "Unknown";
        }
    }
}

RocketSettings::RocketSettings(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    LC("[RocketSettings]: "),
    proxy_(0),
    vsyncSupported_(false),
    notificationChecked_(false)
{
    ConfigData config("adminotech", "clientplugin");
    
    // Set our configures FPS limit for Rocket startup
    int fpsLimit = framework_->Config()->Read(config, "fps target", -1).toInt();
    if (fpsLimit > -1 && (int)framework_->App()->TargetFpsLimit() != fpsLimit && !framework_->HasCommandLineParameter("--fpslimit"))
        framework_->App()->SetTargetFpsLimit(static_cast<double>(fpsLimit));

    // Port old keys to new keys and changed mode defaults. These can be removed after a few releases!
    ConfigAPI &cfg = *framework_->Config();
    const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);

    /** Added @ 2.5.3.2
        "shadow texture size pssm" -> "shadow texture size csm" */
    if (!cfg.HasKey(renderingCfg, "shadow texture size csm") && cfg.HasKey(renderingCfg, "shadow texture size pssm"))
        cfg.Write(renderingCfg, "shadow texture size csm", cfg.Read(renderingCfg, "shadow texture size pssm").toInt());
    
    /** Added @ 2.5.4.0
        GraphicsMedium setting defaults were changed. This will auto upgrade the config without the need
        to enabled a different mode, restart, enable the old mode back and restart. This will upgrade the settings and they
        be in effect after the next restart that the user will do eventually. */
    if (CurrentGraphicsMode() == ClientSettings::GraphicsMedium)
    {
        // Changed from 0-2 to 0
        QString antialiasShadowLast = cfg.Read(renderingCfg, "shadow texture antialias", "0").toString();
        if (antialiasShadowLast != "0")
            cfg.Write(renderingCfg, "shadow texture antialias", "0");
        // Changed from Shadows_Low to Shadows_High
        int shadowQualityLast = cfg.Read(renderingCfg, "shadow quality", static_cast<int>(OgreRenderer::Renderer::Shadows_Off)).toInt();
        if (shadowQualityLast != static_cast<int>(OgreRenderer::Renderer::Shadows_High))
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_High);
        // Changed from 1024 to 512
        int shadowSizeCsmLast = cfg.Read(renderingCfg, "shadow texture size csm", 0).toInt();
        if (shadowSizeCsmLast != 512)
            cfg.Write(renderingCfg, "shadow texture size csm", 512);
    }

    ApplyHttpProxy();
}

RocketSettings::~RocketSettings()
{
    SaveSettings(false);

    if (proxy_)
    {
        framework_->Ui()->GraphicsScene()->removeItem(proxy_);
        SAFE_DELETE(proxy_);
    }
}

void RocketSettings::DumpSettings(QTextStream &stream)
{
    const ClientSettings settings = CurrentSettings();

    stream << "Rocket Settings" << endl
           << "=================================================================================" << endl;

    // Window/desktop-related
    //auto availableGeometry1 = QApplication::desktop()->availableGeometry();
    const QRect availableGeometry = QApplication::desktop()->availableGeometry(framework_->Ui()->MainWindow());
    const QRect screenGeometry = QApplication::desktop()->screenGeometry(framework_->Ui()->MainWindow());
    stream << "Full screen                   : " << BoolToString(framework_->Ui()->MainWindow()->isFullScreen()) << endl;
    stream << "Window size                   : " << QString("%1x%2").arg(framework_->Ui()->MainWindow()->width()).arg(framework_->Ui()->MainWindow()->height()) << endl;
    stream << "Screen count                  : " << QApplication::desktop()->screenCount() << endl;
    stream << "Primary screen                : " << QApplication::desktop()->primaryScreen() << endl;
    stream << "Window on screen              : " << QApplication::desktop()->screenNumber(framework_->Ui()->MainWindow()) << endl;
    stream << "Screen geom. on active screen : " << QString("%1x%2").arg(screenGeometry.width()).arg(screenGeometry.height()) << endl;
    stream << "Avail. geom. on active screen : " << QString("%1x%2").arg(availableGeometry.width()).arg(availableGeometry.height()) << endl;
    stream << "Desktop size                  : " << QString("%1x%2").arg(UiMainWindow::DesktopWidth()).arg(UiMainWindow::DesktopHeight()) << endl << endl;

    // Graphics/rendering-related
    ConfigAPI &cfg = *framework_->Config();
    using namespace OgreRenderer;
    const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);
    Renderer::ShadowQualitySetting shadowQuality = static_cast<Renderer::ShadowQualitySetting>(cfg.Read(renderingCfg, "shadow quality").toInt());
    int shadowTexSize = cfg.Read(renderingCfg, (shadowQuality == Renderer::Shadows_High ? "shadow texture size csm" : "shadow texture size")).toInt();

    stream << "Graphics mode                 : " << GraphicsModeToString(settings.graphicsMode) << endl
           << "Target FPS                    : " << framework_->App()->TargetFpsLimit() << endl
           << "Target FPS when inactive      : " << framework_->App()->TargetFpsLimitWhenInactive() << endl
           << "View distance                 : " << ViewDistance() << endl
           << "Antialiasing                  : " << cfg.Read(renderingCfg, "antialias").toString() << endl
           << "Shadow antialiasing           : " << cfg.Read(renderingCfg, "shadow texture antialias").toString() << endl
           << "Shadow quality                : " << ShadowQualityToString(shadowQuality) << endl
           << "Shadow texture size           : " << shadowTexSize << "x" << shadowTexSize << endl
           << "V-sync                        : " << cfg.Read(renderingCfg, "vsync", "No").toString() << endl << endl;

    // Misc.
    stream << "Sounds enabled                : " << BoolToString(settings.enableSounds) << endl
           << "Show news                     : " << BoolToString(settings.showHostingNews) << endl
           << "Show stats                    : " << BoolToString(settings.showHostingStats) << endl
           << "Detect rendering performance  : " << BoolToString(settings.detectRenderingPerformance) << endl
           << "Detect network performance    : " << BoolToString(settings.detectNetworkPerformance) << endl
           << "Max web browser processes     : " << settings.numProcessesWebBrowser << endl
           << "Max media player processes    : " << settings.numProcessesMediaPlayer << endl;
    if (!settings.proxyHost.isEmpty())
        stream << "HTTP proxy host               : " << settings.proxyHost << endl;
    if (settings.proxyPort > 0)
        stream << "HTTP proxy port               : " << settings.proxyPort << endl;
    
    stream << endl << flush;
}

bool OculusScreenSort(int v1, int v2)
{
    QRect screen1 = QApplication::desktop()->availableGeometry(v1);
    QRect screen2 = QApplication::desktop()->availableGeometry(v2);
    if (screen1.left() < screen2.left())
        return true;
    return false;
}

void RocketSettings::InitUi()
{
    if (proxy_)
        return;
        
    proxy_ = new RocketSceneWidget(framework_);
    ui_.setupUi(proxy_->widget_);
    ui_.frameCustom->hide();
    
    framework_->Ui()->GraphicsScene()->addItem(proxy_);
    
    DetectRenderingOptions();

    LoadSettings();
    
    connect(ui_.buttonClose, SIGNAL(clicked()), SLOT(Close()), Qt::QueuedConnection);
    connect(ui_.boxShowNews, SIGNAL(clicked()), SLOT(EmitSettings()));
    connect(ui_.boxShowStats, SIGNAL(clicked()), SLOT(EmitSettings()));
    connect(ui_.buttonGfxCustom, SIGNAL(toggled(bool)), SLOT(OnCustomGfxSettingsToggled()));
    connect(ui_.comboBoxShadowQuality, SIGNAL(currentIndexChanged(int)), SLOT(OnShadowQualitySelected(int)));
    connect(ui_.caveEnabled, SIGNAL(toggled(bool)), this, SLOT(OnCaveEnabledToggled(bool)));
    connect(ui_.oculusEnabled, SIGNAL(toggled(bool)), this, SLOT(OnOculusEnabledToggled(bool)));
    connect(ui_.comboBoxProxySelection, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProxyTypeChanged(int)));

    proxy_->hide();
    
    OnProxyTypeChanged(ui_.comboBoxProxySelection->currentIndex());
}

void RocketSettings::DetectRenderingOptions()
{
    fsaaSupportedModes_.clear();
    vsyncSupported_ = false;
    
    ui_.comboBoxFSAA->clear();
    ui_.comboBoxShadowFSAA->clear();
    
    // Find out ogre render system capabilities
    Ogre::RenderSystem *renderSystem = Ogre::Root::getSingleton().getRenderSystem();
    if (!renderSystem)
    {
        LogError(LC + "No render system found, cannot detect options!");
        return;
    }

    // While we inspect some of the more interesting modes dump everything possible to the (debug) console for potential end user debugging!
    const Ogre::ConfigOptionMap &options = renderSystem->getConfigOptions();
    for(Ogre::ConfigOptionMap::const_iterator optionIter = options.begin(); optionIter!= options.end(); ++optionIter)
    {
        const Ogre::ConfigOption &option = optionIter->second;
        const QString optionName = QString::fromStdString(option.name);
        const QString currentValue = QString::fromStdString(option.currentValue);
        
        LogDebug(LC + "Render system configure option \"" + optionName + "\"");
        LogDebug(LC + "  Current value: " + currentValue);
        for(Ogre::StringVector::const_iterator iter = option.possibleValues.begin(); iter != option.possibleValues.end(); ++iter)
        {
            QString possibleValue = QString::fromStdString((*iter));
            QString possibleValueLowerTrimmed = possibleValue.toLower().trimmed();
            LogDebug(LC + "  - " + possibleValue);

            if (optionName .compare("FSAA", Qt::CaseInsensitive) == 0)
                fsaaSupportedModes_ << possibleValue;
            else if (optionName.compare("VSync", Qt::CaseInsensitive) == 0)
            {
                if (possibleValueLowerTrimmed == "yes" || possibleValueLowerTrimmed == "1" || possibleValueLowerTrimmed == "true")
                {
                    vsyncSupported_ = true;
                    vsyncSupportedYes_ = possibleValue;
                }
                else if (possibleValueLowerTrimmed == "no" || possibleValueLowerTrimmed == "0" || possibleValueLowerTrimmed == "false")
                    vsyncSupportedNo_ = possibleValue;
            }
        }
    }
    
    // Update custom option UI
    if (!fsaaSupportedModes_.isEmpty())
    {
        ui_.comboBoxFSAA->addItems(fsaaSupportedModes_);
        
        // Don't add anything but plain numbers into shadow FSAA.
        // The supported list might have things like 8 [High]
        foreach(QString fsaaMode, fsaaSupportedModes_)
        {
            bool ok = false;
            int test = fsaaMode.toInt(&ok);
            if (ok && test >= 0 && !fsaaMode.contains(" ", Qt::CaseInsensitive))
                ui_.comboBoxShadowFSAA->addItem(fsaaMode);
        }
    }

    if (!vsyncSupported_)
        ui_.boxShowVsync->setChecked(false);
    ui_.boxShowVsync->setEnabled(vsyncSupported_);
}

QString RocketSettings::GetHighestSupportedFSAA(int maxLevel, bool includeQuality)
{
    if (fsaaSupportedModes_.isEmpty())
        return "0";
    QString result = "";
    
    while(true)
    {
        QString maxLevelStr = QString::number(maxLevel);
        foreach(QString supportedMode, fsaaSupportedModes_)
        {
            if (supportedMode == maxLevelStr)
                result = supportedMode;
        }
        
        // Found.
        if (!result.isEmpty())
        {
            if (includeQuality && fsaaSupportedModes_.contains(result + " [Quality]"))
                result += " [Quality]";
            break;
        }
        
        // Not found, try the next lowest level: 16 -> 8 -> 4 etc.
        if (maxLevel <= 2)
            return "0";
        maxLevel /= 2;
    }
    
    return result;
}

void RocketSettings::OnSceneResized(const QRectF &rect)
{
    if (proxy_ && proxy_->isVisible())
    {
        QPointF pos(((rect.width()-220)/2.0)-(proxy_->size().width()/2.0), 0);
        if (pos.x() < 0) pos.setX(0);
        proxy_->widget_->setFixedHeight(rect.height());
        proxy_->widget_->setFixedWidth(Min(780, static_cast<int>(rect.width()) - 220));
        proxy_->setPos(pos);
    }
}

void RocketSettings::EmitSettings()
{
    ClientSettings settings = CurrentSettings();
    emit SettingsApplied(&settings);
}

ClientSettings RocketSettings::CurrentSettings() const
{
    ConfigAPI &cfg = *framework_->Config();
    ClientSettings settings;
    if (proxy_)
    {
        // UI
        settings.enableSounds = ui_.boxEnableSounds->isChecked();
        settings.showHostingNews = ui_.boxShowNews->isChecked();
        settings.showHostingStats = ui_.boxShowStats->isChecked();

        // Graphics
        settings.graphicsMode = CurrentGraphicsMode();

        // Reporting
        settings.detectRenderingPerformance = ui_.checkBoxDetectRendering->isChecked();
        settings.detectNetworkPerformance = ui_.checkBoxDetectNetwork->isChecked();

        // Web and Media
        settings.numProcessesWebBrowser = ui_.spinBoxNumProcessWeb->value();
        settings.numProcessesMediaPlayer = ui_.spinBoxNumProcessMedia->value();

        // HTTP Proxy
        settings.proxyHost = ui_.lineEditProxyHost->text().trimmed();
        settings.proxyPort = ui_.spinBoxProxyPort->value();
    }
    else
    {
        ConfigData config("adminotech", "clientplugin");

        // Ui
        settings.enableSounds = cfg.Read(config, "enablesounds", true).toBool();
        settings.showHostingNews = cfg.Read(config, "showhostingnews", true).toBool();
        settings.showHostingStats = cfg.Read(config, "showhostingstats", true).toBool();
        
        // Graphics
        settings.graphicsMode = static_cast<ClientSettings::GraphicsMode>(cfg.Read(config, "graphicsmode", ClientSettings::GraphicsLow).toInt());
        
        // Default to low mode, write it to config and show it as the current setting.
        // Many users don't know what they should select and can even select too high of a level which makes
        // their client crash when logging to scene or even at startup!
        if (settings.graphicsMode == ClientSettings::GraphicsNone)
        {
            settings.graphicsMode = ClientSettings::GraphicsLow;
            
            // This is kind of a hack but SaveSettings cannot be used as it requires the UI to be up.
            cfg.Write(config, "graphicsmode", settings.graphicsMode);
            const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);
            cfg.Write(renderingCfg, "antialias", "0");
            cfg.Write(renderingCfg, "shadow texture antialias", "0");
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_Off);
            cfg.Write(renderingCfg, "shadow texture size", 512);
            cfg.Write(renderingCfg, "shadow texture size csm", 512);
        }
        
        // Reporting
        settings.detectRenderingPerformance = cfg.Read(config, "detect rendering performance", true).toBool();
        settings.detectNetworkPerformance = cfg.Read(config, "detect network performance", true).toBool();
        
        // Web and Media
        settings.numProcessesWebBrowser = cfg.Read(config, "numprocessesweb", 5).toInt();
        settings.numProcessesMediaPlayer = cfg.Read(config, "numprocessesmedia", 5).toInt();
        
        // HTTP Proxy
        settings.proxyHost = cfg.Read(config, "httpproxyhost", "").toString();
        settings.proxyPort = cfg.Read(config, "httpproxyport", 0).toInt();
    }
    return settings;
}

void RocketSettings::SelectUltra()
{
    ui_.buttonGfxUltra->setChecked(true);
}

void RocketSettings::SelectHigh()
{
    ui_.buttonGfxHigh->setChecked(true);
}

void RocketSettings::SelectMedium()
{
    ui_.buttonGfxMedium->setChecked(true);
}

void RocketSettings::SelectLow()
{
    ui_.buttonGfxLow->setChecked(true);
}

void RocketSettings::OnCustomGfxSettingsToggled()
{
    if (ui_.buttonGfxCustom->isChecked())
    {
        
        
        ui_.frameCustom->show();

        ConfigAPI &cfg = *framework_->Config();
        const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);
        QString vsync = cfg.Read(renderingCfg, "vsync", vsyncSupportedNo_).toString();
        QString antialias = cfg.Read(renderingCfg, "antialias", "0").toString();
        QString antialiasShadow = cfg.Read(renderingCfg, "shadow texture antialias", "0").toString();
        int shadowQuality = cfg.Read(renderingCfg, "shadow quality", 0).toInt();
        int shadowSize = cfg.Read(renderingCfg, "shadow texture size", 512).toInt();
        int shadowSizeCsm = cfg.Read(renderingCfg, "shadow texture size csm", 512).toInt();     

        if (!vsyncSupported_)
            ui_.boxShowVsync->setChecked(false);
        else
            ui_.boxShowVsync->setChecked((vsync == vsyncSupportedYes_));
        ui_.boxShowVsync->setEnabled(vsyncSupported_);

        for(int i=0; i<ui_.comboBoxFSAA->count(); ++i)
        {
            if (ui_.comboBoxFSAA->itemText(i) == antialias)
            {
                ui_.comboBoxFSAA->setCurrentIndex(i);
                break;
            }
        }
        for(int i=0; i<ui_.comboBoxShadowFSAA->count(); ++i)
        {
            if (ui_.comboBoxShadowFSAA->itemText(i) == antialiasShadow)
            {
                ui_.comboBoxShadowFSAA->setCurrentIndex(i);
                break;
            }
        }

        if (shadowQuality == 0) 
            ui_.comboBoxShadowQuality->setCurrentIndex(0);
        else if (shadowQuality == 1) 
        {
            ui_.comboBoxShadowQuality->setCurrentIndex(1);
            
            if (shadowSize == 256) ui_.comboBoxShadowTextureSize->setCurrentIndex(0);
            else if (shadowSize == 512) ui_.comboBoxShadowTextureSize->setCurrentIndex(1);
            else if (shadowSize == 1024) ui_.comboBoxShadowTextureSize->setCurrentIndex(2);
            else if (shadowSize == 2048) ui_.comboBoxShadowTextureSize->setCurrentIndex(3);
        }
        else if (shadowQuality == 2) 
        {
            if (ui_.comboBoxShadowQuality->count() >= 3)
                ui_.comboBoxShadowQuality->setCurrentIndex(2);
            
            if (shadowSizeCsm == 256) ui_.comboBoxShadowTextureSize->setCurrentIndex(0);
            else if (shadowSizeCsm == 512) ui_.comboBoxShadowTextureSize->setCurrentIndex(1);
            else if (shadowSizeCsm == 1024) ui_.comboBoxShadowTextureSize->setCurrentIndex(2);
            else if (shadowSizeCsm == 2048) ui_.comboBoxShadowTextureSize->setCurrentIndex(3);
        }
        
        ui_.comboBoxShadowTextureSize->setEnabled((shadowQuality > 0));
        ui_.spinBoxTargetFPS->setValue((int)framework_->App()->TargetFpsLimit());
        ui_.spinBoxViewDistance->setValue(ViewDistance());
    }
    else
        ui_.frameCustom->hide();
}

void RocketSettings::OnShadowQualitySelected(int index)
{
    ui_.comboBoxShadowTextureSize->setEnabled(index > 0);
}

void RocketSettings::OnCaveEnabledToggled(bool enabled)
{
    ui_.caveEnabled->setText(enabled ? tr("Enabled") : tr("Disabled"));
    ui_.frameCave->setVisible(enabled);
}

void RocketSettings::OnOculusEnabledToggled(bool enabled)
{
    ui_.oculusEnabled->setText(enabled ? tr("Enabled") : tr("Disabled"));
    ui_.frameOculusSettings->setVisible(enabled);
}

void RocketSettings::OnOculusScreenToggled(bool enabled)
{
    if (!enabled)
        return;

    QObject* sender = QObject::sender();
    if (!sender)
        return;

    foreach(QPushButton* button, oculusMonitors_)
    {
        if (sender == button)
            continue;
        button->setChecked(false);
    }
}

void RocketSettings::OnProxyTypeChanged(int currentIndex)
{
    // Disabled
    if (currentIndex == 0)
    {
        ui_.lineEditProxyHost->setText("");
        ui_.spinBoxProxyPort->setValue(0);
        
        ui_.lineEditProxyHost->hide();
        ui_.labelProxyPortDelimiter->hide();
        ui_.spinBoxProxyPort->hide();
    }
    // System
    else if (currentIndex == 1)
    {
        LogInfo(LC + "Asking system for HTTP proxys... please wait");
        
        bool httpProxyFound = false;
        QList<QNetworkProxy> proxys = QNetworkProxyFactory::systemProxyForQuery();
        foreach(const QNetworkProxy &proxy, proxys)
        {
            if (proxy.type() == QNetworkProxy::HttpProxy)
            {
                LogInfo(LC + "  >> HTTP proxy found: type=" + QString::number(proxy.type()) + " host=" + proxy.hostName() + " port=" + QString::number(proxy.port()) + " capabilities=" + QString::number(proxy.capabilities()));
                if (!httpProxyFound)
                {
                    httpProxyFound = true;
                    
                    LogInfo(LC + "     >> Enabling as application wide proxy");
                    QNetworkProxy::setApplicationProxy(proxy);
                    
                    ui_.lineEditProxyHost->setText(proxy.hostName());
                    ui_.spinBoxProxyPort->setValue(proxy.port());
                    
                    ui_.lineEditProxyHost->show();
                    ui_.labelProxyPortDelimiter->show();
                    ui_.spinBoxProxyPort->show();
                }
            }
        }

        if (!httpProxyFound)
        {
            ui_.comboBoxProxySelection->setCurrentIndex(0);
            plugin_->Notifications()->ShowSplashDialog("Could not determine HTTP proxy settings from system", ":/images/icon-exit.png");

            ui_.lineEditProxyHost->hide();
            ui_.labelProxyPortDelimiter->hide();
            ui_.spinBoxProxyPort->hide();
        }
    }
    else if (currentIndex == 2)
    {
        ui_.lineEditProxyHost->show();
        ui_.labelProxyPortDelimiter->show();
        ui_.spinBoxProxyPort->show();
    }
}

bool RocketSettings::ApplyHttpProxy()
{
    bool applied = false;
    
    ConfigData config("adminotech", "clientplugin");
    int proxyType = framework_->Config()->Read(config, "httpproxytype", 0).toInt();
    
    // System proxy. Apply first we can find.
    if (proxyType == 1)
    {
        QList<QNetworkProxy> proxys = QNetworkProxyFactory::systemProxyForQuery();
        foreach(const QNetworkProxy &proxy, proxys)
        {
            if (proxy.type() == QNetworkProxy::HttpProxy)
            {
                if (!applied)
                {
                    applied = true;
                    LogInfo(LC + "Enabling system wide HTTP proxy from OS: type=" + QString::number(proxy.type()) + " host=" + proxy.hostName() + " port=" + QString::number(proxy.port()) + " capabilities=" + QString::number(proxy.capabilities()));
                    QNetworkProxy::setApplicationProxy(proxy);
                    break;
                }
            }
        }
    }
    // Manual proxy
    else if (proxyType == 2)
    {
        QString host = framework_->Config()->Read(config, "httpproxyhost", "").toString().trimmed();
        int port = framework_->Config()->Read(config, "httpproxyport", 0).toInt();
        
        if (!host.isEmpty() && port != 0)
        {
            applied = true;
            QNetworkProxy proxy(QNetworkProxy::HttpProxy, host, port);
            LogInfo(LC + "Enabling system wide HTTP proxy: type=" + QString::number(proxy.type()) + " host=" + proxy.hostName() + " port=" + QString::number(proxy.port()) + " capabilities=" + QString::number(proxy.capabilities()));
            QNetworkProxy::setApplicationProxy(proxy);
        }
        else
            LogError(LC + "Manually defined proxy settings invalid: host=" + host + " port=" + QString::number(port));
    }
    
    return applied;
}

/*
void RocketSettings::OnHeadTrackingEnabledToggled(bool enabled)
{
    if(enabled)
    {
        ui_.headTrackingEnabled->setText("Enabled");
        ui_.frameHeadTracking->show();
    }
    else
    {
        ui_.headTrackingEnabled->setText("Disabled");
        ui_.headTrackingEnabled->hide();
    }
}
*/

void RocketSettings::Open()
{
    if (plugin_->IsConnectedToServer())
    {
        plugin_->Notifications()->ShowSplashDialog("Cannot edit settings while connected to a online world, disconnect first.", ":/images/icon-exit.png");
        return;
    }
    if (!proxy_)
        InitUi();

    plugin_->HideCenterContent();
    QTimer::singleShot(300, this, SLOT(DelayedShow()));
    
    connect(plugin_, SIGNAL(SceneResized(const QRectF&)), this, SLOT(OnSceneResized(const QRectF&)), Qt::UniqueConnection);
}

void RocketSettings::DelayedShow()
{
    proxy_->hide();

    QRectF sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
    QPointF pos(((sceneRect.width()-220)/2.0)-(proxy_->size().width()/2.0), 0);
    if (pos.x() < 0) pos.setX(0);
    proxy_->widget_->setFixedHeight(sceneRect.height());
    proxy_->Animate(proxy_->size(), pos, -1.0, sceneRect, RocketAnimations::AnimateUp);
}

void RocketSettings::Close(bool saveChanges, bool restoreMainView)
{
    if (!proxy_)
        return;

    if (saveChanges)
        SaveSettings();

    proxy_->Hide();
    if (restoreMainView)
        plugin_->ShowCenterContent();
        
    disconnect(plugin_, SIGNAL(SceneResized(const QRectF&)), this, SLOT(OnSceneResized(const QRectF&)));
}

bool RocketSettings::IsVisible()
{
    if (proxy_)
        return proxy_->isVisible();
    return false;
}

void RocketSettings::ToggleSettingsWidget()
{
    if (plugin_->IsConnectedToServer())
    {
        plugin_->Notifications()->ShowSplashDialog("Cannot edit settings while connected to a online world, disconnect first.", ":/images/icon-exit.png");
        return;
    }
    if (!proxy_)
        InitUi();
    
    if (!proxy_->isVisible())
        Open();
    else
        Close();
}

void RocketSettings::LoadSettings()
{
    if (proxy_)
    {
        ConfigAPI &cfg = *framework_->Config();
        ConfigData config("adminotech", "clientplugin");
        ui_.boxEnableSounds->setChecked(cfg.Read(config, "enablesounds", true).toBool());
        ui_.boxShowNews->setChecked(cfg.Read(config, "showhostingnews", true).toBool());
        ui_.boxShowStats->setChecked(cfg.Read(config, "showhostingstats", true).toBool());

        ClientSettings::GraphicsMode graphics = static_cast<ClientSettings::GraphicsMode>(
            cfg.Read(config, "graphicsmode", ClientSettings::GraphicsLow).toInt());

        if (graphics == ClientSettings::GraphicsUltra)
            ui_.buttonGfxUltra->setChecked(true);
        else if (graphics == ClientSettings::GraphicsHigh)
            ui_.buttonGfxHigh->setChecked(true);
        else if (graphics == ClientSettings::GraphicsMedium)
            ui_.buttonGfxMedium->setChecked(true);
        else if (graphics == ClientSettings::GraphicsLow)
            ui_.buttonGfxLow->setChecked(true);
        else if (graphics == ClientSettings::GraphicsCustom)
        {
            ui_.buttonGfxCustom->setChecked(true);
            OnCustomGfxSettingsToggled();
        }

        // Reporting
        ui_.checkBoxDetectRendering->setChecked(cfg.Read(config, "detect rendering performance", true).toBool());
        ui_.checkBoxDetectNetwork->setChecked(cfg.Read(config, "detect network performance", true).toBool());

        // Web and Media
        ui_.spinBoxNumProcessWeb->setValue(cfg.Read(config, "numprocessesweb", 5).toInt());
        ui_.spinBoxNumProcessMedia->setValue(cfg.Read(config, "numprocessesmedia", 5).toInt());
        
        // HTTP proxy
        ui_.comboBoxProxySelection->setCurrentIndex(cfg.Read(config, "httpproxytype", 0).toInt());
        ui_.lineEditProxyHost->setText(cfg.Read(config, "httpproxyhost", "").toString());
        ui_.spinBoxProxyPort->setValue(cfg.Read(config, "httpproxyport", 0).toInt());

        // Oculus settings
        ui_.oculusEnabled->setChecked(cfg.Read(config, "oculus enabled").toBool());
        ui_.frameOculusSettings->setVisible(ui_.oculusEnabled->isChecked());
        if(ui_.oculusEnabled->isChecked())
            ui_.oculusEnabled->setText("Enabled");
        else
            ui_.oculusEnabled->setText("Disabled");

        const ConfigData uiCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_UI);

        ui_.frameOculus->setVisible(plugin_->OculusManager()->HasDevice());
        if (plugin_->OculusManager()->HasDevice())
        {
            int screenCount = QApplication::desktop()->screenCount();
            QList<int> screens;
            for (int i = 0; i < screenCount; ++i)
                screens << i;

            qSort(screens.begin(), screens.end(), OculusScreenSort);

            int windowLeft = cfg.Read(uiCfg, "window left", 0).toInt();
            foreach(int screen, screens)
            {
                QRect screenRect = QApplication::desktop()->screenGeometry(screen);

                QPushButton* button = new QPushButton();
                QString numberString = QString::number(screen + 1);
                QString resolution = QString::number(screenRect.width()) + " x " + QString::number(screenRect.height());
                button->setText(resolution + (screenRect.left() == 0 ? "\nprimary" : ""));
                button->setCheckable(true);
                button->setChecked(false);
                button->setObjectName("oculusMonitorButton" + numberString);
                button->setProperty("windowLeft", screenRect.left());
                connect(button, SIGNAL(toggled(bool)), this, SLOT(OnOculusScreenToggled(bool)), Qt::UniqueConnection);

                if (screenRect.left() == windowLeft)
                    button->setChecked(true);

                ui_.oculusMonitorsLayout->addWidget(button);
                oculusMonitors_.append(button);
            }
        }

        // CAVE settings
        config.file = "rocketcave";
        config.section = "main";

        ui_.caveEnabled->setChecked(cfg.Read(config, "enabled").toBool());
        if(!ui_.caveEnabled->isChecked())
        {
            ui_.caveEnabled->setText("Disabled");
            ui_.frameCave->hide();
        }
        else
            ui_.caveEnabled->setText("Enabled");

        int views = cfg.Read(config, "views").toInt();
        if (views == 0)
            views = 2;

        if (views == 2)
            ui_.twoMonitors->setChecked(true);
        else if(views == 3)
            ui_.threeMonitors->setChecked(true);
        else if(views == 4)
            ui_.fourMonitors->setChecked(true);

        QSize desktopSize = cfg.Read(config, "desktop size", QSize(0, 0)).toSize();
        if(desktopSize.isNull() || desktopSize == QSize(-1, -1))
            desktopSize = QApplication::desktop()->size();
        ui_.fullResolutionWidth->setValue(desktopSize.width());
        ui_.fullResolutionHeight->setValue(desktopSize.height());

        QSize viewportSize = QSize(0,0);
        if (cfg.HasKey(config, "viewport size"))
            viewportSize = cfg.Read(config, "viewport size").toSize();
        if (viewportSize.isNull())
            viewportSize = QSize(desktopSize.width() / views, desktopSize.height());
        ui_.viewResolutionWidth->setValue(viewportSize.width());
        ui_.viewResolutionHeight->setValue(viewportSize.height());

        double fov = cfg.Read(config, "horizontal fov").toDouble();
        if(fov == 0)
            fov = 45;
        ui_.fovValue->setValue(fov);

/*
#ifndef ROCKET_OPENNI
        // Not using OpenNI.
        // Hide settings.
        ui_.headTrackingEnabled->hide();
        ui_.frameHeadTracking->hide();
        ui_.labelTitleHeadTracking->hide();
#endif
*/
    }
}

void RocketSettings::DowngradeGraphicsMode()
{
    InitUi();

    ClientSettings::GraphicsMode graphics = CurrentGraphicsMode();
    if (graphics == ClientSettings::GraphicsUltra)
        ui_.buttonGfxHigh->setChecked(true);
    else if (graphics == ClientSettings::GraphicsHigh)
        ui_.buttonGfxMedium->setChecked(true);
    else if (graphics == ClientSettings::GraphicsMedium)
        ui_.buttonGfxLow->setChecked(true);
    else
        return;

    SaveSettings(true);
}

void RocketSettings::SaveSettings(bool emitSettingChanged)
{
    if (!proxy_)
        return;
        
    bool showRestartNotification = false;
    ConfigAPI &cfg = *framework_->Config();

    // UI
    ConfigData config("adminotech", "clientplugin");
    cfg.Write(config, "enablesounds", ui_.boxEnableSounds->isChecked());
    cfg.Write(config, "showhostingnews", ui_.boxShowNews->isChecked());
    cfg.Write(config, "showhostingstats", ui_.boxShowStats->isChecked());

    // Reporting
    cfg.Write(config, "detect rendering performance", ui_.checkBoxDetectRendering->isChecked());
    cfg.Write(config, "detect network performance", ui_.checkBoxDetectNetwork->isChecked());
    
    // Web and Media
    cfg.Write(config, "numprocessesweb", ui_.spinBoxNumProcessWeb->value());
    cfg.Write(config, "numprocessesmedia", ui_.spinBoxNumProcessMedia->value());

    // HTTP Proxy
    QString currentProxyHost = cfg.Read(config, "httpproxyhost", "").toString().trimmed();
    int curentProxyPort = cfg.Read(config, "httpproxyport", 0).toInt();
    if (currentProxyHost != ui_.lineEditProxyHost->text().trimmed() || curentProxyPort != ui_.spinBoxProxyPort->value())
        showRestartNotification = true;
    cfg.Write(config, "httpproxytype", ui_.comboBoxProxySelection->currentIndex());
    cfg.Write(config, "httpproxyhost", ui_.lineEditProxyHost->text().trimmed());
    cfg.Write(config, "httpproxyport", ui_.spinBoxProxyPort->value());
    ApplyHttpProxy();

    // Graphics
    ClientSettings::GraphicsMode graphicsLast = static_cast<ClientSettings::GraphicsMode>(
        cfg.Read(config, "graphicsmode", ClientSettings::GraphicsNone).toInt());

    ClientSettings::GraphicsMode graphicsNow = CurrentGraphicsMode();
    cfg.Write(config, "graphicsmode", graphicsNow);

    const ConfigData renderingCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_RENDERING);

    if (graphicsLast != graphicsNow || graphicsNow == ClientSettings::GraphicsCustom)
    {
        if (graphicsLast != graphicsNow)
            showRestartNotification = true;

        if (graphicsNow == ClientSettings::GraphicsCustom)
        {
            QString vsync = ui_.boxShowVsync->isChecked() ? vsyncSupportedYes_ : vsyncSupportedNo_;
            QString antialias = ui_.comboBoxFSAA->currentText();
            QString antialiasShadow = ui_.comboBoxShadowFSAA->currentText();

            int textureSize = 256;
            int textureSizeIndex = ui_.comboBoxShadowTextureSize->currentIndex();
            if (textureSizeIndex == 0) textureSize = 256;
            else if (textureSizeIndex == 1) textureSize = 512;
            else if (textureSizeIndex == 2) textureSize = 1024;
            else if (textureSizeIndex == 3) textureSize = 2048;

            // Read current config for changed comparisons
            QString vsyncLast = cfg.Read(renderingCfg, "vsync", vsyncSupportedNo_).toString();
            QString antialiasLast = cfg.Read(renderingCfg, "antialias", 0).toString();
            QString antialiasShadowLast = cfg.Read(renderingCfg, "shadow texture antialias", 0).toString();
            int shadowQualityLast = cfg.Read(renderingCfg, "shadow quality", 0).toInt();
            int shadowSizeLast = cfg.Read(renderingCfg, "shadow texture size", 512).toInt();
            int shadowSizeCsmLast = cfg.Read(renderingCfg, "shadow texture size csm", 512).toInt();
            
            // Store new stuff
            cfg.Write(renderingCfg, "vsync", vsync);
            cfg.Write(renderingCfg, "antialias", antialias);
            cfg.Write(renderingCfg, "shadow texture antialias", antialiasShadow);
            cfg.Write(renderingCfg, "shadow quality", ui_.comboBoxShadowQuality->currentIndex());
            cfg.Write(renderingCfg, "shadow texture size", textureSize);
            cfg.Write(renderingCfg, "shadow texture size csm", textureSize);
            cfg.Write(config, "viewdistance", ui_.spinBoxViewDistance->value());

            if (vsyncLast != vsync)
                showRestartNotification = true;
            if (antialiasLast != antialias)
                showRestartNotification = true;
            if (antialiasShadowLast != antialiasShadow)
                showRestartNotification = true;
            if (shadowQualityLast != ui_.comboBoxShadowQuality->currentIndex())
                showRestartNotification = true;
            if (shadowSizeLast != textureSize || shadowSizeCsmLast != textureSize)
                showRestartNotification = true;
        }
        else if (graphicsNow == ClientSettings::GraphicsUltra)
        {
            /// @todo Should ultra use includeQuality = true for normal FSAA? (shadow still needs false!)
            QString highestSupportedFSAA = GetHighestSupportedFSAA(8, false);
            
            cfg.Write(renderingCfg, "vsync", vsyncSupportedNo_);
            cfg.Write(renderingCfg, "antialias", highestSupportedFSAA);
            cfg.Write(renderingCfg, "shadow texture antialias", highestSupportedFSAA);
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_High);
            cfg.Write(renderingCfg, "shadow texture size", 2048);       // Shadows_Low
            cfg.Write(renderingCfg, "shadow texture size csm", 2048);   // Shadows_High
            cfg.Write(config, "viewdistance", 8000);
        }
        else if (graphicsNow == ClientSettings::GraphicsHigh)
        {
            QString highestSupportedFSAA = GetHighestSupportedFSAA(4, false);
            
            cfg.Write(renderingCfg, "vsync", vsyncSupportedNo_);
            cfg.Write(renderingCfg, "antialias", highestSupportedFSAA);
            cfg.Write(renderingCfg, "shadow texture antialias", highestSupportedFSAA);
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_High);
            cfg.Write(renderingCfg, "shadow texture size", 2048);       // Shadows_Low
            cfg.Write(renderingCfg, "shadow texture size csm", 1024);   // Shadows_High
            cfg.Write(config, "viewdistance", 6000);
        }
        else if (graphicsNow == ClientSettings::GraphicsMedium)
        {
            QString highestSupportedFSAA = GetHighestSupportedFSAA(2, false);

            cfg.Write(renderingCfg, "vsync", vsyncSupportedNo_);
            cfg.Write(renderingCfg, "antialias", highestSupportedFSAA);
            cfg.Write(renderingCfg, "shadow texture antialias", "0");
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_High);
            cfg.Write(renderingCfg, "shadow texture size", 512);       // Shadows_Low
            cfg.Write(renderingCfg, "shadow texture size csm", 512);   // Shadows_High
            cfg.Write(config, "viewdistance", 4000);
        }
        else if (graphicsNow == ClientSettings::GraphicsLow)
        {
            cfg.Write(renderingCfg, "vsync", vsyncSupportedNo_);
            cfg.Write(renderingCfg, "antialias", "0");
            cfg.Write(renderingCfg, "shadow texture antialias", "0");
            cfg.Write(renderingCfg, "shadow quality", OgreRenderer::Renderer::Shadows_Off);
            cfg.Write(renderingCfg, "shadow texture size", 512);       // Does not matter, shadows disabled
            cfg.Write(renderingCfg, "shadow texture size csm", 512);   // Does not matter, shadows disabled
            cfg.Write(config, "viewdistance", 2000);
        }
    }
    
    // Oculus
    bool currentOculusEnabled = cfg.Read(config, "oculus enabled", false).toBool();
    if (currentOculusEnabled != ui_.oculusEnabled->isChecked())
        showRestartNotification = true;

    cfg.Write(config, "oculus enabled", ui_.oculusEnabled->isChecked());

    const ConfigData uiCfg(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_UI);

    int windowLeft = cfg.Read(uiCfg, "window left", 0).toInt();
    if (ui_.oculusEnabled->isChecked())
    {
        foreach(QPushButton* button, oculusMonitors_)
        {
            if (!button)
                continue;

            if (button->isChecked())
            {
                int wleft = button->property("windowLeft").toInt();
                if (wleft != windowLeft)
                {
                    cfg.Write(uiCfg, "window left", wleft);
                    showRestartNotification = true;
                }
                break;
            }
        }

        // Set vsync frequency
        cfg.Write(renderingCfg, "vsyncFrequency", 75);
    }
    else
    {
        if (currentOculusEnabled)
            cfg.Write(uiCfg, "window left", 0);
    }

    // Update fps limit to config and runtime.
    int fpsLimit = (graphicsNow == ClientSettings::GraphicsCustom ? ui_.spinBoxTargetFPS->value() : 60);
    if (ui_.oculusEnabled->isChecked())
        fpsLimit = 0;

    if (fpsLimit < 0) fpsLimit = 0;
    cfg.Write(config, "fps target", fpsLimit);
    if ((int)framework_->App()->TargetFpsLimit() != fpsLimit)
        framework_->App()->SetTargetFpsLimit(static_cast<double>(fpsLimit));

    // CAVE
    config.file = "rocketcave";
    config.section = "main";

    cfg.Write(config, "enabled", ui_.caveEnabled->isChecked());
    if(ui_.twoMonitors->isChecked())
        cfg.Write(config, "views", 2);
    else if(ui_.threeMonitors->isChecked())
        cfg.Write(config, "views", 3);
    else if(ui_.fourMonitors->isChecked())
        cfg.Write(config, "views", 4);
    cfg.Write(config, "desktop size", QSize(ui_.fullResolutionWidth->value(), ui_.fullResolutionHeight->value()));
    cfg.Write(config, "viewport size", QSize(ui_.viewResolutionWidth->value(), ui_.viewResolutionHeight->value()));
    cfg.Write(config, "horizontal fov", ui_.fovValue->value());

    if (emitSettingChanged)
    {
        EmitSettings();

        if (showRestartNotification)
        {
            if (plugin_->OculusManager()->IsEnabled())
                framework_->App()->quit();
            else
                plugin_->Notifications()->ShowSplashDialog("You need to restart Rocket for all the settings to be applied", ":/images/icon-settings2.png");
        }
    }
}

ClientSettings::GraphicsMode RocketSettings::CurrentGraphicsMode() const
{
    if (!proxy_)
        return static_cast<ClientSettings::GraphicsMode>(framework_->Config()->Read(
            "adminotech", "clientplugin", "graphicsmode", ClientSettings::GraphicsLow).toInt());

    ClientSettings::GraphicsMode graphics = ClientSettings::GraphicsLow;
    if (ui_.buttonGfxUltra->isChecked()) 
        graphics = ClientSettings::GraphicsUltra;
    else if (ui_.buttonGfxHigh->isChecked())
        graphics = ClientSettings::GraphicsHigh;
    else if (ui_.buttonGfxMedium->isChecked())
        graphics = ClientSettings::GraphicsMedium;
    else if (ui_.buttonGfxLow->isChecked())
        graphics = ClientSettings::GraphicsLow;
    else if (ui_.buttonGfxCustom->isChecked())
        graphics = ClientSettings::GraphicsCustom;
    return graphics;
}

int RocketSettings::ViewDistance() const
{
    int viewDistance = -1;

    // First try the config value
    if (framework_->Config()->HasKey("adminotech", "clientplugin", "viewdistance"))
    {
        bool ok = false;
        viewDistance = framework_->Config()->Read("adminotech", "clientplugin", "viewdistance").toInt(&ok);
        if (viewDistance <= 0 || !ok)
            viewDistance = -1;
    }

    // Set it from the current graphics setting
    if (viewDistance < 0)
    {
        ClientSettings::GraphicsMode graphics = CurrentGraphicsMode();
        if (graphics == ClientSettings::GraphicsUltra)
            viewDistance = 8000;
        else if (graphics == ClientSettings::GraphicsHigh)
            viewDistance = 6000;
        else if (graphics == ClientSettings::GraphicsMedium)
            viewDistance = 4000;
        else
            viewDistance = 2000;
    }

    // Fallback
    if (viewDistance < 0)
        viewDistance = 2000;
    return viewDistance;
}
