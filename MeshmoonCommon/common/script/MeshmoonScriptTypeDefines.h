/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonScriptTypeDefines.h
    @brief  QtScript bindings for common Meshmoon classes. */

#pragma once

#include "common/MeshmoonCommon.h"
#include "common/layers/MeshmoonLayers.h"

#include "LoggingFunctions.h"
#include "QScriptEngineHelpers.h"

#include <QScriptEngine>

static QScriptValue Meshmoon_SceneLayer_toString_const(QScriptContext *context, QScriptEngine *engine)
{
    Meshmoon::SceneLayer This = qscriptvalue_cast<Meshmoon::SceneLayer>(context->thisObject());
    return qScriptValueFromValue(engine, This.toString());
}

QScriptValue SceneLayerToQScriptValue(QScriptEngine *engine, const Meshmoon::SceneLayer &layer)
{
    QScriptValue ret = engine->newObject();
    // Properties
    ret.setProperty("name", layer.name, QScriptValue::ReadOnly);
    ret.setProperty("iconUrl", layer.iconUrl, QScriptValue::ReadOnly);
    ret.setProperty("id", static_cast<quint32>(layer.id), QScriptValue::ReadOnly);
    ret.setProperty("defaultVisible", layer.defaultVisible, QScriptValue::ReadOnly);
    ret.setProperty("visible", layer.visible);
    // Functions
    ret.setProperty("toString", engine->newFunction(Meshmoon_SceneLayer_toString_const, 0), QScriptValue::Undeletable | QScriptValue::ReadOnly);
    return ret;
}

void SceneLayerFromQScriptValue(const QScriptValue &obj, Meshmoon::SceneLayer &ret)
{
    if (!obj.property("name").isValid() || !obj.property("iconUrl").isValid() ||
        !obj.property("id").isValid() || !obj.property("visible").isValid())
    {
        LogError("SceneLayerFromQScriptValue: Object must have 'name', 'iconUrl', 'id', and 'visible' properties.");
        return;
    }

    ret.name = obj.property("name").toString();
    ret.iconUrl = obj.property("iconUrl").toString();
    ret.id = obj.property("id").toUInt32();
    ret.visible = obj.property("visible").toBool();
}

QScriptValue SceneLayerListToQScriptValue(QScriptEngine *engine, const Meshmoon::SceneLayerList &layers)
{
    QScriptValue ret = engine->newArray(static_cast<uint>(layers.size()));
    for(int i = 0; i < layers.size(); ++i)
        ret.setProperty(i, SceneLayerToQScriptValue(engine, layers[i]));
    return ret;
}

/// Deliberately a null function. Currently we don't need setting scene layer lists from scripts.
void SceneLayerListFromQScriptValue(const QScriptValue & /*obj*/, Meshmoon::SceneLayerList & /*ret*/)
{
}

void RegisterMeshmoonCommonMetaTypes(QScriptEngine *engine)
{   
    qScriptRegisterMetaType<Meshmoon::SceneLayer>(engine, SceneLayerToQScriptValue, SceneLayerFromQScriptValue);
    qScriptRegisterMetaType<Meshmoon::SceneLayerList >(engine, SceneLayerListToQScriptValue, SceneLayerListFromQScriptValue);

    qScriptRegisterQObjectMetaType<MeshmoonLayers*>(engine);
}
