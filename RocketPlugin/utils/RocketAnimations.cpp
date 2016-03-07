/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketAnimations.h"

#include <QGraphicsProxyWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

#include "LoggingFunctions.h"

// RocketAnimationEngine

RocketAnimationEngine::RocketAnimationEngine(QObject *parent) :
    QObject(parent),
    engine(0),
    opacity(0),
    size(0),
    scale(0),
    pos(0)
{
}

RocketAnimationEngine::~RocketAnimationEngine()
{
}

void RocketAnimationEngine::Initialize(QGraphicsProxyWidget *proxy, bool sizeEnabled, bool scaleEnabled, bool posEnabled, bool opacityEnabled, uint duration, QEasingCurve::Type easing)
{
    if (!proxy)
    {
        LogError("RocketAnimationEngine::Initialize: Null proxy ptr, cannot initialize!");
        return;
    }
    if (!sizeEnabled && !scaleEnabled && !posEnabled && !opacityEnabled)
    {
        LogError("RocketAnimationEngine::Initialize: All animations marked as false, cannot initialize!");
        return;
    }
    
    engine = new RocketMonitoredParallelAnimationGroup(this, proxy);
    
    if (sizeEnabled)
    {
        size = new QPropertyAnimation(proxy, "size", proxy);
        size->setEasingCurve(easing);
        engine->addAnimation(size);
    }
    if (scaleEnabled)
    {
        scale = new QPropertyAnimation(proxy, "scale", proxy);
        scale->setEasingCurve(easing);
        engine->addAnimation(scale);
    }
    if (posEnabled)
    {
        pos = new QPropertyAnimation(proxy, "pos", proxy);
        pos->setEasingCurve(easing);
        engine->addAnimation(pos);
    }
    if (opacityEnabled)
    {
        opacity = new QPropertyAnimation(proxy, "opacity", proxy);
        opacity->setEasingCurve(easing);
        engine->addAnimation(opacity);
    }
    
    SetDuration(duration);

    connect(engine, SIGNAL(finished()), this, SLOT(EmitCompleted()));
    connect(engine, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)), 
        this, SLOT(OnStateChanged(QAbstractAnimation::State, QAbstractAnimation::State)));
}

void RocketAnimationEngine::SetDuration(int msecs)
{
    if (size)
        size->setDuration(msecs);
    if (scale)
        scale->setDuration(msecs);
    if (pos)
        pos->setDuration(msecs);
    if (opacity)
        opacity->setDuration(msecs);
}

int RocketAnimationEngine::Duration() const
{
    /** @note Returns the first one found as they are all synchronized in SetDuration.
        If you have set different times to the different animation types, this might not work for you. */
    if (size)
        return size->duration();
    if (scale)
        return scale->duration();
    if (pos)
        return pos->duration();
    if (opacity)
        return opacity->duration();
    if (engine)
        return engine->duration();
    return 0;
}

void RocketAnimationEngine::SetDirection(RocketAnimations::Direction direction)
{
    SetDirection(static_cast<QAbstractAnimation::Direction>(direction));
}

void RocketAnimationEngine::SetDirection(QAbstractAnimation::Direction direction)
{
    if (engine)
        engine->setDirection(direction);
}

RocketAnimations::Direction RocketAnimationEngine::Direction() const
{
    return static_cast<RocketAnimations::Direction>(engine ? engine->direction() : QAbstractAnimation::Forward);
}

RocketAnimations::State RocketAnimationEngine::State() const
{
    return static_cast<RocketAnimations::State>(engine ? engine->state() : QAbstractAnimation::Stopped);
}

void RocketAnimationEngine::SynchronizeFrom(RocketAnimationEngine *source, bool duration, bool currentTime)
{
    if (!source || !source->IsInitialized() || !engine)
        return;

    if (duration)
        SetDuration(source->Duration());
    if (currentTime)
        SetCurrentTime(source->CurrentTime());
}

void RocketAnimationEngine::SetCurrentTime(int currentTimeMsec)
{
    if (engine)
        engine->setCurrentTime(currentTimeMsec);
}

int RocketAnimationEngine::CurrentTime() const
{
    return (engine ? engine->currentTime() : 0);
}

bool RocketAnimationEngine::IsInitialized() const
{
    return (engine != 0);
}

bool RocketAnimationEngine::IsRunning() const
{
    if (engine && engine->state() == QPropertyAnimation::Running)
        return true;
    if (size && size->state() == QPropertyAnimation::Running)
        return true;
    if (scale && scale->state() == QPropertyAnimation::Running)
        return true;
    if (pos && pos->state() == QPropertyAnimation::Running)
        return true;
    if (opacity && opacity->state() == QPropertyAnimation::Running)
        return true;
    return false;
}

void RocketAnimationEngine::Start()
{
    if (engine)
        engine->start();
}

void RocketAnimationEngine::Stop()
{
    if (engine)
        engine->stop();
    if (size)
        size->stop();
    if (scale)
        scale->stop();
    if (pos)
        pos->stop();
    if (opacity)
        opacity->stop();
}

void RocketAnimationEngine::OnStateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State oldState)
{
    emit StateChanged(static_cast<RocketAnimations::State>(newState), static_cast<RocketAnimations::State>(oldState));
}

void RocketAnimationEngine::EmitProgress(int currentTime)
{
    if (engine)
        emit Progress(this, static_cast<RocketAnimations::Direction>(engine->direction()), currentTime);
}

void RocketAnimationEngine::EmitCompleted()
{
    if (engine)
        emit Completed(this, static_cast<RocketAnimations::Direction>(engine->direction()));
}

bool RocketAnimationEngine::HookCompleted(const QObject *receiver, const char *slot)
{
    if (engine)
        return connect(this, SIGNAL(Completed(RocketAnimationEngine*, RocketAnimations::Direction)), receiver, slot, Qt::UniqueConnection);
    return false;
}

bool RocketAnimationEngine::HookProgress(const QObject *receiver, const char *slot)
{
    if (engine)
        return connect(this, SIGNAL(Progress(RocketAnimationEngine*, RocketAnimations::Direction, int)), receiver, slot, Qt::UniqueConnection);
    return false;
}

void RocketAnimationEngine::StoreValue(const QString &key, int value)
{
    intStore_[key] = value;
}

void RocketAnimationEngine::StoreValue(const QString &key, qreal value)
{
    qrealStore_[key] = value;
}

void RocketAnimationEngine::StoreValue(const QString &key, const QPointF &value)
{
    qpointStore_[key] = value;
}

void RocketAnimationEngine::StoreValue(const QString &key, const QSizeF &value)
{
    qsizeStore_[key] = value;
}

template<>
int RocketAnimationEngine::Value<int>(const QString &key) const
{
    return intStore_.value(key, 0);
}

template<>
qreal RocketAnimationEngine::Value<qreal>(const QString &key) const
{
    return qrealStore_.value(key, 0.0);
}

template<>
QPointF RocketAnimationEngine::Value<QPointF>(const QString &key) const
{
    return qpointStore_.value(key, QPointF(0,0));
}

template<>
QSizeF RocketAnimationEngine::Value<QSizeF>(const QString &key) const
{
    return qsizeStore_.value(key, QSizeF(0,0));
}

bool AnimationNeedsSceneRect(RocketAnimations::Animation anim)
{
    switch(anim)
    {
        case RocketAnimations::AnimateLeft:
        case RocketAnimations::AnimateRight:
        case RocketAnimations::AnimateDown:
        case RocketAnimations::AnimateUp:
            return true;
        default:
            break;
    }
    return false;
}

QString AnimationToString(RocketAnimations::Animation anim)
{
    switch (anim)
    {
        case RocketAnimations::NoAnimation: return "NoAnimation";
        case RocketAnimations::AnimateFade: return "AnimateFade";
        case RocketAnimations::AnimateLeft: return "AnimateLeft";
        case RocketAnimations::AnimateRight: return "AnimateRight";
        case RocketAnimations::AnimateDown: return "AnimateDown";
        case RocketAnimations::AnimateUp: return "AnimateUp";
    }
    return "Unknown_RocketAnimations::Animation";
}

void RocketMonitoredParallelAnimationGroup::updateCurrentTime(int currentTime)
{
    QParallelAnimationGroup::updateCurrentTime(currentTime);
    if (engine_)
        engine_->EmitProgress(currentTime);
}

RocketMonitoredParallelAnimationGroup::RocketMonitoredParallelAnimationGroup(RocketAnimationEngine *engine, QObject *parent) :
    QParallelAnimationGroup(parent),
    engine_(engine)
{
}
