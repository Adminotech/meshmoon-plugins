
#include "StableHeaders.h"

#include "Framework.h"
#include "IRenderer.h"
#include "UniqueIdGenerator.h"
#include "LoggingFunctions.h"

#include "common/layers/MeshmoonLayers.h"
#include "common/loaders/MeshmoonSceneValidator.h"
#include "common/json/MeshmoonJson.h"

#include "MeshmoonSpaceLoader.h"
#include "MeshmoonHttpPlugin.h"
#include "MeshmoonHttpClient.h"
#include "MeshmoonHttpRequest.h"

#include <QDebug>

MeshmoonSpaceLoader::MeshmoonSpaceLoader(Framework *framework, const QString &source) :
    LC("[MeshmoonSpaceLoader]: "),
    framework_(framework),
    source_(source),
    layers_(new MeshmoonLayers(framework))
{
    MeshmoonHttpPlugin *http = framework_->Module<MeshmoonHttpPlugin>();
    if (http)
    {
        MeshmoonHttpRequestPtr request = http->Client()->Get(source);
        if (request.get())
            connect(request.get(), SIGNAL(Finished(MeshmoonHttpRequest*, int, const QString&)), 
                this, SLOT(SpacesQueryFinished(MeshmoonHttpRequest*, int, const QString&)));
    }
}

MeshmoonSpaceLoader::~MeshmoonSpaceLoader()
{
    SAFE_DELETE(layers_);
    Clear();
}

void MeshmoonSpaceLoader::Clear()
{
    for (int i=0, len=spaces_.size(); i<len; i++)
        SAFE_DELETE(spaces_[i]);
    spaces_.clear();
}

void MeshmoonSpaceLoader::CheckCompleted()
{
    if (spaces_.isEmpty())
        return;

    for (int si=0, slen=spaces_.size(); si<slen; si++)
    {
        QVariant loaded = spaces_[si]->property("txmlDownloaded");
        if (!loaded.isValid() || loaded.isNull() || loaded.toBool() == false)
            return;
            
        for (int li=0, llen=spaces_[si]->NumLayers(); li<llen; li++)
        {
            loaded = spaces_[si]->LayerByIndex(li)->property("txmlDownloaded");
            if (!loaded.isValid() || loaded.isNull() || loaded.toBool() == false)
                return;
        }
    }

    /// @note This might change in the future to not use Meshmoon::SceneLayer but directly Meshmoon::Layer and Meshmoon::Space.
    
    // Gather layers to feed into layer manager for loading.
    UniqueIdGenerator generator;
    Meshmoon::SceneLayerList layers;
    for (int si=0, slen=spaces_.size(); si<slen; si++)
    {
        layers << spaces_[si]->ToSceneLayer(generator.AllocateReplicated());
        for (int li=0, llen=spaces_[si]->NumLayers(); li<llen; li++)
            layers << spaces_[si]->LayerByIndex(li)->ToSceneLayer(generator.AllocateReplicated());
    }
    
    // Add prepared layers and load them.
    layers_->Add(layers);
    layers_->Load();

    // Validate. Removes extra "RocketEnvironmentEntity" etc.
    if (framework_->Renderer())
        MeshmoonSceneValidator::Validate(framework_->Renderer()->MainCameraScene());
    
    // Clear state
    Clear();
    
    // Expose layers for scripting. Better name here like common.layers
    /// @todo This should not be done if RocketPlugin is loaded?
    framework_->RegisterDynamicObject("layers", layers_);
}

Meshmoon::Space *MeshmoonSpaceLoader::SpaceById(const QString &id)
{
    for (int i=0, len=spaces_.size(); i<len; i++)
        if (spaces_[i]->Id() == id)
            return spaces_[i];
    return 0;
}

void MeshmoonSpaceLoader::SpacesQueryFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error)
{
    Clear();

    if (statusCode != 200)
    {
        LogError(LC + QString("Failed to query layers from %1: %2").arg(source_).arg(error));
        return;
    }

    QVariant data = request->ResponseJSON();
    if (!data.isValid() || data.isNull())
    {
        LogError(LC + "Layer query response JSON is invalid");
        return;
    }

    MeshmoonHttpPlugin *http = framework_->Module<MeshmoonHttpPlugin>();
    if (!http)
        return;
    
    spaces_ = Meshmoon::JSON::DeserializeSpaces(data);
    foreach(Meshmoon::Space *space, spaces_)
    {
        MeshmoonHttpRequestPtr req = http->Client()->Get(space->PresignedUrl());
        if (req.get())
        {
            req->setProperty("spaceId", space->Id());
            connect(req.get(), SIGNAL(Finished(MeshmoonHttpRequest*, int, const QString&)), 
                this, SLOT(SpaceTxmlFinished(MeshmoonHttpRequest*, int, const QString&)));
                
            LogInfo(QString("Requested Space %1 %2").arg(space->Id()).arg(space->Name()));
                
            foreach(Meshmoon::Layer *layer, space->Layers())
            {               
                req = http->Client()->Get(layer->PresignedUrl());
                if (req.get())
                {
                    req->setProperty("spaceId", space->Id());
                    req->setProperty("layerId", static_cast<uint>(layer->Id()));
                    connect(req.get(), SIGNAL(Finished(MeshmoonHttpRequest*, int, const QString&)), 
                        this, SLOT(SpaceLayerTxmlFinished(MeshmoonHttpRequest*, int, const QString&)));
                        
                    LogInfo(QString("    Requested Layer %1 %2").arg(layer->Id()).arg(layer->Name()));
                }
            }
        }
    }
}

void MeshmoonSpaceLoader::SpaceTxmlFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error)
{   
    Meshmoon::Space *space = SpaceById(request->property("spaceId").toString());
    if (!space)
    {
        LogError(LC + QString("Failed to find Space '%1' after txml download").arg(request->property("spaceId").toString()));
        return;
    }
    
    space->setProperty("txmlDownloaded", true);
    if (statusCode == 200)
    {
        LogInfo(LC + QString("Loaded Space '%1'").arg(space->Name()));
        space->SetSceneData(request->ResponseBodyBytes());
    }
    else
        LogError(LC + QString("Failed to fetch Space txml: %1").arg(error));
    
    CheckCompleted();
}

void MeshmoonSpaceLoader::SpaceLayerTxmlFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error)
{
    Meshmoon::Space *space = SpaceById(request->property("spaceId").toString());
    if (!space)
    {
        LogError(LC + QString("Failed to find Space '%1' after txml download").arg(request->property("spaceId").toString()));
        return;
    }

    Meshmoon::Layer *layer = space->LayerById(static_cast<u32>(request->property("layerId").toUInt()));
    if (layer)
    {
        layer->setProperty("txmlDownloaded", true);
        if (statusCode == 200)
        {
            LogInfo(LC + QString("Loaded Layer '%1' Space '%2'").arg(layer->Name()).arg(space->Name()));
            layer->SetSceneData(request->ResponseBodyBytes());
        }
        else
            LogError(LC + QString("Failed to fetch Layer txml: %1").arg(error));
    }
    else
    {
        LogError(LC + QString("Failed to find Layer '%1' for Space '%2' after txml download").arg(request->property("layerId").toUInt()).arg(space->Id()));
        return;
    }

    CheckCompleted();
}
