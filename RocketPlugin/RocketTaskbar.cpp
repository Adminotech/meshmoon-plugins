/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketTaskbar.h"

#include "Framework.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "IAsset.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QGraphicsScene>
#include <QShowEvent>
#include <QToolBar>
#include <QDesktopServices>
#include <QAction>
#include <QMovie>

#include "MemoryLeakCheck.h"

RocketTaskbar::RocketTaskbar(Framework *framework, QWidget *observed) :
    QGraphicsProxyWidget(0, Qt::Widget),
    LC("[RocketTaskbar]: "),
    framework_(framework),
    observed_(observed),
    widget_(new QWidget(0)),
    anchor_(Bottom),
    style_(SolidDefault),
    sizePolicy_(QSizePolicy::Minimum),
    loadBarEnabled_(true),
    hideRequested_(false),
    minimumWidth_(0),
    loaderAnimation_(0)
{
    ui_.setupUi(widget_);
    setWidget(widget_);

    loaderAnimation_ = new QMovie(":/images/loader.gif", "GIF", this);
    if (loaderAnimation_->isValid())
    {
        ui_.labelLoader->setMovie(loaderAnimation_);
        loaderAnimation_->setCacheMode(QMovie::CacheAll);
    }

    // Hide loader widgets initially.
    ui_.labelLoader->hide();
    ui_.labelLoaderInfo->hide();
    ui_.frameLoader->hide();
    
    // Prepare toolbar for QActions
    toolBarRight_ = new QToolBar(widget_);
    toolBarRight_->setContentsMargins(0,0,0,0);
    toolBarRight_->setStyleSheet("background-color: none; border: 0px;");
    toolBarRight_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBarRight_->setOrientation(Qt::Horizontal);
    toolBarRight_->setIconSize(QSize(32,32));
    toolBarRight_->setFloatable(false);
    toolBarRight_->setMovable(false);
    toolBarRight_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QHBoxLayout *lRight = qobject_cast<QHBoxLayout*>(ui_.frameRight->layout());
    if (lRight)
        lRight->addWidget(toolBarRight_);
    
    // Set initial style
    CheckStyleSheets();
    
    // Support "legacy" icons from BrowserUiPlugin, without linking to it, we don't want to fail loading if not present.
    QObject *browserPlugin = framework_->property("browserplugin").value<QObject*>();
    if (browserPlugin)
    {
        connect(browserPlugin, SIGNAL(ActionAddRequest(QAction*, QString)), SLOT(AddAction(QAction*)));
        connect(browserPlugin, SIGNAL(OpenUrlRequest(const QUrl&, bool)), SLOT(OnOpenUrl(const QUrl&)));
    }
    
    resizeDelayTimer_.setSingleShot(true);
    messageHideTimer_.setSingleShot(true);

    // Connections.
    connect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), SLOT(OnWindowResized(const QRectF&)));
    connect(&resizeDelayTimer_, SIGNAL(timeout()), SLOT(OnDelayedResize()));
    connect(&messageHideTimer_, SIGNAL(timeout()), SLOT(ClearMessage()));
    connect(ui_.buttonDisconnect, SIGNAL(clicked()), SIGNAL(DisconnectRequest()));
    
    // Add ourselves to the scene.
    framework_->Ui()->GraphicsScene()->addItem(this);
    hide();
}

RocketTaskbar::~RocketTaskbar()
{
    resizeDelayTimer_.stop();
    messageHideTimer_.stop();

    if (loaderAnimation_)
        loaderAnimation_->stop();

    // Remove ourselves from the scene before we are deleted.
    framework_->Ui()->GraphicsScene()->removeItem(this);
    
    framework_ = 0;
    toolBarRight_ = 0;
    loaderAnimation_ = 0;
}

// Public slots

void RocketTaskbar::Show()
{
    if (!isVisible())
    {
        show();
        emit VisibilityChanged(true);
    }
}

void RocketTaskbar::Hide()
{
    // Mark hide as demanded for internal usage.
    // Meaning Hide() is called before our automatic show has
    // been triggered. Now we can detect that we should not automatically show.
    if (!isVisible() && !hideRequested_)
        hideRequested_ = true;
        
    if (isVisible())
    {
        hide();
        emit VisibilityChanged(false);
    }
}

void RocketTaskbar::SetLoadBarEnabled(bool enabled)
{
    if (loadBarEnabled_ != enabled)
    {
        loadBarEnabled_ = enabled;
        
        if (loadBarEnabled_)
        {
            if (loaderAnimation_ && loaderAnimation_->state() == QMovie::Running)
            {
                ui_.frameLoader->show();
                ui_.labelLoader->show();
            }
            else
                ui_.labelLoader->hide();
            if (!ui_.labelLoaderInfo->text().isEmpty())
            {
                ui_.frameLoader->show();
                ui_.labelLoaderInfo->show();
            }
            else
                ui_.labelLoaderInfo->hide();
        }
        else
        {
            ui_.labelLoader->hide();
            ui_.labelLoaderInfo->hide();
            ui_.frameLoader->hide();
        }
        
        resizeDelayTimer_.start(5);
    }
}

void RocketTaskbar::StartLoadAnimation(const QString &message)
{
    if (loaderAnimation_ && loaderAnimation_->state() != QMovie::Running)
        loaderAnimation_->start();

    if (loadBarEnabled_)
    {
        ui_.labelLoader->show();
        if (!ui_.frameLoader->isVisible())
            ui_.frameLoader->show();
        resizeDelayTimer_.start(5);
        
        SetMessage(message);
    }
}

void RocketTaskbar::StopLoadAnimation()
{
    if (loaderAnimation_)
        loaderAnimation_->stop();

    ui_.labelLoader->hide();
    if (ui_.frameLoader->isVisible() && ui_.labelLoaderInfo->text().trimmed().isEmpty())
    {
        ui_.labelLoaderInfo->hide();
        ui_.frameLoader->hide();
    }
    resizeDelayTimer_.start(5);
}

void RocketTaskbar::SetMessage(const QString &message, uint autoHideTimeMsec)
{
    if (message.isEmpty())
    {
        ClearMessage();
        return;
    }

    if (loadBarEnabled_)
    {
        if (ui_.labelLoaderInfo->text() != message)
        {
            ui_.labelLoaderInfo->setText(message);
            ui_.labelLoaderInfo->show();
            if (!ui_.frameLoader->isVisible())
                ui_.frameLoader->show();
            resizeDelayTimer_.start(5);
        }
    }

    if (autoHideTimeMsec > 0)
        messageHideTimer_.start(autoHideTimeMsec);
}

void RocketTaskbar::ClearMessage()
{
    ui_.labelLoaderInfo->setText("");

    ui_.labelLoaderInfo->hide();
    if (ui_.frameLoader->isVisible() && loaderAnimation_->state() != QMovie::Running)
    {
        ui_.labelLoader->hide();
        ui_.frameLoader->hide();
    }
    resizeDelayTimer_.start(5);
}

void RocketTaskbar::ClearMessageAndStopLoadAnimation()
{
    ClearMessage();
    StopLoadAnimation();
}

void RocketTaskbar::AddAction(QAction *action)
{
    if (!action)
    {
        LogError(LC + "Cannot add null QAction to taskbar.");
        return;
    }
    if (toolBarRight_)
    {
        // Set a default icon if action does not have one.
        if (action->icon().isNull())
            action->setIcon(QIcon(":/images/icon-application.png"));
        // Set text as tooltip if action does not have one.
        if (action->toolTip().isEmpty())
            action->setToolTip(action->text());

        // Add to toolbar
        toolBarRight_->addAction(action);
        emit ActionAdded(action);

        // Trigger a resize event if we are not expanded to the screen width.
        if (sizePolicy_ == QSizePolicy::Minimum)
            resizeDelayTimer_.start(5);
    }
    else
        LogError(LC + "Target toolbar is null, cannot add action '" + action->text() + "'");
}

QAction *RocketTaskbar::AddAction(const QString &text, QObject *parent)
{
    return AddAction(QIcon(":/images/icon-application.png"), text, parent);
}

QAction *RocketTaskbar::AddAction(const QIcon &icon, const QString &text, QObject *parent)
{
    if (!parent)
    {
        LogInfo(LC + "Action '" + text + "' parent object is null, setting toolbar as the parent. Please correct the parent to be a QObject, your main QWidget perhaps, in your script to guarantee cleanup when scrip exits.");
        parent = toolBarRight_;
    }

    QAction *action = new QAction(icon, text, parent);
    action->setToolTip(text);
    AddAction(action);
    return action;
}

bool RocketTaskbar::HasActions()
{
    if (!toolBarRight_)
        return false;
    return (toolBarRight_->actions().size() > 0);
}

bool RocketTaskbar::IsTopAnchored()
{
    return (anchor_ == Top);
}

bool RocketTaskbar::IsBottomAnchored()
{
    return (anchor_ == Bottom);
}

bool RocketTaskbar::IsLoadAnimationRunning()
{
    if (loaderAnimation_ && loaderAnimation_->state() == QMovie::Running)
        return true;
    return false;
}

void RocketTaskbar::Disconnect()
{
    emit DisconnectRequest();
}

void RocketTaskbar::UpdateGeometry()
{
    if (this->isVisible() && sizePolicy_ == QSizePolicy::Minimum)
        resizeDelayTimer_.start(50);
}

void RocketTaskbar::SetSolidStyle(const QString &styleOverride)
{        
    if (ui_.frameRight)
        ui_.frameRight->setStyleSheet(!styleOverride.isEmpty() ? styleOverride : frameStyleSolid_);

    style_ = !styleOverride.isEmpty() ? SolidCustom : SolidDefault;
}

void RocketTaskbar::SetTransparentStyle()
{
    if (ui_.frameRight)
        ui_.frameRight->setStyleSheet(frameStyleTransparent_);

    style_ = Transparent;
}

void RocketTaskbar::SetMinimumWidth(int minimumWidth)
{
    if (sizePolicy_ != QSizePolicy::Minimum)
    {
        LogError(LC + "SetMinimumWidth() cannot be used when size policy != QSizePolicy::Minimum, call SetSizePolicy() first.");
        return;
    }
    if (minimumWidth < 0)
        minimumWidth = 0;

    minimumWidth_ = minimumWidth;
    resizeDelayTimer_.start(5);
}

void RocketTaskbar::AnchorToTop()
{
    if (anchor_ != Top)
    {
        anchor_ = Top;

        CheckStyleSheets();
        resizeDelayTimer_.start(5);
    }
}

void RocketTaskbar::AnchorToBottom()
{
    if (anchor_ != Bottom)
    {
        anchor_ = Bottom;

        CheckStyleSheets();
        resizeDelayTimer_.start(5);
    }
}

void RocketTaskbar::SetSizePolicy(const QSizePolicy::Policy &sizePolicy)
{
    if (sizePolicy != QSizePolicy::Expanding && sizePolicy != QSizePolicy::Minimum)
    {
        LogError(LC + "Given Input size policy is not allowed: " + sizePolicy);
        return;
    }

    sizePolicy_ = sizePolicy;

    CheckStyleSheets();
    OnDelayedResize();
}

qreal RocketTaskbar::Height()
{
    return maximumHeight();
}

QRectF RocketTaskbar::Rect()
{
    return rect();
}

QRectF RocketTaskbar::FreeRect()
{
    if (!framework_)
        return QRectF();

    QRectF sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
    QRectF taskbarRect = rect();

    if (anchor_ == Bottom)
        return QRectF(0, 0, sceneRect.width(), sceneRect.height() - taskbarRect.height());
    else if (anchor_ == Top)
        return QRectF(0, taskbarRect.height(), sceneRect.width(), sceneRect.height() - taskbarRect.height());
    else
        return QRectF();
}

bool RocketTaskbar::HasSolidStyle()
{
    return (style_ == SolidDefault || style_ == SolidCustom);
}

bool RocketTaskbar::HasTransparentStyle()
{
    return (style_ == Transparent);
}

// Private slots

void RocketTaskbar::OnWindowResized(const QRectF & /*rect*/)
{
    if (this->isVisible())
        resizeDelayTimer_.start(50);
}

void RocketTaskbar::OnDelayedResize()
{
    if (isVisible() && framework_ && toolBarRight_ && ui_.frameRight)
    {
        ui_.frameRight->layout()->update();
        ui_.frameLoader->layout()->update();

        QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
        QMargins margins = ui_.frameRight->layout()->contentsMargins();
        int horizontalMargins = margins.left() + margins.right();
        int spacing = ui_.frameRight->layout()->spacing();

        if (anchor_ == Bottom)
        {
            if (sizePolicy_ == QSizePolicy::Expanding)
                setGeometry(QRectF(0, rect.height() - maximumHeight(), rect.width(), maximumHeight()));
            else
            {
                int additionalWidth = horizontalMargins + spacing + (ui_.frameLoader->isVisible() ? ui_.frameLoader->width() + spacing : 0) +
                    ui_.buttonDisconnect->width() + spacing;
                int width = toolBarRight_->width() + additionalWidth;
                if (width < minimumWidth_)
                    width = minimumWidth_;
                setGeometry(QRectF(rect.width() - width, rect.height() - maximumHeight(), width, maximumHeight()));
            }
        }
        else if (anchor_ == Top)
        {
            if (sizePolicy_ == QSizePolicy::Expanding)
                setGeometry(QRectF(0, 0, rect.width(), maximumHeight()));
            else
            {
                int additionalWidth = horizontalMargins + spacing + (ui_.frameLoader->isVisible() ? ui_.frameLoader->width() + spacing : 0) +
                    ui_.buttonDisconnect->width() + spacing;
                int width = toolBarRight_->width() + additionalWidth;
                if (width < minimumWidth_)
                    width = minimumWidth_;
                setGeometry(QRectF(rect.width() - width, 0, width, maximumHeight()));
            }
        }
    }
}

void RocketTaskbar::OnOpenUrl(const QUrl &url)
{
    if (url.isValid())
        QDesktopServices::openUrl(url);
    else
        LogError(LC + "Cannot open invalid URL: " + url.toString());
}

// Protected overrides

void RocketTaskbar::showEvent(QShowEvent *e)
{
    QGraphicsProxyWidget::showEvent(e);
    resizeDelayTimer_.start(50);
}

// Protected

bool RocketTaskbar::HasHideRequest() const
{
    return hideRequested_;
}

// Private

void RocketTaskbar::CheckStyleSheets()
{
    if (anchor_ == Top)
    {
        if (sizePolicy_ == QSizePolicy::Expanding)
            frameStyleSolid_ = "QFrame#frameRight { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(248, 248, 248, 255), stop:1 rgba(228, 228, 228, 255)); border: 0px; border-radius: 0px; border-bottom: 1px solid rgb(183, 185, 184); }";
        else
            frameStyleSolid_ = "QFrame#frameRight { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(248, 248, 248, 255), stop:1 rgba(228, 228, 228, 255)); border: 0px; border-radius: 0px; border-bottom: 1px solid rgb(183, 185, 184); border-left: 1px solid rgb(183, 185, 184); border-bottom-left-radius: 4px; }";
    }
    else if (anchor_ == Bottom)
    {
        if (sizePolicy_ == QSizePolicy::Expanding)
            frameStyleSolid_ = "QFrame#frameRight { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(248, 248, 248, 255), stop:1 rgba(228, 228, 228, 255)); border: 0px; border-radius: 0px; border-top: 1px solid rgb(183, 185, 184); }";
        else
            frameStyleSolid_ = "QFrame#frameRight { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(248, 248, 248, 255), stop:1 rgba(228, 228, 228, 255)); border: 0px; border-radius: 0px; border-top: 1px solid rgb(183, 185, 184); border-left: 1px solid rgb(183, 185, 184); border-top-left-radius: 4px; }";
    }
    if (frameStyleTransparent_.isEmpty())
    {
        frameStyleTransparent_ = "QFrame#frameRight { background-color: transparent; border: 0px; }";
        if (style_ == Transparent)
            SetTransparentStyle();
    }
    if (style_ == SolidDefault)
        SetSolidStyle();
}

void RocketTaskbar::CheckLoaderVisibility()
{
    bool visibilityChanges = false;
    bool animVisible = false;
    bool messageVisible = false;

    // Update animation visibility
    if (!loaderAnimation_ && ui_.labelLoader->isVisible())
    {
        ui_.labelLoader->hide();
        visibilityChanges = true;
    }
    if (loaderAnimation_ && ui_.labelLoader->isVisible() && loaderAnimation_->state() == QMovie::NotRunning)
    {
        ui_.labelLoader->hide();
        visibilityChanges = true;
    }
    if (loaderAnimation_ && !ui_.labelLoader->isVisible() && loaderAnimation_->state() == QMovie::Running)
    {
        ui_.labelLoader->show();
        visibilityChanges = true;
        animVisible = true;
    }

    // Update message visibility
    const QString message = ui_.labelLoaderInfo->text().trimmed();
    if (ui_.labelLoaderInfo->isVisible() && message.isEmpty())
    {
        ui_.labelLoaderInfo->hide();
        visibilityChanges = true;
    }
    if (!ui_.labelLoaderInfo->isVisible() && !message.isEmpty())
    {
        ui_.labelLoaderInfo->show();
        visibilityChanges = true;
        messageVisible = true;
    }

    if (ui_.frameLoader->isVisible() && !ui_.labelLoader->isVisible() && !ui_.labelLoaderInfo->isVisible())
    {
        ui_.frameLoader->hide();
        visibilityChanges = true;
        OnDelayedResize();
    }
    if (!ui_.frameLoader->isVisible() && (animVisible || messageVisible))
    {
        ui_.frameLoader->show();
        if (animVisible)
            ui_.labelLoader->show();
        if (messageVisible)
            ui_.labelLoaderInfo->show();
        visibilityChanges = true;
        OnDelayedResize();
    }

    if (visibilityChanges)
        resizeDelayTimer_.start(5);
}
