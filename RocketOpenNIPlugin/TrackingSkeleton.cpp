
#include "StableHeaders.h"

#include "TrackingSkeleton.h"
#include "OpenNIDevice.h"

TrackingSkeleton::TrackingSkeleton(OpenNIDevice *device) :
    device_(device),
    treshold_(10),
    clearChanged_(false)
{
    InitializeBones();
}

TrackingSkeleton::~TrackingSkeleton()
{
    device_ = 0;
    bones_.clear();
    changed_.clear();
    bonePositions_.clear();
    lastBonePositions_.clear();
}

void TrackingSkeleton::SetTreshold(float treshold, BoneCoordinateScale scale)
{
    if(scale == BoneCoordinateScale::MILLIMETERS)
        treshold_ = treshold;
    else if(scale == BoneCoordinateScale::CENTIMETERS)
        treshold_ = treshold * 10.0;
    else if(scale == BoneCoordinateScale::METERS)
        treshold_ = treshold * 1000.0;
}

bool TrackingSkeleton::Update()
{   
    if(!device_)
        return false;

    if(clearChanged_)
        changed_.clear();

    bool updated = true;

    XnSkeletonJointPosition joints[24];

    bool moved = false;

    for(int i = 0; i < 24; ++i)
    {
        int boneIndex = i + 1;

        device_->userGenerator_.GetSkeletonCap().GetSkeletonJointPosition(1, (XnSkeletonJoint)boneIndex, joints[i]);

        XnPoint3D xnPos;
        xnPos = joints[i].position;

        if(lastBonePositions_.size() > 0)
        {
            if((bonePositions_[i] - lastBonePositions_[i]).Length() > treshold_)
            {
                if(!changed_.contains(bones_[i]))
                    changed_.append(bones_[i]);
                moved = true;
            }
        }

        if(lastBonePositions_.empty())
            moved = true;

        if(useProjective_)
            device_->depthGenerator_.ConvertRealWorldToProjective(1, &xnPos, &xnPos);

        float3 pos;
        pos.x = xnPos.X;
        pos.y = xnPos.Y;
        pos.z = xnPos.Z;

        bonePositions_[i] = pos;
    }

    if(moved)
    {
        lastBonePositions_.clear();
        lastBonePositions_.append(bonePositions_);
    }

    return updated;
}

void TrackingSkeleton::InitializeBones()
{
    bonePositions_.clear();
    bones_.clear();

    for(int i = 0; i < MAX_BONE_COUNT; ++i)
    {
        bones_.append(TrackingBone());
        bonePositions_.append(float3(0.0f));
        bones_[i].index = i + 1;
    }

    bones_[0].name = "Head";
    bones_[1].name = "Neck";
    bones_[2].name = "Torso";
    bones_[3].name = "Waist";

    bones_[4].name = "LeftCollar";
    bones_[5].name = "LeftShoulder";
    bones_[6].name = "LeftElbow";
    bones_[7].name = "LeftWrist";
    bones_[8].name = "LeftHand";
    bones_[9].name = "LeftFingertip";
    
    bones_[10].name = "RightCollar";
    bones_[11].name = "RightShoulder";
    bones_[12].name = "RightElbow";
    bones_[13].name = "RightWrist";
    bones_[14].name = "RightHand";
    bones_[15].name = "RightFingertip";

    bones_[16].name = "LeftHip";
    bones_[17].name = "LeftKnee";
    bones_[18].name = "LeftAnkle";
    bones_[19].name = "LeftFoot";

    bones_[20].name = "RightHip";
    bones_[21].name = "RightKnee";
    bones_[22].name = "RightAnkle";
    bones_[23].name = "RightFoot";
}

void TrackingSkeleton::BoneUpdated(TrackingBone bone)
{
    if(!changed_.contains(bone))
    {
        changed_.append(bone);
    }
}

int TrackingSkeleton::BoneIndex(QString boneName)
{
    foreach(TrackingBone bone, bones_)
    {
        if(bone.name == boneName)
            return bone.index;
    }

    return -1;
}

QList<TrackingBone> TrackingSkeleton::AvailableBones()
{
    if(!device_)
        return QList<TrackingBone>();

    QList<TrackingBone> result;

    for(int i = 0; i < MAX_BONE_COUNT; ++i)
    {
        int boneIndex = i + 1;
        if(device_->userGenerator_.GetSkeletonCap().IsJointAvailable((XnSkeletonJoint)boneIndex))
            result.push_back(bones_[i]);
    }

    return result;
}

QList<TrackingBone> TrackingSkeleton::ChangedBones()
{
    QList<TrackingBone> bones = changed_;
    changed_.clear();
    return bones;
}

bool TrackingSkeleton::BoneAvailable(QString bone)
{
    foreach(TrackingBone tBone, AvailableBones())
    {
        if(tBone.name == bone)
            return true;
    }

    return false;
}

bool TrackingSkeleton::BoneChanged(QString bone)
{
    if(BoneIndex(bone) < 0)
        return false;
    else
    {
        if(changed_.contains(bones_[BoneIndex(bone)]))
        {
            clearChanged_ = true;
            return true;
        }
    }
    
    return false;
}

TrackingBone TrackingSkeleton::GetBone(QString boneName)
{
    if(!BoneIndex(boneName))
        return TrackingBone();
    else
        return bones_[BoneIndex(boneName)];

    return TrackingBone();
}

QList<TrackingBone> TrackingSkeleton::PossibleBones()
{
    return bones_;
}

float3 TrackingSkeleton::GetBonePosition(int index, BoneCoordinateScale coord)
{
    if(coord == BoneCoordinateScale::MILLIMETERS)
        return bonePositions_[index];
    else if(coord == BoneCoordinateScale::CENTIMETERS)
        return bonePositions_[index] / 10.0f;
    else if(coord == BoneCoordinateScale::METERS)
        return bonePositions_[index] / 1000.0f;

    return bonePositions_[index];
}

float3 TrackingSkeleton::GetBonePosition(QString boneName, BoneCoordinateScale coord)
{
    foreach(TrackingBone bone, AvailableBones())
    {
        if(bone.name == boneName)
            return GetBonePosition(bone.index - 1, coord);
    }

    return float3(0.0f);
}
