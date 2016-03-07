
#pragma once

#include "RocketOpenNIApi.h"
#include "RocketOpenNIPlugin.h"

#include "Math/float3.h"

#include <QObject>
#include <QVector>

struct TrackingBone
{
    int index;
    QString name;

    bool operator==(const TrackingBone &bone)
    {
        if(index == bone.index)
        {
            if(name == bone.name)
            {
                return true;
            }
        }

        return false;
    }
};

enum BoneCoordinateScale
{
    MILLIMETERS,
    CENTIMETERS,
    METERS
};

#define MAX_BONE_COUNT 24

class OPENNI_MODULE_API TrackingSkeleton : public QObject
{
    Q_OBJECT

public:
    TrackingSkeleton(OpenNIDevice *device);
    ~TrackingSkeleton();
    
    bool Update();

public slots:
    bool Treshold() { return treshold_; }
    void SetTreshold(float treshold, BoneCoordinateScale scale);
    
    bool UseProjectiveCoords() { return useProjective_; }
    void SetUseProjectiveCoords(bool use) { useProjective_ = use; }

    QList<TrackingBone> AvailableBones();
    QList<TrackingBone> PossibleBones();
    
    QList<TrackingBone> ChangedBones();

    bool BoneAvailable(QString bone);
    bool BoneChanged(QString boneName);
    TrackingBone GetBone(QString boneName);

    // Get bone position in millimeters
    float3 GetBonePosition(int index, BoneCoordinateScale coord);
    float3 GetBonePosition(QString boneName, BoneCoordinateScale coord);

private:
    void InitializeBones();
    void BoneUpdated(TrackingBone bone);
    int BoneIndex(QString bone);

    QList<TrackingBone> changed_;
    QList<TrackingBone> bones_;

    QList<float3> bonePositions_;
    QList<float3> lastBonePositions_;

    OpenNIDevice *device_;

    bool useProjective_;
    bool clearChanged_;
    float treshold_;
};