/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "Math/float3.h"
#include <QList>

class RocketSplineCurve3D
{
public:
    RocketSplineCurve3D();
    
    void Clear();
    void AddPoint(float3 point);
    float3 GetPoint(float progress);

private:
    // List for control points
    QList<float3> points;
    
    // These are for calculating current point in spline
    float3 p0, p1, p2, p3;
};
