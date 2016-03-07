/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBlockPlacer.h
    @brief   */

#pragma once

#include <QObject>
#include <QTimer>

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "OgreModuleFwd.h"

#include "MeshmoonAsset.h"
#include "Geometry/OBB.h"
#include "Geometry/Sphere.h"

class QPushButton;

/// @cond PRIVATE

struct BlockMaterial
{
    BlockMaterial() : libraryAsset(0) {}
    /// Used if asset is from a Meshmoon asset library.
    MeshmoonAsset *libraryAsset;
    /// Used if asset is a regular asset from a storage.
    //AssetWeakPtr storageAsset;
};

/// Utility class for the building block placer functionality.
class RocketBlockPlacer : public QObject
{
    Q_OBJECT
    Q_ENUMS(PlacerMode)
    Q_ENUMS(PlacementMode)

public:
    explicit RocketBlockPlacer(RocketPlugin *owner);
    ~RocketBlockPlacer();

    /// Different block placer modes.
    enum PlacerMode
    {
        CreateBlock, ///< Placer is used to create a new block, placer is placed to the position where the new block would be created.
        CloneBlock, ///< Placer is used to clone an existing entity, placer is placed to the position where the new block would be created.
        EditBlock, ///< Placer is used to pick an entity for editing, no placer visible.
        RemoveBlock, ///< Placer is used to remove an existing entity, placer "swallows" the block that would be removed.
        // MergeMode?
        NumPlacerModes
    };

    /// Different placement modes.
    enum PlacementMode
    {
        SnapPlacement, ///< Block is snapped to another block.
        FreePlacement, ///< Block can be placed freely.
        NumPlacementModes
    };

    PlacerMode BlockPlacerMode() const { return placerMode; }
    void SetBlockPlacerMode(PlacerMode mode);

    PlacementMode BlockPlacementMode() const { return placementMode; }
    void SetBlockPlacementMode(PlacementMode mode);

    /// The maximum distance (from a camera) where a block can placed.
    /** Setting MaxBlockDistance <= 0 disables it. */
    float MaxBlockDistance() const { return maxBlockDistance; }
    void SetMaxBlockDistance(float distance);

    void Show();
    /// Hides block placer and resets the ghost blocks
    void Hide();
    bool IsVisible() const;

    void SetActiveBlock(const EntityPtr &entity);
    void Update();
    void DrawVisualHelp();
    void Create();
    void Remove();
    void Refresh();

    /// Returns whether were placing i.e. creating currently.
    //bool IsPlacingActive() const { return placerMode == CreateBlock || placerMode == CloneBlock; }
    bool IsPlacingActive() const { return BlockPlacerMode() != EditBlock; }

    /// Get or create a storage based (not asset library based) MeshmoonAsset.
    MeshmoonAsset *GetOrCreateCachedStorageAsset(MeshmoonAsset::Type type, const QString &absoluteAssetRef, QString name = "");

    /// Sets current mesh asset from a Meshmoon library item.
    /** @note Clears the current materials if in edit mode. */
    void SetMeshAsset(MeshmoonAsset *mesh);

    /// Returns current mesh asset.
    MeshmoonAsset *MeshAsset() const { return blockMeshAsset; }

    /// Sets a material to a specific index.
    void SetMaterialAsset(uint submeshIndex, MeshmoonAsset *material);

    /// Returns current material assets.
    const QList<BlockMaterial> &Materials() const { return blockMaterials; }

    void SaveSettingsToConfig();
    void SaveVisualsToConfig();

    void LoadSettingsFromConfig();
    void LoadVisualsFromConfig();
    void LoadShapeFromConfig();
    void LoadMaterialsFromConfig();

    //const EntityPtr &Entity() const {return placerEntity; }
    shared_ptr<EC_Placeable> Placeable() const { return placerPlaceable.lock(); }
    shared_ptr<EC_Mesh> Mesh() const { return placerMesh.lock(); }

public slots:
    void AllocateMaterialSlots();
    void GeneratePolyhderonForBlockPlacer();
    void GenerateGhostBlocksForCurrentBlock();
    void ApplyMaterial();
    void ResetMaxBlockDistancePlane();

signals:
    /// Emitted when any of the block placer settings (mode, max block distance, etc.) changes.
    void SettingsChanged();

private:
    friend class RocketBuildEditor;
    RocketPlugin *rocket;
    OBB ghostObbs[6];
//    Polyhedron blockPlacerPolyhedron;
    Sphere maxBlockDistanceSphere;
    EntityWeakPtr lastBlockUnderMouse;
    EntityPtr placerEntity;
    weak_ptr<EC_Placeable> placerPlaceable;
    weak_ptr<EC_Mesh> placerMesh;
    OgreWorldWeakPtr ogreWorld;
    PlacerMode placerMode;
    PlacementMode placementMode;
    float maxBlockDistance;
    MeshmoonAsset *blockMeshAsset;
    QList<BlockMaterial> blockMaterials;
    QTimer maxBlockDistanceTimer;

    QHash<QString, MeshmoonAsset*> storageAssetCache;

private slots:
    void ResetGhostBlocks();
    void GenerateGhostBlocks(const OBB &hitObb);
    void InitializeAssetSelection();

    /// Searches for the default cube mesh from the list of assets and sets it use use if found.
    void SetDefaultBlockMesh(const MeshmoonAssetList &assets);
};
Q_DECLARE_METATYPE(RocketBlockPlacer*)
Q_DECLARE_METATYPE(RocketBlockPlacer::PlacerMode)
Q_DECLARE_METATYPE(RocketBlockPlacer::PlacementMode)

/// @endcond