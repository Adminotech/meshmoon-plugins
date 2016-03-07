/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"

#include "EC_Placeable.h"
#include "EC_MeshmoonOccluder.h"
#include "EC_MeshmoonCulling.h"

class RocketOcclusionManager : public QObject
{
    Q_OBJECT

public:
    RocketOcclusionManager(RocketPlugin *plugin);
    ~RocketOcclusionManager();

private slots:
    void GenerateOcclusion();

    bool CheckBoundingBoxIncludes(EC_Mesh *targetMesh, const EntityList& entities);

    void TomeGenerated(const QString& fileName);

private:
    void CreateOcclusionComponents();

    void StartTomeGeneration();

    RocketPlugin* plugin_;
    Framework* framework_;

    QString LC;
};
