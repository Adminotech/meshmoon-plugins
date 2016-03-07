
#pragma once

#include "RocketOpenNIPlugin.h"
#include "TrackingSkeleton.h"

#include "Math/float3.h"

#include <QObject>

class OPENNI_MODULE_API TrackingUser : public QObject
{
    Q_OBJECT

public:
    TrackingUser(int id, OpenNIDevice *device);
    ~TrackingUser();

    void StartTracking();

    void Update();

signals:
    void Updated(int id, TrackingSkeleton *skeleton);

public slots:
    int Id() { return id_; }

    TrackingSkeleton *Skeleton() { return skeleton_; }
    float3 CenterOfMass();

private:
    bool tracking_;

    OpenNIDevice *device_;

    TrackingSkeleton *skeleton_;
    int id_;
};
