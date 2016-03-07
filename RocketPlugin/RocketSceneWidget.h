/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "utils/RocketAnimationsFwd.h"
#include "MeshmoonData.h"

#include <QGraphicsProxyWidget>
#include <QSizeF>
#include <QPointF>

/// Main Rocket UI class for QGraphicsScene based widgets.
/** @ingroup MeshmoonRocket */
class RocketSceneWidget : public QGraphicsProxyWidget
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketSceneWidget(Framework *framework, bool setupWidget = true);
    virtual ~RocketSceneWidget();
    /// @endcond

    /** Returns if this widget has been initialized. Indicating that
        it is safe to use the underlying widget and embedded ui.
        @see InitializeWidget. */
    bool IsWidgetInitialized() const;

    /// Framework getter.
    Framework *Fw() const;

    /// Underlying proxied widget ptr.
    QWidget *widget_;

    /// Opacity level that is used when widget is fully visible.
    qreal showOpacity;

    /// Get widget_ as another QWidget type.
    template<typename T>
    T* WidgetAs() { return dynamic_cast<T*>(widget_); }

    /// Returns the underlying Meshmoon animation engine.
    RocketAnimationEngine *AnimationEngine() { return animations_; }

public slots:
    /// Immediate show without animations.
    /** @note Force stops any current animations. Makes sure the widget is shown 
        via showEvent. Cancels any pending close after hide actions. */
    virtual void Show();

    /// Set visibility.
    virtual void SetVisible(bool visible);

    /// Returns if this widget is visible.
    /** @see IsHiding and IsClosing. */
    bool IsVisible() const;

    /// Returns if this widget is currently being hidden.
    bool IsHiding() const;

    /// Returns if this widget is currently being closed.
    bool IsClosing() const;

    /// @overload
    /** @note If animation type needs scene rect, its taken from the parent sceneRect() function. */
    void Animate(const QRectF &newRect, RocketAnimations::Animation anim = RocketAnimations::NoAnimation);

    /// @overload
    /** @note If animation type needs scene rect and the @c sceneRect is invalid, its taken from the parent sceneRect() function. */
    void Animate(const QRectF &newRect, const QRectF &sceneRect, RocketAnimations::Animation anim = RocketAnimations::NoAnimation);

    /// @overload
    /** @note If animation type needs scene rect, its taken from the parent sceneRect() function. */
    void Animate(const QSizeF &newSize, const QPointF &newPos, RocketAnimations::Animation anim);

    /// Animates the widget. Widget will be shown automatically if not visible.
    /** Be careful when overriding this function. Only do it if you wish this widget cannot be animated or you provide your own animations.
        @note If animation type needs scene rect and the @c sceneRect is invalid, its taken from the parent sceneRect() function.
        @note The other overloads always calls this function.
        @param New size.
        @param New position.
        @param Scene rect if animation needs it.
        @param New opacity. <0.0 means opacity wont be animated.
        @param Animation type. */
    virtual void Animate(const QSizeF &newSize, const QPointF &newPos, qreal newOpacity = -1.0, const QRectF &sceneRect = QRectF(), RocketAnimations::Animation anim = RocketAnimations::NoAnimation);

    /// Hides the widget with optional animation.
    virtual void Hide(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF &sceneRect = QRectF());

    /// Hides the widget with animation and closes it once the animations finish.
    virtual void HideAndClose(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF sceneRect = QRectF());

    /// Closes the widget without animations.
    virtual void Close();
    
    /// Returns if this widget is currently hidden or being hidden with an animation.
    bool IsHiddenOrHiding() const;    

    /// Returns the parent scene visible rect.
    QRectF VisibleSceneRect() const;

signals:
    /// Emitted after the widget is shown/hidden. Only emitted when the visibility actually changed.
    void VisibilityChanged(bool visible);

    /// Emitted when widget is closed.
    void Closed();

private slots:
    void OnVisibilityChanged();
    void OnAnimationsCompleted();
    void OnFadeAnimationsCompleted();

protected:
    /** This function is called when when the widget is about to be shown for the first time.
        This is a optimization you can use if you have lots of widget instances.
        The widget is lazily set to the proxy, before that happens this function is called.
        @note This function is guaranteed to be called only once per RocketSceneWidget.
        @note The input param ptr is the same as the widget_ member variable,
        it is never null even before this function is called. If you wish you can operate on it beforehand.
        @note If you want to get everything out of this optimization pass in 'setupWidget' as false to the ctor. */
    virtual void InitializeWidget(QWidget* /*widget*/) {}

    /** @note Called before show triggered from show() or setVisible(true) to this widget or its parent.
        Despite Qt docs saying this will only called if the widget is really show, it appears to be
        triggered also when widget/proxy was already visible.
        If you override this function, you must call the RocketSceneWidget impl to keep state consistent! */
    virtual void showEvent(QShowEvent *e);

    /** @note Called after hide has occurred from hide() or setVisible(false) to this widget or its parent.
        Despite Qt docs saying this will only called if the widget is really hidden, it appears to be
        triggered also when widget/proxy was already visible.
        You can optionally use the VisibilityChanged signal which arrives logically at the same time. */
    virtual void hideEvent(QHideEvent *e);

    /** @note If you override this function, you must call the RocketSceneWidget impl to keep state consistent! */
    virtual void closeEvent(QCloseEvent *e);

    Framework *framework_;
    RocketAnimationEngine *animations_;

    bool hiding_;
    bool closing_;
    bool widgetInitialized_;
};
Q_DECLARE_METATYPE(RocketSceneWidget*)
