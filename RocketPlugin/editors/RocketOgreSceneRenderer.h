/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "OgreModuleFwd.h"
#include "AssetFwd.h"

#define MATH_OGRE_INTEROP
#include "Transform.h"
#include "Color.h"
#include "Math/float2.h"
#include "Geometry/AABB.h"
#include "Geometry/OBB.h"
#include "Geometry/LineSegment.h"

#include "OgreBulletCollisionsDebugLines.h"

#include <QObject>
#include <QSize>
#include <QImage>
#include <QTimer>

#include <OgreLight.h>
#include <OgreSceneManager.h>

class DebugLines;

namespace Ogre
{
    class PlaneBoundedVolumeListSceneQuery;
    class OverlayElement;
    class Overlay;
}

/// Utility class for managing an Ogre scene environment and providing multiple ways of rendering it esp. with Qt widgets/graphics.
class RocketOgreSceneRenderer : public QObject
{
    Q_OBJECT
    
public:
    /// Ctor for widget based RM_RenderWidget rendering.
    /** The rendering is updated with the speed of normal Tundra main loop (which updated Ogre).
        The result is rendered directly into the surface of @c renderTarget. */
    explicit RocketOgreSceneRenderer(RocketPlugin *plugin, QWidget *renderTarget, QObject *parent = 0);
    
    /// Ctor for off screen RM_RenderTexture rendering.
    /** This constructor is suggested for taking snap shots of a scene state for eg. thumbnails. 
        @note You might want to use the same scene renderer for multiple snapshots once created,
        the creation overhead and resizing of the scene has the biggest impact on performance
        (exluding the actual Ogre::Image based rendering, which is slow in itself). 
        1) Add your objects via CreateMesh() etc.
        2) Call Update() when you know the scene has changed
        3) Call ToQImage() to render a snapshot */
    explicit RocketOgreSceneRenderer(RocketPlugin *plugin, const QSize &size = QSize(256, 256), Ogre::PixelFormat format = Ogre::PF_R8G8B8, QObject *parent = 0);
    
    /// Dtor.
    /** @note Will destroy any scene nodes and entities created to this scene renderer.
        You don't need to do manual cleanup, just make sure to null out any ptrs returned from this object. */
    ~RocketOgreSceneRenderer();
    
    /// Typedefs
    typedef QList<Ogre::SceneNode* > OgreSceneNodeList;
    typedef QList<Ogre::MovableObject* > OgreMovableObjectList;
    
    /// Render mode.
    enum RenderMode
    {
        RM_RenderWidget = 0,
        RM_RenderTexture
    };
    RenderMode GetRenderMode() const { return renderMode_; }
    
    /// Returns if the object is valid to be operated on.
    bool IsValid() const;
    
    /// Returns if camera and its parent scene node are valid.
    bool IsCameraValid() const;
    
    /// Unique editor ID.
    QString Id(const QString &postfix = "") const;
    QString MaterialId(const QString &postfix = "Material") const;
    QString OverlayId(const QString &postfix = "Overlay") const;

    std::string IdStd(const std::string &postfix = "") const;
    std::string MaterialIdStd(const std::string &postfix = "Material") const;
    std::string OverlayIdStd(const std::string &postfix = "Overlay") const;
    
    // Create overlay for the RM_RenderTexture technique.
    Ogre::OverlayElement *CreateOverlay(float2 size, float2 position, bool autoUpdate = true);
    
    /// Set overlay size (in pixels).
    void SetOverlayPosition(float2 position);
    void SetOverlayPosition(float x, float y);  /**< @overload */

    /// Set overlay position (in pixels).
    void SetOverlaySize(float2 position);
    void SetOverlaySize(float width, float height); /**< @overload */
        
    /// Set auto update interval. By default auto update is disabled.
    void SetAutoUpdateInterval(int updateIntervalMsec);
    void SetAutoUpdateInterval(float updateIntervalSeconds);

    /// Update scene rendering.
    /** @note Won't do anything if a auto update interval has been set */
    void Update();
    
    /// Rendering as QImage.
    QImage ToQImage();

    /// Resize render texture.
    void Resize(const QSize &size);

    /// Get active scene manager.
    Ogre::SceneManager *SceneManager() const;

    /// Get active camera.
    Ogre::Camera *OgreCamera() const;

    /// Get active view port.
    Ogre::Viewport *OgreViewport() const;

    /// Get active overlay.
    /** @see CreateOverlay. */
    Ogre::Overlay *OgreOverlay() const;

    /// Get active overlay element.
    /** @see CreateOverlay. */
    Ogre::OverlayElement *OgreOverlayElement() const;

    /// Returns the render texture. Will only be valid in RM_RenderTexture mode.
    Ogre::Texture *RenderTexture() const;
    
    /// Returns the render target.
    Ogre::RenderTarget *RenderTarget() const;
    
    /// QWidget override.
    bool eventFilter(QObject *obj, QEvent *e);
    
public slots:
    /// Creates new scene node.
    /** @note Prefer using CreateMesh etc. utility functions.
        @note Do not use this ptr after this RocketOgreSceneRenderer has been destroyed. */
    Ogre::SceneNode *CreateSceneNode();

    /// Create new mesh node and set materials from asset ref(s).
    /** @note This function does not return a entity as its loaded once the assets are done.
        This function is mostly useful if you don't care about a delay in showing the mesh
        and you don't need to manipulate its position etc. */
    void CreateMesh(const QString &meshAssetRef, const QStringList &materialAssetRefs = QStringList(), 
        Transform transform = Transform(float3::zero, float3::zero, float3::one));

    /// Create new mesh node from asset.
    /** You can fetch the scene node by using Ogre::Entity::getParentSceneNode().
        @note Do not use this ptr after this RocketOgreSceneRenderer has been destroyed. */
    Ogre::Entity *CreateMesh(OgreMeshAsset *asset);

    /// Create new mesh node with Ogre prefab type.
    /** You can fetch the scene node by using Ogre::Entity::getParentSceneNode().
        @note Do not use this ptr after this RocketOgreSceneRenderer has been destroyed. */    
    Ogre::Entity *CreateMesh(Ogre::SceneManager::PrefabType type);

    /// Set material to entitys submesh index.
    bool SetMaterial(Ogre::Entity *entity, OgreMaterialAsset *asset, uint submeshIndex = 0);

    /// Set materials to entitys submesh index. Applied to submeshes by the list index.
    /** @return True if all materials were set successfully. False if any failed. */
    bool SetMaterials(Ogre::Entity *entity, QList<OgreMaterialAsset*> assets);

    /// Create new light.
    /** You can fetch the scene node by using Ogre::Light::getParentSceneNode().
        @note Do not use this ptr after this RocketOgreSceneRenderer has been destroyed. */
    Ogre::Light *CreateLight(Ogre::Light::LightTypes type = Ogre::Light::LT_SPOTLIGHT, const Color &diffuse = Color::White);
    
    /// Attach a object to a node. This will correctly handle re-parenting.
    bool AttachObject(Ogre::MovableObject *object, Ogre::SceneNode *newParent);

    /// Destroys the object and its parent scene node.
    /** Call this function with the object returned from CreateMesh(), CreateLight() etc. if you wish to destroy it.
        @note Do not dereference @c object after calling this. */
    bool DestroyObject(Ogre::MovableObject *object);

    /// Get node transform.
    Transform NodeTransform(Ogre::SceneNode *node);

    /// Set node transform.
    void SetNodeTransform(Ogre::SceneNode *node, const Transform &transform);

    /// Get camera position.
    float3 CameraPosition();
    
    /// Get camera transform.
    Transform CameraTransform();
    
    /// Set camera distance from main object.
    /** @note If you call this function automatic camera focus with this distance is enabled.
        Any prior calls to SetCameraTransform, SetCameraPosition and SetCameraLookAt are ignored. */
    void SetCameraDistance(float distance);
    
    /// Returns set camera distance.
    float CameraDistance() const;

    /// Set camera transform.
    /** @note If you call this function automatic camera focus with this distance is disabled. */
    void SetCameraTransform(const Transform &transform);

    // Set camera position.
    /** @note If you call this function automatic camera focus with this distance is disabled. */
    void SetCameraPosition(float3 position);
    
    /// Set camera lookat.
    /** @note If you call this function automatic camera focus with this distance is disabled. */
    void SetCameraLookAt(float3 position);
    
    /// Set the view ports background color.
    void SetBackgroundColor(const Color &color);

    /// Set the scene managers ambient light color.
    void SetAmbientLightColor(const Color &color);

    /// Debug visualization
    void AddDebugDrawAABB(const AABB &aabb, const Color &clr, bool depthTest = true);
    void AddDebugDrawOBB(const OBB &obb, const Color &clr, bool depthTest = true);
    void AddDebugDrawLineSegment(const LineSegment &l, const Color &clr, bool depthTest = true);

private:
    struct PendingObjectCreation
    {
        QString UUID;
        QStringList materialAssetRefs;
        QStringList materialAssetRefsTransfer;
        
        Transform transform;
        Ogre::Entity *entity;
        
        RocketOgreSceneRenderer *sceneRenderer;
        
        PendingObjectCreation(RocketOgreSceneRenderer *renderer) :
            sceneRenderer(renderer), entity(0) {}
        
        bool ApplyMaterial(const QString &sourceUrl, AssetPtr asset);
        void Listen(IAssetTransfer *transfer);
        bool Listening(IAssetTransfer *transfer);
    };
    typedef QList<PendingObjectCreation*> PendingObjectCreationList;

private slots:
    /// State update.
    void OnStateUpdate(float frametime);
    
    /// Frame updates.
    void OnUpdate(float frametime);
    
    /// Render target widget was resized.
    void OnRenderTargetResized();

    /// Render target widget was resized.
    void OnDeviceCreated();

    /// Pending mesh/material asset loaded.
    void OnMeshAssetLoaded(AssetPtr asset);
    void OnMaterialAssetLoaded(AssetPtr asset);

    /// Pending mesh/material asset load failed.
    void OnMeshAssetFailed(IAssetTransfer *transfer, QString reason);
    void OnMaterialAssetFailed(IAssetTransfer *transfer, QString reason);

    /// New pending creation.
    PendingObjectCreation *NewPendingObjectCreation(AssetTransferPtr transfer, const QStringList &materialAssetRefs = QStringList(), 
        Transform transform = Transform(float3::zero, float3::zero, float3::one));
        
    /// Get pending creation for transfer.
    PendingObjectCreationList PendingCreation(IAssetTransfer *transfer);
    PendingObjectCreation *PendingCreationByUUID(const QString &UUID);
    
    /// Apply completed material to a pending entity.
    void ApplyPendingMaterial(PendingObjectCreation *pending, IAssetTransfer *transfer);
    
    /// Remove pending creation
    bool RemovePendingCreation(PendingObjectCreation *pending);
    bool RemovePendingCreation(const QString &UUID);
    
private:
    bool Initialize();
    void CreateRenderWindow(QWidget *targetWindow, const QString &name, int width, int height, int left, int top, bool fullscreen);
    void CreateViewport(bool autoUpdate, bool overlayEnabled, Color backgroundColor);
    
    void DestroyOverlay();
    void RecreateTexture(Ogre::Texture *&texture, const QSize &size);
    
    uint ReadFSAASetting();

    RocketPlugin *plugin_;
    
    static const QString LC;
    QString id_;
    RenderMode renderMode_;
    
    OgreRendererPtr renderer_;
    Ogre::RenderWindow *renderWindow_;
    Ogre::SceneManager *sceneManager_;
    Ogre::SceneNode *rootNode_;
    
    Ogre::Viewport *viewPort_;
    Ogre::Camera *camera_;

    Ogre::OverlayElement *overlayElement_;
    Ogre::Overlay *overlay_;

    Ogre::PixelFormat format_;

    OgreSceneNodeList createdNodes_;
    OgreMovableObjectList createdObjects_;

    DebugLines* debugLines_;
    DebugLines* debugLinesNoDepth_;

    QTimer resizeTimer_;
    
    QPointer<QWidget> renderTarget_;
    QHash<Ogre::SceneNode*, float3> nodeRotationCache_;
    
    QList<PendingObjectCreation*> pendingCreations_;
    
    float2 overlaySize_;
    float2 overlayPosition_;
    
    bool autoMeshFocus_;
    float cameraDistance_;
    float updateInterval_;
    float t_;
};
