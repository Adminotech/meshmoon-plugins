/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   RocketOpenNIPlugin.h
    @brief  The Meshmoon Rocket OpenNI plugin. */

#pragma once

#include "SceneFwd.h"
#include "InputFwd.h"
#include "AttributeChangeType.h"
#include "IModule.h"

#include "RocketOpenNIApi.h"
#include "RocketOpenNIPluginFwd.h"
#include "TrackingUser.h"

/// The Meshmooon Rocket OpenNI plugin.
class OPENNI_MODULE_API RocketOpenNIPlugin : public IModule
{
    Q_OBJECT

public:
    RocketOpenNIPlugin();
    ~RocketOpenNIPlugin();

    void Initialize();///< IModule override.
    void Uninitialize();///< IModule override.

    void Update(f64 UNUSED_PARAM(frametime));

    bool Initialized();

    OpenNIDevice *GetDevice() { return device_; };

signals:
    void UserFound(TrackingUser *user);
    void UserLost(TrackingUser *user);

    void PoseDetected(TrackingUser *user);
    void CalibrationStart(TrackingUser *user);
    void CalibrationComplete(TrackingUser *user);

    void UserUpdated(int id, TrackingSkeleton *skeleton);

private slots:    
    void DeviceUserFound(TrackingUser *user);
    void DeviceUserLost(TrackingUser *user);

    void DevicePoseDetected(TrackingUser *user);
    void DeviceCalibrationStart(TrackingUser *user);
    void DeviceCalibrationComplete(TrackingUser *user);

    void UpdatedUser(int id, TrackingSkeleton *skeleton);

private:
    const QString LC;

    OpenNIDevice *device_;

    float elapsedTime; // For 30 fps updating
};
