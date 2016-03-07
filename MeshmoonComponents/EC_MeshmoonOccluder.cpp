/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#include "StableHeaders.h"

#ifdef ROCKET_UMBRA_ENABLED

#include "EC_MeshmoonOccluder.h"

#include "Ogre.h"
#include "OgreMeshAsset.h"

EC_MeshmoonOccluder::EC_MeshmoonOccluder(Scene *scene) :
    IComponent(scene),
    LC("MeshmoonOccluder: "),
    INIT_ATTRIBUTE_VALUE(tomeId, "Tome Id", 0),
    INIT_ATTRIBUTE_VALUE(meshRef, "Mesh ref", AssetReference("", "Binary")),
    INIT_ATTRIBUTE_VALUE(id, "Id", 0),
    INIT_ATTRIBUTE_VALUE(occluder, "Occluder", false),
    INIT_ATTRIBUTE_VALUE(target, "Target", false),
    targetMesh_(0)
{
}

EC_MeshmoonOccluder::~EC_MeshmoonOccluder()
{
    targetMesh_ = 0;
}

bool EC_MeshmoonOccluder::MeshReady()
{
    if(framework->IsHeadless())
        return false;

    Entity* parent = parentEntity;
    if(!parent)
        return false;

    if(targetMesh_)
    {
        OgreMeshAssetPtr meshAsset = targetMesh_->MeshAsset();
        if(meshAsset.get())
            return meshAsset->IsLoaded();
        else
            return false;
    }

    Entity::ComponentVector meshes = parent->ComponentsOfType(EC_Mesh::TypeIdStatic());
    if(meshRef.Get().ref == "" && meshes.size() > 1)
    {
        LogWarning(LC + "Parent entity has more than one mesh, but mesh ref is not set.");
        return false;
    }

    if(meshRef.Get().ref == "")
        targetMesh_ = dynamic_cast<EC_Mesh*>(meshes.front().get());
    else
    {
        for(size_t i = 0; i < meshes.size(); ++i)
        {
            EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(meshes[i].get());
            if(mesh->meshRef.Get().ref == meshRef.Get().ref)
            {
                targetMesh_ = mesh;
                break;
            }
        }
    }

    if(!targetMesh_)
        return false;
    OgreMeshAssetPtr meshAsset = targetMesh_->MeshAsset();
    if(meshAsset.get())
        return meshAsset->IsLoaded();
    else
        return false;
}

void EC_MeshmoonOccluder::Show()
{
    if(framework->IsHeadless() || !targetMesh_)
        return;

    if(!targetMesh_->HasMesh())
        return;

    if(!targetMesh_->OgreEntity()->isVisible())
        targetMesh_->OgreEntity()->setVisible(true);
}

void EC_MeshmoonOccluder::Hide()
{
    if(framework->IsHeadless() || !targetMesh_)
        return;

    if(!targetMesh_->HasMesh())
        return;

    if(targetMesh_->OgreEntity()->isVisible())
        targetMesh_->OgreEntity()->setVisible(false);
}

EC_Mesh* EC_MeshmoonOccluder::Mesh()
{
    return targetMesh_;
}

void EC_MeshmoonOccluder::AttributesChanged()
{
    if(framework->IsHeadless())
        return;
    if(!meshRef.ValueChanged())
        return;

    Entity* parent = parentEntity;
    if(!parent)
        return;

    Entity::ComponentVector meshes = parent->ComponentsOfType(EC_Mesh::TypeIdStatic());
    if(meshRef.Get().ref == "" && meshes.size() > 1)
    {
        LogWarning(LC + "Parent entity has more than one mesh, but mesh ref is not set.");
        return;
    }

    if(meshRef.Get().ref == "")
        targetMesh_ = dynamic_cast<EC_Mesh*>(meshes.front().get());
    else
    {
        for(size_t i = 0; i < meshes.size(); ++i)
        {
            EC_Mesh* mesh = dynamic_cast<EC_Mesh*>(meshes[i].get());
            if(QString::fromStdString(mesh->MeshName()) == meshRef.Get().ref)
            {
                targetMesh_ = mesh;
                break;
            }
        }
    }
}

#endif
