/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.h
    @brief  The Meshmoon Rocket client plugin. */

#include "StableHeaders.h"

#include "RocketGPUProgramConstant.h"
#include "Math/float2.h"
#include "Math/float4.h"

IRocketGPUProgramConstant::IRocketGPUProgramConstant(const std::string& name, int extra) :
    _name(name),
    _extra(extra)
{
}

IRocketGPUProgramConstant::~IRocketGPUProgramConstant()
{
}

RocketGPUProgramAutoConstant::RocketGPUProgramAutoConstant(const std::string& name, Ogre::GpuProgramParameters::AutoConstantType type, int extra) :
    IRocketGPUProgramConstant(name, extra),
    _type(type)
{
}

RocketGPUProgramAutoConstant::~RocketGPUProgramAutoConstant()
{
}

void RocketGPUProgramAutoConstant::SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters)
{
    if(ogreGpuParameters.isNull())
        return;

    ogreGpuParameters->setNamedAutoConstant(_name, _type, _extra);
}

template<> void RocketGPUProgramConstant<float>::SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters)
{
    if(ogreGpuParameters.isNull())
        return;

    ogreGpuParameters->setNamedConstant(_name, _value);
}

template<> void RocketGPUProgramConstant<float2>::SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters)
{
    if(ogreGpuParameters.isNull())
        return;

    ogreGpuParameters->setNamedConstant(_name, &_value.x, 1, 2);
}

template<> void RocketGPUProgramConstant<float4>::SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters)
{
    if(ogreGpuParameters.isNull())
        return;

    ogreGpuParameters->setNamedConstant(_name, &_value.x, 1, 4);
}

template<> void RocketGPUProgramArrayConstant<float4>::SetOgreConstant(Ogre::GpuProgramParametersSharedPtr ogreGpuParameters)
{
    if(ogreGpuParameters.isNull() || !_value)
        return;

    ogreGpuParameters->setNamedConstant(_name, &_value[0].x, _count);
}
