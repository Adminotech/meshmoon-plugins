/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "Transform.h"

#include <QString>
#include <QHash>

/// @cond PRIVATE

class MeshmoonOpenAssetImporter;

namespace Ogre
{
    class Animation;
}

struct ImportInfo
{
    /// Asset reference that was imported.
    QString importedAssetRef;
    
    /// Created Mesh name in the Ogre resource system.
    QString createdOgreMeshName;

    /// Created Skeleton name in the Ogre resource system.
    QString createdOgreSkeletonName;

    /// Materials
    QHash<uint, QString> materials;

    // Information found from the source file (eg. Collada xml)
    QString exporter;
    float3 upAxis;
    Transform transform;

    ImportInfo() : upAxis(float3::zero) {}

    void Reset() { *this = ImportInfo(); }

    QString toString() const
    {
        QString str = QString("source='%1' createdMesh='%2' createdSkeleton='%3' numMaterials=%4 exporter='%5'")
            .arg(importedAssetRef).arg(createdOgreMeshName).arg(createdOgreSkeletonName)
            .arg(materials.size()).arg(exporter);

        if (!upAxis.IsZero())
            str += " upAxis=" + upAxis.toString();
        else
            str += " upAxis=none";
        if (!transform.pos.IsZero() || !transform.rot.IsZero() || !transform.scale.Equals(float3::one))
            str += " transform=" + transform.toString();
        else
            str += " transform=none";
        return str;
    }
};

struct AnimationInfo
{
    QString name;
    uint framerate;
    uint frameStart;
    uint frameEnd;
    Ogre::Animation *animation;

    float StartInSeconds() const
    {
        return (float)frameStart / (float)framerate;
    }
    float EndInSeconds() const
    {
        return (float)frameEnd / (float)framerate;
    }
    float DurationInSeconds() const
    {
        return ((float)(frameEnd - frameStart)) / (float)framerate;
    }

    AnimationInfo() : animation(0), framerate(24), frameStart(0), frameEnd(0) {}
    QString toString() const { return QString("name=%1 framerate=%2 start frame=%3 end frame=%4").arg(name).arg(framerate).arg(frameStart).arg(frameEnd); }
};

/// @endcond
