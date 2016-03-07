/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonLayers.h"
#include "MeshmoonLayerProcessor.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "IRenderer.h"
#include "Scene.h"
#include "Entity.h"
#include "AssetAPI.h"

#include "TundraLogicModule.h"
#include "Server.h"
#include "UserConnection.h"
#include "SyncState.h"

#include "EC_Script.h"

#include <QNetworkReply>

MeshmoonLayers::MeshmoonLayers(Framework *framework) :
    framework_(framework),
    processor_(new MeshmoonLayerProcessor(framework)),
    LC("[MeshmoonLayers]: ")
{
}

MeshmoonLayers::~MeshmoonLayers()
{
    SAFE_DELETE(processor_);
}

const Meshmoon::SceneLayerList &MeshmoonLayers::Layers() const
{
    return layers_;
}

Meshmoon::SceneLayer MeshmoonLayers::Layer(u32 id) const
{
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &existing = layers_[i];
        if (existing.id == id)
            return existing;
    }
    return Meshmoon::SceneLayer();
}

Meshmoon::SceneLayer MeshmoonLayers::Layer(const QString &name) const
{
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &existing = layers_[i];
        if (existing.name == name)
            return existing;
    }
    return Meshmoon::SceneLayer();
}

void MeshmoonLayers::RemoveAll()
{
    layers_.clear();
}

bool MeshmoonLayers::Remove(u32 id)
{
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &existing = layers_[i];
        if (existing.id == id)
        {
            layers_.removeAt(i);
            return true;
        }
    }
    return false;
}

bool MeshmoonLayers::Add(const Meshmoon::SceneLayer &layer)
{
    if (Exists(layer))
        return false;
    layers_ << layer;
    return true;
}

bool MeshmoonLayers::Add(const Meshmoon::SceneLayerList &layers)
{
    bool allok = true;
    for (int i=0,len=layers.size(); i<len; ++i)
    {
        bool ok = Add(layers[i]);
        if (allok && !ok)
            allok = false;
    }
    return allok;
}

bool MeshmoonLayers::Exists(const Meshmoon::SceneLayer &layer) const
{
    const QString baseUrl = layer.txmlUrl.toString(QUrl::RemoveQuery);
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &existing = layers_[i];
        if (baseUrl.compare(existing.txmlUrl.toString(QUrl::RemoveQuery), Qt::CaseSensitive) == 0)
        {
            LogWarning(LC + QString("Layer '%1' already has txml URL of %2. Skipping layer '%3'.")
                .arg(existing.name).arg(baseUrl).arg(layer.name));
            return true;
        }
    }
    return false;
}

bool MeshmoonLayers::CheckLayerDownload(QNetworkReply *reply)
{
    if (!reply)
        return false;

    QString replyUrl = reply->url().toString();
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        Meshmoon::SceneLayer &layer = layers_[i];
        if (replyUrl == layer.txmlUrl.toString())
        {
            layer.downloaded = true;
            if (reply->error() == QNetworkReply::NoError)
                layer.sceneData = reply->readAll();
            else
            {
                layer.sceneData.clear();
                LogError(LC + QString("Failed to download scene layer %1 txml = %2")
                    .arg(layer.toString()).arg(layer.txmlUrl.toString(QUrl::RemoveQuery|QUrl::StripTrailingSlash)));
            }
            return true;
        }
    }
    return false;
}

bool MeshmoonLayers::AllDownloaded() const
{
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &layer = layers_[i];
        if (!layer.downloaded)
            return false;
    }
    return true;
}

bool MeshmoonLayers::AllLoaded() const
{
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        const Meshmoon::SceneLayer &layer = layers_[i];
        if (!layer.loaded)
            return false;
    }
    return true;
}

void MeshmoonLayers::Load()
{
    if (layers_.isEmpty())
        return;

    LogInfo(" ");
    for (int i=0,len=layers_.size(); i<len; ++i)
    {
        Meshmoon::SceneLayer &layer = layers_[i];
        if (layer.loaded)
            continue;

        layer.loaded = true;
        if (processor_->LoadSceneLayer(layer))
            emit LayerLoaded(layer);        
    }
    LogInfo(" ");

    emit LayersLoaded();
}

bool MeshmoonLayers::UnloadAll()
{
    LogInfo(LC + QString("Unloading %1 scene layers").arg(layers_.size()));
    for (int i=0,len=layers_.size(); i<len; ++i)
        Unload(layers_[i]);
    return true;
}

bool MeshmoonLayers::Unload(u32 id)
{
    Meshmoon::SceneLayer layer = Layer(id);
    if (!layer.IsValid())
        return false;
        
    LogInfo(LC + QString("Unloading layer with id %1").arg(layer.id));
    Unload(layer);
    return true;
}

bool MeshmoonLayers::Unload(const Meshmoon::SceneLayer &layer)
{
    Scene *scene = framework_->Renderer()->MainCameraScene();
    if (!scene)
    {
        LogError(LC + "Cannot unload layers, scene is null.");
        return false;
    }

    LogInfo(LC + QString("  %1 with %2 entities").arg(layer.name).arg(layer.entities.size()));

    foreach(EntityWeakPtr layerEnt, layer.entities)
    {
        Entity *ent = layerEnt.lock().get();
        if (ent)
        {
            entity_id_t entId = ent->Id();

            // Unload script and its assets
            std::vector<shared_ptr<EC_Script> > scripts = ent->ComponentsOfType<EC_Script>();
            for (std::vector<shared_ptr<EC_Script> >::iterator iter=scripts.begin(); iter != scripts.end(); ++iter)
            {
                EC_Script *script = (*iter).get();
                if (!script)
                    continue;

                script->Unload();

                AssetReferenceList scriptRefs = script->scriptRef.Get();
                for(int i=0; i<scriptRefs.Size(); ++i)
                {
                    QString ref = scriptRefs[i].ref;
                    framework_->Asset()->ForgetAsset(ref, true);
                }
            }

            /// @todo unload mesh assets if not used elsewhere, so they get reloaded from source.

            // Remove entity from scene
            ent = 0;
            layerEnt.reset();
            scene->RemoveEntity(entId, AttributeChange::Replicate);
        }
    }

    emit LayerRemoved(layer);
    return true;
}



bool MeshmoonLayers::UpdateClientLayerState(UserConnection *connection, u32 layerId, bool visible)
{
    if (!connection)
    {
        LogError(LC + "UpdateClientLayerState: Target UserConnection is null");
        return false;
    }

    Meshmoon::SceneLayer layer = Layer(layerId);
    if (layer.IsValid())
    {
        SceneSyncState *sceneState = connection->syncState.get();
        if (sceneState)
        {
            foreach(EntityWeakPtr ent, layer.entities)
            {
                if (!ent.expired())
                {
                    if (visible)
                        sceneState->MarkPendingEntityDirty(ent.lock()->Id());
                    else
                        sceneState->MarkEntityPending(ent.lock()->Id());
                }
            }
            emit LayerVisibilityChanged(connection, layer, visible);
            return true;
        }
        else
            LogError(LC + QString("UpdateClientLayerState: Client SceneSyncState is null for connection id %1")
                .arg(connection->ConnectionId()));
    }
    else
        LogError(LC + QString("UpdateClientLayerState: Layer with id %1 does not exist").arg(layerId));
    return false;
}

bool MeshmoonLayers::UpdateClientLayerState(u32 connectionId, u32 layerId, bool visible)
{
    TundraLogicModule *tundraLogic = framework_->Module<TundraLogicModule>();
    if (tundraLogic && !tundraLogic->IsServer())
        return false;
    UserConnection *connection = (tundraLogic ? tundraLogic->GetServer()->GetUserConnection(connectionId).get() : 0);
    if (connection)
    {
        UpdateClientLayerState(connection, layerId, visible);    
        return true;
    }
    LogError(LC + QString("UpdateClientLayerState: Failed to find user connection %1").arg(connectionId));
    return false;
}
