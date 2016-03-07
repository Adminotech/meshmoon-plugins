/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#pragma once

#ifdef ROCKET_UMBRA_ENABLED

#include "MeshmoonComponentsApi.h"
#include "IComponent.h"
#include "Profiler.h"

#include "AssetFwd.h"
#include "SceneFwd.h"
#include "InputFwd.h"
#include "OgreModuleFwd.h"
#include "AssetReference.h"

#include "Color.h"

#include "EC_MeshmoonOccluder.h"

#include "umbraDefs.hpp"
#include "optimizer/umbraBuilder.hpp"
#include "optimizer/umbraScene.hpp"
#include "runtime/umbraTome.hpp"
#include "runtime/umbraQuery.hpp"
#include "optimizer/umbraTask.hpp"


class OcclusionDebugRenderer : public Umbra::DebugRenderer
{
public:
    OcclusionDebugRenderer(OgreRenderer::RendererPtr renderer);
    ~OcclusionDebugRenderer();

    void addLine(const Umbra::Vector3& start, const Umbra::Vector3& end, const Umbra::Vector4& color);
    void addPoint(const Umbra::Vector3& pt, const Umbra::Vector4& color);
    void addAABB(const Umbra::Vector3& mn, const Umbra::Vector3& mx, const Umbra::Vector4& color);
    void addQuad(const Umbra::Vector3& x0y0, const Umbra::Vector3& x0y1, const Umbra::Vector3& x1y1, const Umbra::Vector3& x1y0, const Umbra::Vector4& color);

private:
    float3 Vector3ToFloat3(const Umbra::Vector3& vec);
    Color Vector4ToColor(const Umbra::Vector4& col);

    OgreRenderer::RendererPtr renderer_;
};


struct MESHMOON_COMPONENTS_API MeshData
{
    size_t vertexCount_;
    size_t indexCount_;

    float *vertices_;
    unsigned int *indices_;

    MeshData();
    ~MeshData();

    void GetVertexData(Ogre::Mesh *mesh);
};

class MESHMOON_COMPONENTS_API EC_MeshmoonCulling : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonCulling", 505) // Note this is the closed source EC Meshmoon range ID.

public:
    Q_PROPERTY(AssetReference tomeRef READ gettomeRef WRITE settomeRef);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReference, tomeRef);

    Q_PROPERTY(int id READ getid WRITE setid);
    DEFINE_QPROPERTY_ATTRIBUTE(int, id);

    Q_PROPERTY(float smallestOccluder READ getsmallestOccluder WRITE setsmallestOccluder);
    DEFINE_QPROPERTY_ATTRIBUTE(float, smallestOccluder);

    Q_PROPERTY(float smallestHole READ getsmallestHole WRITE setsmallestHole);
    DEFINE_QPROPERTY_ATTRIBUTE(float, smallestHole);

    Q_PROPERTY(int backfaceLimit READ getbackfaceLimit WRITE setbackfaceLimit);
    DEFINE_QPROPERTY_ATTRIBUTE(int, backfaceLimit);

    Q_PROPERTY(float tileSize READ gettileSize WRITE settileSize);
    DEFINE_QPROPERTY_ATTRIBUTE(float, tileSize);

    Q_PROPERTY(bool drawDebug READ getdrawDebug WRITE setdrawDebug)
    DEFINE_QPROPERTY_ATTRIBUTE(bool, drawDebug)

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonCulling(Scene *scene);
    ~EC_MeshmoonCulling();
    /// @endcond

public slots:
    void GenerateTome(const QString& fileName);

    void StartCulling();

    void StopCulling(const QString& message = "");

private slots:
    void WaitForLoading();

    void OnWaitForMeshes(float /* elapsedTime */);

    void OnActiveCameraChanged(Entity *newMainWindowCamera);

    void OnLoadTome(const QByteArray& bytes);

    void OnLoadTome(AssetPtr tomeAsset); /// Overload

    void OnTomeGenerated(const QByteArray& bytes);

    void OnTomeLoadingFailed(IAssetTransfer *transfer, QString reason);

    void OnUpdate(float /* elapsedTime */);

    void OnKeyPress(KeyEvent* e);

    void OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change);

    void OnWaitForTomeCalculation(/*float elapsedTime */);

signals:
    void TomeGenerationStarted();

    void TomeGenerationFailed();

    void TomeGenerated(const QString& fileName);

private:
    /// IComponent override
    void AttributesChanged();

    void GetOcclusionComponents(Scene *scene);

    void StartTomeGeneration();

    QString LC;

    QHash<int, EC_MeshmoonOccluder*> occluders_;
    QVector<EC_MeshmoonOccluder*> loadingOccluders_;

    AssetRefListenerPtr tomeListener_;

    OgreRenderer::RendererPtr renderer_;

    // Current camera
    EC_Camera *activeCamera_;
    EC_Placeable *activeCameraTransform_;

    // Camera transform to Umbra
    bool freezeOcclusionCamera_;
    Umbra::Matrix4x4 umbraCameraTransform_;

    // Umbra scene data
    const Umbra::Tome *umbraTome_;
    Umbra::Query *umbraQuery_;
    OcclusionDebugRenderer *umbraDebugRenderer_;

    // Tome calculation
    Umbra::Scene *umbraScene_;
    Umbra::Task *umbraTask_;

    // List for visible objects
    int* umbraObjectList_;
    Umbra::IndexList umbraObjects_;

    // Umbra camera
    Umbra::Frustum umbraFrustum_;
    Umbra::CameraTransform umbraCamera_;

    QString tomePath_;

    // Max waiting time for meshes to load.
    float maxWaitingTime_;
    float timeWaited_;

    int* lastVisibleObjects_;
    int lastVisibleObjectsSize_;
};
COMPONENT_TYPEDEFS(MeshmoonCulling);
#endif
