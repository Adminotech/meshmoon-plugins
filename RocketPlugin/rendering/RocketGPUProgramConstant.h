/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.h
    @brief  The Meshmoon Rocket client plugin. */

#pragma once

#include "RocketFwd.h"
#include <OgreGpuProgramParams.h>

/// @cond PRIVATE

class IRocketGPUProgramConstant
{
public:
    IRocketGPUProgramConstant(const std::string& name, int extra);
    virtual ~IRocketGPUProgramConstant();

    const std::string& Name() { return _name; }

    virtual void SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters) = 0;
protected:
    std::string _name;
    int _extra;
};

class RocketGPUProgramAutoConstant : public IRocketGPUProgramConstant
{
public:
    RocketGPUProgramAutoConstant(const std::string& name, Ogre::GpuProgramParameters::AutoConstantType type, int extra = 0);
    ~RocketGPUProgramAutoConstant();

    void SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters);

private:
    Ogre::GpuProgramParameters::AutoConstantType _type;
};

template<typename T>
class RocketGPUProgramConstant : public IRocketGPUProgramConstant
{
public:
    RocketGPUProgramConstant(const std::string& name, const T& value, int extra = 0) :
        IRocketGPUProgramConstant(name, extra),
        _value(value)
    {}

    ~RocketGPUProgramConstant()
    {}

    void SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters);

private:
    T _value;
};

template<typename T>
class RocketGPUProgramArrayConstant : public IRocketGPUProgramConstant
{
public:
    RocketGPUProgramArrayConstant(const std::string& name, T* value, int count, int extra = 0) :
        IRocketGPUProgramConstant(name, extra),
        _value(value),
        _count(count)
    {
    }

    ~RocketGPUProgramArrayConstant()
    {}

    void SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters);

private:
    T *_value;
    int _count;
};

typedef QMap<std::string, QList<IRocketGPUProgramConstant*> > RocketGPUProgramConstantMap;

/// @endcond
