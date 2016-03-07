/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketSplineCurve3D.h"

#include <cmath>
#include <QDebug>

#include "MemoryLeakCheck.h"

RocketSplineCurve3D::RocketSplineCurve3D() :
    p0(float3::zero),
    p1(float3::zero),
    p2(float3::zero),
    p3(float3::zero)
{
}

void RocketSplineCurve3D::AddPoint(float3 point)
{
    points.push_back(point);
}

void RocketSplineCurve3D::Clear()
{
    p0 = float3::zero;
    p1 = float3::zero;
    p2 = float3::zero;
    p3 = float3::zero;

    points.clear();
}

float3 RocketSplineCurve3D::GetPoint(float progress)
{
    float i = floor((points.size() - 1) * progress);
    float t = ((points.size() - 1) * progress) - floor((points.size() - 1) * progress);

    float3 out;
    
    if(progress == 1.0f)
    {
        return points.last();
    }
    else if (i == 0)
    {
        p0 = points[0];
        p1 = points[0];
        p2 = points[1];
        p3 = points[2];
    }
    else if(i == (this->points.size() - 2))
    {
        p0 = points[points.size() - 3];
        p1 = points[points.size() - 2];
        p2 = points[points.size() - 1];
        p3 = points[points.size() - 1];
    }
    else
    {
        p0 = points[i - 1];
        p1 = points[i];
        p2 = points[i + 1];
        p3 = points[i + 2];
    }
        
    float t2 = t * t;
    float t3 = t2 * t;

    // Calculate output point
    /// Calculate x coordinate
    out.x = 0.5f * ((2.0 * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0 * p2.x + p3.x) * t3);
    out.y = 0.5f * ((2.0 * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0 * p2.y + p3.y) * t3);
    out.z = 0.5f * ((2.0 * p1.z) + (-p0.z + p2.z) * t + (2.0f * p0.z - 5.0f * p1.z + 4 * p2.z - p3.z) * t2 + (-p0.z + 3.0f * p1.z - 3.0 * p2.z + p3.z) * t3);
        
    return out;
}
