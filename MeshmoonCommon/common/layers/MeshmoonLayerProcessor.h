/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"

#include "FrameworkFwd.h"
#include "SceneFwd.h"
#include "CoreTypes.h"

#include "common/MeshmoonCommon.h"

#include <QObject>
#include <QStringList>
#include <QList>

/// @cond PRIVATE

class MESHMOON_COMMON_API MeshmoonLayerProcessor : public QObject
{
Q_OBJECT

public:
    explicit MeshmoonLayerProcessor(Framework *plugin);
    ~MeshmoonLayerProcessor();

    /// Returns scene layer list from valid Meshmoon JSON layer data.
    Meshmoon::SceneLayerList DeserializeFrom(const QByteArray &meshmoonLayersJsonData);

    /// Load Meshmoon layer to currently active scene.
    /** Reads Meshmoon::SceneLayer::sceneData XML. */
    bool LoadSceneLayer(Meshmoon::SceneLayer &layer) const;

private:
    Framework *framework_;
    const QString LC;
    
    /// Layer conversion inspect component types.
    QStringList interestingComponentTypeNames_; 
    QList<u32> interestingComponentTypeIds_;

    /// Layer conversion inspect attribute types.
    QList<u32> interestingAttributeTypeIds_;
};

/// @endcond
