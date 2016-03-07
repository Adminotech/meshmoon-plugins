/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#include "StableHeaders.h"

#include "RocketOcclusionManager.h"

#include "RocketPlugin.h"
#include "Framework.h"
#include "Entity.h"
#include "Scene.h"

#include "EC_Name.h"
#include "EC_Mesh.h"
#include "EC_MeshmoonOccluder.h"
#include "EC_MeshmoonCulling.h"
#include "EC_Script.h"
#include "EC_Placeable.h"

#include "SceneAPI.h"
#include "ConsoleAPI.h"
#include "AssetAPI.h"

RocketOcclusionManager::RocketOcclusionManager(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin_->GetFramework()),
    LC("RocketOcclusionManager: ")
{
#ifdef ROCKET_UMBRA_ENABLED
    framework_->Console()->RegisterCommand("generateOcclusion", "Creates EC_MeshmoonOccluder component for every entity that has meshes. This will remove all existing EC_MeshmoonOccluder components. After creating components, tome will be generated.", this, SLOT(GenerateOcclusion()));
#endif
}

RocketOcclusionManager::~RocketOcclusionManager()
{
}

#ifdef ROCKET_UMBRA_ENABLED
void RocketOcclusionManager::GenerateOcclusion()
{
    CreateOcclusionComponents();
    StartTomeGeneration();
}

void RocketOcclusionManager::CreateOcclusionComponents()
{
    LogInfo(LC + "Creating occlusion culling components...");

    Scene *tundraScene = framework_->Scene()->MainCameraScene();
    if(!tundraScene)
        return;

    // Create EC_MeshmoonCulling
    EntityList cullingEntities = tundraScene->EntitiesWithComponent(EC_MeshmoonCulling::TypeIdStatic());
    if(!cullingEntities.empty())
    {
        for(EntityList::iterator it = cullingEntities.begin(); it != cullingEntities.end(); ++it)
        {
            EntityPtr entity = (*it);

            Entity::ComponentVector components = entity->ComponentsOfType(EC_MeshmoonCulling::TypeIdStatic());
            if(!components.empty())
            {
                for(size_t i = 0; i < components.size(); ++i)
                {
                    entity->RemoveComponent(components[i], AttributeChange::LocalOnly);
                }
            }
        }
    }

    QStringList cullingComponents;
    cullingComponents << "EC_Name" << "EC_MeshmoonCulling";
    EntityPtr cullingEntity = tundraScene->EntityByName("OcclusionCulling");
    if(cullingEntity.get())
        tundraScene->DeleteEntityById(cullingEntity->Id());
    cullingEntity = tundraScene->CreateEntity(0, cullingComponents, AttributeChange::LocalOnly, false, false);

    LogInfo(LC + "Create EC_MeshmoonCulling");
    EC_Name* name = cullingEntity->Component<EC_Name>().get();
    name->name.Set("OcclusionCulling", AttributeChange::LocalOnly);

    EC_MeshmoonCulling *culling = cullingEntity->Component<EC_MeshmoonCulling>().get();
    culling->id.Set(1, AttributeChange::LocalOnly);

    EntityList entities = tundraScene->EntitiesWithComponent(EC_Mesh::TypeIdStatic());

    unsigned int occluderId = 0;

    LogInfo(LC + "Creating EC_MeshmoonOccluders...");
    for(EntityList::iterator it = entities.begin(); it != entities.end(); ++it)
    {
        EntityPtr entity = (*it);

        EC_Placeable *placeable = entity->Component<EC_Placeable>().get();
        if(!placeable)
            continue;

        EntityPtr parent = placeable->getparentRef().Lookup(tundraScene);
        if(parent.get())
        {
            Entity::ComponentVector parentScripts = entity->ComponentsOfType(EC_Script::TypeIdStatic());
            if(!parentScripts.empty())
                continue;
        }

        Entity::ComponentVector scripts = entity->ComponentsOfType(EC_Script::TypeIdStatic());
        if(!scripts.empty())
            continue;

        Entity::ComponentVector meshes = entity->ComponentsOfType(EC_Mesh::TypeIdStatic());
        Entity::ComponentVector occluders = entity->ComponentsOfType(EC_MeshmoonOccluder::TypeIdStatic());
        if(!occluders.empty())
        {
            for(size_t i = 0; i < occluders.size(); ++i)
                entity->RemoveComponent(occluders[i], AttributeChange::LocalOnly);
        }

        for(size_t i = 0; i < meshes.size(); ++i)
        {
            QString componentName = "(" + QString::number(i) + ")";

            EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(meshes[i].get());
            if(!mesh)
                continue;

            if(CheckBoundingBoxIncludes(mesh, entities))
                continue;

            if(mesh->meshRef.Get().ref.isEmpty())
                continue;

            EC_MeshmoonOccluder* occluder = dynamic_cast<EC_MeshmoonOccluder*>(entity->CreateComponent(EC_MeshmoonOccluder::TypeIdStatic(), componentName, AttributeChange::LocalOnly, false).get());

            occluder->id.Set(occluderId, AttributeChange::LocalOnly);
            occluder->meshRef.Set(mesh->meshRef.Get(), AttributeChange::LocalOnly);
            occluder->tomeId.Set(1, AttributeChange::LocalOnly);

            // Set object to be both occluder and target
            occluder->occluder.Set(true, AttributeChange::LocalOnly);
            occluder->target.Set(true, AttributeChange::LocalOnly);

            ++occluderId;
        }
    }
}

bool RocketOcclusionManager::CheckBoundingBoxIncludes(EC_Mesh *targetMesh, const EntityList& entities)
{
    OBB targetOBB = targetMesh->WorldOBB();

    if(targetOBB.Size().x < 10 || targetOBB.Size().y < 10 || targetOBB.Size().z < 10)
        return false;

    for(EntityList::const_iterator it = entities.begin(); it != entities.end(); ++it)
    {
        EntityPtr entity = (*it);

        Entity::ComponentVector meshes = entity->ComponentsOfType(EC_Mesh::TypeIdStatic());
        for(size_t i = 0; i < meshes.size(); ++i)
        {
            EC_Mesh *mesh = dynamic_cast<EC_Mesh*>(meshes[i].get());
            if(!mesh)
                continue;

            if(mesh == targetMesh)
                continue;

            OBB testOBB = mesh->WorldOBB();
            if(targetOBB.Contains(testOBB))
                return true;
        }
    }
    return false;
}

void RocketOcclusionManager::StartTomeGeneration()
{
    Scene *tundraScene = framework_->Scene()->MainCameraScene();
    if(!tundraScene)
        return;

    EntityPtr cullingEntity = tundraScene->EntityByName("OcclusionCulling");
    if(!cullingEntity.get())
    {
        LogError(LC + QString("No entity \"OcclusionCulling\""));
        return;
    }

    EC_MeshmoonCulling *culling = cullingEntity->Component<EC_MeshmoonCulling>().get();
    if(!culling)
    {
        LogError(LC + "No EC_MeshmoonCulling component.");
        return;
    }

    QString tomePath = framework_->Asset()->GenerateTemporaryNonexistingAssetFilename(".tome");

    connect(culling, SIGNAL(TomeGenerated(const QString&)), this, SLOT(TomeGenerated(const QString&)));
    culling->GenerateTome(tomePath);
}

void RocketOcclusionManager::TomeGenerated(const QString& fileName)
{
    Scene *tundraScene = framework_->Scene()->MainCameraScene();
    if(!tundraScene)
        return;

    EntityPtr cullingEntity = tundraScene->EntityByName("OcclusionCulling");
    if(!cullingEntity.get())
        return;

    EC_MeshmoonCulling *culling = cullingEntity->Component<EC_MeshmoonCulling>().get();
    if(!culling)
        return;

    disconnect(culling, SIGNAL(TomeGenerated(const QString&)), this, SLOT(TomeGenerated(const QString&)));

    culling->tomeRef.Set(AssetReference(fileName, "Binary"), AttributeChange::LocalOnly);
}

#else

void RocketOcclusionManager::GenerateOcclusion()
{
}
void RocketOcclusionManager::CreateOcclusionComponents()
{
}
bool RocketOcclusionManager::CheckBoundingBoxIncludes(EC_Mesh * /*targetMesh*/, const EntityList& /*entities*/)
{
    return false;
}
void RocketOcclusionManager::StartTomeGeneration()
{
}
void RocketOcclusionManager::TomeGenerated(const QString& /*fileName*/)
{
}

#endif
