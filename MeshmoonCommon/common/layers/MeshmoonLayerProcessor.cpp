/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonLayerProcessor.h"
#include "common/json/MeshmoonJson.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "IRenderer.h"
#include "CoreJsonUtils.h"
#include "UniqueIdGenerator.h"

#include "SceneAPI.h"
#include "Scene.h"
#include "SceneDesc.h"
#include "Entity.h"
#include "IComponent.h"

#include "AssetAPI.h"

#include "EC_Name.h"
#include "EC_DynamicComponent.h"
#include "EC_Placeable.h"
#include "EC_Mesh.h"
#include "EC_Sky.h"
#include "EC_Billboard.h"
#include "EC_ParticleSystem.h"
#include "EC_Material.h"
#include "EC_Script.h"
#include "EC_Sound.h"
#include "EC_HoveringText.h"

#include <QTextStream>
#include <QDomDocument>

#include <kNet/PolledTimer.h>

/// @cond PRIVATE

namespace
{
    const u32 InvalidTypeId = 0xffffffff;
}

MeshmoonLayerProcessor::MeshmoonLayerProcessor(Framework *framework) :
    framework_(framework),
    LC("[MeshmoonLayers]: ")
{
    interestingComponentTypeNames_ << EC_Mesh::TypeNameStatic()
                                   << EC_Material::TypeNameStatic()
                                   << EC_Sound::TypeNameStatic()
                                   << EC_Sky::TypeNameStatic()
                                   << EC_HoveringText::TypeNameStatic()
                                   << EC_Billboard::TypeNameStatic()
                                   << EC_ParticleSystem::TypeNameStatic() 
                                   << EC_Script::TypeNameStatic()
                                   // Manual names for components where we don't want direct linking.
                                   << "EC_RigidBody"
                                   << "EC_Terrain"
                                   << "EC_WaterPlane"
                                   << "EC_SlideShow"
                                   << "EC_WidgetBillboard"
                                   << "EC_Hydrax";

    interestingComponentTypeIds_   << EC_Mesh::TypeIdStatic()
                                   << EC_Material::TypeIdStatic()
                                   << EC_Sound::TypeIdStatic()
                                   << EC_Sky::TypeIdStatic()
                                   << EC_HoveringText::TypeIdStatic()
                                   << EC_Billboard::TypeIdStatic()
                                   << EC_ParticleSystem::TypeIdStatic()
                                   << EC_Script::TypeIdStatic()
                                   // Manual IDs for components where we don't want direct linking.
                                   // @note Hazard is that these *might* change, but its very unlikely.
                                   << 23    // EC_RigidBody
                                   << 11    // EC_Terrain
                                   << 12    // EC_WaterPlane
                                   << 41    // EC_SlideShow
                                   << 42    // EC_WidgetBillboard
                                   << 39;   // EC_Hydrax

    interestingAttributeTypeIds_   << (u32)IAttribute::StringId
                                   << (u32)IAttribute::AssetReferenceId
                                   << (u32)IAttribute::AssetReferenceListId
                                   << (u32)IAttribute::VariantListId;
}

MeshmoonLayerProcessor::~MeshmoonLayerProcessor()
{
}

Meshmoon::SceneLayerList MeshmoonLayerProcessor::DeserializeFrom(const QByteArray &meshmoonLayersJsonData)
{   
    QTextStream layersStream(meshmoonLayersJsonData);
    layersStream.setCodec("UTF-8");
    layersStream.setAutoDetectUnicode(true);
    QByteArray layersDataStr = layersStream.readAll().toUtf8();

    Meshmoon::SceneLayerList out;
    
    // Parse the application JSON.
    bool ok = false;
    QVariantList layerList = TundraJson::Parse(layersDataStr, &ok).toList();
    if (ok)
    {
        UniqueIdGenerator idGenerator;

        foreach(const QVariant &layerVariant, layerList)
        {
            QVariantMap layerData = layerVariant.toMap();
              
            // Read layer data
            Meshmoon::SceneLayer layer;
            layer.id = idGenerator.AllocateReplicated(); /// @todo This can generate duplicate IDs if this function is called multiple times per run!
            layer.name = layerData.contains("Name") ? QString(layerData.value("Name").toString().data()) : "";
            layer.iconUrl = layerData.contains("IconUrl") ? QString(layerData.value("IconUrl").toString().data()) : "";
            layer.defaultVisible = layerData.contains("DefaultVisible") ? layerData.value("DefaultVisible").toBool() : false;
            layer.txmlUrl = QUrl::fromEncoded(QString(layerData.value("Url").toString().data()).toUtf8(), QUrl::StrictMode);
            
            if (layer.txmlUrl.toString().isEmpty())
            {
                LogError(LC + "Found layer without txml URL, skipping layer: " + layer.toString());
                continue;
            }

            // Support two different ways of parsing position! Old and new Kirnu implementation.
            QVariant positionVariant = layerData.value("Position");
            if (positionVariant.isValid())
            {
                if (!Meshmoon::JSON::DeserializeFloat3(positionVariant, layer.centerPosition))
                    LogError(LC + QString("Found layer unsupported QVariant type as 'Position': %1").arg(positionVariant.type()));
            }
            else
                LogError(LC + "Found layer description without 'Position': " + layer.toString());

            out << layer;
        }
    }
    else
    {
        LogError(LC + "Failed to deserialize layers list from JSON data");
        qDebug() << "RAW JSON DATA" << endl << layersDataStr;
    }
    return out;
}

bool MeshmoonLayerProcessor::LoadSceneLayer(Meshmoon::SceneLayer &layer) const
{
    kNet::PolledTimer timer;
    if (layer.sceneData.isEmpty())
    {
        LogWarning(LC + "-- Scene layer content is empty: " + layer.toString() + " txml = " + layer.txmlUrl.toString());
        return false;
    }
    QString txmlUrlNoQuery = layer.txmlUrl.toString(QUrl::RemoveQuery|QUrl::StripTrailingSlash);

    Scene *activeScene = framework_->Renderer()->MainCameraScene();
    if (!activeScene)
    {
        LogError(LC + "Failed to get active scene to load scene layer: " + txmlUrlNoQuery);
        return false;
    }
    
    // Load txml scene
    QString txmlUrl = layer.txmlUrl.toString();
    QString txmlUrlLower = txmlUrl.toLower();
    QString layerBaseUrl = txmlUrlNoQuery.left(txmlUrlNoQuery.lastIndexOf("/") + 1);
    QString layerFilename = txmlUrlNoQuery.mid(layerBaseUrl.length());
    
    LogInfo(LC + QString("Loading %1 '%2'").arg(layer.id).arg(layer.name));
    LogInfo(LC + QString("  - Base %1").arg(layerBaseUrl));
    LogInfo(LC + QString("  - File %1").arg(layerFilename));
    LogInfo(LC + QString("  - Default Visibility %1").arg(layer.defaultVisible));
    LogInfo(LC + QString("  - Offset %1").arg(layer.centerPosition.toString()));

    SceneDesc sceneDesc;
    if (txmlUrlLower.contains(".txml"))
    {
        activeScene->CreateSceneDescFromXml(layer.sceneData, sceneDesc, false);
        if (sceneDesc.entities.isEmpty())
        {
            LogInfo(LC + "  * No Entities in scene data");
            return false;
        }
    }
    else
    {
        LogError(LC + "-- Format of layer load request asset is invalid, aborting: " + txmlUrl);
        return false;
    }

    // Manipulate scene description from relative to full asset refs before loading entities to scene
    /// @todo Profile; there's porbably room for optimization here.
    const float timeBeforeAssetRefAdjustment = timer.MSecsElapsed();
    int adjustedRefs = 0;
    for (int iE=0; iE<sceneDesc.entities.size(); ++iE)
    {
        EntityDesc &ent = sceneDesc.entities[iE];
        ent.temporary = true;

        for (int iC=0; iC<ent.components.size(); ++iC)
        {
            ComponentDesc &comp = ent.components[iC];
            
            bool compIsInteresting = false;
            if (comp.typeId != InvalidTypeId)
                compIsInteresting = interestingComponentTypeIds_.contains(comp.typeId);
            else
                compIsInteresting = interestingComponentTypeNames_.contains(IComponent::EnsureTypeNameWithPrefix(comp.typeName), Qt::CaseInsensitive);

            if (!compIsInteresting)
                continue;
            
            //qDebug() << "  " << comp.typeName;
            for (int iA=0; iA<comp.attributes.size(); ++iA)
            {
                AttributeDesc &attr = comp.attributes[iA];
                u32 attrTypeId = SceneAPI::GetAttributeTypeId(attr.typeName);
                if (interestingAttributeTypeIds_.contains(attrTypeId))
                {
                    // AssetReference, generic conversion
                    if (attrTypeId == cAttributeAssetReference)
                    {
                        QString ref = attr.value;
                        if (ref.trimmed().isEmpty())
                            continue;
                            
                        if (AssetAPI::ParseAssetRef(ref) == AssetAPI::AssetRefRelativePath)
                        {
                            attr.value = framework_->Asset()->ResolveAssetRef(layerBaseUrl, ref);
                            //qDebug() << "  " << attr.typeName.toStdString().c_str() << ":" << ref.toStdString().c_str() << "->" << attr.value.toStdString().c_str();
                            adjustedRefs++;
                        }
                    }
                    // AssetReferenceList, generic conversion
                    else if (attrTypeId == cAttributeAssetReferenceList)
                    {
                        QString ref = attr.value;
                        if (ref.trimmed().isEmpty())
                            continue;

                        QStringList inputRefs;         
                        if (ref.contains(";"))
                            inputRefs = ref.split(";", QString::KeepEmptyParts);
                        else
                            inputRefs << ref;
                        
                        QStringList outputRefs;    
                        foreach(QString inputRef, inputRefs)
                        {
                            if (!inputRef.trimmed().isEmpty() && AssetAPI::ParseAssetRef(inputRef) == AssetAPI::AssetRefRelativePath)
                            {
                                outputRefs << framework_->Asset()->ResolveAssetRef(layerBaseUrl, inputRef);
                                //qDebug() << "  " << attr.typeName.toStdString().c_str() << ":" << inputRef.toStdString().c_str() << "->" << framework_->Asset()->ResolveAssetRef(layerBaseUrl, inputRef).toStdString().c_str();
                                adjustedRefs++;
                            }
                            else
                                outputRefs << inputRef;
                        }
                        
                        if (outputRefs.size() > 1)
                            attr.value = outputRefs.join(";");
                        else if (outputRefs.size() == 1)
                            attr.value = outputRefs.first();
                    }
                    // String
                    else if (attrTypeId == cAttributeString)
                    {
                        // Per component conversion
                        if (comp.typeName == EC_Material::TypeNameStatic())
                        {
                            /** @todo We could fix non "generated://" output refs right here,
                                but then we would have to go through all the meshes for usage.
                                Lets trust that the original layer txml is proper. */
                            if (attr.name == "Input Material")
                            {
                                QString ref = attr.value;
                                if (ref.trimmed().isEmpty())
                                    continue;

                                if (AssetAPI::ParseAssetRef(ref) == AssetAPI::AssetRefRelativePath)
                                {
                                    attr.value = framework_->Asset()->ResolveAssetRef(layerBaseUrl, ref);
                                    //qDebug() << "  " << attr.typeName.toStdString().c_str() << ":" << ref.toStdString().c_str() << "->" << attr.value.toStdString().c_str();
                                    adjustedRefs++;
                                }
                            }
                        }
                    }
                    // QVariantList
                    else if (attrTypeId == cAttributeQVariantList)
                    {
                        // Per component conversion
                        if (comp.typeName == EC_Material::TypeNameStatic())
                        {
                            if (attr.name == "Parameters")
                            {
                                // We only want to check the "texture = <ref>" parameter
                                QString ref = attr.value;
                                if (ref.isEmpty() || ref.indexOf("texture", 0, Qt::CaseInsensitive) == -1)
                                    continue;

                                QStringList inputRefs;         
                                if (ref.contains(";"))
                                    inputRefs = ref.split(";", QString::KeepEmptyParts);
                                else
                                    inputRefs << ref;

                                QStringList outputRefs;
                                foreach(QString inputRef, inputRefs)
                                {
                                    if (!inputRef.trimmed().isEmpty() && inputRef.trimmed().startsWith("texture =", Qt::CaseInsensitive))
                                    {
                                        // Split ref from: "texture = <ref>"
                                        QString texAssetRef = inputRef.mid(inputRef.lastIndexOf("= ") + 2).trimmed();
                                        if (AssetAPI::ParseAssetRef(texAssetRef) == AssetAPI::AssetRefRelativePath)
                                        {
                                            outputRefs << "texture = " + framework_->Asset()->ResolveAssetRef(layerBaseUrl, texAssetRef);
                                            //qDebug() << "  " << attr.typeName.toStdString().c_str() << ":" << inputRef.toStdString().c_str() << "->" << QString("texture = ") + framework_->Asset()->ResolveAssetRef(layerBaseUrl, texAssetRef).toStdString().c_str();
                                            adjustedRefs++;
                                        }
                                        else
                                            outputRefs << inputRef;
                                    }
                                    else
                                        outputRefs << inputRef;
                                }

                                if (outputRefs.size() > 1)
                                    attr.value = outputRefs.join(";");
                                else if (outputRefs.size() == 1)
                                    attr.value = outputRefs.first();
                            } 
                        }
                        else if (comp.typeName == "EC_SlideShow")
                        {
                            QString ref = attr.value;
                            if (ref.trimmed().isEmpty())
                                continue;

                            QStringList inputRefs;         
                            if (ref.contains(";"))
                                inputRefs = ref.split(";", QString::KeepEmptyParts);
                            else
                                inputRefs << ref;

                            QStringList outputRefs;    
                            foreach(QString inputRef, inputRefs)
                            {
                                if (!inputRef.trimmed().isEmpty() && AssetAPI::ParseAssetRef(inputRef) == AssetAPI::AssetRefRelativePath)
                                {
                                    outputRefs << framework_->Asset()->ResolveAssetRef(layerBaseUrl, inputRef);
                                    //qDebug() << "  " << attr.typeName.toStdString().c_str() << ":" << inputRef.toStdString().c_str() << "->" << framework_->Asset()->ResolveAssetRef(layerBaseUrl, inputRef).toStdString().c_str();
                                    adjustedRefs++;
                                }
                                else
                                    outputRefs << inputRef;
                            }

                            if (outputRefs.size() > 1)
                                attr.value = outputRefs.join(";");
                            else if (outputRefs.size() == 1)
                                attr.value = outputRefs.first();
                        }
                    }
                }
            }
        }
    }
    const float assetRefAdjustingTime = timer.MSecsElapsed() - timeBeforeAssetRefAdjustment;

    QList<Entity*> createdEnts = activeScene->CreateContentFromSceneDesc(sceneDesc, false, AttributeChange::Replicate);

    const QString layerIndentifier = "Meshmoon Layer: " + layer.name;
    const bool adjustPlaceables = !layer.centerPosition.IsZero();
    uint adjustedPositions = 0;

    layer.entities.clear();
    foreach(Entity *createdEntity, createdEnts)
    {
        layer.entities << createdEntity->shared_from_this();
        
        // Set layer group and/or desc identifier (don't overwrite existing, app logic might depend on these)
        if (createdEntity->Group().trimmed().isEmpty())
            createdEntity->SetGroup(layerIndentifier);
        if (createdEntity->Description().trimmed().isEmpty())
            createdEntity->SetDescription(layerIndentifier);

        // Layer offset. Do not apply on Entity level parented Entities.
        if (adjustPlaceables && !createdEntity->Parent().get())
        {
            std::vector<shared_ptr<EC_Placeable> > placeables = createdEntity->ComponentsOfType<EC_Placeable>();
            for(size_t i = 0; i < placeables.size(); ++i)
            {
                // Do not apply on Placeable level parented Entities.
                if (placeables[i]->parentRef.Get().ref.trimmed().isEmpty())
                {
                    Transform t = placeables[i]->transform.Get();
                    t.pos += layer.centerPosition;
                    placeables[i]->transform.Set(t, AttributeChange::Replicate);
                    adjustedPositions++;
                }
            }
        }
    }

    if (adjustedRefs > 0)
    {
        LogInfo(LC + QString("  - Adjusted %1 relative layer refs with base url in %2 msecs.")
            .arg(adjustedRefs).arg(assetRefAdjustingTime));
    }
    if (adjustPlaceables)
    {
        LogInfo(LC + QString("  - Adjusted layer with position offset %1 for %2 non-parented Placeables.")
            .arg(layer.centerPosition.toString()).arg(adjustedPositions));
    }
    LogInfo(LC + QString("  - Scene layer loaded with %1 temporary entities in %2 msecs.")
        .arg(createdEnts.size()).arg(timer.MSecsElapsed()));
        
    return true;
}

/// @endcond
