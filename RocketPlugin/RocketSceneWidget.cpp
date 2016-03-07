/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketSceneWidget.h"
#include "utils/RocketAnimations.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"

#include "MemoryLeakCheck.h"

// RocketSceneWidget

RocketSceneWidget::RocketSceneWidget(Framework *framework, bool setupWidget) :
    QGraphicsProxyWidget(0, Qt::Widget),
    framework_(framework),
    widget_(new QWidget(0)),
    animations_(new RocketAnimationEngine(this)),
    hiding_(false),
    closing_(false),
    widgetInitialized_(false),
    showOpacity(0.95)
{
    if (setupWidget)
    {
        InitializeWidget(widget_);
        setWidget(widget_);
        widgetInitialized_ = true;
    }
    else
        hide(); // Important hide esp. for the lazy proxy widget init optimization!

    // Setup typical properties
    setFocusPolicy(Qt::ClickFocus);
    setZValue(100);

    // Connect inherited signals
    connect(this, SIGNAL(visibleChanged()), this, SLOT(OnVisibilityChanged()));

    // Create and connect animations
    animations_->Initialize(this, true, false, true, true, 500, QEasingCurve::OutCubic);
    animations_->HookCompleted(this, SLOT(OnAnimationsCompleted()));
    connect(animations_->opacity, SIGNAL(finished()), SLOT(OnFadeAnimationsCompleted()));
}

RocketSceneWidget::~RocketSceneWidget()
{
    // widget_ has not been embedded to the proxy yet
    // due to showEvent() optimization that delays the emdeb.
    // In this case we need to cleanup here.
    if (!widget() && widget_)
        SAFE_DELETE(widget_);
}

bool RocketSceneWidget::IsWidgetInitialized() const
{
    return widgetInitialized_;
}

void RocketSceneWidget::Close()
{
    close();
}

bool RocketSceneWidget::IsHiddenOrHiding() const
{
    if (!isVisible())
        return true;
    else if (hiding_ && animations_->IsRunning())
        return true;
    return false;
}

QRectF RocketSceneWidget::VisibleSceneRect() const
{
    QRectF rect(scenePos(), size());
    if (scene())
    {
        QRectF sceneRect = scene()->sceneRect();
        if (!sceneRect.contains(rect))
        {
            qreal left = rect.left();
            qreal top = rect.top();
            qreal sceneRight = sceneRect.right();
            qreal sceneBottom = sceneRect.bottom();
            if (left < 0.0)
                rect.setLeft(0.0);
            else if (left > sceneRight) // Out if scene bound on the right side
                return QRectF(0,0,0,0);
            if (top < 0.0)
                rect.setTop(0.0);
            else if (top > sceneBottom)
                return QRectF(0,0,0,0); // Out if scene bound on the bottom side

            qreal wDiff = sceneRect.width() - (rect.x() + rect.width());
            if (wDiff < 0.0)
            {
                qreal newWidth = rect.width() + wDiff;
                if (newWidth < 0.0) newWidth = 0.0;
                rect.setWidth(newWidth);
            }
            qreal hDiff = sceneRect.height() - (rect.y() + rect.height());
            if (hDiff < 0.0)
            {
                qreal newHeight = rect.height() + hDiff;
                if (newHeight < 0.0) newHeight = 0.0;
                rect.setHeight(newHeight);
            }
        }
    }
    return rect;
}

void RocketSceneWidget::Show()
{
    closing_ = false;
    hiding_ = false;

    if (!isVisible())
        show();
}

void RocketSceneWidget::SetVisible(bool visible)
{
    /** @note Base Show/Hide check this correctly, 
        but overrides might not, so do it here. */
    if (isVisible() == visible)
        return;

    if (visible)
        Show();
    else
        Hide();
}

void RocketSceneWidget::Animate(const QRectF &newRect, RocketAnimations::Animation anim)
{
    Animate(newRect.size(), newRect.topLeft(), -1.0, (AnimationNeedsSceneRect(anim) && scene() ? scene()->sceneRect() : QRectF()), anim);
}

void RocketSceneWidget::Animate(const QRectF &newRect, const QRectF &sceneRect, RocketAnimations::Animation anim)
{
    Animate(newRect.size(), newRect.topLeft(), -1.0, (!sceneRect.isValid() && AnimationNeedsSceneRect(anim) && scene() ? scene()->sceneRect() : QRectF()), anim);
}

void RocketSceneWidget::Animate(const QSizeF &newSize, const QPointF &newPos, RocketAnimations::Animation anim)
{
    Animate(newSize, newPos, -1.0, (AnimationNeedsSceneRect(anim) && scene() ? scene()->sceneRect() : QRectF()), anim);
}

void RocketSceneWidget::Animate(const QSizeF &newSize, const QPointF &newPos, qreal newOpacity, const QRectF &sceneRect, RocketAnimations::Animation anim)
{
    hiding_ = false;
    animations_->Stop();

    animations_->size->setEndValue(newSize);
    animations_->pos->setEndValue(newPos);
    
    // Opacity not enabled
    if (newOpacity < 0.0)
    {
        animations_->opacity->setStartValue(showOpacity);
        animations_->opacity->setEndValue(showOpacity);
    }
    else
    {
        animations_->opacity->setStartValue(opacity());
        animations_->opacity->setEndValue(newOpacity);
    }
    
    if (isVisible())
    {
        animations_->size->setStartValue(size());
        animations_->pos->setStartValue(pos());
    }
    else
    {
        setVisible(true);
        
        animations_->size->setStartValue(newSize);
        if (anim != RocketAnimations::NoAnimation && anim != RocketAnimations::AnimateFade)
        {
            if (!sceneRect.isValid() && AnimationNeedsSceneRect(anim))
            {
                LogError("RocketSceneWidget::Animate: Animation type " + AnimationToString(anim) + " needs sceneRect for calculation but it was invalid! Getting it from parent scene.");
                if (scene() && scene()->sceneRect().isValid())
                {
                    Animate(newSize, newPos, newOpacity, scene()->sceneRect(), anim);
                    return;
                }
                else
                    return;
            }

            if (anim == RocketAnimations::AnimateRight)
                animations_->pos->setStartValue(QPointF(-(sceneRect.width() - newPos.x()), newPos.y()));
            else if (anim == RocketAnimations::AnimateLeft)
                animations_->pos->setStartValue(QPointF(sceneRect.width() + newPos.x(), newPos.y()));
            else if (anim == RocketAnimations::AnimateUp)
                animations_->pos->setStartValue(QPointF(newPos.x(), sceneRect.height() + newSize.height() + 9));
            else if (anim == RocketAnimations::AnimateDown)
                animations_->pos->setStartValue(QPointF(newPos.x(), -newSize.height() - 9));
        }
        else if (anim == RocketAnimations::AnimateFade)
        {
            setPos(newPos);
            resize(newSize);
            animations_->opacity->setStartValue(newOpacity < 0.0 ? 0.0 : opacity());
            animations_->opacity->setEndValue(newOpacity < 0.0 ? showOpacity : newOpacity);
            animations_->opacity->start();
            return;
        }
        else
            animations_->pos->setStartValue(newPos);
    }

    animations_->engine->start();
}

void RocketSceneWidget::HideAndClose(RocketAnimations::Animation anim, const QRectF sceneRect)
{
    if (isVisible() && !hiding_)
    {
        closing_ = true;
        Hide(anim, sceneRect);
    }
    else if (!isVisible())
        close();
}

void RocketSceneWidget::Hide(RocketAnimations::Animation anim, const QRectF &sceneRect)
{
    if (isVisible() && !hiding_)
    {
        hiding_ = true;
        animations_->Stop();

        if (anim != RocketAnimations::NoAnimation && anim != RocketAnimations::AnimateFade)
        {
            if (!sceneRect.isValid() && AnimationNeedsSceneRect(anim))
            {
                LogError("RocketSceneWidget::Hide: Animation type " + AnimationToString(anim) + " needs sceneRect for calculation but it was invalid! Getting it from parent scene.");
                if (scene() && scene()->sceneRect().isValid())
                {
                    Hide(anim, scene()->sceneRect());
                    return;
                }
                else
                {
                    Hide(RocketAnimations::NoAnimation);
                    return;
                }
            }

            QPointF nowPos = pos();
            if (sceneRect.contains(nowPos))
            {
                animations_->size->setStartValue(size());
                animations_->size->setEndValue(size());
                
                animations_->opacity->setStartValue(1.0);
                animations_->opacity->setEndValue(1.0);

                animations_->pos->setStartValue(nowPos);
                
                if (anim != RocketAnimations::NoAnimation && anim != RocketAnimations::AnimateFade)
                {
                    if (anim == RocketAnimations::AnimateRight)
                        animations_->pos->setEndValue(QPointF(sceneRect.width() + nowPos.x(), nowPos.y()));
                    else if (anim == RocketAnimations::AnimateLeft)
                        animations_->pos->setEndValue(QPointF(-(sceneRect.width() - nowPos.x()), nowPos.y()));
                    else if (anim == RocketAnimations::AnimateDown)
                        animations_->pos->setEndValue(QPointF(nowPos.x(), sceneRect.height() + size().height() + 9));
                    else if (anim == RocketAnimations::AnimateUp)
                    {
                        animations_->pos->setEndValue(QPointF(nowPos.x(), -(size().height() + 9)));
                        
                        // Animating things off scene rect on the top will produce 
                        // nasty rendering warnings/errors and randomly lock up 
                        // rendering at least in windows. Do a fade instead.
                        //animations_->pos->setEndValue(nowPos);
                        //animations_->opacity->setEndValue(0.0);
                    }
                }
                else if (anim == RocketAnimations::AnimateFade)
                {
                    animations_->opacity->setStartValue(opacity());
                    animations_->opacity->setEndValue(0.0);
                    animations_->opacity->start();
                    return;
                }
                else
                    animations_->pos->setEndValue(nowPos);
                    
                animations_->engine->start();
            }
            else
                hide();
        }
        else if (anim == RocketAnimations::AnimateFade)
        {          
            animations_->opacity->setStartValue(opacity());
            animations_->opacity->setEndValue(0.0);
            animations_->opacity->start();
        }
        else
            hide();
    }
}

void RocketSceneWidget::OnVisibilityChanged()
{
    if (!isVisible())
    {
        // HideAndClose has set a pending close after hide.
        if (closing_)
            QTimer::singleShot(1, this, SLOT(close()));
    }

    closing_ = false;
    hiding_ = false;
    animations_->Stop();

    emit VisibilityChanged(isVisible());
}

void RocketSceneWidget::OnAnimationsCompleted()
{
    if (hiding_)
        hide();
}

void RocketSceneWidget::OnFadeAnimationsCompleted()
{
    /** The opacity animation is tracked separately because of the Hide logic.
        The fade is started without starting the engine, which is nice as
        we don't have to worry about positions and such to the engine. */
    if (isVisible() && hiding_ && opacity() < (showOpacity - 0.5))
        hide();
}

void RocketSceneWidget::showEvent(QShowEvent *e)
{
    // QGraphicsProxyWidget::showEvent is a no-op.
    Q_UNUSED(e);

    // If widget has not been set to the proxy (an optional ctor optimization)
    // Do it now that we are about to show the proxy.
    if (!widget() && widget_)
    {
        InitializeWidget(widget_);
        setWidget(widget_);
        widgetInitialized_ = true;
    }

    if (!isVisible())
        animations_->Stop();
}

void RocketSceneWidget::hideEvent(QHideEvent *e)
{
    // QGraphicsProxyWidget::hideEvent is a no-op.
    Q_UNUSED(e);
}

void RocketSceneWidget::closeEvent(QCloseEvent *e)
{
    Q_UNUSED(e);

    hiding_ = false;
    closing_ = false;
    animations_->Stop();

    QGraphicsProxyWidget::closeEvent(e);
    emit Closed();
}

Framework *RocketSceneWidget::Fw() const
{
    return framework_;
}

bool RocketSceneWidget::IsVisible() const
{
    return isVisible();
}

bool RocketSceneWidget::IsHiding() const
{
    return hiding_;
}

bool RocketSceneWidget::IsClosing() const
{
    return closing_;
}
