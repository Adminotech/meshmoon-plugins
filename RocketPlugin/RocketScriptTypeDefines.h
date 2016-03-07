/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketScriptTypeDefines.h
    @brief  QtScript bindings for RocketPlugin classes. */

#pragma once

#include "RocketSceneWidget.h"
#include "RocketTaskbar.h"
#include "RocketMenu.h"
#include "RocketLayersWidget.h"
#include "RocketSounds.h"
#include "RocketAvatarEditor.h"
#include "RocketNotifications.h"
#include "MeshmoonUser.h"
#include "MeshmoonAssetLibrary.h"
#include "buildmode/RocketBuildEditor.h"
#include "buildmode/RocketBuildWidget.h"
#include "buildmode/RocketBuildContextWidget.h"
#include "utils/RocketFileSystem.h"

#include "LoggingFunctions.h"
#include "ScriptMetaTypeDefines.h"
#include "QScriptEngineHelpers.h"

#include <QScriptEngine>

QScriptValue toMeshmoonLibrarySource(QScriptEngine *engine, const MeshmoonLibrarySource &source)
{
    QScriptValue scriptSource = engine->newObject();
    scriptSource.setProperty("name", source.first);
    scriptSource.setProperty("url", source.second);
    return scriptSource;
}

void fromMeshmoonLibrarySource(const QScriptValue &obj, MeshmoonLibrarySource &source)
{
    if (!obj.property("name").isValid() || !obj.property("url").isValid())
    {
        LogError("fromMeshmoonLibrarySource: Object must have 'name' and 'url' properties.");
        return;
    }

    source.first = obj.property("name").toString();
    source.second = obj.property("url").toString();
}

QScriptValue toMeshmoonUserList(QScriptEngine *engine, const MeshmoonUserList &source)
{
    QScriptValue arr = engine->newArray(source.size());
    for (int i=0; i<source.size(); ++i)
        arr.setProperty(i, engine->newQObject(source[i]));
    return arr;
}

void fromMeshmoonUserListNoImpl(const QScriptValue &obj, MeshmoonUserList &source)
{
}

void RegisterRocketPluginMetaTypes(QScriptEngine *engine)
{
    // Register custom QObjects
    qScriptRegisterQObjectMetaType<RocketSceneWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketTaskbar*>(engine);
    qScriptRegisterQObjectMetaType<RocketMenu*>(engine);
    qScriptRegisterQObjectMetaType<RocketAssetMonitor*>(engine);
    qScriptRegisterQObjectMetaType<RocketNotifications*>(engine);
    qScriptRegisterQObjectMetaType<RocketNotificationWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketUserAuthenticator*>(engine);
    qScriptRegisterQObjectMetaType<RocketLayersWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketSounds*>(engine);
    qScriptRegisterQObjectMetaType<RocketAvatarEditor*>(engine);
    qScriptRegisterQObjectMetaType<RocketBuildEditor*>(engine);
    qScriptRegisterQObjectMetaType<RocketBuildWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketBuildContextWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketBuildContextWidget*>(engine);
    qScriptRegisterQObjectMetaType<RocketFileSystem*>(engine);
    qScriptRegisterQObjectMetaType<RocketFileDialogSignaler*>(engine);

    qScriptRegisterQObjectMetaType<MeshmoonWorld*>(engine);
    qScriptRegisterQObjectMetaType<MeshmoonUser*>(engine);
    qScriptRegisterQObjectMetaType<MeshmoonStorage*>(engine);
    qScriptRegisterQObjectMetaType<MeshmoonAssetLibrary*>(engine);
    qScriptRegisterQObjectMetaType<MeshmoonAsset*>(engine);

    // Register to and from functions.
    qScriptRegisterMetaType<MeshmoonUserList>(engine, toMeshmoonUserList, fromMeshmoonUserListNoImpl);
    qScriptRegisterMetaType<MeshmoonLibrarySource>(engine, toMeshmoonLibrarySource, fromMeshmoonLibrarySource);

    //qScriptRegisterMetaType(engine, toScriptValueEnum<RocketPlugin::RocketView>, fromScriptValueEnum<RocketPlugin::RocketView>);
    QScriptValue rocketPluginNs = engine->newObject();
    rocketPluginNs.setProperty("MainView", RocketPlugin::MainView, QScriptValue::Undeletable | QScriptValue::ReadOnly);
    rocketPluginNs.setProperty("WebBrowserView", RocketPlugin::WebBrowserView, QScriptValue::Undeletable | QScriptValue::ReadOnly);
    rocketPluginNs.setProperty("AvatarEditorView", RocketPlugin::AvatarEditorView, QScriptValue::Undeletable | QScriptValue::ReadOnly);
    rocketPluginNs.setProperty("SettingsView", RocketPlugin::SettingsView, QScriptValue::Undeletable | QScriptValue::ReadOnly);
    rocketPluginNs.setProperty("PresisView", RocketPlugin::PresisView, QScriptValue::Undeletable | QScriptValue::ReadOnly);
    engine->globalObject().setProperty("RocketPlugin", rocketPluginNs, QScriptValue::Undeletable | QScriptValue::ReadOnly);
}
