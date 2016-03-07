/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#include "StableHeaders.h"

#ifdef ROCKET_UMBRA_ENABLED

#include "AssetAPI.h"
#include "SceneAPI.h"
#include "InputAPI.h"
#include "FrameAPI.h"

#include "IAsset.h"
#include "AssetRefListener.h"

#include "Framework.h"

#include "MeshmoonComponents.h"
#include "EC_MeshmoonCulling.h"
#include "EC_Placeable.h"
#include "EC_Camera.h"

#include "Ogre.h"
#include "OgreRenderingModule.h"
#include "OgreWorld.h"

#include "Renderer.h"
#include "Scene.h"
#include "Color.h"
#include "Math/MathFunc.h"

static QString MESHMOON_TOME_SUFFIX = ".tome";

// Debug Renderer
OcclusionDebugRenderer::OcclusionDebugRenderer(OgreRenderer::RendererPtr renderer) :
    renderer_(renderer)
{
}
OcclusionDebugRenderer::~OcclusionDebugRenderer()
{
}

void OcclusionDebugRenderer::addLine(const Umbra::Vector3& start, const Umbra::Vector3& end, const Umbra::Vector4& color)
{
    OgreWorldPtr ogreWorld = renderer_->GetActiveOgreWorld();
    if(!ogreWorld.get())
        return;

    float3 start_ = Vector3ToFloat3(start);
    float3 end_ = Vector3ToFloat3(end);

    ogreWorld->DebugDrawLine(start_, end_, Color::Yellow);
}

void OcclusionDebugRenderer::addPoint(const Umbra::Vector3& pt, const Umbra::Vector4& color)
{
    OgreWorldPtr ogreWorld = renderer_->GetActiveOgreWorld();
    if(!ogreWorld.get())
        return;

    // Nothing to do here
    float3 point = Vector3ToFloat3(pt);
    ogreWorld->DebugDrawSphere(point, 0.1f, 6, Color::Red);
}

void OcclusionDebugRenderer::addAABB(const Umbra::Vector3& mn, const Umbra::Vector3& mx, const Umbra::Vector4& color)
{
    OgreWorldPtr ogreWorld = renderer_->GetActiveOgreWorld();
    if(!ogreWorld.get())
        return;

    float3 mn_ = Vector3ToFloat3(mn);
    float3 mx_ = Vector3ToFloat3(mx);
    Color color_ = Vector4ToColor(color);

    AABB aabb(mn_, mx_);
    ogreWorld->DebugDrawAABB(aabb, color_);
}

void OcclusionDebugRenderer::addQuad(const Umbra::Vector3& x0y0, const Umbra::Vector3& x0y1, const Umbra::Vector3& x1y1, const Umbra::Vector3& x1y0, const Umbra::Vector4& color)
{
    OgreWorldPtr ogreWorld = renderer_->GetActiveOgreWorld();
    if(!ogreWorld.get())
        return;

    // Nothing to do here
    float3 x0y0_, x0y1_, x1y0_, x1y1_;
    x0y0_ = Vector3ToFloat3(x0y0);
    x0y1_ = Vector3ToFloat3(x0y1);
    x1y0_ = Vector3ToFloat3(x1y0);
    x1y1_ = Vector3ToFloat3(x1y1);

    ogreWorld->DebugDrawLine(x0y0_, x1y0_, Color::Yellow);
    ogreWorld->DebugDrawLine(x1y0_, x1y1_, Color::Yellow);
    ogreWorld->DebugDrawLine(x1y1_, x0y1_, Color::Yellow);
    ogreWorld->DebugDrawLine(x0y1_, x0y0_, Color::Yellow);
}

float3 OcclusionDebugRenderer::Vector3ToFloat3(const Umbra::Vector3& vec)
{
    return float3(vec.v[0], vec.v[1], vec.v[2]);
}

Color OcclusionDebugRenderer::Vector4ToColor(const Umbra::Vector4& col)
{
    return Color(col.v[0], col.v[1], col.v[2], col.v[3]);
}

// MeshData

MeshData::MeshData()
{
    vertices_ = 0;
    indices_ = 0;
    indexCount_ = 0;
    vertexCount_ = 0;
}

MeshData::~MeshData()
{
    vertices_ = 0;
    indices_ = 0;

    indexCount_ = 0;
    vertexCount_ = 0;
}

void MeshData::GetVertexData(Ogre::Mesh *mesh)
{
    bool added_shared = false;
    size_t current_offset = 0;
    size_t shared_offset = 0;
    size_t next_offset = 0;
    size_t index_offset = 0;

    vertexCount_ = indexCount_ = 0;

    // Calculate how many vertices and indices we're going to need
    for(unsigned short i = 0; i < mesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh* submesh = mesh->getSubMesh(i);
        // We only need to add the shared vertices once
        if(submesh->useSharedVertices)
        {
            if(!added_shared)
            {
                added_shared = true;
                vertexCount_ += mesh->sharedVertexData->vertexCount;
            }
        }
        else
        {
            vertexCount_ += submesh->vertexData->vertexCount;
        }
        // Add the indices
        indexCount_ += submesh->indexData->indexCount;
    }

    // Allocate space for the vertices and indices
    Ogre::Vector3 *vertices = new Ogre::Vector3[vertexCount_];
    indices_ = new unsigned int[indexCount_];

    added_shared = false;

    // Run through the submeshes again, adding the data into the arrays
    for(unsigned short i = 0; i < mesh->getNumSubMeshes(); ++i)
    {
        Ogre::SubMesh* submesh = mesh->getSubMesh(i);

        Ogre::VertexData* vertex_data = submesh->useSharedVertices ? mesh->sharedVertexData : submesh->vertexData;

        if((!submesh->useSharedVertices) || (submesh->useSharedVertices && !added_shared))
        {
            if(submesh->useSharedVertices)
            {
                added_shared = true;
                shared_offset = current_offset;
            }

            const Ogre::VertexElement* posElem =
                vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);

            Ogre::HardwareVertexBufferSharedPtr vbuf =
                vertex_data->vertexBufferBinding->getBuffer(posElem->getSource());

            unsigned char* vertex =
                static_cast<unsigned char*>(vbuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));

            // There is _no_ baseVertexPointerToElement() which takes an Ogre::Real or a double
            //  as second argument. So make it float, to avoid trouble when Ogre::Real will
            //  be comiled/typedefed as double:
            //Ogre::Real* pReal;
            float* pReal;

            for(size_t j = 0; j < vertex_data->vertexCount; ++j, vertex += vbuf->getVertexSize())
            {
                posElem->baseVertexPointerToElement(vertex, &pReal);
                Ogre::Vector3 pt(pReal[0], pReal[1], pReal[2]);

                vertices[current_offset + j] = pt;
            }

            vbuf->unlock();
            next_offset += vertex_data->vertexCount;
        }

        Ogre::IndexData* index_data = submesh->indexData;
        size_t numTris = index_data->indexCount / 3;
        Ogre::HardwareIndexBufferSharedPtr ibuf = index_data->indexBuffer;

        bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);

        unsigned long* pLong = static_cast<unsigned long*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
        unsigned short* pShort = reinterpret_cast<unsigned short*>(pLong);

        size_t offset = (submesh->useSharedVertices)? shared_offset : current_offset;

        if(use32bitindexes)
        {
            for(size_t k = 0; k < numTris*3; ++k)
            {
                indices_[index_offset++] = pLong[k] + static_cast<unsigned long>(offset);
            }
        }
        else
        {
            for(size_t k = 0; k < numTris*3; ++k)
            {
                indices_[index_offset++] = static_cast<unsigned long>(pShort[k]) +
                    static_cast<unsigned long>(offset);
            }
        }

        ibuf->unlock();
        current_offset = next_offset;
    }

    // Put vertices to float array
    vertices_ = new float[vertexCount_ * 3];
    int index = 0;
    for(unsigned long i = 0; i < vertexCount_; ++i)
    {
        vertices_[index] = vertices[i].x;
        ++index;

        vertices_[index] = vertices[i].y;
        ++index;

        vertices_[index] = vertices[i].z;
        ++index;
    }
}


// EC_MeshmoonCulling

EC_MeshmoonCulling::EC_MeshmoonCulling(Scene *scene) :
    IComponent(scene),
    LC("EC_MeshmoonCulling: "),
    INIT_ATTRIBUTE_VALUE(tomeRef, "Tome ref", AssetReference("", "Binary")),
    INIT_ATTRIBUTE_VALUE(id, "Id", 0),
    INIT_ATTRIBUTE_VALUE(smallestOccluder, "Smallest occluder", 3),
    INIT_ATTRIBUTE_VALUE(smallestHole, "Smallest hole", 0.25f),
    INIT_ATTRIBUTE_VALUE(backfaceLimit, "Backface limit", 100),
    INIT_ATTRIBUTE_VALUE(tileSize, "Tile size", 24),
    INIT_ATTRIBUTE_VALUE(drawDebug, "Draw debug", false),
    umbraScene_(0),
    umbraTome_(0),
    umbraObjectList_(0),
    umbraQuery_(0),
    umbraDebugRenderer_(0),
    freezeOcclusionCamera_(false),
    tomePath_(""),
    maxWaitingTime_(10.0),
    lastVisibleObjects_(0),
    lastVisibleObjectsSize_(0)
{
    if (!framework)
    {
        LogError(LC + "TODO: Make EC_MeshmoonCulling work when created as unparented!");
        return;
    }
    if (framework->IsHeadless())
        return;

    tomeListener_ = MAKE_SHARED(AssetRefListener);

    connect(tomeListener_.get(), SIGNAL(Loaded(AssetPtr)), this, SLOT(OnLoadTome(AssetPtr)));
    connect(tomeListener_.get(), SIGNAL(TransferFailed(IAssetTransfer *, QString)), this, SLOT(OnTomeLoadingFailed(IAssetTransfer *, QString)));

    connect(framework->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)));

    OgreRenderer::OgreRenderingModule* renderingModule = framework->GetModule<OgreRenderer::OgreRenderingModule>();
    if(!renderingModule)
        return;

    renderer_ = renderingModule->GetRenderer();
    if(!renderer_.get())
        return;

    connect(renderer_.get(), SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)), Qt::UniqueConnection);
}

EC_MeshmoonCulling::~EC_MeshmoonCulling()
{
    disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
    disconnect(renderer_.get(), SIGNAL(MainCameraChanged(Entity *)), this, SLOT(OnActiveCameraChanged(Entity *)));

    Scene *tundraScene = ParentScene();
    if(tundraScene)
        disconnect(tundraScene, SIGNAL(OnComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), this, SLOT(OnComponentRemoved(Entity*, IComponent*, AttributeChange::Type)));

    Umbra::TomeLoader::freeTome(umbraTome_);
    umbraTome_ = 0;
    if(umbraDebugRenderer_)
        SAFE_DELETE(umbraDebugRenderer_);
    if(umbraQuery_)
        SAFE_DELETE(umbraQuery_);
    if(umbraObjectList_)
        SAFE_DELETE_ARRAY(umbraObjectList_);
    if(lastVisibleObjects_)
        SAFE_DELETE_ARRAY(lastVisibleObjects_);

    occluders_.clear();
    loadingOccluders_.clear();
    umbraScene_ = 0;
    tomeListener_.reset();
}

void EC_MeshmoonCulling::WaitForLoading()
{
    Scene *tundraScene = ParentScene();
    if(!tundraScene)
        return;

    qDebug() << "Wait for loading.";

    connect(tundraScene, SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)), this, SLOT(OnComponentRemoved(Entity*, IComponent*, AttributeChange::Type)));

    GetOcclusionComponents(tundraScene);

    qDebug() << "Store loading occluders...";

    loadingOccluders_.clear();
    foreach(EC_MeshmoonOccluder *occluder, occluders_)
        loadingOccluders_.append(occluder);

    qDebug() << "Set active camera...";
    OnActiveCameraChanged(renderer_->MainCamera());

    timeWaited_ = 0;

    disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
    connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)), Qt::UniqueConnection);
}

void EC_MeshmoonCulling::OnWaitForMeshes(float elapsedTime)
{
    for(int i = 0; i < loadingOccluders_.size(); ++i)
    {
        EC_MeshmoonOccluder *occluder = loadingOccluders_[i];
        if(!occluder)
        {
            loadingOccluders_.remove(i);
            if(loadingOccluders_.size() == 0)
                break;
            --i;
        }
        if(occluder->MeshReady())
        {
            loadingOccluders_.remove(i);
            if(loadingOccluders_.size() == 0)
                break;
            --i;
        }
    }

    timeWaited_ += elapsedTime;

    if(loadingOccluders_.size() > 0 && timeWaited_ < maxWaitingTime_)
        return;

    loadingOccluders_.clear();


    // Hide all objects
    foreach(EC_MeshmoonOccluder *occluder, occluders_)
        occluder->Hide();

    disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));
    connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
}

void EC_MeshmoonCulling::OnActiveCameraChanged(Entity *newMainWindowCamera)
{
    if(!newMainWindowCamera)
    {
        activeCamera_ = 0;
        activeCameraTransform_ = 0;
        return;
    }

    if(!newMainWindowCamera->Component<EC_Camera>().get())
        return;

    if(!newMainWindowCamera->Component<EC_Placeable>().get())
        return;

    activeCamera_ = newMainWindowCamera->Component<EC_Camera>().get();
    activeCameraTransform_ = newMainWindowCamera->Component<EC_Placeable>().get();

    float4x4 identityMatrix = float4x4::identity;
    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            umbraCameraTransform_.m[j][i] = identityMatrix.v[i][j];

    umbraFrustum_ = Umbra::Frustum(DegToRad(activeCamera_->verticalFov.Get()), activeCamera_->AspectRatio(), activeCamera_->nearPlane.Get(), activeCamera_->farPlane.Get());
    umbraCamera_ = Umbra::CameraTransform(umbraCameraTransform_, umbraFrustum_, Umbra::MF_COLUMN_MAJOR);
}

void EC_MeshmoonCulling::StartCulling()
{
    if(framework->IsHeadless())
        return;

    Scene *tundraScene = ParentScene();
    if(!tundraScene)
    {
        LogError(LC + "No scene found.");
        return;
    }

    GetOcclusionComponents(tundraScene);

    connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)), Qt::UniqueConnection);
}

void EC_MeshmoonCulling::OnLoadTome(AssetPtr tomeAsset)
{
    if(!tomeAsset.get())
        return;

    OnLoadTome(tomeAsset->RawData());

    LogInfo(LC + "Tome " + tomeAsset->Name() + " loaded.");
}

void EC_MeshmoonCulling::OnLoadTome(const QByteArray& bytes)
{
    Umbra::UINT8 *tomeBytes = new Umbra::UINT8[bytes.length()];

    for(int i = 0; i < bytes.length(); ++i)
        tomeBytes[i] = bytes[i];

    if(umbraTome_)
        Umbra::TomeLoader::freeTome(umbraTome_);

    umbraTome_ = Umbra::TomeLoader::loadFromBuffer(tomeBytes, bytes.length());

    delete tomeBytes;

    if(!umbraTome_)
    {
        LogError(LC + "Error while loading tome.");
        return;
    }

    if(umbraQuery_)
        SAFE_DELETE(umbraQuery_);
    if(umbraObjectList_)
        SAFE_DELETE(umbraObjectList_);
    if(umbraDebugRenderer_)
        SAFE_DELETE(umbraDebugRenderer_);

    umbraDebugRenderer_ = new OcclusionDebugRenderer(renderer_);

    // Create occlusion query
    umbraQuery_ = new Umbra::Query(umbraTome_);
    umbraQuery_->setDebugRenderer(umbraDebugRenderer_);

    umbraObjectList_ = new int[umbraTome_->getObjectCount()];
    umbraObjects_ = Umbra::IndexList(umbraObjectList_, umbraTome_->getObjectCount());

    lastVisibleObjects_ = new int[umbraTome_->getObjectCount()];
}


void EC_MeshmoonCulling::GenerateTome(const QString& fileName)
{
    if(framework->IsHeadless())
        return;

    tomePath_ = fileName;

    StartTomeGeneration();
}

void EC_MeshmoonCulling::OnTomeGenerated(const QByteArray& bytes)
{
    emit TomeGenerated(bytes);
}

void EC_MeshmoonCulling::OnTomeLoadingFailed(IAssetTransfer *transfer, QString reason)
{
    if(framework->IsHeadless())
        return;

    LogError("MeshmoonCulling: Tome loading failed: " + reason);
}

void EC_MeshmoonCulling::AttributesChanged()
{
    if(framework->IsHeadless())
        return;
    if(!tomeRef.ValueChanged())
        return;

    QString ref = tomeRef.Get().ref.trimmed();
    if(!ref.endsWith(MESHMOON_TOME_SUFFIX, Qt::CaseInsensitive))
    {
        LogError(LC + "Asset ref is not tome file.");
        return;
    }
    else
        LogInfo(LC + "Valid tome file.");

    QTimer::singleShot(1000, this, SLOT(WaitForLoading()));

    tomeListener_->HandleAssetRefChange(framework->Asset(), ref, "Binary");
}

void EC_MeshmoonCulling::GetOcclusionComponents(Scene *scene)
{
    if(framework->IsHeadless())
        return;

    occluders_.clear();
    EntityList occlusionEntities = scene->EntitiesWithComponent(EC_MeshmoonOccluder::TypeIdStatic());
    if(occlusionEntities.empty())
    {
        LogError(LC + "No objects marked for occlusion culling.");
        return;
    }

    for(EntityList::iterator it = occlusionEntities.begin(); it != occlusionEntities.end(); ++it)
    {
        EntityPtr entity = (*it);
        Entity::ComponentVector occlusionComponents = entity->ComponentsOfType(EC_MeshmoonOccluder::TypeIdStatic());
        for(unsigned int i = 0; i < occlusionComponents.size(); ++i)
        {
            EC_MeshmoonOccluder *occluder = static_cast<EC_MeshmoonOccluder*>(occlusionComponents[i].get());
            if(!occluder)
                continue;

            if(occluder->tomeId.Get() != id.Get())
                continue;

            occluders_.insert(occluder->id.Get(), occluder);
        }
    }
}

void EC_MeshmoonCulling::StartTomeGeneration()
{
    if(framework->IsHeadless())
        return;

    Scene *tundraScene = ParentScene();
    if(!tundraScene)
    {
        LogError(LC + "No scene found.");
        return;
    }

    StopCulling("Starting tome generation...");

    // Get all EC_MeshmoonOccluders
    GetOcclusionComponents(tundraScene);

    // Initialize Umbra scene
    umbraScene_ = Umbra::Scene::create();

    foreach(EC_MeshmoonOccluder *occluder, occluders_)
    {
        EC_Mesh *mesh = occluder->Mesh();
        if(!mesh)
            continue;

        if(!mesh->HasMesh())
            continue;

        Ogre::Mesh *ogreMesh = mesh->OgreEntity()->getMesh().get();
        if(!ogreMesh)
            continue;

        // Create Umbra model
        MeshData meshData;
        meshData.GetVertexData(ogreMesh);
        const Umbra::SceneModel *umbraModel = umbraScene_->insertModel(meshData.vertices_, meshData.indices_, meshData.vertexCount_, meshData.indexCount_ / 3);

        // Convert MathGeoLib matrix to Umbra matrix
        float4x4 placeableTransform = mesh->LocalToWorld();

        Umbra::Matrix4x4 umbraTransform;
        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                umbraTransform.m[j][i] = placeableTransform.v[i][j];

        // Insert object to umbra scene
        int flags = 0;
        if(occluder->occluder.Get())
            flags |= Umbra::SceneObject::OCCLUDER;
        if(occluder->target.Get())
            flags |= Umbra::SceneObject::TARGET;

        umbraScene_->insertObject(umbraModel, umbraTransform, occluder->id.Get(), flags, Umbra::MF_COLUMN_MAJOR, Umbra::WINDING_CCW);
    }

    QString sceneFile = framework->Asset()->GenerateTemporaryNonexistingAssetFilename(".scene");
    umbraScene_->writeToFile(sceneFile.toStdString().c_str());

    if(occluders_.empty())
    {
        LogError(LC + "No object marked for occlusion culling.");
        umbraScene_->release();
        umbraScene_ = 0;
        return;
    }

    umbraTask_ = Umbra::Task::create(umbraScene_);
    if(!umbraTask_)
    {
        LogError(LC + "Error while creating Umbra task.");
        return;
    }

    Umbra::ComputationParams params;
    params.setParam(Umbra::ComputationParams::SMALLEST_OCCLUDER, smallestOccluder.Get());
    params.setParam(Umbra::ComputationParams::SMALLEST_HOLE, smallestHole.Get());
    params.setParam(Umbra::ComputationParams::BACKFACE_LIMIT, (Umbra::UINT32)backfaceLimit.Get());
    params.setParam(Umbra::ComputationParams::TILE_SIZE, tileSize.Get());

    // Set view volume
    Umbra::Vector3 mn = { -1000.f, -1000.f, -1000.f };
    Umbra::Vector3 mx = {  1000.f,  1000.f,  1000.f };
    umbraScene_->insertViewVolume(mn, mx, 0);

    umbraTask_->setComputationParams(params);

    umbraTask_->start(".");
    umbraTask_->waitForFinish();
    //connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForTomeCalculation(float)), Qt::UniqueConnection);
    //QTimer::singleShot(1000, this, SLOT(OnWaitForTomeCalculation()));

    if(umbraTask_->getError() != Umbra::Task::ERROR_OK)
    {
        LogError(LC + "Tome generation failed: " + umbraTask_->getErrorString());

        umbraTask_->release();
        umbraScene_->release();
        umbraScene_ = 0;

        //disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));

        emit TomeGenerationFailed();
        return;
    }

    if(!umbraTask_->isFinished())
        return;

    //disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));

    int tomeBufferSize = umbraTask_->getTomeSize();
    Umbra::UINT8 *tomeBuffer = new Umbra::UINT8[tomeBufferSize];
    if(!umbraTask_->getTome(tomeBuffer, tomeBufferSize))
    {
        LogError(LC + "Failed to get computed Tome data.");
        umbraTask_->release();
        delete[] tomeBuffer;
        return;
    }

    QByteArray tomeBytes;
    tomeBytes.clear();
    for(int i = 0; i < tomeBufferSize; ++i)
        tomeBytes.append(tomeBytes[i]);

    LogInfo(LC + "Tome generated. Tome size: " + QString::number(tomeBufferSize));

    umbraTask_->writeTomeToFile(tomePath_.toStdString().c_str());

    tomeBytes.clear();
    QFile inFile(tomePath_);
    if(inFile.open(QIODevice::ReadOnly))
    {
        tomeBytes = inFile.readAll();
        inFile.close();
    }

    emit TomeGenerated(tomePath_);

    umbraTask_->release();
    umbraScene_->release();
    umbraScene_ = 0;
}

void EC_MeshmoonCulling::OnWaitForTomeCalculation(/* float elapsedTime */)
{
    if(!umbraTask_)
    {
        //disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));
        return;
    }

    float progress = umbraTask_->getProgress() * 100;

    if(umbraTask_->getError() != Umbra::Task::ERROR_OK)
    {
        switch(umbraTask_->getError())
        {
        case Umbra::Task::ERROR_UNKNOWN:
            LogError(LC + "Tome generation failed!: Unknown error occurred.");
            break;
        case Umbra::Task::ERROR_EXECUTABLE_NOT_FOUND:
            LogError(LC + "Tome generation failed: Executable for setRunAsProcess() was not found in the given path.");
            break;
        case Umbra::Task::ERROR_INVALID_FILE:
            LogError(LC + "Tome generation failed: Invalid file or filename.");
            break;
        case Umbra::Task::ERROR_INVALID_PATH:
            LogError(LC + "Tome generation failed: Invalid path.");
            break;
        case Umbra::Task::ERROR_INVALID_SCENE:
            LogError(LC + "Tome generation failed: Invalid scene.");
            break;
        case Umbra::Task::ERROR_ALREADY_RUNNING:
            LogError(LC + "Tome generation failed: Cannot run two computations simultaneously.");
            break;
        case Umbra::Task::ERROR_ABORTED:
            LogError(LC + "Tome generation failed: Computation aborted.");
            break;
        case Umbra::Task::ERROR_OUT_OF_MEMORY:
            LogError(LC + "Tome generation failed: Out of memory.");
            break;
        case Umbra::Task::ERROR_PROCESS:
            LogError(LC + "Tome generation failed: Failed to create process.");
            break;
        case Umbra::Task::ERROR_PARAM:
            LogError(LC + "Tome generation failed: Bad computation parameters.");
            break;
        case Umbra::Task::ERROR_EVALUATION_EXPIRED:
            LogError(LC + "Tome generation failed: Expired evaluation key, invalid key or license file not found.");
            break;
        }

        umbraTask_->release();
        umbraScene_->release();
        umbraScene_ = 0;

        //disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));

        emit TomeGenerationFailed();
        return;
    }

    if(!umbraTask_->isFinished())
        QTimer::singleShot(1000, this, SLOT(OnWaitForTomeCalculation()));

    //disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnWaitForMeshes(float)));

    int tomeBufferSize = umbraTask_->getTomeSize();
    Umbra::UINT8 *tomeBuffer = new Umbra::UINT8[tomeBufferSize];
    if(!umbraTask_->getTome(tomeBuffer, tomeBufferSize))
    {
        LogError(LC + "Failed to get computed Tome data.");
        umbraTask_->release();
        delete[] tomeBuffer;
        return;
    }

    QByteArray tomeBytes;
    tomeBytes.clear();
    for(int i = 0; i < tomeBufferSize; ++i)
        tomeBytes.append(tomeBytes[i]);

    LogInfo(LC + "Tome generated. Tome size: " + QString::number(tomeBufferSize));

    umbraTask_->writeTomeToFile(tomePath_.toStdString().c_str());

    tomeBytes.clear();
    QFile inFile(tomePath_);
    if(inFile.open(QIODevice::ReadOnly))
    {
        tomeBytes = inFile.readAll();
        inFile.close();
    }

    emit TomeGenerated(tomePath_);

    umbraTask_->release();
    umbraScene_->release();
    umbraScene_ = 0;
}

void EC_MeshmoonCulling::OnUpdate(float /* elapsedTime */)
{
    PROFILE(Rocket_Occlusion_Update);

    if(occluders_.empty())
        return;
    if(!umbraTome_)
        return;
    if(!activeCamera_)
        return;
    if(!activeCameraTransform_)
        return;

    if(!freezeOcclusionCamera_)
    {
        PROFILE(Rocket_Occlusion_Debug_Camera);
        Quat camOrientation = activeCameraTransform_->WorldOrientation();
        camOrientation = camOrientation * Quat(float3(0, 1, 0), DegToRad(180));
        float4x4 cameraMatrix(camOrientation, activeCameraTransform_->WorldPosition());

        for(int i = 0; i < 4; ++i)
            for(int j = 0; j < 4; ++j)
                umbraCameraTransform_.m[j][i] = cameraMatrix.v[i][j];
        ELIFORP(Rocket_Occlusion_Debug_Camera);
    }

    // Set Umbra camera matrix
    umbraCamera_.setCameraToWorld(umbraCameraTransform_);

    PROFILE(Rocket_Occlusion_Umbra_Query);
    umbraQuery_->queryPortalVisibility(
        drawDebug.Get() ? Umbra::Query::DEBUGFLAG_VIEW_FRUSTUM | Umbra::Query::DEBUGFLAG_OBJECT_BOUNDS : 0,
        Umbra::Visibility(&umbraObjects_, NULL), umbraCamera_);
    ELIFORP(Rocket_Occlusion_Umbra_Query);

    Scene *tundraScene = ParentScene();
    if(!tundraScene)
        return;

    PROFILE(Rocket_Occlusion_Umbra_Set_Visibility);

    for(int i = 0;  i < lastVisibleObjectsSize_; ++i)
    {
        EC_MeshmoonOccluder *occluder = occluders_[lastVisibleObjects_[i]];
        if (occluder)
            occluder->Hide();
    }

    lastVisibleObjectsSize_ = umbraObjects_.getSize();
    int* visibleObjects = umbraObjects_.getPtr();
    for(int i = 0; i < umbraObjects_.getSize(); ++i)
    {
        Umbra::UINT32 obj = umbraTome_->getObjectUserID(visibleObjects[i]);
        EC_MeshmoonOccluder *occluder = occluders_[obj];
        if (occluder)
            occluder->Show();

        lastVisibleObjects_[i] = obj;
    }

    ELIFORP(Rocket_Occlusion_Umbra_Set_Visibility);
    ELIFORP(Rocket_Occlusion_Update);
}

void EC_MeshmoonCulling::OnKeyPress(KeyEvent* e)
{
    if (!e || e->eventType != KeyEvent::KeyPressed || e->IsRepeat())
        return;

    QKeySequence toggleFreeze = QKeySequence(Qt::ShiftModifier +  Qt::Key_O);

    if(e->Sequence() == toggleFreeze)
    {
        if(freezeOcclusionCamera_)
            freezeOcclusionCamera_ = false;
        else
            freezeOcclusionCamera_ = true;

        if(freezeOcclusionCamera_)
            LogInfo(LC + "Occlusion camera freezed.");
    }
}

void EC_MeshmoonCulling::StopCulling(const QString& message)
{
    foreach(EC_MeshmoonOccluder *occluder, occluders_)
        occluder->Show();

    LogWarning(message);

    disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
}

void EC_MeshmoonCulling::OnComponentRemoved(Entity *entity, IComponent *component, AttributeChange::Type change)
{
    QHash<int, EC_MeshmoonOccluder*>::iterator it = occluders_.begin();
    for(; it != occluders_.end(); ++it)
    {
        EC_MeshmoonOccluder *occluder = (*it);
        if(component == occluder)
        {
            StopCulling(LC + QString("An occluder object has been removed (Entity #%1 %2). Occlusion culling stopped.").arg(entity->Id()).arg(component->TypeName()));
            occluders_.erase(it);
            break;
        }
        else if(component == occluder->Mesh())
        {
            StopCulling(LC + QString("Mesh that was used for occlusion culling is removed (Entity #%1 %2). Occlusion culling stopped.").arg(entity->Id()).arg(component->TypeName()));
            occluders_.erase(it);
            break;
        }
    }
}

#endif
