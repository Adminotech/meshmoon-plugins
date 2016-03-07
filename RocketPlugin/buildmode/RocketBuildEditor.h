/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildEditor.h
    @brief  RocketBuildEditor manages the Rocket build UIs and editor entities (block placer, transform editor, etc.). */

#pragma once

#include "RocketFwd.h"
#include "RocketSceneWidget.h"
#include "RocketBuildContextWidget.h"

#include "SceneFwd.h"
#include "AssetFwd.h"
#include "OgreModuleFwd.h"
#include "PhysicsModuleFwd.h"
#include "FrameworkFwd.h"
#include "InputFwd.h"
#include "OgreModuleFwd.h"
//#include "Geometry/Polyhedron.h"
#include "IAttribute.h"
#include "UndoCommands.h"
#include "UndoManager.h"
#include "Scene.h"
#include "Math/float3x4.h"

#include <QPointer>
#include <QString>

class TransformEditor;
class QUndoCommand;

//typedef std::map<QString, std::vector<EntityWeakPtr > > EntityGroupMap;

/// Represents a group of entities that can be considered as a single object.
/*
struct EntityGroup
{
    EntityGroup() : parentGroup(0) {}
    EntityGroup(int groupId, const QString &groupName) : id(groupId), name(groupName), parentGroup(0) {}

    int id;
    QString name;
    EntityList entitites;
    EntityGroup *parentGroup;
};
*/

/// Defaults to tri mesh (4).
int StringToRigidBodyShapeType(const QString &str);

float3 ComputePositionInFrontOfCamera(Entity *cameraEntity, float offset);

/// Returns the camera world position. Set @c inspectParentChain to true to get position from top most parent instead.
/// This is useful on eg. avatar camera, where you might actually want the avatar position, not the camera position.
float3 CameraPosition(Entity *cameraEntity, bool inspectParentChain);

inline bool IsEntityAnAvatar(Entity *e) { return e && (e->Component(1 /*Avatar*/) || e->Component(500 /*MeshmoonAvatar*/)); }

/// Manages the Rocket build mode UI and editors.
/** @todo Move Block placer stuff from RocketBuildWidget to here.
    @ingroup MeshmoonRocket */
class RocketBuildEditor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(RocketBuildWidget *buildWidget READ BuildWidget) /**< @copydoc BuildWidget */
    Q_PROPERTY(RocketBuildContextWidget *contextWidget READ ContextWidget) /**< @copydoc ContextWidget */
    Q_PROPERTY(EntityPtr environmentEntity READ EnvironmentEntity) /**< @copydoc EnvironmentEntity */

public:
    /// @cond PRIVATE
    explicit RocketBuildEditor(RocketPlugin *plugin);
    ~RocketBuildEditor();

    /// Adds Build action to the tasbar.
    void UpdateTaskbarEntry(RocketTaskbar *taskbar);
    Framework *Fw() const { return framework; }
    /// @endcond

    /// Build context widget.
    /** Null if not visible, or if user has no building rights. */
    RocketBuildWidget *BuildWidget() const { return buildWidget; }

    /// The main build widget.
    /** Null if not visible, or if user has no building rights.
        @note For now the build widget owns the context widget. */
    RocketBuildContextWidget *ContextWidget() const { return contextWidget; }

    /// @todo Remove this
    EntityPtr ActiveEntity() const { return Selection().size() == 1 ? *Selection().begin() : EntityPtr(); }

    EntityList Selection() const { return EntityList(activeSelection.begin(), activeSelection.end()); }

    /// Sets new selection of entities, clears possible previous selection.
    /** @note Calling with empty value is same as calling ClearSelection. */
    void SetSelection(const EntityList &entities);
    void SetSelection(const EntityPtr &entity); /**< @overload */

    /// Appends selection with new entities.
    void AppendSelection(const EntityList &entities);
    void AppendSelection(const EntityPtr &entity); /**< @overload */

    /// Removes entities from selection.
    void RemoveFromSelection(const EntityList &entities);
    void RemoveFromSelection(const EntityPtr &entity); /**< @overload */

    /// Clears current selection.
    void ClearSelection();

    /// Generates polyhedron representation for a mesh using StanHull.
//    Polyhedron GeneratePolyhderonForMesh(Ogre::Mesh *mesh);

    /// The scene of which content we are currently editing.
    ScenePtr Scene() const { return currentScene.lock(); }
    void SetScene(const ScenePtr &scene);

    UndoManager *BuildUndoManager() const { return undoManager; }
    const shared_ptr<TransformEditor> &BuildTransformEditor() const { return transformEditor; }

    /// Returns the Rocket Environment Entity. If the entity does not exist, it will be created.
    EntityPtr EnvironmentEntity();

    /// Creates a new component in a way that it can be undoed and redoed by using Undo and Redo.
    template <typename T>
    QPair<T *, QUndoCommand *> CreateComponent(const EntityPtr &block, QUndoCommand *parent = 0,
        const QString &name = "", bool replicated = true, bool temporary = false);

    QPair<ComponentPtr, QUndoCommand *> CreateComponent(const EntityPtr &block, u32 componentTypeId, QUndoCommand *parent = 0,
        const QString &name = "", bool replicated = true, bool temporary = false);

    /// Sets value of an attribute in a way that it can be undoed and redoed by using Undo and Redo.
    template <typename T>
    void SetAttribute(Attribute<T> &attribute, const T &value, QUndoCommand *parent = 0);

    /// Copies all attribute values of an component in a way that it can be undoed and redoed by using Undo and Redo.
    /** This function is templated to ensure that the two components are of same type at compile time.
        If you are using shared_ptr<IComponent> this wont work, but the function will not do anything in that case. */
    template <typename T>
    void CopyAttributeValues(const shared_ptr<T> &source, const shared_ptr<T> &dest, QUndoCommand *parent = 0);

    /// Copies value of an attribute in a way that it can be undoed and redoed by using Undo and Redo.
    void CopyAttributeValue(IAttribute *source, IAttribute *dest, QUndoCommand *parent = 0);

    RocketBlockPlacer *BlockPlacer() const { return blockPlacer; }

    void UpdateBuildWidgetAnimationsState(int mouseX, bool forceShow = false);

    /// Returns all known entity groups in the current Scene.
//    const EntityGroupMap &EntityGroups() const { return entityGroups; }

public slots:
    /// Creates a new block using current settings and block placer's position.
    /** Name and Placeable components are created always, other components depend on the user's choice. */
//    EntityPtr CreateBlock(bool applyVisuals, bool applyPhysics, bool applyFunctionality);
    /// Creates a new block using current settings and block placer's position.
    /** @param components The component of which are wanted to be added the new building block.
        The data of the components is cloned to the new building block. */
    EntityPtr CreateBlock(const std::vector<ComponentPtr> &components);
    /// Creates a new block by cloning an existing entity.
    EntityPtr CloneBlock(const EntityPtr &source, bool clonePhysics, bool cloneFunctionality);

    /// Deletes a block that is currently selected (under mouse).
    void DeleteBlock();
    /// Deletes the whole current active selection.
    void DeleteSelection(bool showConfirmationDialog = true);

    /// Undoes the possible last applied action.
    bool Undo();

    /// Redoes the possible previously undoed action.
    bool Redo();

    void ShowBuildWidget(bool show);

    void SetSelectGroupsEnabled(bool select);
    bool IsSelectGroupsEnabled() const { return selectGroups; }

signals:
    /// @note @c entities can can be an empty list.
    void SelectionChanged(const EntityList &entities);

    /// @note @c groupName can be "" and @c entities an empty list.
    /// @todo Evaluate if needed.
    void ActiveGroupChanged(const QString &groupName, const EntityList &entities);

    /// Emitted when build related widgets are created.
    void BuildWidgetsCreated(RocketBuildWidget *buildWidget, RocketBuildContextWidget *contextWidget);

private:
    bool AppendSelectionInternal(const EntityPtr &entity);
    bool RemoveFromSelectionInternal(const EntityPtr &entity);

    RocketPlugin *rocket;
    Framework *framework;
    RocketBuildWidget *buildWidget;
    QPointer<RocketBuildContextWidget> contextWidget;
    QAction *toggleAction;
    InputContextPtr inputCtx;
    InputContextPtr inputCtxMove;
    EntityWeakPtr activeCamera;
    OgreWorldWeakPtr ogreWorld;
    //std::map<std::string, Polyhedron> polyhedrons;
    SceneWeakPtr currentScene;
    shared_ptr<TransformEditor> transformEditor;
    UndoManager *undoManager;
    RocketBlockPlacer *blockPlacer;
    float3x4 originalTm;
    //EntityGroupMap entityGroups;
    std::set<EntityPtr> activeSelection;
    bool selectGroups;

private slots:
    void OnAuthenticated(int permissionLevel);
    void OnAuthReset();
    void OnActiveCameraChanged(Entity *);
    void CheckForGroupChange(IComponent *comp, IAttribute *attr);
    void CheckForGroupChange(const EntityList &oldSelection);

    void OnBuildWidgetVisibilityChanged(bool visible);
    void OnContextWidgetVisibilityChanged(bool visible);

    void HandleMouseEvent(MouseEvent *e);
    void HandleMouseMove(MouseEvent *e);
    void HandleKeyEvent(KeyEvent *e);
    
    void OnStorageWidgetCreated(RocketStorageWidget *storageWidget);

    void OnBuildWidgetAnimationsCompleted();
    void OnBuildWidgetAnimationsProgress();
    void OnBuildContextWidgetAnimationsProgress();
    
    void OnWindowResize(int, int);
};
Q_DECLARE_METATYPE(RocketBuildEditor*)

template <typename T>
QPair<T*, QUndoCommand*> RocketBuildEditor::CreateComponent(const EntityPtr &block, QUndoCommand *parent, const QString &name, bool replicated, bool temporary)
{
    AddComponentCommand *action = new AddComponentCommand(block->ParentScene()->shared_from_this(), undoManager->Tracker(), EntityIdList() << block->Id(), T::ComponentTypeId, name, replicated, temporary, parent);
    if (!parent)
        undoManager->Push(action);
    else
        action->redo();
    return QPair<T*, QUndoCommand*>(block->Component<T>().get(), action);
}

template <typename T>
void RocketBuildEditor::SetAttribute(Attribute<T> &attribute, const T &value, QUndoCommand *parent)
{
    EditAttributeCommand<T> *action = new EditAttributeCommand<T>(&attribute, value, parent);
    if (!parent)
        undoManager->Push(action);
    else
        action->redo();
}
