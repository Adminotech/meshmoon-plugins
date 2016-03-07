/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketReporter.h"
#include "RocketPlugin.h"
#include "RocketMenu.h"
#include "RocketNotifications.h"
#include "RocketSettings.h"
#include "MeshmoonBackend.h"
#include "MeshmoonUser.h"
#include "ui_RocketSupportRequestDialog.h"

#include "Framework.h"
#include "Application.h"
#include "SystemInfo.h"
#include "LoggingFunctions.h"
#include "ConfigAPI.h"
#include "FrameAPI.h"
#include "AssetAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"
#include "EC_Mesh.h"
#include "EC_OgreCustomObject.h"
#include "EC_Terrain.h"
#include "OgreRenderingModule.h"
#include "OgreWorld.h"
#include "Renderer.h"
#include "TundraLogicModule.h"
#include "Client.h"
#include "UserConnection.h"
#ifdef PROFILING
#include "Profiler.h"
#include "DebugStats.h"
#include "TimeProfilerWindow.h"
#endif

#include <kNet/Network.h>
#include <kNet/UDPMessageConnection.h>

#include <Ogre.h>
#include <OgreFontManager.h>

#ifdef WIN32
#include <tchar.h>
#include <strsafe.h>
#endif

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "MemoryLeakCheck.h"

/// @cond PRIVATE

namespace Meshmoon
{
    template<typename T>
    uint CountSize(Ogre::MapIterator<T> iter)
    {
        uint count = 0;
        while(iter.hasMoreElements())
        {
            ++count;
            iter.getNext();
        }
        return count;
    }

    static QString ReadOgreManagerStatus(Ogre::ResourceManager &manager)
    {
        QString budget = QString::fromStdString(kNet::FormatBytes((u64)manager.getMemoryBudget()));
        QString usage = QString::fromStdString(kNet::FormatBytes((u64)manager.getMemoryUsage()));
        if (budget.lastIndexOf(".") >= budget.lastIndexOf(" ")-3) budget.insert(budget.lastIndexOf(" "), "0");
        if (budget.endsWith(" B")) budget += " ";
        if (usage.lastIndexOf(".") >= usage.lastIndexOf(" ")-3) usage.insert(usage.lastIndexOf(" "), "0");
        if (usage.endsWith(" B")) usage += " ";

        Ogre::ResourceManager::ResourceMapIterator iter = manager.getResourceIterator();
        return QString("Budget: %1    Usage: %2     Resource count: %3").arg(budget, 12).arg(usage, 12).arg(CountSize(iter), 6);
    }
}

// SystemManufacturer and SystemProductName functions
namespace
{
#ifdef WIN32
    QString ReadHardwareConfigFromRegistry(const wchar_t *key)
    {
        TCHAR szKey[256] = TEXT("SYSTEM\\HardwareConfig\\Current\\");

        HKEY hKey;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
            return "";

        DWORD dwSize = 256;
        TCHAR szData[256];
        DWORD dwType;
        // RegGetValue doesn't exist on XP so use RegQueryValueEx for now, even if don't officially support XP.
        //if (RegGetValue(HKEY_LOCAL_MACHINE, szKey, key, RRF_RT_ANY, NULL, (PVOID)&szData, &dwSize) != ERROR_SUCCESS)
        if (RegQueryValueEx(hKey, key, NULL, &dwType, (BYTE*)szData, &dwSize) != ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            return "";
        }

        RegCloseKey(hKey);
        return WStringToQString(szData);
    }
#endif

    QString SystemManufacturer()
    {
#ifdef WIN32
        return ReadHardwareConfigFromRegistry(L"SystemManufacturer");
#elif defined(__APPLE__)
        return "Apple Inc.";
#else
        return "Unknown";
#endif
    }

    QString SystemProductName()
    {
#ifdef WIN32
        return ReadHardwareConfigFromRegistry(L"SystemProductName");
#elif defined(__APPLE__)
        QString result("Unknown");
        size_t len = 0;
        sysctlbyname("hw.model", NULL, &len, NULL, 0);
        if (len)
        {
            char *model = new char[len*sizeof(char) + 1];
            sysctlbyname("hw.model", model, &len, NULL, 0);
            result = model;
            delete model;
        }

        return result;
#else
        return "Unknown";
#endif
    }
}

/// @endcond

RocketReporter::RocketReporter(RocketPlugin *plugin) :
    plugin_(plugin),
    LC_("[RocketReporter]: "),
    detectRenderingPerf_(true),
    detectNetworkPerf_(true)
{
    Reset();

    QMenu *helpMenu = plugin_->Menu()->TopLevelMenu("Help");
    if (helpMenu)
    {
        helpMenu->addSeparator();
        QAction *act = helpMenu->addAction(QIcon(":/images/icon-envelope-32x32.png"), tr("Send Support Request..."));
        connect(act, SIGNAL(triggered()), SLOT(ShowSupportRequestDialog()));
    }

    connect(plugin_, SIGNAL(ConnectionStateChanged(bool)), SLOT(OnConnectionStateChanged(bool)));
    connect(plugin_->Settings(), SIGNAL(SettingsApplied(const ClientSettings*)), this, SLOT(OnSettingsApplied(const ClientSettings*)));
}

RocketReporter::~RocketReporter()
{
    if (supportDialog_)
        supportDialog_->close();
}

void RocketReporter::Reset()
{
    tProfile_ = 0.0f,
    numFpsLow_ = 0;
    numRttHigh_ = 0;
    renderingPerformanceWarningShowed_ = false;
    networkPerformanceWarningShowed_ = false;
}

void RocketReporter::OnSettingsApplied(const ClientSettings *settings)
{
    detectRenderingPerf_ = settings->detectRenderingPerformance;
    detectNetworkPerf_ = settings->detectNetworkPerformance;
}

void RocketReporter::OnShowRenderingPerformanceInfo()
{
    QString adminText = "";
    if (plugin_->User()->PermissionLevel() > Meshmoon::Basic)
        adminText = QString("<b>What can I do as a world author?</b><ul>") +
            "<li>You can inspect real time performance information with our built in profiler from <b>Tools > Profiler</b> to try to find out the cause.</li>" +
            "<li>Please see our build tools for the <b>Scene Optimizer</b> that can optimize your worlds 3D assets.</li></ul>";

    QMessageBox *dialog = plugin_->Notifications()->ShowSplashDialog(
        QString("<b>What can cause poor rendering performance?</b><ul>") +
        "<li>Your CPU and/or graphics card cannot handle the world you are currently in.</li>" +
        "<li>The current world might have too heavy 3D assets and/or scripts for your computer.</li></ul>" +
        "<b>What can this problem cause?</b><ul>" +
        "<li>All rendering/movement can become choppy and can cause the whole application to be unresponsive.</li>" +
        "<li>Voice chat can have problems playing incoming audio and sending your audio at a steady rate. Expect choppy audio playback.</li></ul>" +
        "<b>How can I resolve these problems?</b><ul>" +
        "<li>You should try downgrading your graphics settings from <b>Session > Settings</b>.</li>" +
        "<li>You can contact the world administrators and let them know about your problem.</li></ul>" +
        adminText
    );
    dialog->setFont(QFont("Arial", 12));
}

void RocketReporter::OnShowNetworkPerformanceInfo()
{
    QMessageBox *dialog = plugin_->Notifications()->ShowSplashDialog(
        QString("<b>What can cause poor network performance?</b><ul>") +
        "<li>Your network connection cannot handle the traffic that is produced by the world you are currently in.</li>" +
        "<li>Your network connection has high latency.</li>" +
        "<li>You are on a unstable 3G/4G/WLAN network that is cutting your connection.</li></ul>" +
        "<b>What can this problem cause?</b><ul>" +
        "<li>All real time object movement can become choppy and can cause the whole application to be unresponsive.</li></ul>" +
        "<b>How can I resolve these problems?</b><ul>"
        "<li>There is very little Rocket can do to help with this. You should try to get to a better network connection or contact your ISP.</li></ul>"
        );
    dialog->setFont(QFont("Arial", 12));
}

void RocketReporter::OnConnectionStateChanged(bool connected)
{
#ifdef PROFILING
    if (connected && (detectRenderingPerf_ || detectNetworkPerf_))
    {
        Reset();
        connect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
    }
    else
        disconnect(plugin_->GetFramework()->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
#endif

    // Reset world information.
    if (!connected)
    {
        loginUrl_ = QUrl();
        currentServer_.Reset();
    }
}

void RocketReporter::OnUpdate(float frametime)
{
    // Don't profile if active asset transfers.
    if (!detectRenderingPerf_ && !detectNetworkPerf_)
        return;
    if (plugin_->GetFramework()->Asset()->NumCurrentTransfers() > 0)
        return;

#ifdef PROFILING
    tProfile_ += frametime;

    if (tProfile_ >= 1.0f)
    {
        tProfile_ = 0.0f;
               
        // Rendering. If FPS is < 25 for 60 seconds, show a warning.
        if (detectRenderingPerf_ && !renderingPerformanceWarningShowed_)
        {
            // Don't do profiling if the application (aka main window) is not active
            // as it will drop the Tundra main loop FPS to half, so we cant trust the below measurements.
            if (!plugin_->GetFramework()->App()->IsActive())
            {
                numFpsLow_ = 0;
                numRttHigh_ = 0;
                return;
            }
            
            DebugStatsModule *debugModule = plugin_->GetFramework()->GetModule<DebugStatsModule>();
            bool profilerVisible = (debugModule && debugModule->ProfilerWindow() && debugModule->ProfilerWindow()->isVisible());
            if (!profilerVisible)
            {
                static tick_t timerFrequency = GetCurrentClockFreq();
                static tick_t timePrev = GetCurrentClockTime();
                tick_t timeNow = GetCurrentClockTime();
                double msecsOccurred = (double)((timeNow - timePrev) * 1000.0 / timerFrequency);
                timePrev = timeNow;

                Profiler &profiler = *plugin_->GetFramework()->GetProfiler();
                ProfilerNode *processFrameNode = dynamic_cast<ProfilerNode*>(profiler.FindBlockByName("Framework_ProcessOneFrame"));
                if (processFrameNode)
                {
                    int numFrames = std::max<int>(processFrameNode->num_called_custom_, 1);
                    float fps = (float)(numFrames * 1000.f / msecsOccurred);
                    if (fps < 25.0f)
                    {
                        numFpsLow_++;
                        if (numFpsLow_ >= 60)
                        {
                            renderingPerformanceWarningShowed_ = true;

                            RocketNotificationWidget *notifWidget = plugin_->Notifications()->ShowNotification("Rocket seems to be suffering from poor rendering performance", ":/images/icon-cauge-low-32x32.png");
                            notifWidget->SetWarningTheme();

                            QPushButton *button = notifWidget->AddButton(false, "What does this mean?");
                            connect(button, SIGNAL(clicked()), this, SLOT(OnShowRenderingPerformanceInfo()));

                            // Check if we can auto downgrade graphics settings.
                            ClientSettings settings = plugin_->Settings()->CurrentSettings();
                            if (settings.graphicsMode < ClientSettings::GraphicsLow && settings.graphicsMode > ClientSettings::GraphicsCustom)
                            {
                                QString nextModeText = " Settings";
                                if (settings.graphicsMode == ClientSettings::GraphicsUltra)
                                    nextModeText = " to High detail";
                                else if (settings.graphicsMode == ClientSettings::GraphicsHigh)
                                    nextModeText = " to Medium detail";
                                else if (settings.graphicsMode == ClientSettings::GraphicsMedium)
                                    nextModeText = " to Low detail";
                                button = notifWidget->AddButton(true, "Downgrade rendering" + nextModeText);
                                connect(button, SIGNAL(clicked()), plugin_->Settings(), SLOT(DowngradeGraphicsMode()));
                            }
                        }
                    }
                    else
                        numFpsLow_ = 0;

                    // Ugh, node/tree/profiler->ResetValues() does not work
                    processFrameNode->num_called_custom_ = 0;
                    processFrameNode->total_custom_ = 0;
                    processFrameNode->custom_elapsed_min_ = 1e9;
                    processFrameNode->custom_elapsed_max_ = 0;
                }
            }
        }

        // Networking. If RTT is >= 200 for 60 seconds, show a warning.
        if (detectNetworkPerf_ && !networkPerformanceWarningShowed_)
        {
            TundraLogicModule *tundraLogic = plugin_->GetFramework()->GetModule<TundraLogicModule>();
            kNet::MessageConnection *connection = (tundraLogic && tundraLogic->GetClient().get() ? tundraLogic->GetClient()->GetConnection() : 0);
            if (connection && connection->GetSocket())
            {
                float rtt = connection->RoundTripTime();
                if (rtt >= 200.0f)
                {
                    numRttHigh_++;
                    if (numRttHigh_ >= 60)
                    {
                        networkPerformanceWarningShowed_ = true;

                        RocketNotificationWidget *notifWidget = plugin_->Notifications()->ShowNotification("Rocket seems to be suffering from poor network performance", ":/images/icon-cauge-low-32x32.png");
                        notifWidget->SetWarningTheme();

                        QPushButton *button = notifWidget->AddButton(false, "What does this mean?");
                        connect(button, SIGNAL(clicked()), this, SLOT(OnShowNetworkPerformanceInfo()));
                    }
                }
                else
                    numRttHigh_ = 0;
            }
        }
    }
#endif
}

void RocketReporter::SetServerInfo(const QUrl &loginUrl, const Meshmoon::Server &serverInfo)
{
    loginUrl_ = loginUrl;
    currentServer_ = serverInfo;
}

void RocketReporter::ShowSupportRequestDialog(const QString &message)
{
    if (supportDialog_)
        return;
    if (plugin_->User()->Hash().isEmpty())
    {
        plugin_->Notifications()->ShowSplashDialog("You need to be authenticated to send a support request", ":/images/icon-exit.png");
        return;
    }

    supportDialog_ = new QWidget();
    supportDialog_->setWindowIcon(QIcon(":/images/icon-envelope-32x32.png"));
    supportDialog_->setAttribute(Qt::WA_DeleteOnClose);
#ifndef Q_WS_X11
    supportDialog_->setWindowFlags(Qt::WindowStaysOnTopHint);
#else
    supportDialog_->setWindowFlags(Qt::WindowStaysOnTopHint|Qt::X11BypassWindowManagerHint);
#endif

    Ui::RocketSupportRequestDialog ui;
    ui.setupUi(supportDialog_);
    ui.editMessage->setPlainText(message);

    connect(ui.buttonShowReport, SIGNAL(clicked()), this, SLOT(OnShowReportData()));
    connect(ui.buttonSend, SIGNAL(clicked()), this, SLOT(OnSendReport()));
    connect(ui.buttonCancel, SIGNAL(clicked()), supportDialog_, SLOT(close()), Qt::QueuedConnection);

    supportDialog_->show();

    plugin_->Notifications()->CenterToMainWindow(supportDialog_);
}

void RocketReporter::OnShowReportData()
{
    if (!supportDialog_)
        return;

    QPlainTextEdit *widget = new QPlainTextEdit();
    widget->setWindowTitle("Support Report Data Preview");
    widget->setWindowIcon(QIcon(":/images/icon-envelope-32x32.png"));
    widget->setAttribute(Qt::WA_DeleteOnClose);
#ifndef Q_WS_X11
    widget->setWindowFlags(Qt::WindowStaysOnTopHint);
#else
    widget->setWindowFlags(Qt::WindowStaysOnTopHint|Qt::X11BypassWindowManagerHint);
#endif

    widget->setStyleSheet("font-family: 'Courier New'; font-size: 13px;");
    widget->setReadOnly(true);
    widget->setPlainText(DumpEverything());
    widget->resize(1000, 750);
    widget->show();
}

void RocketReporter::OnSendReport()
{
    QPlainTextEdit *editMessage = (!supportDialog_.isNull() ? supportDialog_->findChild<QPlainTextEdit*>("editMessage") : 0); //-V623
    if (!editMessage)
        return;

    QByteArray data;
    QTextStream stream(&data);
    stream.setCodec("UTF-8");

    stream << "User Defined Message" << endl
           << "=================================================================================" << endl
           << editMessage->toPlainText().trimmed() << endl << endl
           << DumpEverything() << flush;

    if (!plugin_->Backend()->Post(Meshmoon::UrlSupportRequest, data))
    {
        plugin_->Notifications()->ShowSplashDialog("Error occurred while sending support request, please try again later.", ":/images/icon-exit.png");
        return;
    }

    supportDialog_->close();
    supportDialog_ = 0;
}

QByteArray RocketReporter::DumpEverything()
{
    QByteArray data;
    QTextStream stream(&data);

    stream << Application::FullIdentifier() << " (" << Application::Architecture() << ")" << endl << endl;

    if (plugin_ && plugin_->Settings())
        plugin_->Settings()->DumpSettings(stream);

    DumpSystemInfo(stream);
    DumpUserInfo(stream);
    DumpLoginInfo(stream);
    DumpServerInfo(stream);
    DumpNetworkInfo(stream);
    DumpTundraSceneComplexity(stream);
    DumpOgreSceneOverview(stream);
    DumpOgreSceneComplexity(stream);
    
    stream << flush;
    return data;
}

void RocketReporter::DumpSystemInfo(QTextStream &stream)
{
    stream << "System" << endl
            << "=================================================================================" << endl
            << "Manufacturer              : " << SystemManufacturer() << endl
            << "Product name              : " << SystemProductName() << endl
            << "OS                        : " << OsDisplayString() << endl
            << "CPU                       : " << ProcessorBrandName() << endl
            << "CPU details               : " << ProcessorExtendedCpuIdInfo() << endl
            << "Max. simultaneous threads : " << MaxSimultaneousThreads() << endl
            << "Memory                    : " << (TotalSystemPhysicalMemory() / 1024 / 1024) << " MB" << endl;

    Ogre::RenderSystem *renderer = Ogre::Root::getSingleton().getRenderSystem();
    if (renderer && renderer->getCapabilities())
    {
        stream
            << "GPU model                 : " << renderer->getCapabilities()->getDeviceName().c_str() << endl
            << "GPU vendor                : " << Ogre::RenderSystemCapabilities::vendorToString(renderer->getCapabilities()->getVendor()).c_str() << endl
            << "GPU driver                : " << renderer->getCapabilities()->getDriverVersion().toString().c_str() << endl;
    }
#ifndef Q_OS_LINUX
    stream  << "GPU memory                : " << (TotalVideoMemory() / 1024 / 1024) << " MB" << endl;
#endif

    stream << flush;
}

void RocketReporter::DumpUserInfo(QTextStream &stream)
{
    if (!plugin_->User()->Hash().isEmpty())
    {
        stream << endl
               << "User" << endl
               << "=================================================================================" << endl
               << "Name             : " << plugin_->User()->Name() << endl
               << "Hash             : " << plugin_->User()->Hash().left(6) + "..." << endl;
        if (plugin_->IsConnectedToServer())
            stream << "Auth level       : " << plugin_->User()->PermissionLevelString() << endl;
        stream << flush;
    }
}

void RocketReporter::DumpLoginInfo(QTextStream &stream)
{
    if (plugin_->IsConnectedToServer() && loginUrl_.isValid())
    {
        stream << endl
               << "Login" << endl
               << "=================================================================================" << endl
               << "Host             : " << CensorIP(loginUrl_.host()) << endl
               << "Port             : " << loginUrl_.port() << endl << endl;

        Meshmoon::QueryItems queryItems = loginUrl_.queryItems();
        for(int i=0; i<queryItems.size(); ++i)
            stream << "" << QString("%1").arg(QString(queryItems[i].first), -16) << " = " << queryItems[i].second << endl;
        stream << flush;
    }
}

void RocketReporter::DumpServerInfo(QTextStream &stream)
{
    if (plugin_->IsConnectedToServer() && !currentServer_.txmlId.isEmpty())
    {
        stream << endl
               << "Server" << endl
               << "=================================================================================" << endl
               << "Account          : " << currentServer_.company << endl
               << "Name             : " << currentServer_.name << endl
               << "Id               : " << currentServer_.txmlId << endl
               << "Public           : " << BoolToString(currentServer_.isPublic) << endl;
        stream << flush;
    }
}

QString RocketReporter::CensorIP(const QString &ip)
{
    if (ip.count(".") >= 2)
        return "xxx.xxx" + ip.mid(ip.indexOf(".", ip.indexOf(".")+1));
    return ip;
}

void RocketReporter::DumpNetworkInfo(QTextStream &stream)
{
    TundraLogicModule *tundraLogic = plugin_->GetFramework()->GetModule<TundraLogicModule>();
    TundraLogic::Client *client = tundraLogic != 0 ? tundraLogic->GetClient().get() : 0;
    if (!client || !client->IsConnected())
        return;

    kNet::MessageConnection *connection = client->GetConnection();
    if (!connection || !connection->GetSocket())
        return;

    kNet::UDPMessageConnection *udpConnection = dynamic_cast<kNet::UDPMessageConnection*>(connection);
            
    stream << endl
           << "Network" << endl
           << "=================================================================================" << endl
           << SocketTransportLayerToString(connection->GetSocket()->TransportLayer()).c_str() << " connection to " 
           << CensorIP(connection->RemoteEndPoint().ToString().c_str()) << " from " << connection->LocalEndPoint().ToString().c_str() << " (" << ConnectionStateToString(connection->GetConnectionState()).c_str() << ")" << endl << endl;

    stream << "Connection state       : " << (connection->Connected() ? QString("Connected") : QString("Disconnected")) << (connection->IsReadOpen() ? "  Read-open" : "") << (connection->IsWriteOpen() ? "  Write-open" : "") << endl
           << "Socket state           : " << (connection->GetSocket()->Connected() ? QString("Connected") : QString("Disconnected")) << (connection->GetSocket()->IsReadOpen() ? "  Read-open" : "") << (connection->GetSocket()->IsWriteOpen() ? "  Write-open" : "") << endl << endl
           << "Last heard time        : " << QString("%1 ms ago").arg(connection->LastHeardTime()) << endl;

    if (udpConnection)
    {
        stream << "Retransmission timeout : " << QString("%1 ms").arg(udpConnection->RetransmissionTimeout()) << endl
               << "Datagram send rate     : " << QString("%1 / sec").arg(udpConnection->DatagramSendRate()) << endl
               << "Packet loss count      : " << udpConnection->PacketLossCount() << endl
               << "Packet loss rate       : " << udpConnection->PacketLossRate() << endl;
    }
    
    stream << "Round trip time        : " << QString("%1 ms").arg(connection->RoundTripTime()) << endl;
    if (udpConnection)
    {
        stream << "       smoothed        : " << QString("%1 ms").arg(udpConnection->SmoothedRtt()) << endl
               << "       variation       : " << QString("%1 ms").arg(udpConnection->RttVariation())<< endl;
    }
    stream << endl;

    stream << "Pending messages   IN  : " << connection->NumInboundMessagesPending() << endl
           << "                   OUT : " << connection->NumOutboundMessagesPending() << " (unaccepted: " << connection->OutboundAcceptQueueSize() << ")" << endl
           << "Packets            IN  : " << QString("%1 / sec").arg(connection->PacketsInPerSec(), -8) << endl
           << "                   OUT : " << QString("%1 / sec").arg(connection->PacketsOutPerSec(), -8) << endl
           << "Messages           IN  : " << QString("%1 / sec").arg(connection->MsgsInPerSec(), -8) << endl
           << "                   OUT : " << QString("%1 / sec").arg(connection->MsgsOutPerSec(), -8) << endl
           << "Current bytes      IN  : " << QString("%1 / sec").arg(QString::fromStdString(kNet::FormatBytes(connection->BytesInPerSec())), -8) << endl
           << "                   OUT : " << QString("%1 / sec").arg(QString::fromStdString(kNet::FormatBytes(connection->BytesOutPerSec())), -8) << endl
           << "Total bytes        IN  : " << QString("%1").arg(QString::fromStdString(kNet::FormatBytes(connection->BytesInTotal())), -8) << endl
           << "                   OUT : " << QString("%1").arg(QString::fromStdString(kNet::FormatBytes(connection->BytesOutTotal())), -8) << endl;

    if (udpConnection)
    {
        stream << "Unacked datagrams  IN  : " << udpConnection->NumReceivedUnackedDatagrams() << endl
               << "                   OUT : " << udpConnection->NumOutboundUnackedDatagrams() << endl;
    }
    stream << flush;
}

QString RocketReporter::DumpOgreSceneOverview()
{
    QByteArray ogreOverview;
    QTextStream stream(&ogreOverview);
    DumpOgreSceneOverview(stream);
    return ogreOverview;
}

void RocketReporter::DumpOgreSceneOverview(QTextStream &stream)
{
    Ogre::Root *root = Ogre::Root::getSingletonPtr();
    if (!root)
    {
        LogError(LC_ + "DumpOgreSceneOverview: Ogre Root is null, cannot continue!");
        return;
    }

    stream << endl
           << "Ogre Resources" << endl
           << "=================================================================================" << endl
           << "Textures         : " << Meshmoon::ReadOgreManagerStatus(Ogre::TextureManager::getSingleton()) << endl
           << "Meshes           : " << Meshmoon::ReadOgreManagerStatus(Ogre::MeshManager::getSingleton()) << endl
           << "Materials        : " << Meshmoon::ReadOgreManagerStatus(Ogre::MaterialManager::getSingleton()) << endl
           << "Skeletons        : " << Meshmoon::ReadOgreManagerStatus(Ogre::SkeletonManager::getSingleton()) << endl
           << "Compositors      : " << Meshmoon::ReadOgreManagerStatus(Ogre::CompositorManager::getSingleton()) << endl
           << "GPU programs     : " << Meshmoon::ReadOgreManagerStatus(Ogre::HighLevelGpuProgramManager::getSingleton()) << endl
           << "Fonts            : " << Meshmoon::ReadOgreManagerStatus(Ogre::FontManager::getSingleton()) << endl;
    
    OgreRendererPtr renderer = plugin_->GetFramework()->Module<OgreRenderingModule>()->Renderer();
    if (renderer && renderer->GetActiveOgreWorld())
    {
        Ogre::SceneManager *scene = renderer->GetActiveOgreWorld()->OgreSceneManager();
        if (scene)
        {
            stream << endl
                   << "Ogre Object Instances" << endl
                   << "=================================================================================" << endl
                   << "Cameras          : " << Meshmoon::CountSize(scene->getCameraIterator()) << endl
                   << "Animations       : " << Meshmoon::CountSize(scene->getAnimationIterator()) << endl
                   << "Animation states : " << Meshmoon::CountSize(scene->getAnimationStateIterator()) << endl
                   << "Lights           : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::LightFactory::FACTORY_TYPE_NAME)) << endl
                   << "Entities         : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::EntityFactory::FACTORY_TYPE_NAME)) << endl
                   << "Manual objects   : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::ManualObjectFactory::FACTORY_TYPE_NAME)) << endl
                   << "Billboard sets   : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::BillboardSetFactory::FACTORY_TYPE_NAME)) << endl
                   << "Billboard chains : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::BillboardChainFactory::FACTORY_TYPE_NAME)) << endl
                   << "Ribbon trails    : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::RibbonTrailFactory::FACTORY_TYPE_NAME)) << endl
                   << "Particle systems : " << Meshmoon::CountSize(scene->getMovableObjectIterator(Ogre::ParticleSystemFactory::FACTORY_TYPE_NAME)) << endl;
        }
    }
    
    stream << flush;
}

QString RocketReporter::DumpOgreSceneComplexity()
{
    QByteArray ogreComplexity;
    QTextStream stream(&ogreComplexity);
    DumpOgreSceneComplexity(stream);
    return ogreComplexity;
}

void RocketReporter::DumpOgreSceneComplexity(QTextStream &stream)
{
    Scene *scene = plugin_->GetFramework()->Scene()->MainCameraScene();
    if (!scene)
        return;
    OgreRendererPtr renderer = plugin_->GetFramework()->Module<OgreRenderingModule>()->Renderer();
    if (!renderer)
        return;
    Ogre::Root *root = Ogre::Root::getSingletonPtr();
    if (!root)
    {
        LogError(LC_ + "DumpOgreSceneComplexity: Ogre Root is null, cannot continue!");
        return;
    }
    Ogre::RenderSystem* rendersys = root->getRenderSystem();
    if (!rendersys)
        return;

    uint visible_entities = renderer->GetActiveOgreWorld()->VisibleEntities().size();
    size_t batches = 0;
    size_t triangles = 0;
    float avgfps = 0.0f;

    // Get overall batchcount/trianglecount/fps data
    Ogre::RenderSystem::RenderTargetIterator rtiter = rendersys->getRenderTargetIterator();
    while(rtiter.hasMoreElements())
    {
        Ogre::RenderTarget* target = rtiter.getNext();
        // Sum batches & triangles from all rendertargets updated last frame
        batches += target->getBatchCount();
        triangles += target->getTriangleCount();
        // Get FPS only from the primary
        if (target->isPrimary())
            avgfps = floor(target->getAverageFPS() * 100.0f) / 100.0f;
    }
    
    stream << endl
           << "Ogre Scene Complexity" << endl
           << "=================================================================================" << endl
           << "Average FPS " << avgfps << (!plugin_->GetFramework()->App()->IsActive() ? "    (Application inactive, FPS dropped to half)" : "") << endl << endl
           << "Viewport" << endl
           << QString("  %1").arg("# of currently visible entities ", -45) << visible_entities << endl
           << QString("  %1").arg("# of total batches rendered last frame ", -45) << batches << endl
           << QString("  %1").arg("# of total triangles rendered last frame ", -45) << triangles << endl
           << QString("  %1").arg("# of avg. triangles per batch ", -45) << triangles / (batches ? batches : 1) << endl
           << endl;
    
    uint entities = 0;
    uint prims = 0;
    uint invisible_prims = 0;
    uint meshentities = 0;
    uint animated = 0;
    
    std::set<Ogre::Mesh*> all_meshes;
    std::set<Ogre::Mesh*> scene_meshes;
    std::set<Ogre::Mesh*> other_meshes;
    std::set<Ogre::Texture*> all_textures;
    std::set<Ogre::Texture*> scene_textures;
    std::set<Ogre::Texture*> other_textures;
    size_t scene_meshes_size = 0;
    size_t other_meshes_size = 0;
    size_t mesh_vertices = 0;
    size_t mesh_triangles = 0;
    size_t mesh_instance_vertices = 0;
    size_t mesh_instance_triangles = 0;
    size_t mesh_instances = 0;
    
    // Loop through entities to see mesh usage
    for(Scene::iterator iter = scene->begin(); iter != scene->end(); ++iter)
    {
        Entity &entity = *iter->second;
        entities++;

        EC_Terrain* terrain = entity.GetComponent<EC_Terrain>().get();
        EC_Mesh* mesh = entity.GetComponent<EC_Mesh>().get();
        EC_OgreCustomObject* custom = entity.GetComponent<EC_OgreCustomObject>().get();
        Ogre::Entity* ogre_entity = 0;
        
        // Get Ogre mesh from mesh EC
        if (mesh)
        {
            ogre_entity = mesh->GetEntity();
            if (ogre_entity)
            {
                Ogre::Mesh* ogre_mesh = ogre_entity->getMesh().get();
                scene_meshes.insert(ogre_mesh);
                GetVerticesAndTrianglesFromMesh(ogre_mesh, mesh_instance_vertices, mesh_instance_triangles);
                mesh_instances++;
                std::set<Ogre::Material*> temp_mat;
                GetMaterialsFromEntity(ogre_entity, temp_mat);
                GetTexturesFromMaterials(temp_mat, scene_textures);
            }
        }
        // Get Ogre mesh from customobject EC
        else if (custom)
        {
            ogre_entity = custom->GetEntity();
            if (ogre_entity)
            {
                Ogre::Mesh* ogre_mesh = ogre_entity->getMesh().get();
                scene_meshes.insert(ogre_mesh);
                GetVerticesAndTrianglesFromMesh(ogre_mesh, mesh_instance_vertices, mesh_instance_triangles);
                mesh_instances++;
                std::set<Ogre::Material*> temp_mat;
                GetMaterialsFromEntity(ogre_entity, temp_mat);
                GetTexturesFromMaterials(temp_mat, scene_textures);
            }
        }
        // Get Ogre meshes from terrain EC
        else if (terrain)
        {
            for(uint y=0; y<terrain->PatchHeight(); ++y)
            {
                for(uint x=0; x<terrain->PatchWidth(); ++x)
                {
                    Ogre::SceneNode *node = terrain->GetPatch(static_cast<int>(x), static_cast<int>(y)).node;
                    if (!node)
                        continue;
                    if (!node->numAttachedObjects())
                        continue;
                    ogre_entity = dynamic_cast<Ogre::Entity*>(node->getAttachedObject(0));
                    if (ogre_entity)
                    {
                        Ogre::Mesh* ogre_mesh = ogre_entity->getMesh().get();
                        scene_meshes.insert(ogre_mesh);
                        GetVerticesAndTrianglesFromMesh(ogre_mesh, mesh_instance_vertices, mesh_instance_triangles);
                        mesh_instances++;
                        std::set<Ogre::Material*> temp_mat;
                        GetMaterialsFromEntity(ogre_entity, temp_mat);
                        GetTexturesFromMaterials(temp_mat, scene_textures);
                    }
                }
            }
        }
        
        {
            if (ogre_entity)
                meshentities++;
        }
        if (entity.GetComponent("EC_AnimationController").get())
            animated++;
    }
    
    // Go through all meshes and see which of them are in the scene
    Ogre::ResourceManager::ResourceMapIterator iter = Ogre::MeshManager::getSingleton().getResourceIterator();
    while(iter.hasMoreElements())
    {
        Ogre::ResourcePtr resource = iter.getNext();
        if (!resource->isLoaded())
            continue;
        Ogre::Mesh* mesh = dynamic_cast<Ogre::Mesh*>(resource.get());
        if (mesh)
            all_meshes.insert(mesh);
        if (scene_meshes.find(mesh) == scene_meshes.end())
            other_meshes.insert(mesh);
    }
    
    stream << "Scene" << endl
           << QString("  %1").arg("# of entities in the scene ", -45) << entities << endl
           << QString("  %1").arg("# of prims with geometry in the scene ", -45) << prims << endl
           << QString("  %1").arg("# of invisible prims in the scene ", -45) << invisible_prims << endl
           << QString("  %1").arg("# of mesh entities in the scene ", -45) << meshentities << endl
           << QString("  %1").arg("# of animated entities in the scene ", -45) << animated << endl
           << endl;
    
    // Count total vertices/triangles per mesh
    std::set<Ogre::Mesh*>::iterator mi = all_meshes.begin();
    while(mi != all_meshes.end())
    {
        Ogre::Mesh* mesh = *mi;
        GetVerticesAndTrianglesFromMesh(mesh, mesh_vertices, mesh_triangles);
        ++mi;
    }
    // Count scene/other mesh byte sizes
    mi = scene_meshes.begin();
    while(mi != scene_meshes.end())
    {
        scene_meshes_size += (*mi)->getSize();
        ++mi;
    }
    mi = other_meshes.begin();
    while(mi != other_meshes.end())
    {
        other_meshes_size += (*mi)->getSize();
        ++mi;
    }
    
    stream << "Ogre Meshes" << endl
           << QString("  %1").arg("# of loaded meshes ", -45) << all_meshes.size() << endl
           << QString("  %1").arg("# of unique meshes in scene ", -45) << scene_meshes.size() << endl
           << QString("  %1").arg("# of mesh instances in scene ", -45) << mesh_instances << endl
           << QString("  %1").arg("# of vertices in the meshes ", -45) << mesh_vertices << endl
           << QString("  %1").arg("# of triangles in the meshes ", -45) << mesh_triangles << endl
           << QString("  %1").arg("# of vertices in the scene ", -45) << mesh_instance_vertices << endl
           << QString("  %1").arg("# of triangles in the scene ", -45) << mesh_instance_triangles << endl
           << QString("  %1").arg("# of avg. triangles in the scene per mesh ", -45) << mesh_instance_triangles / (mesh_instances ? mesh_instances : 1) << endl
           << QString("  %1").arg("Total mesh data size ", -45) << "(" << scene_meshes_size / 1024 << " KBytes scene) + (" << other_meshes_size / 1024 << " KBytes other)" << endl
           << endl;
    
    // Go through all textures and see which of them are in the scene
    Ogre::ResourceManager::ResourceMapIterator tex_iter = ((Ogre::ResourceManager*)Ogre::TextureManager::getSingletonPtr())->getResourceIterator();
    while(tex_iter.hasMoreElements())
    {
        Ogre::ResourcePtr resource = tex_iter.getNext();
        if (!resource->isLoaded())
            continue;
        Ogre::Texture* texture = dynamic_cast<Ogre::Texture*>(resource.get());
        if (texture)
        {
            all_textures.insert(texture);
            if (scene_textures.find(texture) == scene_textures.end())
                other_textures.insert(texture);
        }
    }
    
    // Count total/scene texture byte sizes and amount of total pixels
    size_t scene_tex_size = 0;
    size_t total_tex_size = 0;
    //size_t other_tex_size = 0; /**< @todo do we want to report this? */
    size_t scene_tex_pixels = 0;
    size_t total_tex_pixels = 0;
    //size_t other_tex_pixels = 0;
    std::set<Ogre::Texture*>::iterator ti = scene_textures.begin();
    while(ti != scene_textures.end())
    {
        scene_tex_size += (*ti)->getSize();
        scene_tex_pixels += (*ti)->getWidth() * (*ti)->getHeight();
        ++ti;
    }
    ti = all_textures.begin();
    while(ti != all_textures.end())
    {
        total_tex_size += (*ti)->getSize();
        total_tex_pixels += (*ti)->getWidth() * (*ti)->getHeight();
        ++ti;
    }
    //ti = other_textures.begin();
    //while(ti != other_textures.end())
    //{
    //    other_tex_size += (*ti)->getSize();
    //    other_tex_pixels += (*ti)->getWidth() * (*ti)->getHeight();
    //    ++ti;
    //}
    
    // Sort textures into categories
    uint scene_tex_categories[5];
    uint other_tex_categories[5];
    for(uint i = 0; i < 5; ++i)
    {
        scene_tex_categories[i] = 0;
        other_tex_categories[i] = 0;
    }
    
    ti = scene_textures.begin();
    while(ti != scene_textures.end())
    {
        size_t dimension = std::max((*ti)->getWidth(), (*ti)->getHeight());
        if (dimension > 2048)
            scene_tex_categories[0]++;
        else if (dimension > 1024)
            scene_tex_categories[1]++;
        else if (dimension > 512)
            scene_tex_categories[2]++;
        else if (dimension > 256)
            scene_tex_categories[3]++;
        else
            scene_tex_categories[4]++;
        ++ti;
    }
    
    ti = other_textures.begin();
    while(ti != other_textures.end())
    {
        size_t dimension = std::max((*ti)->getWidth(), (*ti)->getHeight());
        if (dimension > 2048)
            other_tex_categories[0]++;
        else if (dimension > 1024)
            other_tex_categories[1]++;
        else if (dimension > 512)
            other_tex_categories[2]++;
        else if (dimension > 256)
            other_tex_categories[3]++;
        else
            other_tex_categories[4]++;
        ++ti;
    }
    
    if (!total_tex_pixels)
        total_tex_pixels = 1;
    if (!scene_tex_pixels)
        scene_tex_pixels = 1;
    //if (!other_tex_pixels)
    //    other_tex_pixels = 1;
    
    stream << "Ogre Textures" << endl
           << QString("  %1").arg("# of loaded textures ", -45) << all_textures.size() << endl
           << QString("  %1").arg("Texture data size in scene ", -45) << scene_tex_size / 1024 << " KBytes" << endl
           << QString("  %1").arg("Texture data size total ", -45) << total_tex_size / 1024 << " KBytes" << endl
           << QString("  %1").arg("Average. bytes/pixel ", -45) 
              << "(" << floor(((float)scene_tex_size / scene_tex_pixels) * 100.0f) / 100.f << " scene) / " 
              << "(" << floor(((float)total_tex_size / total_tex_pixels) * 100.0f) / 100.f << " total)" << endl
           << QString("  %1").arg("Texture dimensions ", -45) << endl
           << QString("    %1").arg("< 2048 px ", -44) << "(" << scene_tex_categories[0] << " scene) + (" << other_tex_categories[0] << " other)" << endl
           << QString("    %1").arg("< 1024 px ", -44) << "(" << scene_tex_categories[1] << " scene) + (" << other_tex_categories[1] << " other)" << endl
           << QString("    %1").arg("<  512 px ", -44) << "(" << scene_tex_categories[2] << " scene) + (" << other_tex_categories[2] << " other)" << endl
           << QString("    %1").arg("<  256 px ", -44) << "(" << scene_tex_categories[3] << " scene) + (" << other_tex_categories[3] << " other)" << endl
           << QString("    %1").arg("<= 256 px ", -44) << "(" << scene_tex_categories[4] << " scene) + (" << other_tex_categories[4] << " other)" << endl
           << endl;
    
    stream << "Ogre Materials" << endl
           << QString("  %1").arg("# of materials total ", -45) << GetNumResources(Ogre::MaterialManager::getSingleton()) << endl;
    
    uint vertex_shaders = 0;
    uint pixel_shaders = 0;
    Ogre::ResourceManager::ResourceMapIterator shader_iter = ((Ogre::ResourceManager*)Ogre::HighLevelGpuProgramManager::getSingletonPtr())->getResourceIterator();
    while(shader_iter.hasMoreElements())
    {
        Ogre::ResourcePtr resource = shader_iter.getNext();
        // Count only loaded programs
        if (!resource->isLoaded())
            continue;
        Ogre::HighLevelGpuProgram* program = dynamic_cast<Ogre::HighLevelGpuProgram*>(resource.get());
        if (program)
        {
            Ogre::GpuProgramType type = program->getType();
            if (type == Ogre::GPT_VERTEX_PROGRAM)
                vertex_shaders++;
            if (type == Ogre::GPT_FRAGMENT_PROGRAM)
                pixel_shaders++;
        }
    }
    stream << QString("  %1").arg("# of vertex shaders total ", -45) << vertex_shaders << endl
           << QString("  %1").arg("# of pixel shaders total ", -45) << pixel_shaders << endl
           << endl;
           
    stream << "Ogre Skeletons" << endl
           << QString("  %1").arg("# of loaded skeletons ", -45) << GetNumResources(Ogre::SkeletonManager::getSingleton()) << endl;
    stream << flush;
}

uint RocketReporter::GetNumResources(Ogre::ResourceManager& manager)
{
    uint count = 0;
    Ogre::ResourceManager::ResourceMapIterator iter = manager.getResourceIterator();
    while(iter.hasMoreElements())
    {
        Ogre::ResourcePtr resource = iter.getNext();
        if (!resource->isLoaded())
            continue;
        count++;
    }
    return count;
}

void RocketReporter::GetVerticesAndTrianglesFromMesh(Ogre::Mesh* mesh, size_t & vertices, size_t & triangles)
{
    // Count total vertices/triangles for each mesh instance
    for(unsigned short  i = 0; i < mesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh* submesh = mesh->getSubMesh(i);
        if (submesh)
        {
            Ogre::VertexData* vtx = submesh->vertexData;
            Ogre::IndexData* idx = submesh->indexData;
            if (vtx)
                vertices += vtx->vertexCount;
            if (idx)
                triangles += idx->indexCount / 3;
        }
    }
}

void RocketReporter::GetMaterialsFromEntity(Ogre::Entity* entity, std::set<Ogre::Material*>& dest)
{
    for(uint i = 0; i < entity->getNumSubEntities(); ++i)
    {
        Ogre::SubEntity* subentity = entity->getSubEntity(i);
        if (subentity)
        {
            Ogre::Material* mat = subentity->getMaterial().get();
            if (mat)
                dest.insert(mat);
        }
    }
}

void RocketReporter::GetTexturesFromMaterials(const std::set<Ogre::Material*>& materials, std::set<Ogre::Texture*>& dest)
{
    Ogre::TextureManager& texMgr = Ogre::TextureManager::getSingleton();
    
    std::set<Ogre::Material*>::const_iterator i = materials.begin();
    while(i != materials.end())
    {
        Ogre::Material::TechniqueIterator iter = (*i)->getTechniqueIterator();
        while(iter.hasMoreElements())
        {
            Ogre::Technique *tech = iter.getNext();
            Ogre::Technique::PassIterator passIter = tech->getPassIterator();
            while(passIter.hasMoreElements())
            {
                Ogre::Pass *pass = passIter.getNext();
                
                Ogre::Pass::TextureUnitStateIterator texIter = pass->getTextureUnitStateIterator();
                while(texIter.hasMoreElements())
                {
                    Ogre::TextureUnitState *texUnit = texIter.getNext();
                    std::string texName = texUnit->getTextureName();
                    Ogre::Texture* tex = dynamic_cast<Ogre::Texture*>(texMgr.getByName(texName).get());
                    if ((tex) && (tex->isLoaded()))
                        dest.insert(tex);
                }
            }
        }
        ++i;
    }
}

QString RocketReporter::DumpTundraSceneComplexity()
{
    QByteArray tundraComplexity;
    QTextStream stream(&tundraComplexity);
    DumpTundraSceneComplexity(stream);
    return tundraComplexity;
}

bool SortComponentsByCount(const QPair<size_t, QString> &pair1, const QPair<size_t, QString> &pair2)
{
    return (pair1.first > pair2.first);
}

void RocketReporter::DumpTundraSceneComplexity(QTextStream &stream)
{
    Scene *scene = plugin_->GetFramework()->Scene()->MainCameraScene();
    if (!scene)
        return;
        
    uint replicated = 0;
    uint local = 0;
    uint temporary = 0;
    uint visual = 0;
    uint empty = 0;
    
    const Scene::EntityMap &entities = scene->Entities();    
    for(Scene::EntityMap::const_iterator iter = entities.begin(); iter != entities.end(); ++iter)
    {
        Entity *ent = iter->second.get();
        if (!ent)
            continue;

        // Replicated, local and temporary count
        if (ent->IsReplicated())
            replicated++;
        else
            local++;
        if (ent->IsTemporary())
            temporary++;
            
        // Visual entities only have Mesh and Placeable (and optionally name)
        if (ent->Components().size() == 2 && ent->GetComponent("EC_Placeable").get() && ent->GetComponent("EC_Mesh").get())
            visual++;
        else if (ent->Components().size() == 3 && ent->GetComponent("EC_Name").get() && ent->GetComponent("EC_Placeable").get() && ent->GetComponent("EC_Mesh").get())
            visual++;
        
        // Empty entities
        if (ent->Components().empty())
            empty++;
    }

    stream << endl
           << "Tundra Scene Complexity" << endl
           << "=================================================================================" << endl
           << "Entities" << endl
           << "  Total               : " << entities.size() << endl
           << "  Replicated          : " << replicated << endl
           << "  Local               : " << local << endl
           << "  Temporary           : " << temporary << endl
           << "  Visual              : " << visual << " (contains only Mesh and Placeable components)" << endl
           << "  Empty               : " << empty << " (no components)" << endl
           << "Components" << endl;

    QList<QPair<size_t, QString> > compCounts;
    foreach(const QString &compType, plugin_->GetFramework()->Scene()->ComponentTypes())
    {
        EntityList compEnts = scene->EntitiesWithComponent(compType);
        if (compEnts.empty())
            continue;

        QString compTypeClean = compType;
        if (compTypeClean.startsWith("EC_"))
            compTypeClean = compTypeClean.mid(3);
        compCounts << QPair<size_t, QString>(compEnts.size(), compTypeClean);
    }
    qSort(compCounts.begin(), compCounts.end(), SortComponentsByCount);
    for (int i=0; i<compCounts.size(); ++i)
        stream << QString("  %1").arg(compCounts[i].second, -19) << " : " << compCounts[i].first << endl;
    stream << flush;
}
