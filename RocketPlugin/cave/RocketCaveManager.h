/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "InputFwd.h"

#include "RocketMenu.h"

#ifdef ROCKET_OPENNI
#include "RocketOpenNIPluginFwd.h"
#endif

#include "Renderer.h"
#include "OgreTexture.h"
#include "OgreRoot.h"
#include "OgreDepthBuffer.h"
#include "OgreRenderTargetListener.h"

#include <QSize>

class RocketCaveAdvancedWidget;

class RocketCaveManager : public QObject
{
    Q_OBJECT
public:
    explicit RocketCaveManager(RocketPlugin* plugin);

    ~RocketCaveManager();

private: 
    enum CAVEWindowType
    {
        TEXTURE,
        WINDOW
    };

    enum CAVEWindowLocation
    {
        LEFT,
        CENTER,
        RIGHT,
        FLOOR,
        ROOF
    };

private slots:
    void LoadCaveSettings();
    void LoadCaveSettings(const QString& file);

    void OnActiveCameraChanged(Entity *newMainWindowCamera);
    
    void OnConnectionStateChanged(bool connected);

    // Destroy windows when scene is deleted
    void OnDestroyWindows(const QString& sceneName);

    void SetCameraCulling();

    void SetupWindows();

    void SetupViewports();

    void AddCAVEWindow(int index, double fovx, CAVEWindowLocation location);

    void OnUpdateEyePosition(QVector3D eye_position);
    void OnUpdateViewDimensions(int view, QVector3D top_left, QVector3D bottom_left, QVector3D bottom_right);
    void OnUseDefaultViews();

    void OnSceneAdded(const QString& sceneName);

    void OnCAVESettingsOpen();

    void OnKeyPress(KeyEvent* e);

    void LoadSettings();

#ifdef ROCKET_OPENNI
    void TrackingUserFound(TrackingUser *user);
    void TrackingUserLost(TrackingUser *user);
    void TrackingCalibrationComplete(TrackingUser *user);
    void TrackingUserUpdated(int id, TrackingSkeleton *skeleton);
#endif

private:

    struct ViewSettings
    {
        QSize viewportSize;
        QSize desktopSize;

        int viewCount;
        int currentRendertarget;

        double fov;

        ViewSettings();
    };

    struct SceneData
    {
        // Current scene name, usually "TundraClient"
        QString sceneName;

        Ogre::SceneNode *cameraRoot;
        Entity *cameraEntity;

        SceneData();

        void Destroy(Framework *framework);
        void CreateCameraRoot(Framework *framework);
    };

    struct CAVEWindow
    {
        bool customMatrix;

        Ogre::Vector2 screenSize;

        double nearClip;
        double farClip;

        // Viewport fow
        double fovx;

        // Location of monitor
        CAVEWindowLocation location;

        // Eye position
        Ogre::Vector3 eye_position;

        // Screen dimensions in space
        // These are relative to camera position
        Ogre::Vector3 top_left;
        Ogre::Vector3 bottom_left;
        Ogre::Vector3 bottom_right;

        // If one monitor in fullscreen, windows are actually rendertextures
        Ogre::TexturePtr texture;
        Ogre::RenderTexture *rendertexture;

        // If multiple monitors, use real windows
        /* @Todo. Support multiple windows. */
        Ogre::RenderWindow *window;

        Ogre::Camera *camera;
        
        int index;
        bool viewportCreated;

        CAVEWindowType type;

        CAVEWindow();

        void SetViewDimensions(Ogre::Vector3 topLeft, Ogre::Vector3 bottomLeft, Ogre::Vector3 bottomRight);
        void CalculateViewDimensions();
        void CalculateProjection(RocketCaveManager *caveManager_);

        void CreateCamera();
        void SetupViewport();
        void SetupCamera();
        void AttachCamera();

        void DestroyViewport();
        void DestroyCamera(Framework *framework);
    };

    struct CaveWindowSettings
    {
        Ogre::Vector3 topLeft;
        Ogre::Vector3 bottomLeft;
        Ogre::Vector3 bottomRight;
        float nearClip;
        
        CaveWindowSettings() : nearClip(0.1f) {}
    };

    bool settingsLoadedFromCaveIni_;
    Ogre::Vector3 settingsEyePosition_;
    QList<CaveWindowSettings> windowSettings_;

    QAction *caveAction_;

    bool caveEnabled;
    bool headTrackingEnabled;

    QList<CAVEWindow*> CAVEWindows_;

    static ViewSettings viewSettings_;
    static SceneData sceneData_;
    static OgreRenderer::Renderer *renderer_;

    RocketPlugin *plugin_;
    Framework *framework_;

    RocketCaveAdvancedWidget *widget_;

#ifdef ROCKET_OPENNI
    // Track only one user at time
    TrackingUser *user_;
    RocketOpenNIPlugin *openNi_;
#endif
};
