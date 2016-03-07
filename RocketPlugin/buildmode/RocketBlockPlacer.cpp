/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBlockPlacer.cpp
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketBlockPlacer.h"
#include "RocketBuildEditor.h"
#include "RocketBuildWidget.h"
#include "RocketPlugin.h"

#include "MeshmoonAssetLibrary.h"
#include "storage/MeshmoonStorage.h"

#include "Geometry/Ray.h"
#include "Geometry/Plane.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "Profiler.h"
#include "Framework.h"
#include "InputAPI.h"
#include "IRenderer.h"
#include "OgreWorld.h"
#include "Renderer.h"
#include "EC_Camera.h"
#include "FrameAPI.h"
#include "ConfigAPI.h"
#include "AssetAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QFileInfo>

#include "MemoryLeakCheck.h"

namespace
{
    const ushort cNumObbFaces = 6;
    const float cDefaultInteractionDistance = 40.f;

    const AssetReference cBlockPlacerMaterialRef("Ogre Media:BlockPlacer.material", "OgreMaterial");
    const AssetReference cBlockRemoverMaterialRef("Ogre Media:BlockRemover.material", "OgreMaterial");

    const ConfigData cBlockMaxDistSetting("adminotech", "build mode", "max block distance", cDefaultInteractionDistance);
    const ConfigData cBlockPlacerSetting("adminotech", "build mode", "placer mode", RocketBlockPlacer::CreateBlock);
    const ConfigData cBlockPlacementSetting("adminotech", "build mode", "placement mode", RocketBlockPlacer::SnapPlacement);
    const ConfigData cBlockMeshSetting("adminotech", "build mode", "mesh");
    const ConfigData cBlockMaterialSetting("adminotech", "build mode", "material");

    const char * const cBuildingBlockTag = "building block";
    const char * const cEmptyMaterialRef = "empty";

    Plane FindNearestObbFace(const OBB &obb, const float3 &point)
    {
        Plane faces[cNumObbFaces];
        obb.GetFacePlanes(faces);
        size_t faceIndex = 0;
        float closestDistance = 1e9f;

        for(size_t i = 0; i < cNumObbFaces; ++i)
        {
            const float d = faces[i].Distance(point);
            if (d < closestDistance)
            {
                closestDistance = d;
                faceIndex = i;
            }
        }

        return faces[faceIndex];
    }
}

RocketBlockPlacer::RocketBlockPlacer(RocketPlugin *plugin) :
    rocket(plugin),
    blockMeshAsset(0),
    placerMode(CreateBlock),
    placementMode(SnapPlacement),
    maxBlockDistance(cDefaultInteractionDistance),
    maxBlockDistanceSphere(float3::inf, FLOAT_INF)
{
    ResetGhostBlocks();
    connect(&maxBlockDistanceTimer, SIGNAL(timeout()), this, SLOT(ResetMaxBlockDistancePlane()));

    rocket->AssetLibrary()->UpdateLibrary();
    connect(rocket->AssetLibrary(), SIGNAL(LibraryUpdated(const MeshmoonAssetList&)), this, SLOT(InitializeAssetSelection()));
}

RocketBlockPlacer::~RocketBlockPlacer()
{
    ResetGhostBlocks();
    maxBlockDistanceTimer.stop();
    SaveSettingsToConfig();
    Remove();
}

void RocketBlockPlacer::SetBlockPlacerMode(PlacerMode mode)
{
    const bool changed = (placerMode != mode);

    if (placerMode == RocketBlockPlacer::CreateBlock && blockMeshAsset)
        SaveVisualsToConfig(); // Changing to non-create mode: save the current visuals.

    placerMode = mode;

    if (placerMode == RocketBlockPlacer::CreateBlock)
        LoadVisualsFromConfig(); // Changed to create mode: load the previously used visuals.

    Refresh();

    if (changed)
        emit SettingsChanged();
}

void RocketBlockPlacer::SetBlockPlacementMode(PlacementMode mode)
{
    const bool changed = (placementMode != mode);

    placementMode = mode;
    Refresh();

    if (changed)
        emit SettingsChanged();
}

void RocketBlockPlacer::ResetGhostBlocks()
{
    for(size_t i = 0; i < cNumObbFaces; ++i)
        ghostObbs[i].SetNegativeInfinity();
}

void RocketBlockPlacer::GenerateGhostBlocks(const OBB &hitObb)
{
    const float adjust = 0.980625f; /**< @todo hacking around the gaps */
    Plane faces[cNumObbFaces];
    hitObb.GetFacePlanes(faces);
    for(size_t i = 0; i < cNumObbFaces; ++i)
    {
        OBB obb = hitObb;
        if (faces[i].normal.Cross(obb.axis[0]).IsZero())
            obb.pos = obb.pos + adjust * 2.f * faces[i].normal * obb.r[0];
        else if (faces[i].normal.Cross(obb.axis[1]).IsZero())
            obb.pos = obb.pos + adjust * 2.f * faces[i].normal * obb.r[1];
        else if (faces[i].normal.Cross(obb.axis[2]).IsZero())
            obb.pos = obb.pos + adjust * 2.f * faces[i].normal * obb.r[2];
        ghostObbs[i] = obb;
    }
}

void RocketBlockPlacer::Hide()
{
    shared_ptr<EC_Placeable> placeable = placerPlaceable.lock();
    if (placeable && placeable->visible.Get())
        placeable->visible.Set(false, AttributeChange::Default);
    if (!lastBlockUnderMouse.expired())
        SetActiveBlock(EntityPtr());
    ResetGhostBlocks();
}

bool RocketBlockPlacer::IsVisible() const
{
    return !placerPlaceable.expired() && placerPlaceable.lock()->visible.Get();
}

void RocketBlockPlacer::SetActiveBlock(const EntityPtr &block)
{
    lastBlockUnderMouse = block;

    /// @todo Had to comment out the following line due to multi-edit refactoring.
    /// As a side effects, at least red OBB and context menu for "item under mouse" are not shown.
    //rocket->BuildEditor()->SetSelection(block);

    EC_Mesh *mesh = (block ? block->Component<EC_Mesh>().get() : 0);
    if (mesh)
    {
        // Block hit: generate a set of "ghost" block OBBs around the last block under the mouse so that we don't need to hit
        // the block exactly with a raycast in order to be able to place a new block next to that.
        if (mesh->HasMesh())
            GenerateGhostBlocks(mesh->WorldOBB());
        else // Mesh not ready yet: delay ghost block generation.
            connect(mesh, SIGNAL(MeshChanged()), SLOT(GenerateGhostBlocksForCurrentBlock()), Qt::UniqueConnection);
    }
    else
    {
        ResetGhostBlocks();
        //blockMeshAsset = 0;
    }
}

void RocketBlockPlacer::Update()
{
    /// @note If modifying this code, be super-duper extra careful to check that all the placer and placement modes and their permutations work correctly!
    if (placerMode == EditBlock)
        return;
    OgreWorldPtr world = ogreWorld.lock();
    if (!world)
        return;
    shared_ptr<EC_Placeable> placeable = placerPlaceable.lock();
    if (!placeable)
        return;

    PROFILE(RocketBlockPlacer_Update);
    const QPoint mousePos = rocket->GetFramework()->Input()->MousePos();
    if (rocket->GetFramework()->Input()->ItemAtCoords(mousePos) ||
        !rocket->GetFramework()->Ui()->MainWindow()->geometry().contains(QCursor::pos()) ||
        rocket->GetFramework()->Ui()->MainWindow()->menuBar()->geometry().contains(mousePos))
    {
        // Mouse on top of UI, menu bar, or out the main window: hide placer and do nothing.
        if (placeable->visible.Get())
            Hide();
        return;
    }

    // Ghost OBB placement, uses the closest hit OBB. Ghost obb hit takes precedence over raycast hit when in Add + Snap mode.
    const float maxBlockDistanceSq = MaxBlockDistance() * MaxBlockDistance();
    // Ignore maxBlockDistance always in remove mode.
    const bool ignoreMaxBlockDist = (placerMode == RemoveBlock || maxBlockDistanceSq <= 0.f);
    Entity *cam = world->Renderer()->MainCamera();
    Ray mouseRay = cam->Component<EC_Camera>()->ScreenPointToRay(mousePos);
    const float3 camWorldPos = cam->Component<EC_Placeable>()->WorldPosition();
    OBB closestHitObb;
    //float dNear; // distance from mouseRay's origin to the OBB intersection point
    closestHitObb.pos = float3::inf; // If closestHitObb.IsFinite() == true, we have hit a ghost OBB.
    //float3 obbHitPoint;

    if (placementMode == SnapPlacement && placerMode != RemoveBlock && !lastBlockUnderMouse.expired() && ghostObbs[0].IsFinite())
    {
        float closestDistanceToCam = 1e9f;
        for(size_t i = 0; i < cNumObbFaces; ++i)
            if (mouseRay.Intersects(ghostObbs[i]))
            {
                const float distance = ghostObbs[i].pos.DistanceSq(camWorldPos);
                if (distance < closestDistanceToCam && (ignoreMaxBlockDist || distance < maxBlockDistanceSq))
                {
                    //obbHitPoint = mouseRay.GetPoint(dNear);
                    closestHitObb = ghostObbs[i];
                    closestDistanceToCam = distance;
                }
            }
    }
    else if (ghostObbs[0].IsFinite())
        ResetGhostBlocks();

//    world->DebugDrawSphere(obbHitPoint, 1, 384, 0, 0, 1, false);

    // Raycast, ignore hits to non-mesh and avatar objects and objects that are more far than the maxBlockDistance (if applicable)
    RaycastResult *result = (ignoreMaxBlockDist ? world->Raycast(mousePos) : world->Raycast(mousePos, maxBlockDistance));
    EntityPtr hitEntity = (result && result->entity && result->entity->Component<EC_Mesh>() && !IsEntityAnAvatar(result->entity) &&
        (ignoreMaxBlockDist || result->pos.DistanceSq(camWorldPos) < maxBlockDistanceSq) ? result->entity->shared_from_this() : EntityPtr());

    // Use the raycast result, if:
    // -in remove mode,
    // -we did not hit any ghost,
    // -hit ghost, but the camera is inside the hit OBB (can happen for large and irregularly shaped objects (terrain f.ex.), or
    // -both ghost and raycast hit but the hit entity is more near than the ghost OBB.
    const bool useRaycastResult = (placerMode == RemoveBlock || !closestHitObb.IsFinite() ||
        (closestHitObb.IsFinite() && closestHitObb.Contains(mouseRay.pos)) ||
        (hitEntity && closestHitObb.IsFinite() && result->pos.DistanceSq(camWorldPos) < closestHitObb.pos.DistanceSq(camWorldPos)));

    if (hitEntity && hitEntity != lastBlockUnderMouse.lock() && useRaycastResult)
        SetActiveBlock(hitEntity);

    // Cloning and removal: clone appearance from the hit.
    if (placerMode == CloneBlock || placerMode == RemoveBlock)
    {
        // Cloning and removal: clone appearance from the hit.
        EC_Mesh *blockMesh = placerMesh.lock().get();
        EC_Mesh *hitMesh = (hitEntity ? hitEntity->Component<EC_Mesh>().get() : 0);
        if (blockMesh && hitMesh && useRaycastResult)
        {
            // 1) Mesh, if different than ours.
            if (hitMesh->meshRef.Get() != blockMesh->meshRef.Get())
            {
                blockMesh->meshRef.Set(hitMesh->meshRef.Get(), AttributeChange::LocalOnly);
                AssetReferenceList materials("OgreMaterial");
                const uint numMaterials = hitMesh->NumMaterials();
                for(uint i = 0; i < numMaterials; ++i)
                    materials.Append(placerMode == RemoveBlock ? cBlockRemoverMaterialRef : cBlockPlacerMaterialRef);
                blockMesh->materialRefs.Set(materials, AttributeChange::LocalOnly);
            }

            // 2) Mesh transform
            blockMesh->nodeTransformation.Set(hitMesh->nodeTransformation.Get(), AttributeChange::LocalOnly);

            // 3) World transform, pos will be overwritten later on for Cloning
            placeable->SetWorldTransform(hitEntity->Component<EC_Placeable>()->WorldTransform());
            if (placerMode == RemoveBlock)
                placeable->SetScale(placeable->WorldScale() * 1.02f); // Make sure that the placer "swallows" the target
        }
        else if (placerMode == RemoveBlock) // No mesh hit, hide the placer and empty selection.
        {
            if (placeable->visible.Get())
                placeable->visible.Set(false, AttributeChange::LocalOnly);

            if (!lastBlockUnderMouse.expired())
                SetActiveBlock(EntityPtr());
        }
    }

    // Snap mode orientation
    if (placementMode == SnapPlacement && !lastBlockUnderMouse.expired())
        placeable->SetOrientation(lastBlockUnderMouse.lock()->Component<EC_Placeable>()->WorldOrientation());

    // Positioning
    if (hitEntity && useRaycastResult)
    {
        //const bool isPrimitiveBlock = result->entity->Description().contains(cBuildingBlockTag, Qt::CaseInsensitive);
        EC_Mesh *hitMesh = hitEntity->Component<EC_Mesh>().get();
        if (hitMesh && hitMesh->HasMesh() && placerMesh.lock() && placerMesh.lock()->HasMesh()) // The mesh assets must be there in order to get proper OBB information.
        {
            const OBB hitObb = hitMesh->WorldOBB();
            OBB placerObb = placerMesh.lock()->WorldOBB();

            if (placerMode == CreateBlock || placerMode == CloneBlock)
            {
                placerObb.pos = (placementMode == SnapPlacement ? hitObb.pos : result->pos);
                const float3 dir = (placementMode == SnapPlacement ? FindNearestObbFace(hitObb, result->pos).normal : result->normal);
                //if (isPrimitiveBlock)
                if (placementMode == SnapPlacement)
                {
                    while(hitObb.Intersects(placerObb))
                        placerObb.pos += 0.001f * dir;
                }
                //else
                //{
                //    Polyhedron hitPolyhderon = rocket->BuildEditor()->GeneratePolyhderonForMesh(hitMesh->MeshAsset()->ogreMesh.get());
                //    while(hitPolyhderon.Intersects(placerObb))
                //        placerObb.pos += 0.001f * result->normal;
                //}
                placeable->SetPosition(placerObb.pos);
            }
        }
/*
        else // Currently no world bounding volume information available for non-mesh objects, simply use the raycast hit pos.
        {
            placeable->SetPosition(result->pos);
        }
*/
//        if (isPrimitiveBlock)
//            placeable->SetOrientation(hitplaceable->WorldOrientation());//Quat::LookAt(-float3::unitX, result->normal, float3::unitY, float3::unitY));
//        else
//            placeable->SetOrientation(Quat::identity);
    }
    else
    {
        if (closestHitObb.IsFinite())
            placeable->SetPosition(closestHitObb.pos);
        else // No ghost obb was hit, position the placer to the mouse ray direction.
            placeable->SetPosition(camWorldPos + MaxBlockDistance() * mouseRay.dir);

        if ((placerMode == RemoveBlock || (placerMode == CloneBlock && !closestHitObb.IsFinite())) && placeable->visible.Get())
            placeable->visible.Set(false, AttributeChange::Default);

        if (!closestHitObb.IsFinite() && !lastBlockUnderMouse.expired()) // No entity nor ghost block hit: reset orientation and lastBlockUnderMouse.
        {
            placeable->SetOrientation(Quat::identity);
            SetActiveBlock(EntityPtr());
        }
    }

    if ((hitEntity && !placeable->visible.Get()) || (placerMode == CreateBlock && !placeable->visible.Get()))
        placeable->visible.Set(true, AttributeChange::Default);
}

void RocketBlockPlacer::DrawVisualHelp()
{
    OgreWorldPtr world = ogreWorld.lock();
    if (!world)
        return;

    if (!lastBlockUnderMouse.expired())
    {
        if ((placerMode == CreateBlock || placerMode == CloneBlock) && placementMode == SnapPlacement && ghostObbs[0].IsFinite())
            for(size_t i = 0; i < cNumObbFaces; ++i)
                world->DebugDrawOBB(ghostObbs[i], Color::Green);
    }

    if (maxBlockDistanceSphere.IsFinite())
    {
        maxBlockDistanceSphere.pos = ogreWorld.lock()->Renderer()->MainCamera()->Component<EC_Placeable>()->WorldPosition();
        world->DebugDrawSphere(maxBlockDistanceSphere.pos, maxBlockDistanceSphere.r, 240*3*3*3, Color::White);
    }
}

void RocketBlockPlacer::Create()
{
    if (!ogreWorld.expired() && !placerEntity)
    {
        placerEntity = ogreWorld.lock()->Scene()->CreateLocalEntity(QStringList(), AttributeChange::Default, false, true);
        placerEntity->SetName("RocketBlockPlacer");
        placerPlaceable = placerEntity->CreateComponent<EC_Placeable>();
        placerPlaceable.lock()->selectionLayer.Set(0, AttributeChange::Default); // ignore regular raycasts
        placerMesh = placerEntity->CreateComponent<EC_Mesh>();
        connect(placerMesh.lock().get(), SIGNAL(MeshChanged()), SLOT(GeneratePolyhderonForBlockPlacer()), Qt::UniqueConnection);
        connect(placerMesh.lock().get(), SIGNAL(MeshChanged()), SLOT(AllocateMaterialSlots()), Qt::UniqueConnection);
    }
}

void RocketBlockPlacer::Remove()
{
    Hide();

    if (placerEntity)
    {
        Scene *scene = placerEntity->ParentScene();
        entity_id_t id = placerEntity->Id();
        placerEntity.reset();
        if (scene)
            scene->RemoveEntity(id);
    }
}

void RocketBlockPlacer::Refresh()
{
    if (placerMode == EditBlock)
    {
        blockMeshAsset = 0;
        SetActiveBlock(EntityPtr());
        Remove();
        return;
    }

    if (!placerEntity) // Create block placer
    {
        Create();
        assert(placerEntity);
        if (!placerEntity)
            return;
    }

    EC_Placeable *placeable = placerPlaceable.lock().get();
    EC_Mesh *mesh = placerMesh.lock().get();
    if (!placeable || !mesh)
        return;

    // Only update mesh shape if we are in add mode. Do this before material check as this mesh change may change the material count or load a new mesh.
    bool shapeChanged = false;
    if (placerMode == RocketBlockPlacer::CreateBlock && blockMeshAsset && mesh->meshRef.Get().ref.compare(blockMeshAsset->assetRef, Qt::CaseSensitive) != 0)
    {
        // Set new mesh ref and reset materials as the submesh count can be different (leaving the old material list will span errors)
        mesh->materialRefs.Set(AssetReferenceList(), AttributeChange::Default);
        mesh->meshRef.Set(blockMeshAsset->AssetReference(), AttributeChange::Default);
        shapeChanged = true;
    }

    // Make sure that the placer materials are applied when the mesh is loaded. If the mesh was swapped we should rather listen to the MeshChanged signal.
    if (mesh->HasMesh() && !shapeChanged)
        ApplyMaterial();
    connect(mesh, SIGNAL(MeshChanged()), SLOT(ApplyMaterial()), Qt::UniqueConnection);

    // Transform
    placeable->SetScale(float3::one);
    placeable->SetPosition(ComputePositionInFrontOfCamera(ogreWorld.lock()->Renderer()->MainCamera(), MaxBlockDistance()));

    SetActiveBlock(EntityPtr());
}

void RocketBlockPlacer::SetMaxBlockDistance(float distance)
{
    const bool changed = (!EqualAbs(maxBlockDistance, distance, 0.1f));

    maxBlockDistance = Clamp(distance, 0.f, 200.f); // 200 is just some arbitrary max for now, could be bigger if wanted.
    if (maxBlockDistance > 0.f && !ogreWorld.expired())
    {
        // Show visualization for two seconds, restart timer if not yet expired.
        if (maxBlockDistanceTimer.isActive())
            maxBlockDistanceTimer.stop();
        maxBlockDistanceTimer.start(2000);

        maxBlockDistanceSphere.pos = ogreWorld.lock()->Renderer()->MainCamera()->Component<EC_Placeable>()->WorldPosition();
        maxBlockDistanceSphere.r = maxBlockDistance;
    }
    else
        ResetMaxBlockDistancePlane();

    if (changed)
        emit SettingsChanged();
}

MeshmoonAsset *RocketBlockPlacer::GetOrCreateCachedStorageAsset(MeshmoonAsset::Type type, const QString &absoluteAssetRef, QString name)
{
    MeshmoonAsset *storageAsset = storageAssetCache.value(absoluteAssetRef, 0);
    if (!storageAsset)
    {
        if (name.isEmpty())
            name = absoluteAssetRef.split("/", QString::SkipEmptyParts).last();

        // Convert '-' and '_' to spaces and start the name with upper case char.
        QString displayName = QFileInfo(name).baseName().replace("-", " ").replace("_", " ");
        displayName = displayName.left(1).toUpper() + displayName.mid(1);
        storageAsset = new MeshmoonAsset(displayName, absoluteAssetRef, type, this);
        storageAssetCache[absoluteAssetRef] = storageAsset;
    }
    return storageAsset;
}

void RocketBlockPlacer::SetMeshAsset(MeshmoonAsset *mesh)
{
    if (!mesh)
        return;

    const bool changed = (mesh != blockMeshAsset);

    if (placerMode == EditBlock && rocket->BuildEditor()->ActiveEntity() && rocket->BuildEditor()->ActiveEntity()->Component<EC_Mesh>().get())
        rocket->BuildEditor()->ActiveEntity()->Component<EC_Mesh>()->meshRef.Set(mesh->AssetReference(), AttributeChange::Default);

    blockMeshAsset = mesh;

    if (placerMode == EditBlock)
        blockMaterials.clear();

    if (placerMode != EditBlock)
        Refresh();

    if (changed)
        emit SettingsChanged();
}

void RocketBlockPlacer::SetMaterialAsset(uint submeshIndex, MeshmoonAsset *material)
{
    if (!material)
        return;

    while(submeshIndex >= (uint)blockMaterials.size())
        blockMaterials.push_back(BlockMaterial());

    const bool changed = (material != blockMaterials[submeshIndex].libraryAsset);

    // If editing set material to the active entity
    if (placerMode == EditBlock && rocket->BuildEditor()->ActiveEntity())
    {
        // Don't use EC_Mesh::SetMaterial, it is buggy as it does not always apply correctly!
        EC_Mesh *mesh = rocket->BuildEditor()->ActiveEntity()->Component<EC_Mesh>().get();
        if (mesh && mesh->NumSubMeshes() >= submeshIndex)
        {
            AssetReferenceList materials = mesh->materialRefs.Get();
            while(materials.Size() <= (int)submeshIndex)
                materials.Append(AssetReference());
            materials.Set((int)submeshIndex, material->AssetReference());
            mesh->materialRefs.Set(materials, AttributeChange::Default);
        }
    }

    blockMaterials[submeshIndex].libraryAsset = material;

    if (placerMode != EditBlock)
        Refresh();

    if (changed)
        emit SettingsChanged();
}

void RocketBlockPlacer::SaveSettingsToConfig()
{
    ConfigAPI &cfg = *rocket->GetFramework()->Config();
    cfg.Write(cBlockPlacementSetting, placementMode);
    cfg.Write(cBlockMaxDistSetting, static_cast<double>(maxBlockDistance));
    cfg.Write(cBlockPlacerSetting, placerMode);

    if (placerMode == CreateBlock)
        SaveVisualsToConfig();
}

void RocketBlockPlacer::SaveVisualsToConfig()
{
    // Mesh selection
    rocket->GetFramework()->Config()->Write(cBlockMeshSetting, blockMeshAsset ? blockMeshAsset->assetRef : "");

    // Material selections
    QStringList materialRefs;
    foreach(const BlockMaterial &mat, Materials())
    {
        if (mat.libraryAsset)
            materialRefs << mat.libraryAsset->assetRef;
        else
            materialRefs << cEmptyMaterialRef;
    }
    rocket->GetFramework()->Config()->Write(cBlockMaterialSetting, materialRefs);
}

void RocketBlockPlacer::LoadSettingsFromConfig()
{
    ConfigAPI &cfg = *rocket->GetFramework()->Config();

    SetMaxBlockDistance(cfg.DeclareSetting(cBlockMaxDistSetting).toFloat());
    SetBlockPlacerMode(static_cast<PlacerMode>(cfg.DeclareSetting(cBlockPlacerSetting).toInt()));
    SetBlockPlacementMode(static_cast<PlacementMode>(cfg.DeclareSetting(cBlockPlacementSetting).toInt()));

    LoadVisualsFromConfig();
}

void RocketBlockPlacer::LoadVisualsFromConfig()
{
    LoadShapeFromConfig();
    LoadMaterialsFromConfig();
}

void RocketBlockPlacer::LoadShapeFromConfig()
{
    // If the library is not yet loaded, return. This function will be called again once assets are loaded.
    if (rocket->AssetLibrary()->Assets().isEmpty())
        return;

    const QString meshRef = rocket->GetFramework()->Config()->Read(cBlockMeshSetting).toString();
    if (meshRef.trimmed().isEmpty() || meshRef == cEmptyMaterialRef)
        return;

    // Library asset? Note returns null if library has not been loaded yet.
    MeshmoonAsset *asset = rocket->AssetLibrary()->FindAsset(meshRef);
    if (!asset && rocket->GetFramework()->Asset()->ParseAssetRef(meshRef) == AssetAPI::AssetRefExternalUrl)
    {
        // Storage asset? Check if this absolute url is applicable to this worlds storage!
        QString storageBase = rocket->Storage()->BaseUrl();
        if (!storageBase.isEmpty() && (meshRef.startsWith(storageBase, Qt::CaseInsensitive) ||
            MeshmoonAsset::S3RegionVariationURL(meshRef).startsWith(storageBase, Qt::CaseInsensitive)))
        {
            asset = GetOrCreateCachedStorageAsset(MeshmoonAsset::Mesh, meshRef);
        }
    }

    if (asset)
        SetMeshAsset(asset);
    else
        LogWarning("[RocketBuildEditor]: Failed to resolve asset for previously used mesh: " + meshRef);
}

void RocketBlockPlacer::LoadMaterialsFromConfig()
{
    // If the library is not yet loaded, return. This function will be called again once assets are loaded.
    if (rocket->AssetLibrary()->Assets().isEmpty())
        return;

    QVariant materials = rocket->GetFramework()->Config()->Read(cBlockMaterialSetting);
    if (materials.isValid())
    {
        // Find the the previously used materials are available. If yes, use them.
        const QStringList matRefs = materials.toStringList();
        for(int i = 0; i < matRefs.size(); ++i)
        {
            QString materialRef = matRefs[i];
            if (materialRef.trimmed().isEmpty() || materialRef == cEmptyMaterialRef)
                continue;

            // Library asset?
            MeshmoonAsset *asset = rocket->AssetLibrary()->FindAsset(materialRef);
            if (!asset && rocket->GetFramework()->Asset()->ParseAssetRef(materialRef) == AssetAPI::AssetRefExternalUrl)
            {
                // Check if this absolute url is applicable to this worlds storage!
                QString storageBase = rocket->Storage()->BaseUrl();
                if (!storageBase.isEmpty() && (materialRef.startsWith(storageBase, Qt::CaseInsensitive) ||
                    MeshmoonAsset::S3RegionVariationURL(materialRef).startsWith(storageBase, Qt::CaseInsensitive)))
                {
                    asset = GetOrCreateCachedStorageAsset(MeshmoonAsset::Material, materialRef);
                }
            }

            if (asset)
                SetMaterialAsset(i, asset);
            else
                LogWarning("[RocketBuildEditor]: Failed to resolve asset for previously used material: " + materialRef);
        }
    }
}

void RocketBlockPlacer::Show()
{
    Create();
    LoadSettingsFromConfig();
    Refresh();
}

void RocketBlockPlacer::AllocateMaterialSlots()
{
    //blockMaterials.clear();
    shared_ptr<EC_Mesh> mesh = placerMesh.lock();
    if (!mesh)
        return;

    while((uint)blockMaterials.size() < mesh->NumMaterials())
        blockMaterials.push_back(BlockMaterial());
}

void RocketBlockPlacer::GeneratePolyhderonForBlockPlacer()
{
    /// @todo
//    blockPlacerPolyhedron = rocket->BuildEditor()->GeneratePolyhderonForMesh(entity->Component<EC_Mesh>()->MeshAsset()->ogreMesh.get());
}

void RocketBlockPlacer::GenerateGhostBlocksForCurrentBlock()
{
    EC_Mesh *mesh = (!lastBlockUnderMouse.expired() ? lastBlockUnderMouse.lock()->Component<EC_Mesh>().get() : 0);
    if (mesh)
        GenerateGhostBlocks(mesh->WorldOBB());
}

void RocketBlockPlacer::ApplyMaterial()
{
    shared_ptr<EC_Mesh> mesh = placerMesh.lock();
    if (mesh && mesh->HasMesh() && mesh->NumMaterials() > 0)
    {
        AssetReferenceList material("OgreMaterial");
        const uint numMaterials = mesh->NumMaterials();
        for(uint i = 0; i < numMaterials; ++i)
            material.Append(placerMode == RemoveBlock ? cBlockRemoverMaterialRef : cBlockPlacerMaterialRef);
        mesh->materialRefs.Set(material, AttributeChange::Default);
    }
}

void RocketBlockPlacer::ResetMaxBlockDistancePlane()
{
    maxBlockDistanceSphere.SetDegenerate();
}

void RocketBlockPlacer::InitializeAssetSelection()
{
    if (!IsPlacingActive())
        return;

    if (!blockMeshAsset)
        LoadShapeFromConfig();

    // If could not load the visuals, fall back to default cube
    if (!blockMeshAsset)
    {
        SetDefaultBlockMesh(rocket->AssetLibrary()->Assets(MeshmoonAsset::Mesh));
        if (!blockMeshAsset)
        {
            LogError("RocketBuildWidget::InitializeAssetSelection: could not find mesh asset for block building.");
            return;
        }
    }

    LoadMaterialsFromConfig();
}

void RocketBlockPlacer::SetDefaultBlockMesh(const MeshmoonAssetList &meshAssets)
{
    // Use the first 'cube' we can find to init out mesh asset.
    foreach(MeshmoonAsset *asset, meshAssets)
        if (asset->ContainsTag(cBuildingBlockTag) && asset->name.contains("cube", Qt::CaseInsensitive))
        {
            SetMeshAsset(asset);
            break;
        }
}
