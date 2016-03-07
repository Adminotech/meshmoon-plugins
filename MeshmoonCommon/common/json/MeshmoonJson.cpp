/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "CoreJsonUtils.h"
#include "MeshmoonJson.h"

#include "UniqueIdGenerator.h"
#include "Math/MathFunc.h"

#include <QDebug>

/// @cond PRIVATE

namespace Meshmoon
{
    const QString LC = "[Meshmoon::JSON]: ";
    
    void JSON::PrettyPrint(const QVariant &source)
    {
        bool ok = false;
        QByteArray data = TundraJson::Serialize(source, TundraJson::IndentFull, &ok);
        if (ok)
            qDebug() << data;
    }
    
    void JSON::PrettyPrint(const QByteArray &source)
    {
        bool ok = false;
        QVariant data = TundraJson::Parse(source, &ok);
        if (ok)
            PrettyPrint(data);
    }

    bool JSON::DeserializeFloat2(const QVariant &source, float2 &dest)
    {
        float4 temp = float4::zero;
        bool ok = DeserializeFloat4(source, temp);
        dest.Set(temp.x, temp.y);
        return ok;    
    }

    bool JSON::DeserializeFloat3(const QVariant &source, float3 &dest)
    {
        float4 temp = float4::zero;
        bool ok = DeserializeFloat4(source, temp);
        dest.Set(temp.x, temp.y, temp.z);
        return ok;    
    }

    bool JSON::DeserializeFloat4(const QVariant &source, float4 &dest)
    {
        if (!source.isValid())
            return false;

        if (source.type() == QVariant::Map || source.type() == QVariant::Hash)
        {
            QVariantMap map;
            if (source.type() == QVariant::Map)
                map = source.toMap();
            else
            {
                QVariantHash hash = source.toHash();
                foreach (const QString &key, hash.keys())
                    map[key] = hash[key];
            }
        
            DeserializeFloatFromMap(map, "x", dest.x);
            DeserializeFloatFromMap(map, "y", dest.y);
            DeserializeFloatFromMap(map, "z", dest.z);
            DeserializeFloatFromMap(map, "w", dest.w);
        }
        /** Legacy fallback for Kirnu that is writing float3 like shit. 
            Still in production and cannot be removed.
            { "Position" : [ { "Key" : "x", "Value" : -100 }, ... ] } */
        else if (source.type() == QVariant::List)
        {
            foreach(const QVariant &part, source.toList())
            {
                if (part.type() == QVariant::Map || part.type() == QVariant::Hash)
                {
                    QVariantMap map;
                    if (part.type() == QVariant::Map)
                        map = source.toMap();
                    else
                    {
                        QVariantHash hash = source.toHash();
                        foreach (const QString &key, hash.keys())
                            map[key] = hash[key];
                    }

                    QString key = map.contains("Key") ? map.value("Key").toString().trimmed().toLower() :  map.value("key", "").toString().trimmed().toLower();
                    float value = map.contains("Value") ? map.value("Value", 0.0f).toFloat() :  map.value("value", 0.0f).toFloat();
                    if (key == "x")
                        dest.x = value;
                    else if (key == "y")
                        dest.y = value;
                    else if (key == "z")
                        dest.z = value;
                    else if (key == "w")
                        dest.w = value;
                }
            }
        }
        else
            return false;
        return true;
    }

    QVariant JSON::DeserializeFloatFromMap(const QVariantMap &source, QString key, float &dest)
    {
        QVariant s = (source.contains(key) ? source.value(key) : (source.contains(key.toUpper()) ? source.value(key.toUpper()) : QVariant()));
        if (!s.isValid())
            return s;
        bool ok = true;
        if (s.type() == QVariant::Double)
            dest = s.toFloat(&ok);
        else if (s.type() == QVariant::LongLong)
            dest = static_cast<float>(s.toLongLong(&ok));
        else if (s.type() == QVariant::ULongLong)
            dest = static_cast<float>(s.toULongLong(&ok));
        else if (s.type() == QVariant::Int)
            dest = static_cast<float>(s.toInt(&ok));
        else if (s.type() == QVariant::UInt)
            dest = static_cast<float>(s.toUInt(&ok));
        if (!ok)
        {
            qDebug() << "JSON::DeserializeFloatFromMap: Failed to read variant" << s << "for key" << key;
            dest = 0.0f;
        }
        return s;
    }

    bool JSON::DeserializeColorChannel(const QVariantMap &source, QString key, float &dest)
    {
        QVariant s = DeserializeFloatFromMap(source, key, dest);
        if (!s.isValid())
            return false;
        if (s.type() == QVariant::Double)
            dest = Clamp<float>(dest, 0.0f, 1.0f);
        else if (s.type() == QVariant::Int || s.type() == QVariant::UInt || 
                 s.type() == QVariant::LongLong || s.type() == QVariant::ULongLong)
        {
            if (dest > 1.0f)
                dest = Clamp<float>(dest / 255.0f, 0.0f, 1.0f);
            else
                dest = Clamp<float>(dest, 0.0f, 1.0f);
        }
        else
        {
            dest = 0.0f;
            return false;
        }
        return true;
    }

    bool JSON::DeserializeColor(const QVariant &source, Color &dest)
    {
        if (!source.isValid())
            return false;

        if (source.type() == QVariant::Map || source.type() == QVariant::Hash)
        {
            QVariantMap map;
            if (source.type() == QVariant::Map)
                map = source.toMap();
            else
            {
                QVariantHash hash = source.toHash();
                foreach (const QString &key, hash.keys())
                    map[key] = hash[key];
            }

            DeserializeColorChannel(map, "r", dest.r);
            DeserializeColorChannel(map, "g", dest.g);
            DeserializeColorChannel(map, "b", dest.b);
            DeserializeColorChannel(map, "a", dest.a);
        }
        else
            return false;
        return true;
    }
    
    Meshmoon::SpaceList JSON::DeserializeSpaces(const QVariant &source)
    {
        Meshmoon::SpaceList spaces;
        
        if (!source.isValid() || source.type() != QVariant::List)
            return spaces;
            
        foreach(const QVariant &spaceVariant, source.toList())
        {
            QVariantMap spaceData = spaceVariant.toMap();
            if (spaceData.isEmpty())
                continue;

            Meshmoon::Space *space = new Meshmoon::Space();
            space->Set(spaceData.value("Id", "").toString(),
                      spaceData.value("Name", "").toString(),
                      spaceData.value("IconUrl", "").toString(),
                      spaceData.value("Url", "").toString(),
                      DeserializeLayers(spaceData.value("Layers", QVariantList())));

            if (space->IsValid())
                spaces << space;
        }
        return spaces;
    }
    
    Meshmoon::LayerList JSON::DeserializeLayers(const QVariant &source)
    {
        Meshmoon::LayerList layers;

        if (!source.isValid() || source.type() != QVariant::List)
            return layers;
            
        UniqueIdGenerator generator;

        foreach(const QVariant &layerVariant, source.toList())
        {
            QVariantMap layerData = layerVariant.toMap();
            if (layerData.isEmpty())
                continue;
                
            Meshmoon::Layer *layer = new Meshmoon::Layer();
            layer->Set(generator.AllocateReplicated(),
                      layerData.value("Name", "").toString(),
                      layerData.value("IconUrl", "").toString(),
                      layerData.value("Url", "").toString(),
                      layerData.value("DefaultVisible", false).toBool());

            QVariant positionVariant = layerData.value("Position");
            if (positionVariant.isValid())
            {
                float3 position = float3::zero;
                if (DeserializeFloat3(positionVariant, position))
                    layer->SetPosition(position);
                else
                    LogError(LC + QString("Found layer with unsupported QVariant type as 'Position': %1").arg(positionVariant.type()));
            }
            else
                LogWarning(LC + "Found layer description without 'Position': " + layer->toString());

            if (layer->IsValid())
                layers << layer;
        }
        return layers;
    }
}

/// @endcond
