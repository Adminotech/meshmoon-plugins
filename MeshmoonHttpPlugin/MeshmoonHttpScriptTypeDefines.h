/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonHttpScriptTypeDefines.h
    @brief  QtScript bindings for MeshmoonHttpPlugin classes. */

#pragma once

#include "MeshmoonHttpClient.h"

#include "ScriptMetaTypeDefines.h"
#include "QScriptEngineHelpers.h"

#include <QScriptEngine>

template<typename T>
void qScriptValueToBoostSharedPtr_noop(const QScriptValue& /*value*/, shared_ptr<T>& /*ptr*/)
{
}

void RegisterMeshmoonHttpPluginMetaTypes(QScriptEngine *engine)
{
    // Register custom QObjects
    qScriptRegisterQObjectMetaType<MeshmoonHttpClient*>(engine);
    qScriptRegisterQObjectMetaType<MeshmoonHttpRequest*>(engine);

    // Shared -> raw ptr with noop assignment from scrip to c++
    qScriptRegisterMetaType<MeshmoonHttpClientPtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr_noop);
    qScriptRegisterMetaType<MeshmoonHttpRequestPtr>(engine, qScriptValueFromBoostSharedPtr, qScriptValueToBoostSharedPtr_noop);
}
