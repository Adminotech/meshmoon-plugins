/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketCaveManager.h"
#include "RocketPlugin.h"
#include "RocketMenu.h"

#include "RocketCaveAdvancedWidget.h"

#include "Framework.h"
#include "Scene.h"
#include "SceneAPI.h"
#include "FrameAPI.h"
#include "UiAPI.h"
#include "InputAPI.h"
#include "ConfigAPI.h"
#include "UiMainWindow.h"
#include "Entity.h"
#include "EC_Camera.h"
#include "EC_Placeable.h"
#include "OgreRenderingModule.h"
#include "RenderWindow.h"
#include "Application.h"

#ifdef ROCKET_OPENNI
#include "RocketOpenNIPlugin.h"
#endif

#include <OgreCommon.h>
#include <OgreRoot.h>
#include <OgreOverlayManager.h>
#include <OgreOverlayContainer.h>
#include <OgreWorld.h>
#include <OgreRenderWindow.h>
#include <OgreHardwarePixelBuffer.h>

#include <QKeySequence>
#include <QMessageBox>

#include "MemoryLeakCheck.h"

RocketCaveManager::ViewSettings RocketCaveManager::viewSettings_;
RocketCaveManager::SceneData RocketCaveManager::sceneData_;
OgreRenderer::Renderer *RocketCaveManager::renderer_;

RocketCaveManager::RocketCaveManager(RocketPlugin* plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    widget_(0),
    caveAction_(0),
    caveEnabled(false),
    settingsLoadedFromCaveIni_(false),
#ifdef ROCKET_OPENNI
    headTrackingEnabled(false),
    user_(0),
    openNi_(0)
#else
    headTrackingEnabled(false)
#endif
{
    if(framework_->IsHeadless())
        return;

#ifdef ROCKET_OPENNI
    // Get OpenNI plugin
    openNi_ = framework_->Module<RocketOpenNIPlugin>();
#endif

    // Get renderer
    OgreRenderer::OgreRenderingModule* renderingModule = framework_->GetModule<OgreRenderer::OgreRenderingModule>();
    renderer_ = renderingModule->GetRenderer().get();
    if (!renderer_)
        return;
   
    // Hook to connection changed
    connect(plugin_, SIGNAL(ConnectionStateChanged(bool)), this, SLOT(OnConnectionStateChanged(bool)));

    // Hook to disconnect
    connect(framework_->Scene(), SIGNAL(SceneRemoved(const QString&)), this, SLOT(OnDestroyWindows(const QString&)));

    connect(framework_->Scene(), SIGNAL(SceneAdded(const QString&)), this, SLOT(OnSceneAdded(const QString&)));

    // Add widget to menu
    caveAction_ = new QAction(QIcon(":/images/icon-gear.png"), tr("CAVE Settings"), this);
    connect(caveAction_, SIGNAL(triggered()), this, SLOT(OnCAVESettingsOpen()));
    
    plugin_->Menu()->AddAction(caveAction_, "Utilities");
    caveAction_->setVisible(false);

    if (framework_->HasCommandLineParameter("--rocketCaveConfig"))
    {
        const QStringList& params = framework_->CommandLineParameters("--rocketCaveConfig");
        if (params.size() >= 1)
            LoadCaveSettings(params[0]);
    }
    else
        LoadCaveSettings();
}

RocketCaveManager::~RocketCaveManager()
{
    OnDestroyWindows(sceneData_.sceneName);

    sceneData_.Destroy(framework_);

    disconnect(renderer_, SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)));
    disconnect(plugin_, SIGNAL(ConnectionStateChanged(bool)), this, SLOT(OnConnectionStateChanged(bool)));
    disconnect(framework_->Scene(), SIGNAL(SceneRemoved(const QString&)), this, SLOT(OnDestroyWindows(const QString&)));
    disconnect(framework_->Scene(), SIGNAL(SceneAdded(const QString&)), this, SLOT(OnSceneAdded(const QString&)));
    disconnect(caveAction_, SIGNAL(triggered()), this, SLOT(OnCAVESettingsOpen()));

    SAFE_DELETE(widget_);
}

RocketCaveManager::ViewSettings::ViewSettings() :
    desktopSize(QApplication::desktop()->size()),
    viewCount(3),
    currentRendertarget(0),
    fov(0.0)
{
}

RocketCaveManager::SceneData::SceneData()
{
    cameraEntity = 0;
    cameraRoot = 0;
}

void RocketCaveManager::SceneData::Destroy(Framework * /*framework*/)
{
    cameraEntity = 0;
    cameraRoot = 0;
}

void RocketCaveManager::SceneData::CreateCameraRoot(Framework *framework)
{
    Ogre::Camera* original_cam = sceneData_.cameraEntity->GetComponent<EC_Camera>()->OgreCamera();
    if(!original_cam)
        return;

    // Attach node to main camera
    Ogre::SceneNode* originalNode = original_cam->getParentSceneNode();
    if(!originalNode)
        return;

    ScenePtr tundraScene = framework->Scene()->GetScene(sceneName);
    if(tundraScene)
    {
        OgreWorld* world = tundraScene->GetWorld<OgreWorld>().get();
        if(world)
        {
            try
            {
                if(cameraRoot)
                {
                    world->OgreSceneManager()->destroySceneNode(cameraRoot);
                    cameraRoot = 0;
                }
            }
            catch(...){ /* Cannot delete root node or it does not exist. */ }

            try
            {
                cameraRoot = originalNode->createChildSceneNode("CAVECameraRoot");
                cameraRoot->setPosition(Ogre::Vector3::ZERO);
                cameraRoot->setOrientation(Ogre::Quaternion::IDENTITY);
            }
            catch(...){ /* Cannot create root node. */ }
        }
    }
}

RocketCaveManager::CAVEWindow::CAVEWindow() :
    window(0),
    rendertexture(0),
    camera(0),
    nearClip(0.1f),
    farClip(2000.0f),
    type(TEXTURE),
    customMatrix(false),
    fovx(0.0),
    location(LEFT),
    index(-1),
    viewportCreated(false)
{   
    screenSize = Ogre::Vector2(viewSettings_.viewportSize.width(), viewSettings_.viewportSize.height());
    eye_position = Ogre::Vector3(0.0f);
}

void RocketCaveManager::CAVEWindow::SetViewDimensions(Ogre::Vector3 topLeft, Ogre::Vector3 bottomLeft, Ogre::Vector3 bottomRight)
{
    top_left = topLeft;
    bottom_left = bottomLeft;
    bottom_right = bottomRight;
}

void RocketCaveManager::CAVEWindow::CalculateViewDimensions()
{
    if (customMatrix)
        return;

    double dist = 2.0;

    ///field of view in the y axis.
    double aspectRatio = screenSize.x / screenSize.y;

    // Calculate horizontal and vertical fov
    double dfovx = fovx;
    double dfovy = fovx / aspectRatio;

    double dyaw = 0;

    if(location == LEFT)
        dyaw = -dfovx;
    else if(location == RIGHT)
        dyaw = dfovx;

    if(viewSettings_.viewCount == 2)
    {
        if(location == LEFT)
            dyaw = -dfovx * 0.5;
        if(location == RIGHT)
            dyaw = dfovx * 0.5;
    }

    Ogre::Quaternion rot(Ogre::Degree(dyaw), Ogre::Vector3::NEGATIVE_UNIT_Y);
    rot = Ogre::Quaternion(Ogre::Radian(0), Ogre::Vector3::UNIT_X) * rot;

    const Ogre::Vector3 dir = (Ogre::Vector3::NEGATIVE_UNIT_Z) * dist;
    const Ogre::Vector3 local_x_axis = Ogre::Vector3::UNIT_X;
    const Ogre::Vector3 local_y_axis = Ogre::Vector3::UNIT_Y;

    Ogre::Vector3 left = Ogre::Quaternion(Ogre::Degree(dfovx * 0.5), local_y_axis) * dir;
    left = left - dir;
    left.y = 0;


    Ogre::Vector3 right = Ogre::Quaternion(Ogre::Degree(dfovx * 0.5), -local_y_axis) * dir;
    right = right - dir;
    right.y = 0;

    Ogre::Vector3 up = Ogre::Quaternion(Ogre::Degree(dfovy * 0.5), local_x_axis) * dir;
    up = up - dir;
    up.x = 0;
    up.z = 0;

    Ogre::Vector3 down = Ogre::Quaternion(Ogre::Degree(dfovy * 0.5), -local_x_axis) * dir;
    down = down - dir;
    down.x = 0;
    down.z = 0;

    bottom_left = dir + left + down;
    top_left = dir + left + up;
    bottom_right = dir + right + down;

    bottom_left = rot * bottom_left;
    top_left = rot * top_left;
    bottom_right = rot * bottom_right;

    if (IsLogChannelEnabled(LogChannelDebug))
    {
        qDebug() << "RocketCave settings calculated from screen size" << endl << endl
                 << "Screen" << endl
                 << "- Top left     :" << top_left.x << top_left.y << top_left.z << endl
                 << "- Bottom left  :" << bottom_left.x << bottom_left.y << bottom_left.z << endl
                 << "- Bottom right :" << bottom_right.x << bottom_right.y << bottom_right.z << endl;
    }
}

void RocketCaveManager::CAVEWindow::CalculateProjection(RocketCaveManager *caveManager_)
{
    if(!viewportCreated)
        return;
    if (!camera)
    {
        LogError("[RocketCaveManager]: CalculateProjection() camera is null!");
        return;
    }

    double left, right, bottom, top;

    Ogre::Vector3 screenRight, screenUp, screenNormal;
    Ogre::Vector3 eyeBottomLeft, eyeTopLeft, eyeBottomRight;

    double nClip = this->nearClip;
    double fClip = this->farClip;

    screenRight = bottom_right - bottom_left;
    screenUp = top_left - bottom_left;

    screenRight.normalise();
    screenUp.normalise();

    screenNormal = screenUp.crossProduct(screenRight);
    screenNormal.normalise();

    eyeTopLeft = top_left - eye_position;
    eyeBottomLeft = bottom_left - eye_position;
    eyeBottomRight = bottom_right - eye_position;

    double dist = screenNormal.dotProduct(eyeBottomLeft);

    left = screenRight.dotProduct(eyeBottomLeft) * nClip / dist;
    right = screenRight.dotProduct(eyeBottomRight) * nClip / dist;
    bottom = screenUp.dotProduct(eyeBottomLeft) * nClip / dist;
    top = screenUp.dotProduct(eyeTopLeft) * nClip / dist;

    Ogre::Matrix4 changeBase = Ogre::Matrix4(
        screenRight.x,    screenRight.y,    screenRight.z,    0,
        screenUp.x,        screenUp.y,        screenUp.z,        0,
        screenNormal.x,    screenNormal.y, screenNormal.z, 0,
        0,              0,                0,                1);

    Ogre::Matrix4 transform = Ogre::Matrix4::IDENTITY;
    transform.makeTrans(-eye_position);

    Ogre::Matrix4 projection;
    Ogre::Root::getSingleton().getRenderSystem()->_makeProjectionMatrix(left, right, bottom, top, nClip, fClip, projection);

    Ogre::Matrix4 final = projection * changeBase * transform;

    camera->setCustomProjectionMatrix(true, final);
    camera->setOrientation(Ogre::Quaternion::IDENTITY);
    camera->setPosition(Ogre::Vector3::ZERO);
    camera->setNearClipDistance(nClip);
}

void RocketCaveManager::CAVEWindow::CreateCamera()
{
    Ogre::Camera* original_cam = sceneData_.cameraEntity->GetComponent<EC_Camera>()->OgreCamera();
    if(!original_cam)
        return;

    camera = renderer_->GetActiveOgreWorld()->OgreSceneManager()->createCamera("CAVE_camera" + Ogre::StringConverter::toString(index));
    camera->setPosition(original_cam->getPosition());
    camera->setOrientation(Ogre::Quaternion::IDENTITY);
}

void RocketCaveManager::CAVEWindow::SetupViewport()
{
    Ogre::Camera* original_cam = sceneData_.cameraEntity->GetComponent<EC_Camera>()->OgreCamera();
    if(!original_cam)
        return;

    // Attach node to main camera
    Ogre::SceneNode* originalNode = original_cam->getParentSceneNode();
    if(!originalNode)
        return;

    if(type == WINDOW)
    {
        window->setActive(true);
        window->setAutoUpdated(true);
        window->addViewport(camera, index);
    }
    else if(type == TEXTURE)
    {
        rendertexture = texture->getBuffer()->getRenderTarget();
        rendertexture->addViewport(camera);
        rendertexture->setAutoUpdated(true);
        rendertexture->setActive(true);

        // Draw windows to debug overlay
        Ogre::TexturePtr debugTex;
        Ogre::Overlay* caveOverlay = Ogre::OverlayManager::getSingleton().getByName("caveOverlay");
        if (!caveOverlay)
            caveOverlay = Ogre::OverlayManager::getSingleton().create("caveOverlay");

        debugTex = texture;

        // Remove material before creating new
        Ogre::MaterialManager::getSingleton().remove("CAVETexture" + Ogre::StringConverter::toString(index));

        // Set up a debug panel to display the shadow
        Ogre::MaterialPtr debugMat = Ogre::MaterialManager::getSingleton().create(
            "CAVETexture" +  Ogre::StringConverter::toString(index), 
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        debugMat->getTechnique(0)->getPass(0)->setLightingEnabled(false);
        Ogre::TextureUnitState *t = debugMat->getTechnique(0)->getPass(0)->createTextureUnitState(debugTex->getName());
        t->setTextureAddressingMode(Ogre::TextureUnitState::TAM_CLAMP);

        double xScalar = (double)viewSettings_.desktopSize.width() / (double)viewSettings_.viewportSize.width();
        double yScalar = (double)viewSettings_.desktopSize.height() / (double)viewSettings_.viewportSize.height();

        Ogre::OverlayContainer* debugPanel = (Ogre::OverlayContainer*)
            (Ogre::OverlayManager::getSingleton().createOverlayElement("Panel", "CAVETexPanel" + Ogre::StringConverter::toString(index)));

        bool debugPanels = false;
        if(debugPanels)
        {
            if(location == FLOOR)
                debugPanel->setPosition((1.0/ xScalar), (1.0 / yScalar));
            else
                debugPanel->setPosition(index * (1.0 / xScalar), 0.0);
        }
        else
            debugPanel->setPosition(index * (1.0 / xScalar), 0.0);

        debugPanel->setDimensions((1.0 / xScalar), (1.0 / yScalar));

        debugPanel->setMaterialName(debugMat->getName());
        caveOverlay->add2D(debugPanel);

        caveOverlay->show();
    }

    camera->getViewport()->setOverlaysEnabled(false);
    camera->getViewport()->setBackgroundColour(Ogre::ColourValue::Black);
    camera->getViewport()->setShadowsEnabled(false);
    camera->getViewport()->setClearEveryFrame(true);
}

void RocketCaveManager::CAVEWindow::SetupCamera()
{
    EC_Camera *ecCamera = sceneData_.cameraEntity->GetComponent<EC_Camera>().get();
    if(!ecCamera)
        return;

    Ogre::Camera* original_cam = sceneData_.cameraEntity->GetComponent<EC_Camera>()->OgreCamera();
    if(!original_cam)
        return;

    camera->setCustomProjectionMatrix(false);
    camera->setVisibilityFlags(original_cam->getVisibilityFlags());
    nearClip = ecCamera->nearPlane.Get();
    farClip = ecCamera->farPlane.Get();
    camera->setNearClipDistance(nearClip);
    camera->setFarClipDistance(farClip);
}

void RocketCaveManager::CAVEWindow::AttachCamera()
{
    if(viewportCreated)
    {
        if(!camera)
            return;

        if(camera->isAttached())
            camera->detachFromParent();

        if(sceneData_.cameraRoot)
            sceneData_.cameraRoot->attachObject(camera);
    }
}

void RocketCaveManager::CAVEWindow::DestroyViewport()
{
    if(type == WINDOW)
    {
        window->removeAllViewports();
        SAFE_DELETE(window);
    }
    else if(type == TEXTURE)
    {
        if(rendertexture)
        {
            rendertexture->setActive(false);
            rendertexture->setAutoUpdated(false);
            rendertexture->removeAllViewports();

            Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName("CAVETexture" +  Ogre::StringConverter::toString(index));
            if (!mat.isNull())
            {
                mat->getTechnique(0)->getPass(0)->removeAllTextureUnitStates();
                Ogre::MaterialManager::getSingleton().remove(mat->getHandle());
            }

            Ogre::Root::getSingleton().getRenderSystem()->destroyRenderTarget(rendertexture->getName());
            Ogre::TextureManager::getSingleton().remove(texture->getName());
        }

        try
        {
            Ogre::OverlayManager::getSingleton().destroyOverlayElement("CAVETexPanel" + Ogre::StringConverter::toString(index));
        }
        catch(const Ogre::Exception &e)
        {
            qDebug() << QString::fromStdString(e.getDescription());
        }
    }
}

void RocketCaveManager::CAVEWindow::DestroyCamera(Framework *framework)
{
    if(camera)
    {
        ScenePtr tundraScene = framework->Scene()->GetScene(sceneData_.sceneName);
        if(tundraScene)
        {
            OgreWorld* world = tundraScene->GetWorld<OgreWorld>().get();
            if(world)
            {
                try
                {
                    world->OgreSceneManager()->destroyCamera(camera);
                }
                catch(const Ogre::Exception &e)
                {
                    qDebug() << QString::fromStdString(e.getDescription());
                }
            }
        }
    }
}

void RocketCaveManager::OnCAVESettingsOpen()
{
    if(widget_)
        widget_->Show();
}

void RocketCaveManager::LoadCaveSettings()
{
    // Check if cave.ini can be found.
    QString documentsPath = Application::UserDataDirectory();
    int index = documentsPath.indexOf("Rocket");
    documentsPath = documentsPath.left(index) + "Satavision";
    
    QDir configDir(documentsPath);
    if (!configDir.exists())
        return;

    QString caveIniFile = configDir.absoluteFilePath("cave.ini");
    if (QFile::exists(caveIniFile))
        LoadCaveSettings(caveIniFile);
}

void RocketCaveManager::LoadCaveSettings(const QString& file)
{
    // Read cave config
    QSettings caveSettings(file, QSettings::IniFormat);
    QStringList keys = caveSettings.allKeys();
    bool ok = false;

    foreach(const QString &sectionKey, keys)
    {
        int index = sectionKey.indexOf("/");
        QString section = sectionKey.left(index);

        int windowIndex = section.right(1).toInt(&ok);
        if (!ok)
            continue;

        windowIndex = windowIndex - 1;
        if (windowIndex == windowSettings_.size())
            windowSettings_.append(CaveWindowSettings());
    }

    foreach(const QString &sectionKey, keys)
    {
        int index = sectionKey.indexOf("/");
        QString section = sectionKey.left(index);
        QString key = sectionKey.mid(index + 1);
        if(section.startsWith("screen"))
        {
            int index = section.right(1).toInt(&ok);
            if (!ok)
                continue;
            index = index - 1;

            // Read values
            QString value = caveSettings.value(sectionKey).toString();

            if(key == "near_clip")
            {
                if (windowSettings_.size() > index)
                {
                    bool success = false;
                    CaveWindowSettings &settings = windowSettings_[index];
                    settings.nearClip = value.toFloat(&success);
                    if(!success)
                        settings.nearClip = 0.1f;
                }
                continue;
            }

            QStringList vectorValues = value.split(' ');
            if (vectorValues.size() != 3)
                LogWarning("[RocketCave]: Screen section includes a vector3 that does not have 3 values: " + value);

            // Convert stringlist to vector3
            Ogre::Vector3 coord = Ogre::Vector3::ZERO;
            for(int i = 0; i < vectorValues.size(); ++i)
            {
                float val = vectorValues[i].toFloat(&ok);
                if(!ok)
                {
                    LogError("[RocketCave]: Screen section includes a vector3 with invalid floating point number: '" + vectorValues[i] + "'. Defaulting axis to zero.");
                    continue;
                }

                if(i == 0)
                    coord.x = val;
                else if(i == 1)
                    coord.y = val;
                else if(i == 2)
                    coord.z = val;
            }

            CaveWindowSettings settings;
            if(windowSettings_.size() > index)
                settings = windowSettings_[index];
            else
                continue;

            //qDebug() << index << sectionKey << coord.x << coord.y << coord.z;
            if(key == "top_left")
                settings.topLeft = coord;
            else if(key == "bottom_left")
                settings.bottomLeft = coord;
            else if(key == "bottom_right")
                settings.bottomRight = coord;

            windowSettings_[index] = settings;
        }
        else if(section == "eye")
        {
            if(key == "position")
            {
                QString value = caveSettings.value(sectionKey).toString();
                QStringList vectorValues = value.split(' ');
                if (vectorValues.size() != 3)
                    LogWarning("[RocketCave]: Eye section includes a vector3 that does not have 3 values: " + value);

                // Convert stringlist to vector3
                Ogre::Vector3 coord = Ogre::Vector3::ZERO;
                for(int i = 0; i < vectorValues.size(); ++i)
                {
                    float val = vectorValues[i].toFloat(&ok);
                    if (!ok)
                    {
                        LogError("[RocketCave]: Eye section includes a vector3 with invalid floating point number: '" + vectorValues[i] + "'. Defaulting axis to zero.");
                        continue;
                    }

                    if(i == 0)
                        coord.x = val;
                    else if(i == 1)
                        coord.y = val;
                    else if(i == 2)
                        coord.z = val;
                }

                settingsEyePosition_ = coord;
            }
        }
    }

    // Print out window settings
    if (IsLogChannelEnabled(LogChannelDebug))
    {
        qDebug() << "RocketCave settings loaded from config file" << endl << file << endl << endl
                 << "Eye" << endl
                 << settingsEyePosition_.x << settingsEyePosition_.y << settingsEyePosition_.z;

        for(int i = 0; i < windowSettings_.size(); ++i)
        {
            CaveWindowSettings settings = windowSettings_[i];
            qDebug()    << "Screen" << i << endl 
                        << "- Top left     :" << settings.topLeft.x << settings.topLeft.y << settings.topLeft.z << endl
                        << "- Bottom left  :" << settings.bottomLeft.x << settings.bottomLeft.y << settings.bottomLeft.z << endl
                        << "- Bottom right :" << settings.bottomRight.x << settings.bottomRight.y << settings.bottomRight.z << endl
                        << "- Near clip    :" << settings.nearClip;
                    
        }
    }

    settingsLoadedFromCaveIni_ = true;
}

void RocketCaveManager::LoadSettings()
{
    // Check if Satavision/cave.ini exists, load values from it

    ConfigData config("rocketcave", "main");
    caveEnabled = framework_->Config()->Read(config, "enabled").toBool();
    if(!caveEnabled)
        return;

    viewSettings_.desktopSize = framework_->Config()->Read(config, "desktop size").toSize();
    viewSettings_.viewportSize = framework_->Config()->Read(config, "viewport size").toSize();
    viewSettings_.viewCount = framework_->Config()->Read(config, "views").toInt();
    viewSettings_.fov = framework_->Config()->Read(config, "horizontal fov").toDouble();
}

#ifdef ROCKET_OPENNI
void RocketCaveManager::TrackingUserFound(TrackingUser *user)
{
    if(!user || user_)
        return;

    user_ = user;
}

void RocketCaveManager::TrackingUserLost(TrackingUser *user)
{
    if(user == user_)
        user = 0;

    qDebug() << "User lost";
}

void RocketCaveManager::TrackingCalibrationComplete(TrackingUser *user)
{

}

void RocketCaveManager::TrackingUserUpdated(int id, TrackingSkeleton *skeleton)
{
    if(!skeleton)
        return;

    if(!user_)
        return;

    if(id != user_->Id())
        return;

    if(!skeleton->BoneChanged("Head"))
        return;

    float3 bonePos = skeleton->GetBonePosition("Head", BoneCoordinateScale::METERS);

    QVector3D qPos;
    qPos.setX(bonePos.x);
    qPos.setY(bonePos.y);
    qPos.setZ(bonePos.z);

    OnUpdateEyePosition(qPos);
}
#endif

void RocketCaveManager::OnConnectionStateChanged(bool connected)
{
    if(!connected)
    {
        if (caveAction_)
            caveAction_->setVisible(false);

        if (widget_)
            disconnect(widget_, SIGNAL(SetViewDimensions(int, QVector3D, QVector3D, QVector3D)), this, SLOT(OnUpdateViewDimensions(int, QVector3D, QVector3D, QVector3D)));
        SAFE_DELETE(widget_);
    
        disconnect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)));

#ifdef ROCKET_OPENNI
        // Disconnect openni signals
        if(openNi_)
        {
            disconnect(openNi_, SIGNAL(UserFound(TrackingUser*)), this, SLOT(TrackingUserFound(TrackingUser*)));
            disconnect(openNi_, SIGNAL(UserLost(TrackingUser*)), this, SLOT(TrackingUserLost(TrackingUser*)));
            disconnect(openNi_, SIGNAL(CalibrationComplete(TrackingUser*)), this, SLOT(TrackingCalibrationComplete(TrackingUser*)));
            disconnect(openNi_, SIGNAL(UserUpdated(int, TrackingSkeleton*)), this, SLOT(TrackingUserUpdated(int, TrackingSkeleton*)));
        }
#endif

        return;
    }
    
    LoadSettings();
    if(!caveEnabled)
        return;

    widget_ = new RocketCaveAdvancedWidget(framework_);

    connect(widget_, SIGNAL(SetEyePosition(QVector3D)), this, SLOT(OnUpdateEyePosition(QVector3D)));
    connect(widget_, SIGNAL(SetViewDimensions(int, QVector3D, QVector3D, QVector3D)), this, SLOT(OnUpdateViewDimensions(int, QVector3D, QVector3D, QVector3D)));
    connect(widget_, SIGNAL(SetUseDefaultViews()), this, SLOT(OnUseDefaultViews()));

    // Hook to main camera changed event
    connect(renderer_, SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)), Qt::UniqueConnection);
    
    // Key events
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)), Qt::UniqueConnection);

#ifdef ROCKET_OPENNI
    // Connect openni signals
    if(openNi_)
    {
        connect(openNi_, SIGNAL(UserFound(TrackingUser*)), this, SLOT(TrackingUserFound(TrackingUser*)));
        connect(openNi_, SIGNAL(UserLost(TrackingUser*)), this, SLOT(TrackingUserLost(TrackingUser*)));
        connect(openNi_, SIGNAL(CalibrationComplete(TrackingUser*)), this, SLOT(TrackingCalibrationComplete(TrackingUser*)));
        connect(openNi_, SIGNAL(UserUpdated(int, TrackingSkeleton*)), this, SLOT(TrackingUserUpdated(int, TrackingSkeleton*)));
    }
#endif

    if(caveAction_)
        caveAction_->setVisible(true);
}

void RocketCaveManager::SetupWindows()
{
    if(!caveEnabled)
        return;

    OnDestroyWindows(sceneData_.sceneName);

    if(viewSettings_.viewCount == 2)
    {
        AddCAVEWindow(0, viewSettings_.fov, LEFT);
        AddCAVEWindow(1, viewSettings_.fov, RIGHT);
    }
    if(viewSettings_.viewCount == 3)
    {
        AddCAVEWindow(0, viewSettings_.fov, LEFT);
        AddCAVEWindow(1, viewSettings_.fov, CENTER);
        AddCAVEWindow(2, viewSettings_.fov, RIGHT);
    }
    else if(viewSettings_.viewCount == 4)
    {
        AddCAVEWindow(3, viewSettings_.fov, FLOOR);
        AddCAVEWindow(0, viewSettings_.fov, LEFT);
        AddCAVEWindow(1, viewSettings_.fov, CENTER);
        AddCAVEWindow(2, viewSettings_.fov, RIGHT);
    }
}

void RocketCaveManager::OnSceneAdded(const QString& sceneName)
{
    sceneData_.sceneName = sceneName;
}

void RocketCaveManager::OnDestroyWindows(const QString& sceneName)
{
    foreach(CAVEWindow* window, CAVEWindows_)
    {
        window->DestroyViewport();
        window->DestroyCamera(framework_);

        SAFE_DELETE(window);
    }

    CAVEWindows_.clear();

    // Delete debugoverlay
    try
    {
        if(Ogre::OverlayManager::getSingleton().getByName("caveOverlay"))
            Ogre::OverlayManager::getSingleton().destroy("caveOverlay");
    }
    catch(...){}
}

void RocketCaveManager::AddCAVEWindow(int index, double fovx, CAVEWindowLocation location)
{
    CAVEWindow* window = new CAVEWindow();
    window->index = index;
    window->location = location;
    window->fovx = fovx;

    int textureWidth = viewSettings_.viewportSize.width();
    int textureHeight = viewSettings_.viewportSize.height();

    if(textureWidth <= 0)
        textureWidth = 1;
    if(textureHeight <= 0)
        textureHeight = 1;

    window->texture = Ogre::Root::getSingleton().getTextureManager()->createManual("CAVERtt" + Ogre::StringConverter::toString(viewSettings_.currentRendertarget), 
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        textureWidth, textureHeight, 0,
        Ogre::PF_R8G8B8A8,
        Ogre::TU_RENDERTARGET);

    ++viewSettings_.currentRendertarget;

    window->viewportCreated = false;
   
    // Set custom loaded window dimensions
    if (settingsLoadedFromCaveIni_)
    {
        if (windowSettings_.size() > index)
        {
            CaveWindowSettings settings = windowSettings_[index];
            window->SetViewDimensions(settings.topLeft, settings.bottomLeft, settings.bottomRight);
            window->nearClip = settings.nearClip;
        }
        window->eye_position = settingsEyePosition_;
    }
    else
        window->CalculateViewDimensions();
    
    CAVEWindows_.append(window);
}

void RocketCaveManager::OnUpdateViewDimensions(int view, QVector3D top_left, QVector3D bottom_left, QVector3D bottom_right)
{
    foreach(CAVEWindow *window, CAVEWindows_)
    {
        if(window->index == view)
        {
            window->customMatrix = true;
            window->top_left = Ogre::Vector3(top_left.x(), top_left.y(), top_left.z());
            window->bottom_left = Ogre::Vector3(bottom_left.x(), bottom_left.y(), bottom_left.z());
            window->bottom_right = Ogre::Vector3(bottom_right.x(), bottom_right.y(), bottom_right.z());
            window->CalculateProjection(this);
            return;
        }
    }
}

void RocketCaveManager::OnUpdateEyePosition(QVector3D eye_position)
{
    foreach(CAVEWindow *window, CAVEWindows_)
    {
        window->eye_position.x = eye_position.x();
        window->eye_position.y = eye_position.y();
        window->eye_position.z = eye_position.z();
        window->CalculateProjection(this);
    }
}

void RocketCaveManager::OnUseDefaultViews()
{
    foreach(CAVEWindow *window, CAVEWindows_)
    {
        window->customMatrix = false;
        window->CalculateViewDimensions();
        window->CalculateProjection(this);
    }
}

void RocketCaveManager::OnActiveCameraChanged(Entity *newMainWindowCamera)
{
    if (!caveEnabled)
        return;
    if (!newMainWindowCamera)
    {
        sceneData_.cameraEntity = 0;
        return;
    }

    // Store camera entity pointer
    sceneData_.cameraEntity = newMainWindowCamera;

    SetCameraCulling();
    SetupWindows();
    SetupViewports();
}

void RocketCaveManager::SetCameraCulling()
{
    Ogre::Camera *ogreCamera = sceneData_.cameraEntity->GetComponent<EC_Camera>()->OgreCamera();
    if(!ogreCamera)
        return;

#include "DisableMemoryLeakCheck.h"
    Ogre::Frustum *frustum = new Ogre::Frustum();
#include "EnableMemoryLeakCheck.h"
    frustum->setNearClipDistance(0.1f);
    frustum->setFarClipDistance(0.11f);
    frustum->setFOVy(Ogre::Degree(45.f));
    frustum->setAspectRatio(4.f/3.f);
    ogreCamera->setCullingFrustum(frustum);
}

void RocketCaveManager::SetupViewports()
{
    if (!caveEnabled)
        return;
    if (!sceneData_.cameraEntity)
        return;

    // Create root node for cameras
    sceneData_.CreateCameraRoot(framework_);

    foreach(CAVEWindow* window, CAVEWindows_)
    {
        if (window->viewportCreated)
            continue;

        window->CreateCamera();
        window->SetupViewport();
        window->SetupCamera();
        window->CalculateProjection(this);
        window->viewportCreated = true;
    }
    foreach(CAVEWindow *window, CAVEWindows_)
    {
        window->AttachCamera();
        if(settingsLoadedFromCaveIni_ && windowSettings_.size() > window->index)
        {
            CaveWindowSettings settings = windowSettings_[window->index];
            window->nearClip = settings.nearClip;
        }
        window->CalculateProjection(this);
    }
}

void RocketCaveManager::OnKeyPress(KeyEvent* e)
{
    if (!e || e->eventType != KeyEvent::KeyPressed || e->IsRepeat())
        return;

    QKeySequence showCaveSettings = QKeySequence(Qt::ShiftModifier + Qt::Key_T);

    if(e->Sequence() == showCaveSettings)
        widget_->Show();
}
