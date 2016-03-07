/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

#pragma once

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "InputFwd.h"

#include "OgreModuleApi.h"
#include "OgreModuleFwd.h"

#include "AttributeChangeType.h"

#include "Renderer.h"
#include <Ogre.h>

#if OGRE_VERSION_MAJOR >= 1 && OGRE_VERSION_MINOR >= 9
#include <OgreOverlaySystem.h>
#endif

class RocketOculusManager : public QObject
{
    Q_OBJECT

public:
    explicit RocketOculusManager(RocketPlugin* plugin);
    ~RocketOculusManager();

public slots:
    /** Returns true if Oculus device is found */
    bool HasDevice() const;

    /** Returns true if Oculus is enabled and device is found. */
    bool IsEnabled() const;
    
private slots:

#ifdef ROCKET_OCULUS_ENABLED
    void SetupLobby();
    void DestroyLobby();

    void CreateMeshes();

    void CreateOculusTexture();

    void CreateOculusCameras(Ogre::SceneManager* ogreSceneManager, Ogre::SceneNode* parentNode);
    void DestroyOculusCameras(Ogre::SceneManager* ogreSceneManager);

    void CreateOculusMeshEntities(Ogre::SceneManager* ogreSceneManager, Ogre::SceneNode* parentNode);
    void DestroyOculusMeshEntities(Ogre::SceneManager* ogreSceneManager);

    void CreateOculusUI(Ogre::SceneManager* ogreSceneManager);
    void DestroyOculusUI(Ogre::SceneManager* ogreSceneManager);

    void SetupOculusDisplay();
    void DestroyOculusDisplay();

    void UpdateMaterialParams(const Ogre::Matrix4&, const Ogre::Matrix4&, int eye);

    void LoadSettings();

    Ogre::MeshPtr CreateUIMesh(int width, int height);
#endif

    // All slots used via signals
    void PreRenderFrame();
    void SwapBuffers();
    void PostRenderFrame();

    void OnConnectionStateChanged(bool connectedToServer);
    void OnActiveCameraChanged(Entity* newMainWindowCamera);

    void OnKeyPress(KeyEvent* e);
    void OnMouseEvent(MouseEvent* e);
    void OnWindowResized(int width, int height);

    void OnShowRiftAvailableNotification();
    
    void OnEntityCreated(Entity* entity, AttributeChange::Type change);
    void OnComponentAdded(Entity* entity, IComponent* comp, AttributeChange::Type change);
    void OnMeshChanged();

    void OnUpdated(float /* elapsedTime */);

#ifdef ROCKET_OCULUS_ENABLED

private:
    bool enabled_;
    bool lobbyInitialized_;

    struct OculusMembers;
    OculusMembers *oculus_;

    bool notificationShowed_;

    Entity* mainCameraEntity_;

    OgreRenderer::RendererPtr renderer_;

    Ogre::MeshPtr uiMesh_;
    Ogre::MeshPtr leftEyeMesh_;
    Ogre::MeshPtr rightEyeMesh_;
    
    Ogre::Overlay* oculusOverlay_;
    Ogre::SceneNode* headNode_;
    Ogre::Camera* mainCamera_;
    Ogre::Camera* leftEyeCamera_;
    Ogre::Camera* rightEyeCamera_;

    Ogre::Viewport* leftEyeViewport_;
    Ogre::Viewport* rightEyeViewport_;

    Ogre::Matrix4 leftEyeTransform_;
    Ogre::Matrix4 rightEyeTransform_;
    
    // Mesh display
    Ogre::Entity* uiMeshEntity_;
    Ogre::Entity* leftEyeMeshEntity_;
    Ogre::Entity* rightEyeMeshEntity_;

    Ogre::SceneNode* uiMeshNode_;
    Ogre::SceneNode* leftMeshNode_;
    Ogre::SceneNode* rightMeshNode_;

    // Overlay material
    Ogre::MaterialPtr overlayMaterial_;
    Ogre::GpuProgramParametersSharedPtr materialParams_;
    Ogre::GpuProgramParametersSharedPtr uiMaterialParams_;
    
    // UI plane
    Ogre::Entity* uiPlaneEntity_;
    Ogre::SceneNode* uiPlaneNode_;

    // Overlay texture. More easy to handle rendering order.
    Ogre::TexturePtr oculusTexture_;
    Ogre::RenderTarget* oculusRenderTarget_;

    Framework* framework_;
    RocketPlugin* plugin_;

    bool lastConnectionState_;

#endif
};
