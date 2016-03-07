
#include "StableHeaders.h"

#include "TrackingUser.h"
#include "OpenNIDevice.h"

TrackingUser::TrackingUser(int id, OpenNIDevice *device) :
    id_(id),
    device_(device),
    tracking_(false),
    skeleton_(new TrackingSkeleton(device_))
{
    if(!device_)
        return;

    if(device_->NeedPose())
        device_->userGenerator_.GetPoseDetectionCap().StartPoseDetection(device_->strPose_, id_);
    else
       device_->userGenerator_.GetSkeletonCap().RequestCalibration(id_, true);
}

TrackingUser::~TrackingUser()
{
    SAFE_DELETE(skeleton_);
    device_ = 0;
}

void TrackingUser::StartTracking()
{
    tracking_ = true;
}

void TrackingUser::Update()
{
    if(!device_ || !tracking_)
        return;

    if(device_->userGenerator_.GetSkeletonCap().IsTracking(id_))
    {
        if(skeleton_->Update())
            emit Updated(id_, skeleton_);
    }
}

float3 TrackingUser::CenterOfMass()
{
    if(!device_ || !tracking_)
        return float3(0.0f);

    XnPoint3D com;
    device_->userGenerator_.GetCoM(id_, com);

    float3 pos;
    pos.x = com.X;
    pos.y = com.Y;
    pos.z = com.Z;

    return pos;
}
