
#pragma once

#include "RocketOpenNIApi.h"
#include "RocketOpenNIPluginFwd.h"

#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>
#include <XnPropNames.h>

#include <QObject>

#define CONFIG_XML_PATH "OpenNIConfig.xml"

#define CHECK_STATUS(status, message)   \
    if(status != XN_STATUS_OK)  \
    {   \
        QString error = QString::fromUtf8(xnGetStatusString(status));   \
        LogInfo(LC + message + ": " + error);   \
    }   \

class OPENNI_MODULE_API OpenNIDevice : public QObject
{
    Q_OBJECT

public:
    OpenNIDevice();
    ~OpenNIDevice();

    void Initialize();

    void Update();
        
    bool Initialized() { return initialized_; }

    bool NeedPose() { return needPose_; };

    // These are bridge methods for OpenNI callbacks
    void AddUser(int id); ///< For internal use only!
    void RequestUserRemove(int id); ///< For internal use only!

    void RequestCalibration(int id); ///< For internal use only!
    void CalibrationStarted(int id); ///< For internal use only!
    void CalibrationFinished(XnCalibrationStatus eStatus, int id); ///< For internal user only!

signals:
    void Updated(int id, TrackingSkeleton *skeleton);

    void UserFound(TrackingUser *user);
    void UserLost(TrackingUser *user);

    void PoseDetected(TrackingUser *user);
    void CalibrationStart(TrackingUser *user);
    void CalibrationComplete(TrackingUser *user);

private slots:
    void UserUpdated(int id, TrackingSkeleton *skeleton);

protected:
    xn::UserGenerator userGenerator_;
    xn::DepthGenerator depthGenerator_;
    xn::ScriptNode scriptNode_;

    XnChar strPose_[20];

private:
    TrackingUser *GetUser(int id);
    void RemoveUser(int id);

    // New user detected
    static void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie);

    // User was lost
    static void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie);

    // Pose detected
    static void XN_CALLBACK_TYPE UserPose_PoseDetected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie);

    // User calibration started
    static void XN_CALLBACK_TYPE UserCalibration_CalibrationStart(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie);

    // User calibration finished
    static void XN_CALLBACK_TYPE UserCalibration_CalibrationComplete(xn::SkeletonCapability& capability, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie);

    QString LC;

    // OpenNI context
    xn::Context context_;

    // Callbacks
    XnCallbackHandle userCallbacks_;
    XnCallbackHandle calibrationStart_;
    XnCallbackHandle calibrationComplete_;
    XnCallbackHandle poseDetected_;

    // Users list
    QList<TrackingUser*> users_;
    QList<int> usersToDelete_;

    bool initialized_;
    bool needPose_;

    // Friend classes
    friend class TrackingSkeleton;
    friend class TrackingUser;
};
