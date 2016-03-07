/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "AssetFwd.h"

#include "ui_RocketTaskbarWidget.h"

#include <QGraphicsProxyWidget>
#include <QRectF>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QSizePolicy>
#include <QIcon>
#include <QPointer>

class QToolBar;
class QAction;
class QMovie;

/// Provides the built in Rocket taskbar functionality.
/** @ingroup MeshmoonRocket */
class RocketTaskbar : public QGraphicsProxyWidget
{
Q_OBJECT

public:
    /// @cond PRIVATE
    RocketTaskbar(Framework *framework, QWidget *observed = 0);
    ~RocketTaskbar();
    
    friend class RocketPlugin;
    /// @endcond

public slots:
    /// Shows the toolbar. 
    void Show();

    /// Hides the toolbar.
    void Hide();
    
    /// Sets if load bar is enabled.
    /** Default is true, false will make so that load 
        animation or message will not be shown. */
    void SetLoadBarEnabled(bool enabled);

    /// Starts taskbar load animation with optional message.
    /** @note Keep the message short, there is limited space for rendering.
        @see StopLoadAnimation and SetMessage. */
    void StartLoadAnimation(const QString &message = QString());

    /// Stops taskbar load animation.
    /** @see StartLoadAnimation and SetMessage. */
    void StopLoadAnimation();
    
    /// Sets taskbar message with optional auto hide time.
    /** If autoHideTimeMsec is > 0 the message will be cleared after that time.
        @see StartLoadAnimation and StopLoadAnimation. */
    void SetMessage(const QString &message, uint autoHideTimeMsec = 0);

    /// Clears any existing taskbar message.
    /** @see SetMessage. */
    void ClearMessage();

    /// Convenience function that calls ClearMessage and StopLoadAnimation.
    void ClearMessageAndStopLoadAnimation();

    /// Add action to the toolbar. This overload is recommended.
    /** @note If icon is not defined, a default icon will be set.
        @note If tooltip if not defined, actions text will be set as the tooltip. */
    void AddAction(QAction *action);

    /// Add action to the toolbar. AddAction(QAction*) overload is recommended instead.
    /** @note Parent must be set to something in your scrip to guarantee automatic cleanup on scrip exit. */
    QAction *AddAction(const QString &text, QObject *parent);

    /// Add action to the toolbar. AddAction(QAction*) overload is recommended instead.
    /** @note Parent must be set to something in your scrip to guarantee automatic cleanup on scrip exit. */
    QAction *AddAction(const QIcon &icon, const QString &text, QObject *parent);

    /// Sets solid style for the taskbar. Possibility to override the used style.
    void SetSolidStyle(const QString &styleOverride = QString());
    
    /// Sets transparent style for the taskbar. 
    void SetTransparentStyle();
    
    /// Sets a minimum width, this call is ignored if SizePolicy() != Minimum. Call with 0 to reset to Minimum behavior.
    /** This function is handy when you want to have other widgets on the left side of the taskbar,
        just remember to call this function on every window resize if you want perfect results.
        @note The bar is always aligned to the right. Do your calculations with that in mind
        if trying to fit other widgets parallel to the taskbar. */
    void SetMinimumWidth(int minimumWidth);
    
    /// Anchors the taskbar to the top of the window. 
    /** @note Width will always be scene width and 
        height is fixed to Height()
        @see Height, Rect, FreeRect. */
    void AnchorToTop();
    
    /// Anchors the taskbar to the bottom of the window. 
    /** @note Width will always be scene width and 
        height is fixed to Height() 
        @see Height, Rect, FreeRect. */
    void AnchorToBottom();
    
    /// Set size policy for the toolbar. 
    /** Accepted values are QSizePolicy::Expanding and QSizePolicy::Minimum. 
        Expanding will set the width to the scene width, minimum will use as little space as possible. */
    void SetSizePolicy(const QSizePolicy::Policy &sizePolicy);
    
    /// Returns height of the taskbar graphics item.
    /** @see Rect, FreeRect. */
    qreal Height();
    
    /// Returns scene geometry of the taskbar graphics item.
    /** @see FreeRect. */
    QRectF Rect();
    
    /// Returns scene rect that is not occupied by the taskbar. 
    /** In practice you will get a calculated sceneRect - taskbarRect.
        The returned rect is safe to render without painting over the taskbar. 
        Handy for setting geometry for 'full screen' graphics items. 
        @note If SizePolicy() is Minimum, this will only take into account the height of the taskbar. */
    QRectF FreeRect();
    
    /// Return the scenes z value for the taskbar. 
    /** Useful when you want something under/over the taskbar. */
    qreal ZValue() { return zValue(); }

    /// Returns current size policy.
    QSizePolicy::Policy SizePolicy() { return sizePolicy_; }

    /// Returns if current style is solid.
    bool HasSolidStyle();

    /// Returns if current style is transparent.
    bool HasTransparentStyle();

    /// Returns if the toolbar has actions.
    bool HasActions();
    
    /// Returns if the toolbar is anchored to top.
    bool IsTopAnchored();
    
    /// Returns if the toolbar is anchored to bottom.
    bool IsBottomAnchored();

    /// Returns if the load animation is currently running.
    bool IsLoadAnimationRunning();
    
    /// Disconnects client from the server.
    void Disconnect();
    
    /// Updates geometry in the graphics scene.
    void UpdateGeometry();
    
signals:
    /// Visibility changed.
    void VisibilityChanged(bool visible);
    
    /// Emitted when action is added to the taskbar.
    void ActionAdded(QAction *action);
    
    /// Emitted when disconnect from server is requested from taskbar.
    void DisconnectRequest();
    
private slots:
    /// Handles scene resize signal. Triggers OnDelayedResize with a timer.
    void OnWindowResized(const QRectF &rect);

    /// Handles delayed scene resizes.
    void OnDelayedResize();
    
    /// Legacy handler for BrowserUiPlugin open url request signal.
    /** Opens the url with operating systems default browser outside of Tundra. */
    void OnOpenUrl(const QUrl &url);

/// @cond PRIVATE
protected:
    /// Return to friend classes if hide is demanded
    /// to void showing the bar automatically when its time for that.
    bool HasHideRequest() const;
    
    /// QGraphicsProxyWidget override.
    void showEvent(QShowEvent *e);
/// @endcond
    
private:
    void CheckStyleSheets();
    void CheckLoaderVisibility();
    
    enum AnchorPosition
    {
        Bottom,
        Top
    };
    
    enum VisualStyle
    {
        Unknown,
        SolidDefault,
        SolidCustom,
        Transparent
    };
    
    Framework *framework_;
    
    Ui::RocketTaskbarWidget ui_;
    
    QWidget *widget_;
    QToolBar *toolBarRight_;
    QTimer resizeDelayTimer_;
    QTimer messageHideTimer_;
    
    QString LC;
    QString frameStyleSolid_;
    QString frameStyleTransparent_;
    
    AnchorPosition anchor_;
    VisualStyle style_;
    QSizePolicy::Policy sizePolicy_;

    QPointer<QWidget> observed_;
    QMovie *loaderAnimation_;
    
    int minimumWidth_;
    bool loadBarEnabled_;
    bool hideRequested_;
};
Q_DECLARE_METATYPE(RocketTaskbar*)
