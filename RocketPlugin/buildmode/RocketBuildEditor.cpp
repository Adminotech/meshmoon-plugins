/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildEditor.cpp
    @brief  RocketBuildEditor manages the Rocket build UIs and editor entities (block placer, transform editor, etc.). */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketBuildEditor.h"
#include "RocketBuildWidget.h"
#include "RocketBuildContextWidget.h"
#include "RocketBlockPlacer.h"
#include "RocketPlugin.h"
#include "RocketNotifications.h"
#include "RocketTaskbar.h"
#include "RocketMenu.h"
#include "utils/RocketAnimations.h"

#include "MeshmoonBackend.h"
#include "MeshmoonUser.h"
#include "MeshmoonAssetLibrary.h"

#include "storage/MeshmoonStorage.h"
#include "storage/MeshmoonStorageWidget.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"
#include "OgreWorld.h"
#include "Renderer.h"
#include "AssetAPI.h"
#include "FrameAPI.h"
#include "TransformEditor.h"
#include "UndoManager.h"
#include "AssetsWindow.h"
#include "PhysicsWorld.h"
#include "EC_RigidBody.h"
#include "Application.h"
#include "SceneInteract.h"
#include "InputAPI.h"
#include "OgreRenderingModule.h"
#include "ConvexHull.h"
#include "CollisionShapeUtils.h"
#include "hull.h"
#include "TransformEditor.h"
#include "IAttribute.h"
#include "UndoCommands.h"

#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Name.h"
#include "EC_Script.h"
#include "EC_WebBrowser.h"
#include "EC_MediaBrowser.h"

#include <OgreMesh.h>

#include "MemoryLeakCheck.h"

int StringToRigidBodyShapeType(const QString &str)
{
    if (str.trimmed().isEmpty()) return EC_RigidBody::Shape_TriMesh;
    else if (str.contains("box", Qt::CaseInsensitive) || str.contains("cube", Qt::CaseInsensitive)) return EC_RigidBody::Shape_Box;
    else if (str.contains("sphere", Qt::CaseInsensitive)) return EC_RigidBody::Shape_Sphere;
    else if (str.contains("cylinder", Qt::CaseInsensitive)) return EC_RigidBody::Shape_Cylinder;
    else if (str.contains("capsule", Qt::CaseInsensitive)) return EC_RigidBody::Shape_Capsule;
    else if (str.contains("heighfield", Qt::CaseInsensitive)) return EC_RigidBody::Shape_HeightField;
    else if (str.contains("convexhull", Qt::CaseInsensitive)) return EC_RigidBody::Shape_ConvexHull;
    else if (str.contains("cone", Qt::CaseInsensitive)) return EC_RigidBody::Shape_Cone;
    else return EC_RigidBody::Shape_TriMesh;
}

float3 ComputePositionInFrontOfCamera(Entity *cameraEntity, float offset)
{
    EC_Placeable *cameraPlaceable = cameraEntity->Component<EC_Placeable>().get();
    return (cameraPlaceable != 0 ? cameraPlaceable->WorldPosition() + cameraPlaceable->WorldOrientation() * -float3::unitZ * offset : float3::zero);
}

float3 CameraPosition(Entity *cameraEntity, bool inspectParentChain)
{
    EC_Placeable *placeable = cameraEntity->Component<EC_Placeable>().get();
    if (inspectParentChain)
    {
        // Go down in parenting chain, fixes eg. camera being parented to avatar.
        // In this case we want to get the world pos of the avatar, not the camera itself.
        while (placeable && placeable->ParentPlaceableComponent() != 0)
            placeable = placeable->ParentPlaceableComponent();
    }
    return (placeable != 0 ? placeable->WorldPosition() : float3::zero);
}

RocketBuildEditor::RocketBuildEditor(RocketPlugin *plugin) :
    rocket(plugin),
    framework(plugin->GetFramework()),
    toggleAction(0),
    buildWidget(0),
    contextWidget(0),
    undoManager(0),
    blockPlacer(0),
    originalTm(float3x4::nan),
    selectGroups(false)
{
    connect(rocket->Backend(), SIGNAL(AuthReset()), SLOT(OnAuthReset()));
    connect(framework->Module<OgreRenderingModule>()->Renderer().get(), SIGNAL(MainCameraChanged(Entity *)),
        SLOT(OnActiveCameraChanged(Entity *)));

    // Use high priority in order to suppress possible interfering application logic scripts (f.ex. avatar scripts).
    inputCtx = framework->Input()->RegisterInputContext("RocketBuildEditor", 2000);
    connect(inputCtx.get(), SIGNAL(MouseEventReceived(MouseEvent*)), SLOT(HandleMouseEvent(MouseEvent*)));
    connect(inputCtx.get(), SIGNAL(KeyEventReceived(KeyEvent *)), SLOT(HandleKeyEvent(KeyEvent *)));
    
    // Use high priority over Qt to always do auto hide logic from mouse move. This does not suppress the events.
    inputCtxMove = framework->Input()->RegisterInputContext("RocketBuildEditorHighPriorityOverQt", 1);
    inputCtxMove->SetTakeMouseEventsOverQt(true);
    connect(inputCtxMove.get(), SIGNAL(MouseMove(MouseEvent*)), SLOT(HandleMouseMove(MouseEvent*)));
    
    // Window resizing for auto hiding logic.
    connect(framework->Ui()->MainWindow(), SIGNAL(WindowResizeEvent(int, int)), SLOT(OnWindowResize(int, int)));
}

RocketBuildEditor::~RocketBuildEditor()
{
//    SAFE_DELETE(contextWidget);
    SAFE_DELETE(buildWidget);
    SAFE_DELETE(undoManager);
    transformEditor.reset();
    SAFE_DELETE(blockPlacer);
}

void RocketBuildEditor::UpdateTaskbarEntry(RocketTaskbar *taskbar)
{
    if (framework->IsHeadless())
        return;

    if (!taskbar)
    {
        SAFE_DELETE(toggleAction);
        return;
    }
    if (!toggleAction)
    {
        const QKeySequence toggleBuildWidget = framework->Input()->KeyBinding("Rocket.ToggleBuildMode", QKeySequence(Qt::ControlModifier + Qt::Key_B));
    
        toggleAction = new QAction(QIcon(":/images/icon-build-mode.png"), tr("Build Mode"), this);
        toggleAction->setToolTip(QString("%1 (%2)").arg(tr("Build Mode")).arg(toggleBuildWidget.toString()));
        toggleAction->setShortcut(toggleBuildWidget);
        toggleAction->setShortcutContext(Qt::WidgetShortcut);
        toggleAction->setCheckable(true);
        toggleAction->setChecked(false);
        toggleAction->setVisible(false);
        
        rocket->Taskbar()->AddAction(toggleAction);
        connect(toggleAction, SIGNAL(toggled(bool)), SLOT(ShowBuildWidget(bool)));

        connect(rocket->User()->RequestAuthentication(), SIGNAL(Completed(int, const QString &)), SLOT(OnAuthenticated(int)));
    }
}

void RocketBuildEditor::SetSelection(const EntityPtr &entity)
{
    if (entity)
    {
        EntityList entities;
        entities.push_back(entity);
        SetSelection(entities);
    }
    else
    {
        ClearSelection();
    }
}

void RocketBuildEditor::SetSelection(const EntityList &entities)
{
    EntityList oldSelection = Selection();

    activeSelection.clear();

    foreach(const EntityPtr &e, entities)
        AppendSelectionInternal(e);

    if (oldSelection != Selection())
        emit SelectionChanged(Selection());

    CheckForGroupChange(oldSelection);
}

void RocketBuildEditor::AppendSelection(const EntityPtr &entity)
{
    EntityList oldSelection = Selection();
    if (AppendSelectionInternal(entity))
    {
        emit SelectionChanged(Selection());
        CheckForGroupChange(oldSelection);
    }
}

void RocketBuildEditor::AppendSelection(const EntityList &entities)
{
    EntityList oldSelection = Selection();

    foreach(const EntityPtr &e, entities)
        AppendSelectionInternal(e);

    if (oldSelection != Selection())
        emit SelectionChanged(Selection());

    CheckForGroupChange(oldSelection);
}

void RocketBuildEditor::RemoveFromSelection(const EntityList &entities)
{
    EntityList oldSelection = Selection();

    foreach(const EntityPtr e, entities)
        RemoveFromSelectionInternal(e);

    emit SelectionChanged(Selection());
    CheckForGroupChange(oldSelection);
}

void RocketBuildEditor::RemoveFromSelection(const EntityPtr &entity)
{
    EntityList oldSelection = Selection();

    if (RemoveFromSelectionInternal(entity))
    {
        emit SelectionChanged(Selection());
        CheckForGroupChange(oldSelection);
    }
}

void RocketBuildEditor::ClearSelection()
{
    const bool changed = !activeSelection.empty();
    activeSelection.clear();
    if (changed)
    {
        emit SelectionChanged(Selection());
        emit ActiveGroupChanged("", EntityList());
    }
}

/*Polyhedron RocketBuildEditor::GeneratePolyhderonForMesh(Ogre::Mesh *mesh)
{
    // Check if has already been converted
    std::map<std::string, Polyhedron>::const_iterator iter = polyhedrons.find(mesh->getName());
    if (iter != polyhedrons.end())
        return iter->second;

    std::vector<float3> vertices;
    Physics::GetTrianglesFromMesh(mesh, vertices);
    if (!vertices.size())
    {
        LogError("RocketBuildEditor::GeneratePolyhderonForMesh: Mesh had no triangles; aborting convex hull generation.");
        return Polyhedron();
    }
    
    StanHull::HullDesc desc;
    desc.SetHullFlag(StanHull::QF_TRIANGLES);
    desc.mVcount = (uint)vertices.size();
    desc.mVertices = &vertices[0].x;
    desc.mVertexStride = sizeof(float3);
    desc.mSkinWidth = 0.01f; // Hardcoded skin width
    
    StanHull::HullLibrary lib;
    StanHull::HullResult result;
    lib.CreateConvexHull(desc, result);
    if (!result.mNumOutputVertices)
    {
        LogError("RocketBuildEditor::GeneratePolyhderonForMesh: No vertices were generated; aborting convex hull generation.");
        return Polyhedron();
    }

    // Construct the polyhedron
    Polyhedron p;
    // Vertices:
    uint idx = 0;
    for(size_t i = 0; i < result.mNumOutputVertices; ++i)
    {
        float3 vertex;
        memcpy(&vertex[0], &result.mOutputVertices[idx], 12);
        idx += 3;
        p.v.push_back(vertex);
    }
    // Indices:
    for(size_t i = 0; i < result.mNumIndices; i += 3)
    {
        Polyhedron::Face face;
        face.v.push_back(result.mIndices[i]);
        face.v.push_back(result.mIndices[i + 1]);
        face.v.push_back(result.mIndices[i + 2]);
        p.f.push_back(face);
    }

    assert((uint)p.NumVertices() == result.mNumOutputVertices);
    assert((uint)p.NumFaces() == result.mNumFaces);

    lib.ReleaseResult(result);

    polyhedrons[mesh->getName()] = p;

    return p;
}*/

void RocketBuildEditor::SetScene(const ScenePtr &scene)
{
    if (!currentScene.expired())
        disconnect(currentScene.lock().get());

    currentScene = scene;

    SAFE_DELETE(undoManager);
    transformEditor.reset();
//    entityGroups.clear();

    if (scene)
    {
        EnvironmentEntity();

        undoManager = new UndoManager(scene, contextWidget->widget_);

        /** @todo Using the proxy as the undo list view parent, it makes it a proxy when shown.
            This enabled the horrible looking "Qt logo" icon for the Qt::Dialog proxy, below does not
            work, figure out what we can do to hide/replace that icon. */
        /*if (undoManager->UndoView())
        {
            QPixmap p(24,24); p.fill(Qt::transparent);
            undoManager->UndoView()->setWindowIcon(QIcon(p));
        }*/

        contextWidget->undoButton->setMenu(undoManager->UndoMenu());
        contextWidget->redoButton->setMenu(undoManager->RedoMenu());

        transformEditor = MAKE_SHARED(TransformEditor, scene, undoManager);

        connect(undoManager, SIGNAL(CanUndoChanged(bool)), contextWidget, SLOT(SetUndoButtonEnabled(bool)), Qt::UniqueConnection);
        connect(undoManager, SIGNAL(CanRedoChanged(bool)), contextWidget, SLOT(SetRedoButtonEnabled(bool)), Qt::UniqueConnection);
        connect(contextWidget->undoButton, SIGNAL(clicked()), undoManager, SLOT(Undo()), Qt::UniqueConnection);
        connect(contextWidget->redoButton, SIGNAL(clicked()), undoManager, SLOT(Redo()), Qt::UniqueConnection);

        connect(scene.get(), SIGNAL(AttributeChanged(IComponent *, IAttribute *, AttributeChange::Type)),
            SLOT(CheckForGroupChange(IComponent *, IAttribute *)), Qt::UniqueConnection);

        /*
        for(Scene::EntityMap::const_iterator it = scene->begin(); it != scene->end(); ++it)
        {
            const QString groupName = it->second->Group();
            if (!groupName.isEmpty())
                entityGroups[groupName].push_back(it->second);
        }
        */
    }
}

EntityPtr RocketBuildEditor::EnvironmentEntity()
{
    EntityPtr environmentEntity;
    ScenePtr scene = Scene();
    if (!scene)
    {
        LogError("RocketBuildEditor::EnvironmentEntity: scene not set, returning null entity.");
        return EntityPtr();
    }

    const QString entityName = "RocketEnvironment";
    EntityList existing = scene->FindEntitiesByName(entityName);
    if (!existing.empty())
    {       
        // Prioritize a non temporary replicated RocketEnvironment.
        // Layers might pull multiple RocketEnvironment entities in!
        for(EntityList::iterator it = existing.begin(); it != existing.end(); ++it)
        {
            EntityPtr ent = (*it);
            if (!ent->IsTemporary() && ent->IsReplicated())
            {
                environmentEntity = ent;
                break;
            }
        }

        // Secondary loop trying to find and clone temp/local ent or remove any
        // other env ent except the one that we are going to use.
        for(EntityList::iterator it = existing.begin(); it != existing.end(); ++it)
        {
            EntityPtr ent = (*it);
            if (!environmentEntity.get())
            {
                if (ent->IsTemporary() || ent->IsLocal())
                    environmentEntity = ent->Clone(false, false, entityName);
                else
                    environmentEntity = ent;
            }
            else if (ent->Id() != environmentEntity->Id())
            {
                LogWarning("RocketBuildEditor::EnvironmentEntity: Deleting extraneous " + ent->ToString());
                entity_id_t id = ent->Id();
                ent.reset();
                scene->RemoveEntity(id);
            }
        }
    }
    else
    {
        environmentEntity = scene->CreateEntity();
        environmentEntity->SetName(entityName);
    }

    assert(environmentEntity);
    return environmentEntity;
}

QPair<ComponentPtr, QUndoCommand *> RocketBuildEditor::CreateComponent(const EntityPtr &block, u32 componentTypeId,
    QUndoCommand *parent, const QString &name, bool replicated, bool temporary)
{
    AddComponentCommand *action = new AddComponentCommand(block->ParentScene()->shared_from_this(), undoManager->Tracker(),
        EntityIdList() << block->Id(), componentTypeId, name, replicated, temporary, parent);
    if (!parent)
        undoManager->Push(action);
    else
        action->redo();
    return QPair<ComponentPtr, QUndoCommand*>(block->Component(componentTypeId), action);
}

template <typename T>
void RocketBuildEditor::CopyAttributeValues(const shared_ptr<T> &source, const shared_ptr<T> &dest, QUndoCommand *parent)
{
    if (!source || !dest)
        return;
    if (source->TypeId() != dest->TypeId())
    {
        LogError("RocketBuildEditor::CopyAttributeValues: Source and destination type ID mismatch.");
        return;
    }
    const AttributeVector &sourceAttributes = source->Attributes();
    const AttributeVector &destAttributes = dest->Attributes();
    for(size_t i = 0; i < sourceAttributes.size() && i < destAttributes.size(); ++i)
        CopyAttributeValue(sourceAttributes[i], destAttributes[i], parent);
}

void RocketBuildEditor::CopyAttributeValue(IAttribute *source, IAttribute *dest, QUndoCommand *parent)
{
    if (!source || !dest)
        return;
    dest->CopyValue(source, AttributeChange::Default);
    QUndoCommand *action = new EditIAttributeCommand(dest, parent);
    if (!parent)
        undoManager->Push(action);
    else
        action->redo();
}
/*
EntityPtr RocketBuildEditor::CreateBlock(bool applyVisuals, bool applyPhysics, bool applyScript)
{
    ScenePtr scene = Scene();
    if (!scene || !undoManager)
        return EntityPtr();

    /// @todo Proper name for the block entity "Block_<GroupId>_<BlockId>"
    AddEntityCommand *cmdCreate = new AddEntityCommand(scene, undoManager->Tracker(), "", true, false);
    undoManager->Push(cmdCreate);
    EntityPtr block = scene->EntityById(cmdCreate->entityId_);
    if (!block)
    {
        LogError("RocketBuildEditor::CreateBlock: UndoManager failed to create a block entity.");
        return EntityPtr();
    }

    // Name and placeable are created always
    QPair<EC_Name*, QUndoCommand*> nameData = CreateComponent<EC_Name>(block, cmdCreate);
    QPair<EC_Placeable*, QUndoCommand*> placeableData = CreateComponent<EC_Placeable>(block, cmdCreate);

    // The rest is up to the user.
    QPair<EC_Mesh*, QUndoCommand*> meshData = (applyVisuals ? CreateComponent<EC_Mesh>(block, cmdCreate) : QPair<EC_Mesh*, QUndoCommand*>(0, 0));

    EC_Name* targetName = nameData.first;
    EC_Placeable* placeable = placeableData.first;
    EC_Mesh *targetMesh = meshData.first;

    // Set placeable transform and push the current value to the undo stack.
    placeable->SetWorldTransform(blockPlacer->placerPlaceable.lock()->WorldTransform());
    SetAttribute(placeable->transform, placeable->transform.Get(), placeableData.second);

    if (!blockPlacer->MeshAsset())
    {
        scene->RemoveEntity(block->Id());
        return EntityPtr();
    }

    if (applyVisuals)
    {
        // Library mesh
        SetAttribute(targetMesh->meshRef, blockPlacer->MeshAsset()->AssetReference(), meshData.second);

        // Library materials
        AssetReferenceList materials;
        const int numSubmeshes = blockPlacer->MeshAsset()->metadata.value("submeshes", 1).toInt();
        for(int subMeshIdx = 0; subMeshIdx < numSubmeshes; ++subMeshIdx)
        {
            // Check if there is a selected material for this submesh
            bool found = false;
            for(int i = 0; i < blockPlacer->Materials().size(); ++i)
                if (i == subMeshIdx && blockPlacer->Materials()[i].libraryAsset)
                {
                    materials.Append(blockPlacer->Materials()[i].libraryAsset->AssetReference());
                    found = true;
                    break;
                }

            if (!found)
                materials.Append(AssetReference());
        }

        SetAttribute(targetMesh->meshMaterial, materials, meshData.second);
    }

    // Name and 'category'
    SetAttribute(targetName->name, blockPlacer->MeshAsset()->name, nameData.second);
    SetAttribute(targetName->description, QString("building block"), nameData.second);

    if (applyPhysics)
    {
        QPair<EC_RigidBody*, QUndoCommand*> rigidData = CreateComponent<EC_RigidBody>(block, cmdCreate);
        if (applyVisuals)
        {
            SetAttribute(rigidData.first->shapeType, StringToRigidBodyShapeType(blockPlacer->MeshAsset()->metadata.value("rigidbodyshape").toString()), rigidData.second);
            SetAttribute(rigidData.first->size, blockPlacer->placerMesh.lock()->WorldAABB().Size(), rigidData.second);
        }
    }

    if (applyScript)
    {
        QPair<EC_Script *, QUndoCommand *> scriptData = CreateComponent<EC_Script>(block, cmdCreate);
        AssetReferenceList scriptRef("Script");
        scriptRef.Append(AssetReference(buildWidget->blocksScriptRef, "Script"));
        SetAttribute(scriptData.first->scriptRef, scriptRef, scriptData.second);
        scriptData.first->runOnLoad.Set(true, AttributeChange::Default);
    }

    // Force activation of the newly created block.
    blockPlacer->SetActiveBlock(block);

    //currentBlockGroup.push_back(block);

    return block;
}
*/
EntityPtr RocketBuildEditor::CreateBlock(const std::vector<ComponentPtr> &components)
{
    ScenePtr scene = Scene();
    if (!scene || !undoManager)
        return EntityPtr();

    /// @todo Proper name for the block entity "Block_<GroupId>_<BlockId>"
    AddEntityCommand *cmdCreate = new AddEntityCommand(scene, undoManager->Tracker(), "", true, false);
    undoManager->Push(cmdCreate);
    EntityPtr block = scene->EntityById(cmdCreate->entityId_);
    if (!block)
    {
        LogError("RocketBuildEditor::CreateBlock: UndoManager failed to create a block entity.");
        return EntityPtr();
    }

    foreach(const ComponentPtr &component, components)
    {
        QPair<ComponentPtr, QUndoCommand *> undo = CreateComponent(block, component->TypeId(), cmdCreate);
        for(size_t i = 0; i < component->Attributes().size() && i < undo.first->Attributes().size(); ++i)
            CopyAttributeValue(component->Attributes()[i], undo.first->Attributes()[i], undo.second);
        block->AddComponent(undo.first);
    }

    return block;
}

EntityPtr RocketBuildEditor::CloneBlock(const EntityPtr &source, bool applyPhysics, bool cloneFunctionality)
{
    if (!source)
        return EntityPtr();

    ScenePtr scene = Scene();
    if (!scene || !undoManager)
        return EntityPtr();

    /// @todo Proper name for the clone entity "<Entity>_Clone<N>" or something.
    AddEntityCommand *cmdCreate = new AddEntityCommand(scene, undoManager->Tracker(), "", true, false);
    undoManager->Push(cmdCreate);
    EntityPtr block = scene->EntityById(cmdCreate->entityId_);
    if (!block)
    {
        LogError("RocketBuildEditor::CloneBlock: UndoManager failed to create a block entity.");
        return EntityPtr();
    }

    if (!source->Component<EC_Placeable>() || !source->Component<EC_Mesh>())
    {
        LogError("RocketBuildEditor::CloneBlock: source entity is missing some of the required components.");
        return EntityPtr();
    }

    // 1) Visuals i.e. Placeable and Mesh are created always when cloning.
    ComponentPtr sourceComponent = source->Component(EC_Placeable::ComponentTypeId);
    QPair<ComponentPtr, QUndoCommand *> undo = CreateComponent(block, EC_Placeable::ComponentTypeId, cmdCreate);
    CopyAttributeValues(sourceComponent, undo.first, undo.second);

    // Set transform of the clone and push the current value to the undo stack.
    shared_ptr<EC_Placeable> placeable = static_pointer_cast<EC_Placeable>(undo.first);
    placeable->SetWorldTransform(blockPlacer->placerPlaceable.lock()->WorldTransform());
    SetAttribute(placeable->transform, placeable->transform.Get(), undo.second);

    sourceComponent = source->Component(EC_Mesh::ComponentTypeId);
    undo = CreateComponent(block, EC_Mesh::ComponentTypeId, cmdCreate);
    CopyAttributeValues(sourceComponent, undo.first, undo.second);

    // 2) Clone name outside functionality clone as it might be disabled, and we always want the name to transfer
    if ((sourceComponent = source->Component(EC_Name::ComponentTypeId)))
    {
        undo = CreateComponent(block, EC_Name::ComponentTypeId, cmdCreate);
        CopyAttributeValues(sourceComponent, undo.first, undo.second);
    }

    // 3) Physics i.e. RigidBody.
    if (applyPhysics && (sourceComponent = source->Component(EC_RigidBody::ComponentTypeId)))
    {
        undo = CreateComponent(block, EC_RigidBody::ComponentTypeId, cmdCreate);
        CopyAttributeValues(sourceComponent, undo.first, undo.second);
    }

    // 4) Functionality i.e. any other components.
    if (cloneFunctionality)
    {
        // Clone the rest of the components, but ignore the ones we already cloned manually.
        const QList<u32> ignoredComponentTypes(QList<u32>()
            << EC_Name::ComponentTypeId << EC_Placeable::ComponentTypeId
            << EC_Mesh::ComponentTypeId << EC_RigidBody::ComponentTypeId);

        const Entity::ComponentMap &components = source->Components();
        for(Entity::ComponentMap::const_iterator it = components.begin(); it != components .end(); ++it)
        {
            if (!ignoredComponentTypes.contains(it->second->TypeId()))
            {
                QPair<ComponentPtr, QUndoCommand *> undo = CreateComponent(block, it->second->TypeId(), cmdCreate);
                CopyAttributeValues(it->second, undo.first, undo.second);
                block->AddComponent(undo.first);
            }
        }
    }

    // Force activation of the newly created block.
    blockPlacer->SetActiveBlock(block);

    //currentBlockGroup.push_back(block);

    return block;
}

void RocketBuildEditor::DeleteBlock()
{
    ScenePtr scene = Scene();
    if (scene && !blockPlacer->lastBlockUnderMouse.expired())
        undoManager->Push(new RemoveCommand(scene, undoManager->Tracker(), blockPlacer->lastBlockUnderMouse));
}

void RocketBuildEditor::DeleteSelection(bool showConfirmationDialog)
{
    ScenePtr scene = Scene();
    if (scene && !activeSelection.empty())
    {
        if (showConfirmationDialog && rocket->Notifications()->ExecSplashDialog(tr("Are you sure you want to delete %1 %2?")
            .arg(activeSelection.size()).arg(activeSelection.size() == 1 ? tr("entity") : tr("entities")),
            ":/images/icon-questionmark.png", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes) != QMessageBox::Yes)
        {
            return;
        }

        std::set<EntityPtr> selection = activeSelection;
        ClearSelection();
        QList<EntityWeakPtr> entities;
        foreach(const EntityPtr &e, selection)
            entities << e;
        undoManager->Push(new RemoveCommand(scene, undoManager->Tracker(), entities));
    }
}

bool RocketBuildEditor::Undo()
{
    if (undoManager)
    {
        undoManager->Undo();
        return true;
    }
    return false;
}

bool RocketBuildEditor::Redo()
{
    if (undoManager)
    {
        undoManager->Redo();
        return true;
    }
    return false;
}

void RocketBuildEditor::OnAuthenticated(int permissionLevel)
{
    if (toggleAction && permissionLevel >= Meshmoon::Elevated && permissionLevel < Meshmoon::PermissionLevelOutOfRange)
        toggleAction->setVisible(true);
}

void RocketBuildEditor::OnAuthReset()
{
    if (toggleAction)
        toggleAction->setVisible(false);
    if (buildWidget)
        buildWidget->SetVisible(false);
}

void RocketBuildEditor::OnActiveCameraChanged(Entity *entity)
{
    activeCamera = (entity ? entity->shared_from_this() : EntityPtr());
    ogreWorld = (entity ? entity->ParentScene()->Subsystem<OgreWorld>() : OgreWorldPtr());
    if (blockPlacer)
        blockPlacer->ogreWorld = ogreWorld;
}

void RocketBuildEditor::CheckForGroupChange(IComponent *comp, IAttribute *attr)
{
    if (comp->TypeId() == EC_Name::ComponentTypeId && attr->Id().compare("group", Qt::CaseInsensitive) == 0 &&
        ActiveEntity().get() == comp->ParentEntity())
    {
        const QString newGroup = attr->ToString();
        emit ActiveGroupChanged(newGroup, Scene()->EntitiesOfGroup(newGroup));
    }
}

void RocketBuildEditor::CheckForGroupChange(const EntityList &oldSelection)
{
    /// @todo This is pretty random.
    std::set<QString> oldGroups, newGroups;
    foreach(const EntityPtr &e, oldSelection)
        oldGroups.insert(e->Group());
    foreach(const EntityPtr &e, activeSelection)
        newGroups.insert(e->Group());

    EntityPtr entity = ActiveEntity();
    if (oldGroups != newGroups)
    {
        const QString newGroupName = entity ? entity->Group() : "";
        emit ActiveGroupChanged(newGroupName, Scene()->EntitiesOfGroup(newGroupName));
    }

    /// @todo Better place for this.
    /// @todo For now resetting transform possible only for single target.
    if (entity && entity->Component<EC_Placeable>())
        originalTm = entity->Component<EC_Placeable>()->WorldTransform();
    else
        originalTm = float3x4::nan;
}

void RocketBuildEditor::ShowBuildWidget(bool show)
{
    if (!blockPlacer)
    {
        blockPlacer = new RocketBlockPlacer(rocket);
        blockPlacer->ogreWorld = ogreWorld;
    }

    if (!buildWidget)
    {
        buildWidget = new RocketBuildWidget(rocket);
        contextWidget = buildWidget->contextWidget = new RocketBuildContextWidget(rocket);

        connect(buildWidget, SIGNAL(VisibilityChanged(bool)), SLOT(OnBuildWidgetVisibilityChanged(bool)));
        connect(contextWidget, SIGNAL(VisibilityChanged(bool)), SLOT(OnContextWidgetVisibilityChanged(bool)));
        
        // Build widget animations
        buildWidget->AnimationEngine()->HookCompleted(this, SLOT(OnBuildWidgetAnimationsCompleted()));
        buildWidget->AnimationEngine()->HookProgress(this, SLOT(OnBuildWidgetAnimationsProgress()));
        contextWidget->AnimationEngine()->HookProgress(this, SLOT(OnBuildContextWidgetAnimationsProgress()));
        
        // Storage widget animations are hooked when widget is crated
        connect(rocket->Storage(), SIGNAL(StorageWidgetCreated(RocketStorageWidget*)), 
            this, SLOT(OnStorageWidgetCreated(RocketStorageWidget*)), Qt::UniqueConnection);
        OnStorageWidgetCreated(rocket->Storage()->StorageWidget());
        
        emit BuildWidgetsCreated(buildWidget, contextWidget);
    }

    if (buildWidget)
        buildWidget->SetVisible(show);
}

void RocketBuildEditor::OnStorageWidgetCreated(RocketStorageWidget *storageWidget)
{
    if (storageWidget)
    {
        storageWidget->AnimationEngine()->HookCompleted(this, SLOT(OnBuildWidgetAnimationsCompleted()));
        storageWidget->AnimationEngine()->HookProgress(this, SLOT(OnBuildWidgetAnimationsProgress()));
    }
}

void RocketBuildEditor::SetSelectGroupsEnabled(bool select)
{
    selectGroups = select;
    EntityList oldSelection = Selection();
    activeSelection.clear();
    AppendSelection(oldSelection);
}

bool RocketBuildEditor::AppendSelectionInternal(const EntityPtr &entity)
{
    if (activeSelection.find(entity) == activeSelection.end())
    {
        if (selectGroups)
        {
            const EntityList group = Scene()->EntitiesOfGroup(entity->Group());
            if (group.empty())
                activeSelection.insert(entity);
            else
                foreach(const EntityPtr &entityOfGroup, group)
                    activeSelection.insert(entityOfGroup);
        }
        else
            activeSelection.insert(entity);

        return true;
    }

    return false;
}

bool RocketBuildEditor::RemoveFromSelectionInternal(const EntityPtr &entity)
{
    if (activeSelection.find( entity) != activeSelection.end())
    {
        if (selectGroups)
        {
            const EntityList entitiesOfTheSameGroup = Scene()->EntitiesOfGroup(entity->Group());
            if (entitiesOfTheSameGroup.empty())
                activeSelection.erase(entity);
            else
                foreach(const EntityPtr &entityFromGroup, entitiesOfTheSameGroup)
                    activeSelection.erase(entityFromGroup);
        }
        else
            activeSelection.erase(entity);

        return true;
    }

    return false;
}

void RocketBuildEditor::OnBuildWidgetVisibilityChanged(bool visible)
{
    if (!visible)
    {
        SAFE_DELETE_LATER(buildWidget);
        SAFE_DELETE_LATER(blockPlacer);
    }
}

void RocketBuildEditor::OnContextWidgetVisibilityChanged(bool visible)
{
    UpdateBuildWidgetAnimationsState(framework->Input()->MousePos().x(), visible);
}

void RocketBuildEditor::HandleMouseEvent(MouseEvent *e)
{
    if (ogreWorld.expired())
        return;
    if (!buildWidget || !buildWidget->scene())
        return;
    if (e->itemUnderMouse)
    {
        if (!blockPlacer->IsVisible())
            blockPlacer->Hide();
        return;
    }
    /// @todo Different mouse cursor overrides for different block placer modes?
    switch(e->Type())
    {
    case MouseEvent::MousePressed:
    case MouseEvent::MouseDoubleClicked: // steal double-clicks to block avatar's go-to feature
    {
        if (e->button == MouseEvent::LeftButton)
        {
            if (buildWidget && buildWidget->Mode() == RocketBuildWidget::BlocksMode && blockPlacer->IsPlacingActive())
            {
                switch(blockPlacer->BlockPlacerMode())
                {
                case RocketBlockPlacer::CreateBlock:
                    CreateBlock(buildWidget->ComponentsForNewBlock());
                    break;
                case RocketBlockPlacer::CloneBlock:
                    CloneBlock(blockPlacer->lastBlockUnderMouse.lock(),
                        buildWidget->mainWidget.blocksPhysicsCheckBox->isChecked(),
                        buildWidget->mainWidget.blocksFunctionalityCheckBox->isChecked());
                    break;
                case RocketBlockPlacer::RemoveBlock:
                    DeleteBlock();
                    break;
                case RocketBlockPlacer::NumPlacerModes:
                case RocketBlockPlacer::EditBlock:
                    return;
                }
                e->Suppress();
            }
            else // In all other modes pick/toggle the entity to be the active/editable entity.
            {
                // TransformGizmo is nowadays a separate entity, not just a component in the edited entity,
                // so we must use RaycastAll to find the hit entity. Find the nearest hit and use that,
                // but we must *not* have hit a gizmo entity in order to be able to pick the to be edited.
                QList<RaycastResult *> hits = ogreWorld.lock()->RaycastAll(e->x, e->y, 500.f); // 500 is just some random pick.
                RaycastResult *nearestHit = 0;
                bool hitGizmo = false;
                foreach(RaycastResult *hit, hits)
                {
                    if (!hitGizmo)
                        hitGizmo = (hit->entity && hit->entity->Component(30));
                    if ((!nearestHit || hit->t < nearestHit->t) && hit->entity && !hit->entity->Component(30) && !IsEntityAnAvatar(hit->entity))
                        nearestHit = hit;
                }

                const EntityPtr hitEntity = (!hitGizmo && nearestHit && nearestHit->entity ? nearestHit->entity->shared_from_this() : EntityPtr());
                if (hitEntity)
                {
                    const bool alreadySelected = activeSelection.find(hitEntity) != activeSelection.end();
                    if (e->HasCtrlModifier())
                    {
                        if (alreadySelected)
                            RemoveFromSelection(hitEntity);
                        else
                            AppendSelection(hitEntity);
                    }
                    else
                    {
                        SetSelection(alreadySelected ? EntityPtr() : hitEntity);
                    }

                    e->Suppress();
                }
            }
        }
        break;
    }
    case MouseEvent::MouseScroll:
    {
        if (buildWidget && buildWidget->Mode() == RocketBuildWidget::BlocksMode)
        {
            EntityList targets;
            if (blockPlacer->BlockPlacerMode() == RocketBlockPlacer::EditBlock)
                targets = Selection();
            else
                targets.push_back(blockPlacer->placerEntity);

            foreach(const EntityPtr &target, targets)
            {
                shared_ptr <EC_Placeable> placeable = target->Component<EC_Placeable>();
                if (!placeable)
                    continue;

                Transform t = placeable->transform.Get();

                if (e->HadKeyDown(Qt::Key_Q))
                {
                    /// @todo Some kind of sensitivity settings for rotating?
                    const float delta = e->relativeZ / 10.f;
                    if (e->HasShiftModifier())
                        t.rot.x += delta;
                    if (e->HasCtrlModifier())
                        t.rot.y += delta;
                    if (e->HasAltModifier())
                        t.rot.z += delta;
                }
                else if (e->HadKeyDown(Qt::Key_Z))
                {
                    blockPlacer->SetMaxBlockDistance(blockPlacer->MaxBlockDistance() + Clamp(static_cast<float>(e->relativeZ), -1.f, 1.f));
                }
                else
                {
                    /// @todo Some kind of sensitivity settings for scaling?
                    const float delta = e->relativeZ / 1000.f;
                    if (e->modifiers == 0 || e->HasShiftModifier())
                        t.scale.x += delta;
                    if (e->modifiers == 0 || e->HasCtrlModifier())
                        t.scale.y += delta;
                    if (e->modifiers == 0 || e->HasAltModifier())
                        t.scale.z += delta;
                    t.scale = Max(t.scale, float3::FromScalar(0.01f));
                }

                placeable->transform.Set(t, AttributeChange::Default);
            }

            e->Suppress();
        }
        break;
    }
    default:
        break;
    }
}

void RocketBuildEditor::HandleMouseMove(MouseEvent *e)
{
    if (!e->IsLeftButtonDown() && !e->IsRightButtonDown() && !e->IsMiddleButtonDown())
        UpdateBuildWidgetAnimationsState(e->x);
}

void RocketBuildEditor::UpdateBuildWidgetAnimationsState(int mouseX, bool forceShow)
{
    if (!contextWidget || !buildWidget || buildWidget->IsPinned() || buildWidget->IsHiddenOrHiding() || buildWidget->IsClosing())
        return;

    RocketAnimationEngine *engine = buildWidget->AnimationEngine();
    if (!engine->IsInitialized() || (engine->IsRunning() && !forceShow))
        return;

    /// @todo Animate main widget to the side also is context is currently hiding? Needs some tweaks to contextWidget->Resize().
    if (!forceShow && contextWidget->IsVisible())
        forceShow = true;

    // We assume the scene is always full screen, as it currently is in Tundra,
    // and that the build widget is always on the left corner. If these things change
    // the below code needs to be adjusted.

    QRectF visibleRect = buildWidget->VisibleSceneRect();

    const int padding = 70;
    const int animationState = engine->Value<int>("state");

    int triggerWidth = visibleRect.width() + padding;
    if (!forceShow && mouseX > triggerWidth)
    {
        if (animationState != 1)
        {
            QPointF scenePos = buildWidget->scenePos();
            QRectF rect = buildWidget->geometry();
            QSizeF origSize = rect.size();

            engine->StoreValue("state", 1);
            engine->StoreValue("pos", scenePos);
            engine->StoreValue("opacity", buildWidget->showOpacity);
            scenePos.setX(150 - rect.width());
            buildWidget->Animate(origSize, scenePos, 0.5);
        }
    }
    else
    {
        if (animationState != 0)
        {
            engine->StoreValue("state", 0);
            QRectF rect = buildWidget->geometry();
            buildWidget->Animate(rect.size(), engine->Value<QPointF>("pos"), engine->Value<qreal>("opacity"));
        }
    }
}

void RocketBuildEditor::OnWindowResize(int, int)
{
    // If the hide state is active, reset back to show state and trigger a mouse event
    // so the hide calculations are executed again.
    if (buildWidget && buildWidget->IsVisible() && !buildWidget->IsPinned() && buildWidget->AnimationEngine())
    {
        RocketAnimationEngine *engine = buildWidget->AnimationEngine();
        if (engine->Value<int>("state") != 0)
        {
            engine->StoreValue("state", 0);
            engine->StoreValue("state", 0);
            engine->StoreValue("state", 0);
            buildWidget->setPos(QPointF(0,0));
            buildWidget->setOpacity(buildWidget->showOpacity);
            UpdateBuildWidgetAnimationsState(framework->Input()->MousePos().x());
        }
    }
}

void RocketBuildEditor::OnBuildWidgetAnimationsCompleted()
{
    // Show/hide widget only when mouse moves. We want to be able to observe the widget when keyboard shortcuts are used.
    //UpdateBuildWidgetAnimationsState(framework->Input()->MousePos().x());
}

void RocketBuildEditor::OnBuildContextWidgetAnimationsProgress()
{
    if (buildWidget && !buildWidget->AnimationEngine()->IsRunning())
    {
        // Update as the build widget animation is not driving the resize logic.
        if (contextWidget && !contextWidget->IsHiddenOrHiding())
            contextWidget->Resize();
        buildWidget->RepositionSceneAxisOverlay();
    }
}

void RocketBuildEditor::OnBuildWidgetAnimationsProgress()
{
    if (contextWidget && !contextWidget->IsHiddenOrHiding())
        contextWidget->Resize();
    if (buildWidget)
        buildWidget->RepositionSceneAxisOverlay();
}

void RocketBuildEditor::HandleKeyEvent(KeyEvent *e)
{
    InputAPI &input = *framework->Input();

    // Toggle action for build mode widget
    const QKeySequence toggleBuildWidget = input.KeyBinding("Rocket.ToggleBuildMode", QKeySequence(Qt::ControlModifier + Qt::Key_B));
    if (e->sequence == toggleBuildWidget && toggleAction)
    {
        toggleAction->toggle();
        e->Suppress();
        return;
    }

    // Cannot continue further if widget is not visible.
    if (!buildWidget)
        return;

    const QKeySequence undo = input.KeyBinding("TundraEditors.Undo", QKeySequence(Qt::ControlModifier + Qt::Key_Z));
    const QKeySequence redo = input.KeyBinding("TundraEditors.Redo", QKeySequence(Qt::ControlModifier + Qt::Key_Y));

    // Block mode shortcuts
    if (buildWidget->Mode() == RocketBuildWidget::BlocksMode)
    {
        const QKeySequence switchPlacerMode = input.KeyBinding("BlockBuilding.SwitchPlacerMode", QKeySequence(Qt::Key_section));
        const QKeySequence switchPlacementMode = input.KeyBinding("BlockBuilding.SwitchPlacementMode", QKeySequence(Qt::Key_Tab));
        const QKeySequence resetPlacer = input.KeyBinding("BlockBuilding.ResetPlacer", QKeySequence(Qt::ControlModifier + Qt::Key_R));
        const QKeySequence shape1 = input.KeyBinding("BlockBuilding.ShapeShortcut1", QKeySequence(Qt::Key_1));
        const QKeySequence shape2 = input.KeyBinding("BlockBuilding.ShapeShortcut2", QKeySequence(Qt::Key_2));
        const QKeySequence shape3 = input.KeyBinding("BlockBuilding.ShapeShortcut3", QKeySequence(Qt::Key_3));
        const QKeySequence shape4 = input.KeyBinding("BlockBuilding.ShapeShortcut4", QKeySequence(Qt::Key_4));
        const QKeySequence shape5 = input.KeyBinding("BlockBuilding.ShapeShortcut5", QKeySequence(Qt::Key_5));
        const QKeySequence shape6 = input.KeyBinding("BlockBuilding.ShapeShortcut6", QKeySequence(Qt::Key_6));
        const QKeySequence shape7 = input.KeyBinding("BlockBuilding.ShapeShortcut7", QKeySequence(Qt::Key_7));
        const QKeySequence shape8 = input.KeyBinding("BlockBuilding.ShapeShortcut8", QKeySequence(Qt::Key_8));
        const QKeySequence shape9 = input.KeyBinding("BlockBuilding.ShapeShortcut9", QKeySequence(Qt::Key_9));
        const QKeySequence shape10 = input.KeyBinding("BlockBuilding.ShapeShortcut10", QKeySequence(Qt::Key_0));
        const QKeySequence material1 = input.KeyBinding("BlockBuilding.MaterialShortcut1", QKeySequence(Qt::ControlModifier + Qt::Key_1));
        const QKeySequence material2 = input.KeyBinding("BlockBuilding.MaterialShortcut2", QKeySequence(Qt::ControlModifier + Qt::Key_2));
        const QKeySequence material3 = input.KeyBinding("BlockBuilding.MaterialShortcut3", QKeySequence(Qt::ControlModifier + Qt::Key_3));
        const QKeySequence material4 = input.KeyBinding("BlockBuilding.MaterialShortcut4", QKeySequence(Qt::ControlModifier + Qt::Key_4));
        const QKeySequence material5 = input.KeyBinding("BlockBuilding.MaterialShortcut5", QKeySequence(Qt::ControlModifier + Qt::Key_5));
        const QKeySequence material6 = input.KeyBinding("BlockBuilding.MaterialShortcut6", QKeySequence(Qt::ControlModifier + Qt::Key_6));
        const QKeySequence material7 = input.KeyBinding("BlockBuilding.MaterialShortcut7", QKeySequence(Qt::ControlModifier + Qt::Key_7));
        const QKeySequence material8 = input.KeyBinding("BlockBuilding.MaterialShortcut8", QKeySequence(Qt::ControlModifier + Qt::Key_8));
        const QKeySequence material9 = input.KeyBinding("BlockBuilding.MaterialShortcut9", QKeySequence(Qt::ControlModifier + Qt::Key_9));
        const QKeySequence material10 = input.KeyBinding("BlockBuilding.MaterialShortcut10", QKeySequence(Qt::ControlModifier + Qt::Key_0));
        const QKeySequence incrMaxDist = input.KeyBinding("BlockBuilding.IncreaseMaxDistance", QKeySequence(Qt::Key_PageUp));
        const QKeySequence decrMaxDist = input.KeyBinding("BlockBuilding.DecreaseMaxDistance", QKeySequence(Qt::Key_PageDown));
        const QKeySequence deleteSelection = input.KeyBinding("BlockBuilding.DeleteSelection", QKeySequence(Qt::Key_Delete));

        if (e->sequence == switchPlacerMode)
        {
            ClearSelection();
            uint mode = blockPlacer->BlockPlacerMode();
            if (++mode >= RocketBlockPlacer::NumPlacerModes)
                mode = RocketBlockPlacer::CreateBlock;
            blockPlacer->SetBlockPlacerMode(static_cast<RocketBlockPlacer::PlacerMode>(mode));
            e->Suppress();
            UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
        }
        else if (e->sequence == incrMaxDist || e->keyCode == Qt::Key_PageUp) /**< @todo Key repeats don't have key sequences - how to make this work for user-defined key? */
        {
            blockPlacer->SetMaxBlockDistance(blockPlacer->MaxBlockDistance() + 1.f);
            e->Suppress();
            UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
        }
        else if (e->sequence == decrMaxDist || e->keyCode == Qt::Key_PageDown) /**< @todo Key repeats don't have key sequences - how to make this work for user-defined key? */
        {
            blockPlacer->SetMaxBlockDistance(blockPlacer->MaxBlockDistance() - 1.f);
            e->Suppress();
            UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
        }

        // Resetting possible in both Create and Edit modes.
        if (e->sequence == resetPlacer)
        {
            if (blockPlacer->BlockPlacerMode() == RocketBlockPlacer::EditBlock && ActiveEntity() && originalTm.IsFinite())
            {
                ActiveEntity()->Component<EC_Placeable>()->SetWorldTransform(originalTm);
                e->Suppress();
            }
            else if (blockPlacer->IsPlacingActive() && !blockPlacer->placerPlaceable.expired())
            {
                blockPlacer->placerPlaceable.lock()->SetOrientationAndScale(float3x3::identity);
                e->Suppress();
            }
        }
        else if (e->sequence == deleteSelection && blockPlacer->BlockPlacerMode() == RocketBlockPlacer::EditBlock)
        {
            DeleteSelection();
            e->Suppress();
        }

        // Handle rest of the shortcuts only if block placer is active
        if (blockPlacer->IsPlacingActive())
        {
            int shapeIdx = -1, materialIdx = -1;
            if (e->sequence == switchPlacementMode)
            {
                uint mode = blockPlacer->BlockPlacementMode();
                if (++mode >= RocketBlockPlacer::NumPlacementModes)
                    mode = RocketBlockPlacer::SnapPlacement;
                blockPlacer->SetBlockPlacementMode(static_cast<RocketBlockPlacer::PlacementMode>(mode));
                e->Suppress();
                UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
            }
            else if (e->sequence == shape1) shapeIdx = 0;
            else if (e->sequence == shape2) shapeIdx = 1;
            else if (e->sequence == shape3) shapeIdx = 2;
            else if (e->sequence == shape4) shapeIdx = 3;
            else if (e->sequence == shape5) shapeIdx = 4;
            else if (e->sequence == shape6) shapeIdx = 5;
            else if (e->sequence == shape7) shapeIdx = 6;
            else if (e->sequence == shape8) shapeIdx = 7;
            else if (e->sequence == shape9) shapeIdx = 8;
            else if (e->sequence == shape10) shapeIdx = 9;
            else if (e->sequence == material1) materialIdx = 0;
            else if (e->sequence == material2) materialIdx = 1;
            else if (e->sequence == material3) materialIdx = 2;
            else if (e->sequence == material4) materialIdx = 3;
            else if (e->sequence == material5) materialIdx = 4;
            else if (e->sequence == material6) materialIdx = 5;
            else if (e->sequence == material7) materialIdx = 6;
            else if (e->sequence == material8) materialIdx = 7;
            else if (e->sequence == material9) materialIdx = 8;
            else if (e->sequence == material10) materialIdx = 9;

            if (shapeIdx >= 0)
            {
                /// @todo cache available meshes
                MeshmoonAssetList meshAssets = rocket->AssetLibrary()->Assets(MeshmoonAsset::Mesh);
                if (shapeIdx < meshAssets.size())
                    blockPlacer->SetMeshAsset(meshAssets[shapeIdx]);
                e->Suppress();
                UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
            }
            if (materialIdx >= 0)
            {
                /// @todo cache available materials
                MeshmoonAssetList materialAssets = rocket->AssetLibrary()->Assets(MeshmoonAsset::Material);
                if (materialIdx < materialAssets.size())
                    blockPlacer->SetMaterialAsset(0, materialAssets[materialIdx]);
                e->Suppress();
                UpdateBuildWidgetAnimationsState(0, true); // show build widget that user can see the current settings
            }
        }
    }

    if (!e->handled)
    {
        // Undo/Redo in all build modes
        if (e->sequence == undo && Undo())
            e->Suppress();
        else if (e->sequence == redo && Redo())
            e->Suppress();
    }
}
