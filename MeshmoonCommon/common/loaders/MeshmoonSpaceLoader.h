/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"

#include "MeshmoonCommonFwd.h"
#include "MeshmoonHttpPluginFwd.h"
#include "FrameworkFwd.h"

#include <QObject>
#include <QString>

/// @cond PRIVATE

class MESHMOON_COMMON_API MeshmoonSpaceLoader : public QObject
{
Q_OBJECT

public:
    explicit MeshmoonSpaceLoader(Framework *framework, const QString &source);
    ~MeshmoonSpaceLoader();

public slots:
    void SpacesQueryFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error);
    void SpaceTxmlFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error);
    void SpaceLayerTxmlFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error);

private:
    void Clear();
    void CheckCompleted();
    
    Meshmoon::Space *SpaceById(const QString &id);

    Framework *framework_;

    QString LC;
    QString source_;
    
    MeshmoonLayers *layers_;
    Meshmoon::SpaceList spaces_;
};

/// @endcond
