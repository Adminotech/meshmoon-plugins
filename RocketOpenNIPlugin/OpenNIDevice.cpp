
#include "StableHeaders.h"

#include "OpenNIDevice.h"
#include "TrackingUser.h"

OpenNIDevice::OpenNIDevice() :
    LC("[RocketOpenNIPlugin]: "),
    initialized_(false),
    needPose_(false)
{   

}   

OpenNIDevice::~OpenNIDevice()
{
    // Disconnect callbacks
    userGenerator_.UnregisterUserCallbacks(userCallbacks_);
    userGenerator_.GetSkeletonCap().UnregisterFromCalibrationStart(calibrationStart_);
    userGenerator_.GetSkeletonCap().UnregisterFromCalibrationComplete(calibrationComplete_);
    userGenerator_.GetPoseDetectionCap().UnregisterFromPoseDetected(poseDetected_);

    usersToDelete_.clear();
    foreach(TrackingUser *user, users_)
    {
        SAFE_DELETE(user);
        continue;
    }
}

void OpenNIDevice::Initialize()
{
    initialized_ = false;
    // Load configuration from file
    xn::EnumerationErrors errors;
    status = context_.InitFromXmlFile(CONFIG_XML_PATH, scriptNode_, &errors);
    if (status == XN_STATUS_NO_NODE_PRESENT)
    {
        XnChar strError[1024];
        errors.ToString(strError, 1024);
        QString errorString(strError);
        LogInfo(LC + errorString);
        return;
    }
    CHECK_STATUS(status, "Context loading failed");

    // Get DepthGenerator node from context
    status = context_.FindExistingNode(XN_NODE_TYPE_DEPTH, depthGenerator_);
    CHECK_STATUS(status, "Find depth generator node");

    // Get UserGenerator node from context
    status = context_.FindExistingNode(XN_NODE_TYPE_USER, userGenerator_);
    if(status != XN_STATUS_OK)
    {
        status = userGenerator_.Create(context_);
        CHECK_STATUS(status, "Create user generator manually");
    }

    if (!userGenerator_.IsCapabilitySupported(XN_CAPABILITY_SKELETON))
    {
        LogInfo(LC + "Supplied user generator doesn't support skeleton.");
        return;
    }

    // Connect callbacks to UserGenerator
    status = userGenerator_.RegisterUserCallbacks(&User_NewUser, &User_LostUser, this, userCallbacks_);
    CHECK_STATUS(status, "Connect user callbacks to user generator");

    status = userGenerator_.GetSkeletonCap().RegisterToCalibrationStart(&UserCalibration_CalibrationStart, this, calibrationStart_);
    CHECK_STATUS(status, "Connect calibration start callback to user generator");
    status = userGenerator_.GetSkeletonCap().RegisterToCalibrationComplete(&UserCalibration_CalibrationComplete, this, calibrationComplete_);
    CHECK_STATUS(status, "Connect calibration completed callback to user generator");

    if (userGenerator_.GetSkeletonCap().NeedPoseForCalibration())
    {
        needPose_ = true;
        if (userGenerator_.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
        {
            LogInfo(LC + "Pose required, but not supported.");
            return;
        }
        status = userGenerator_.GetPoseDetectionCap().RegisterToPoseDetected(&UserPose_PoseDetected, NULL, poseDetected_);
        CHECK_STATUS(status, "Connect pose detected callback to user generator");
        userGenerator_.GetSkeletonCap().GetCalibrationPose(strPose_);
    }

    userGenerator_.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);

    // Make it start generating data
    status = context_.StartGeneratingAll();
    CHECK_STATUS(status, "Start generating data");

    initialized_ = true;
}

void OpenNIDevice::Update()
{
    context_.WaitAnyUpdateAll();

    foreach(TrackingUser *user, users_)
    {
        if(!user)
            continue;

        user->Update();
    }

    // Delete all needed users
    foreach(int user, usersToDelete_)
        RemoveUser(user);
    usersToDelete_.clear();
}

void OpenNIDevice::AddUser(int id)
{
    LogInfo(LC + "User: " + QString::number(id) + " found.");

    users_.push_back(new TrackingUser(id, this));

    if(!GetUser(id))
        return;

    connect(GetUser(id), SIGNAL(Updated(int, TrackingSkeleton*)), this, SLOT(UserUpdated(int, TrackingSkeleton*)));
    emit UserFound(GetUser(id));
}

void OpenNIDevice::UserUpdated(int id, TrackingSkeleton *skeleton)
{
    emit Updated(id, skeleton);
}

void OpenNIDevice::RequestUserRemove(int id)
{
    LogInfo(LC + "User: " + QString::number(id) + " lost.");
    usersToDelete_.push_back(id);
    emit UserLost(GetUser(id));
}

TrackingUser *OpenNIDevice::GetUser(int id)
{
    foreach(TrackingUser *user, users_)
    {
        if(user->Id() == id)
            return user;
    }

    return NULL;
}

void OpenNIDevice::RemoveUser(int id)
{
    if(users_.size() <= 0)
        return;

    LogInfo(LC + "Pose detected.");

    foreach(TrackingUser *user, users_)
    {
        if(user->Id() == id)
        {
            users_.removeOne(user);
            delete user;
            break;
        }
    }
}

void OpenNIDevice::RequestCalibration(int id)
{
    if(users_.size() <= 0)
        return;

    LogInfo(LC + "Calibration started...");

    userGenerator_.GetPoseDetectionCap().StopPoseDetection(id);
    userGenerator_.GetSkeletonCap().RequestCalibration(id, TRUE);

    if(!users_[id - 1])
        emit PoseDetected(NULL);
    else
        emit PoseDetected(users_[id - 1]);
}

void OpenNIDevice::CalibrationStarted(int id)
{
    if(users_.size() <= 0)
        return;

    qDebug() << "Users size:" << users_.size();

    if(users_.size() < id)
        return;

    if(!users_[id - 1])
        emit CalibrationStart(NULL);
    else
        emit CalibrationStart(users_[id - 1]);
}

void OpenNIDevice::CalibrationFinished(XnCalibrationStatus eStatus, int id)
{
    if(users_.size() <= 0)
        return;

    if (eStatus == XN_CALIBRATION_STATUS_OK)
    {
        // Calibration succeeded
        LogInfo(LC + "Calibration complete, start tracking user " + QString::number(id));		
        userGenerator_.GetSkeletonCap().StartTracking(id);

        users_[id - 1]->StartTracking();

        if(!users_[id - 1])
            emit CalibrationComplete(NULL);
        else
            emit CalibrationComplete(users_[id - 1]);
    }
    else
    {
        LogInfo(LC + "Calibration failed. Trying again...");
        userGenerator_.GetPoseDetectionCap().StopPoseDetection(id);
        userGenerator_.GetSkeletonCap().RequestCalibration(id, TRUE);
    }
}

// OpenNI user callbacks
void XN_CALLBACK_TYPE OpenNIDevice::User_NewUser(xn::UserGenerator& /*generator*/, XnUserID nId, void* pCookie)
{
    if(nId == 0)
        return;

    OpenNIDevice *device = static_cast<OpenNIDevice*>(pCookie);
    if(!device)
        return;

    device->AddUser(nId);
}

void XN_CALLBACK_TYPE OpenNIDevice::User_LostUser(xn::UserGenerator& /*generator*/, XnUserID nId, void* pCookie)
{
    if(nId == 0)
        return;

    OpenNIDevice *device = static_cast<OpenNIDevice*>(pCookie);
    if(!device)
        return;

    device->RequestUserRemove(nId);
}

void XN_CALLBACK_TYPE OpenNIDevice::UserPose_PoseDetected(xn::PoseDetectionCapability& /*capability*/, const XnChar* /*strPose*/, XnUserID nId, void* pCookie)
{
    if(nId == 0)
        return;

    OpenNIDevice *device = static_cast<OpenNIDevice*>(pCookie);
    if(!device)
        return;

    device->RequestCalibration(nId);
}

void XN_CALLBACK_TYPE OpenNIDevice::UserCalibration_CalibrationStart(xn::SkeletonCapability& /*capability*/, XnUserID nId, void* pCookie)
{
    if(nId == 0)
        return;

    OpenNIDevice *device = static_cast<OpenNIDevice*>(pCookie);
    if(!device)
        return;

    device->CalibrationStarted(nId);
}

void XN_CALLBACK_TYPE OpenNIDevice::UserCalibration_CalibrationComplete(xn::SkeletonCapability& /*capability*/, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie)
{
    if(nId == 0)
        return;

    OpenNIDevice *device = static_cast<OpenNIDevice*>(pCookie);
    if(!device)
        return;

    device->CalibrationFinished(eStatus, nId);
}
