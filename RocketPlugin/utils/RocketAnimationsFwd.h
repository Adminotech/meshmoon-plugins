/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

/// @cond PRIVATE

namespace RocketAnimations
{   
    /// @note Interchangeable with QAbstractAnimation::Direction.
    enum Direction
    {
        Forward,
        Backward
    };

    /// @note Interchangeable with QAbstractAnimation::State.
    enum State
    {
        Stopped,
        Paused,
        Running
    };

    enum Animation
    {
        NoAnimation = 0,
        AnimateFade,
        AnimateLeft,
        AnimateRight,
        AnimateDown,
        AnimateUp
    };
}

/// @endcond
