/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   RocketOpenNIPlugin.cpp
    @brief  The Meshmoon Rocket OpenNI plugin. */

#include "StableHeaders.h"

#include "RocketOpenNIPlugin.h"
#include "OpenNIDevice.h"

RocketOpenNIPlugin::RocketOpenNIPlugin() :
    IModule("RocketOpenNIPlugin"),
    LC("[RocketOpenNIPlugin]: "),
    device_(0),
    elapsedTime(0)
{
}

RocketOpenNIPlugin::~RocketOpenNIPlugin()
{
    SAFE_DELETE(device_);
}

void RocketOpenNIPlugin::Initialize()
{
    if(framework_->IsHeadless())
        return;

    device_ = new OpenNIDevice();
    if(!device_)
        return;
    
    device_->Initialize();

    // OpenNI device initialization failed, don't do anything.
    if(!device_->Initialized())
        return;

    // Connect device slots
    connect(device_, SIGNAL(UserFound(TrackingUser*)), this, SLOT(DeviceUserFound(TrackingUser*)));
    connect(device_, SIGNAL(UserLost(TrackingUser*)), this, SLOT(DeviceUserLost(TrackingUser*)));
    connect(device_, SIGNAL(PoseDetected(TrackingUser*)), this, SLOT(DevicePoseDetected(TrackingUser*)));
    connect(device_, SIGNAL(CalibrationStart(TrackingUser*)), this, SLOT(DeviceCalibrationStart(TrackingUser*)));
    connect(device_, SIGNAL(CalibrationComplete(TrackingUser*)), this, SLOT(DeviceCalibrationComplete(TrackingUser*)));
    connect(device_, SIGNAL(Updated(int, TrackingSkeleton*)), this, SLOT(UpdatedUser(int, TrackingSkeleton*)));
}

void RocketOpenNIPlugin::Uninitialize()
{
    if(!device_)
        return;
    
    SAFE_DELETE(device_);
}

bool RocketOpenNIPlugin::Initialized()
{
    if(!device_)
        return false;
    
    if(device_->Initialized())
        return false;

    return true;
}

void RocketOpenNIPlugin::Update(f64 frametime)
{
    if(!device_->Initialized())
        return;

    elapsedTime += frametime;

    if(elapsedTime < (1.0 / 30.0))
        return;

    if(!device_)
        return;

    device_->Update();

    elapsedTime = 0;
}

void RocketOpenNIPlugin::DeviceUserFound(TrackingUser *user)
{
    emit UserFound(user);
}

void RocketOpenNIPlugin::DeviceUserLost(TrackingUser *user)
{
    emit UserLost(user);
}

void RocketOpenNIPlugin::DevicePoseDetected(TrackingUser *user)
{
    emit PoseDetected(user);
}

void RocketOpenNIPlugin::DeviceCalibrationStart(TrackingUser *user)
{
    emit CalibrationStart(user);
}

void RocketOpenNIPlugin::DeviceCalibrationComplete(TrackingUser *user)
{
    emit CalibrationComplete(user);
}

void RocketOpenNIPlugin::UpdatedUser(int id, TrackingSkeleton *skeleton)
{
    emit UserUpdated(id, skeleton);
}

extern "C"
{
    DLLEXPORT void TundraPluginMain(Framework *fw)
    {
        Framework::SetInstance(fw); // Inside this DLL, remember the pointer to the global framework object.
        IModule *module = new RocketOpenNIPlugin();
        fw->RegisterModule(module);
    }
}
