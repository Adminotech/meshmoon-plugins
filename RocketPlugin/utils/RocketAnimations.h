/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "RocketAnimationsFwd.h"

#include <QObject>
#include <QAbstractAnimation>
#include <QParallelAnimationGroup>
#include <QEasingCurve>
#include <QTimer>

/// @cond PRIVATE

// RocketAnimationEngine

class RocketMonitoredParallelAnimationGroup : public QParallelAnimationGroup
{
public:
    RocketMonitoredParallelAnimationGroup(RocketAnimationEngine *engine = 0, QObject *parent = 0);
    
protected:
    void updateCurrentTime (int currentTime);
    
private:
    RocketAnimationEngine *engine_;
};

class RocketAnimationEngine : public QObject
{
    Q_OBJECT

public:
    /// @note You must call Initialize after construction to do anything sensible.
    RocketAnimationEngine(QObject *parent = 0);
    ~RocketAnimationEngine();
    
    friend class RocketMonitoredParallelAnimationGroup;

    /// Initialize animation engine.
    /** All the internal animation logic will be parented to @c proxy.
        You should only call Initialize once in the engines lifetime. */
    void Initialize(QGraphicsProxyWidget *proxy, bool sizeEnabled, bool scaleEnabled, bool posEnabled, bool opacityEnabled,
                    uint duration = 500, QEasingCurve::Type easing = QEasingCurve::InOutCubic);

    /// Set duration for all initialized animations.
    void SetDuration(int msecs);

    /// Get duration of animations.
    int Duration() const;
    
    /// Set direction.
    void SetDirection(RocketAnimations::Direction direction);
    void SetDirection(QAbstractAnimation::Direction direction);
    
    /// Get direction.
    RocketAnimations::Direction Direction() const;

    /// Set current time. Can be called while running.
    void SetCurrentTime(int currentTimeMsec);

    /// Get current time.
    int CurrentTime() const;

    /// Current state.
    RocketAnimations::State State() const;
    
    /// Synchronize this engine with another engines state.
    void SynchronizeFrom(RocketAnimationEngine *source, bool duration = true, bool currentTime = true);

    /// Returns if this engine has been initialized.
    bool IsInitialized() const;
    
    /// Returns if any animation in the engine is running.
    bool IsRunning() const;
    
    /// Start.
    void Start();

    /// Stops all animations.
    void Stop();
    
    /// Hook animation completed signal to @c receivers @c slot.
    bool HookCompleted(const QObject *receiver, const char *slot);
    
    /// Hook animation progress signal.
    /** Fired as often as possible during animation. @see SetProgressInterval. */
    bool HookProgress(const QObject *receiver, const char *slot);
    
    /** Store values, can be used as animation related cache
        for eg. durations, opacitys etc. */
    void StoreValue(const QString &key, int value);
    void StoreValue(const QString &key, qreal value);
    void StoreValue(const QString &key, const QPointF &value);
    void StoreValue(const QString &key, const QSizeF &value);
    
    /// Get value previously set by StoreValue.
    template <typename T> T Value(const QString &key) const;

    /// @note These can be null, depending on the initialization.
    QPropertyAnimation *size;
    QPropertyAnimation *scale;
    QPropertyAnimation *pos;
    QPropertyAnimation *opacity;
    RocketMonitoredParallelAnimationGroup *engine;

signals:
    void StateChanged(RocketAnimations::State newState, RocketAnimations::State oldState);

    void Completed(RocketAnimationEngine *engine, RocketAnimations::Direction direction);
    void Progress(RocketAnimationEngine *engine, RocketAnimations::Direction direction, int currentTimeMsec);

private slots:
    void EmitCompleted();
    
    void OnStateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State oldState);
    
protected:
    void EmitProgress(int currentTime);

private:
    QHash<QString, int> intStore_;
    QHash<QString, qreal> qrealStore_;
    QHash<QString, QPointF> qpointStore_;
    QHash<QString, QSizeF> qsizeStore_;
};

/// Returns if the animation type enum needs a scene rect for calculations.
bool AnimationNeedsSceneRect(RocketAnimations::Animation anim);

/// Returns animation type as string.
QString AnimationToString(RocketAnimations::Animation anim);

/// @endcond
