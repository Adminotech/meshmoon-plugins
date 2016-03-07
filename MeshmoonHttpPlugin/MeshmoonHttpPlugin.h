/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonHttpPlugin.h
    @brief  MeshmoonHttpPlugin provides a easy to use and script exposed safe HTTP API. */

#pragma once

#include "MeshmoonHttpPluginFwd.h"
#include "MeshmoonHttpPluginApi.h"

#include "IModule.h"

class QScriptEngine;

/// Provides a easy to use and script exposed safe HTTP API.
/** MeshmoonHttpPlugin is exposed to scripting as 'http' and MeshmoonHttpClient as 'http.client'. */
class MESHMOON_HTTP_API MeshmoonHttpPlugin : public IModule
{
    Q_OBJECT
    Q_PROPERTY(MeshmoonHttpClientPtr client READ Client)

public:
    explicit MeshmoonHttpPlugin();
    virtual ~MeshmoonHttpPlugin();
    
public slots:
    /// HTTP client. 
    /** @note Exposed to scripts as 'http.client'. */
    MeshmoonHttpClientPtr Client() const;
    
private slots:
    void OnScriptEngineCreated(QScriptEngine *engine);

private:
    /// IModule override.
    void Load();
    
    /// IModule override.
    void Unload();

    MeshmoonHttpClientPtr client_;
};
Q_DECLARE_METATYPE(MeshmoonHttpPlugin*)
