/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketLobbyWidgets.h"
#include "RocketLobbyWidget.h"
#include "RocketPlugin.h"
#include "utils/RocketAnimations.h"

#include "Math/MathFunc.h"

#include "Framework.h"
#include "Application.h"
#include "LoggingFunctions.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "ConfigAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include <QFontMetrics>
#include <QTextStream>

namespace 
{
    static QString styleFrameBackgroundNormal     = "QFrame#frameContent { background-color: rgba(255, 255, 255, 255);";
    static QString styleFrameBackgroundHover      = "QFrame#frameContent { background-color: rgba(245, 245, 245, 255);";
    static QString styleFrameBackgroundSelected   = "QFrame#frameContent { background-color: rgb(243, 154, 41);";

    static QString styleNameLabelColorNormal      = "QLabel#labelName { color: rgb(65, 65, 65);";
    static QString styleNameLabelColorHover       = "QLabel#labelName { color: black;";
    static QString styleNameLabelColorSelected    = "QLabel#labelName { color: rgba(255, 255, 255, 230);";

    static QString styleCompanyLabelColorNormal   = "QLabel#labelCompany { color: rgb(115, 115, 115);";
    static QString styleCompanyLabelColorSelected = "QLabel#labelCompany { color: rgba(255, 255, 255, 210);";
}

// RocketServerWidget

RocketServerWidget::RocketServerWidget(Framework *framework, const Meshmoon::Server &inData) :
    RocketSceneWidget(framework, false),
    zoomed_(false),
    filtered(false),
    focused(false),
    selected(false),
    data(inData)
{
    // Force convert _ to space in name and company. This fixes most of the word wrapping UI issues.
    if (!data.name.contains(" ") && data.name.contains("_"))
        data.name = data.name.replace("_", " ");
    if (!data.company.contains(" ") && data.company.contains("_"))
        data.company = data.company.replace("_", " ");
}

RocketServerWidget::~RocketServerWidget()
{
}

void RocketServerWidget::InitializeWidget(QWidget *widget)
{
    animations_->HookCompleted(this, SLOT(OnTransformAnimationsFinished(RocketAnimationEngine*, RocketAnimations::Direction)));

    ui_.setupUi(widget);
    
    // Data to UI
    ApplyDataToWidgets();
    ApplyDataWidgetStyling();

    // Profile picture
    if (!data.companyPictureUrl.isEmpty() || !data.txmlPictureUrl.isEmpty())
    {
        // Do not request/load images to AssetAPI memory if they are already on disk.
        AssetTransferPtr transfer;
        QString existingImagePath = "";
        if (!data.txmlPictureUrl.isEmpty())
        {
            existingImagePath = framework_->Asset()->Cache()->FindInCache(data.txmlPictureUrl);
            if (existingImagePath.isEmpty())
                transfer = framework_->Asset()->RequestAsset(data.txmlPictureUrl, "Binary");
        }
        else if (!data.companyPictureUrl.isEmpty())
        {
            existingImagePath = framework_->Asset()->Cache()->FindInCache(data.companyPictureUrl);
            if (existingImagePath.isEmpty())
                transfer = framework_->Asset()->RequestAsset(data.companyPictureUrl, "Binary");
        }
        if (transfer.get())
        {
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnProfilePictureLoaded(AssetPtr)));
            connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(SetProfileImage()));
        }
        else
            SetProfileImage(existingImagePath);
    }
    else
        SetProfileImage();
}

void RocketServerWidget::ApplyDataToWidgets()
{
    // Set name and check font
#ifdef Q_OS_MAC
    if (!data.name.contains(" ") && data.name.length() >= 26)
#else 
    if (!data.name.contains(" ") && data.name.length() >= 28)
#endif
    {
        ui_.labelName->setStyleSheet(ui_.labelName->styleSheet().replace("font-size: 16px;", "font-size: 13px;"));
    }
    ui_.labelName->setText(data.name);

    // Company / MEP
    if (!data.mep.IsValid())
        ui_.labelCompany->setText(data.company.toUpper());
    else
        ui_.labelCompany->setText(QString("<span>%1</span> <span style='color:rgb(7,113,145);'>%2</span").arg(data.company.toUpper()).arg(data.mep.name));
        
    // User
    if (data.running && stats.userCount > 0)
    {
        ui_.labelUserCount->setText(QString::number(stats.userCount));
        ui_.labelUserCount->setToolTip(QString(stats.userCount > 1 ? "There are %1 users in this scene" : "There is %1 user in this scene").arg(stats.userCount));
    }
    else
        ui_.labelUserCount->clear();
}

void RocketServerWidget::SetOnlineStats(const Meshmoon::ServerOnlineStats &onlineStats)
{
    stats = onlineStats;

    if (IsWidgetInitialized())
    {
        // User count
        if (data.running && stats.userCount > 0)
        {
            ui_.labelUserCount->setText(QString::number(stats.userCount));
            ui_.labelUserCount->setToolTip(QString(stats.userCount > 1 ? "There are %1 users in this scene" : "There is %1 user in this scene").arg(stats.userCount));
        }
        else
            ui_.labelUserCount->clear();
    }
}

void RocketServerWidget::ResetOnlineStats()
{
    stats.Reset();

    if (IsWidgetInitialized())
        ui_.labelUserCount->clear();
}

void RocketServerWidget::ApplyDataWidgetStyling()
{
    ui_.labelIconPrivate->setVisible(!data.isPublic);
    if (!data.isPublic)
        ui_.labelIconPrivate->setPixmap(QPixmap(":/images/icon-lock-open.png"));

    ui_.labelIconMaintainance->setVisible(data.isUnderMaintainance);
    if (data.isUnderMaintainance)
        ui_.labelIconMaintainance->setPixmap(QPixmap(":/images/icon-maintainance.png"));

    // Left side markers
    if (data.isAdministratedByCurrentUser || data.mep.IsValid())
    {
        QBoxLayout *targetLayout = dynamic_cast<QBoxLayout*>(ui_.frameContent->layout());
        if (targetLayout)
        {
            QHBoxLayout *markers = new QHBoxLayout();
            markers->setSpacing(0);
            markers->setContentsMargins(0, 0, 6, 0);

            if (data.mep.IsValid())
            {
                QLabel *mepMarker = new QLabel(widget_);
                mepMarker->setFixedWidth(6);
                mepMarker->setStyleSheet("background-color: rgb(8, 149, 195); border: 0px; padding: 0px;");
                markers->insertWidget(0, mepMarker);
            }
            if (data.isAdministratedByCurrentUser)
            {
                QLabel *ownerMarker = new QLabel(widget_);
                ownerMarker->setFixedWidth(6);
                ownerMarker->setStyleSheet("background-color: rgb(243, 154, 41); border: 0px; padding: 0px;");
                markers->insertWidget(0, ownerMarker);
            }

            targetLayout->setContentsMargins(0, 0, 2, 0);
            targetLayout->insertLayout(0, markers);
        }
    }
}

void RocketServerWidget::FilterAndSuggest(Meshmoon::RelevancyFilter &filter)
{
    if (filter.term.isEmpty())
    {
        filtered = false;
        return;
    }
    filtered = true;
    
    if (filter.Match(data.name))
        filtered = false;
    if (filter.Match(data.company))
        filtered = false;
    if (filter.MatchIndirect(data.category))
        filtered = false;

    foreach(const QString &userName, stats.userNames)
        if (filter.MatchIndirect(userName))
            filtered = false;
    foreach(const QString &tag, data.tags)
        if (filter.MatchIndirect(tag))
            filtered = false;
}

void RocketServerWidget::OnTransformAnimationsFinished(RocketAnimationEngine*, RocketAnimations::Direction)
{
    if (hiding_)
    {
        hiding_ = false;
        hide();
    }
}

void RocketServerWidget::UpdateGridPosition(int row, int column, int itemsPerHeight, int itemsPerWidth, bool isLast)
{
    if (hiding_)
        return;

    QString borderStyle = "solid rgb(100, 100, 100)";
    
    QString style = styleFrameBackgroundNormal + " border-radius: 0px; " +
        QString("border-top: %1px " + borderStyle + "; border-bottom: %2px " + borderStyle + "; border-left: %3px " + borderStyle + "; border-right: %4px " + borderStyle + "; ")
        .arg(row == 1 ? "1" : "0")
        .arg(row == itemsPerHeight ? "1" : "0")
        .arg(column == 1 ? "1" : "0")
        .arg("1");

    if (row == 1 && column == 1)
        style.append("border-top-left-radius: 3px; ");
    if (row == itemsPerHeight && column == 1)
        style.append("border-bottom-left-radius: 3px; ");
    if ((row == 1 && column == itemsPerWidth) || (itemsPerHeight == 1 && isLast))
        style.append("border-top-right-radius: 3px; ");
    
    if (isLast)
        style.append("border-bottom-right-radius: 3px; }");
    else
        style.append("}");
    
    if (ui_.frameContent->styleSheet() != style)
        ui_.frameContent->setStyleSheet(style);
}

void RocketServerWidget::SetFocused(bool focused_)
{
    focused = false;
    SetSelected(false);
    focused = focused_;

    if (focused)
        ui_.frameContent->setStyleSheet(styleFrameBackgroundNormal + " border: 1px solid rgb(100, 100, 100); border-radius: 3px; }");
}

void RocketServerWidget::SetSelected(bool selected_)
{
    if (selected && !selected_)
    {
        ReplaceStyle(ui_.frameContent, QStringList() << styleFrameBackgroundSelected << styleFrameBackgroundHover, styleFrameBackgroundNormal);
        ReplaceStyle(ui_.labelName, QStringList() << styleNameLabelColorSelected << styleNameLabelColorHover, styleNameLabelColorNormal);
        ReplaceStyle(ui_.labelCompany, styleCompanyLabelColorSelected, styleCompanyLabelColorNormal);
    }

    selected = false;
    SetZoom(false);
    selected = selected_;

    if (selected)
    {
        ReplaceStyle(ui_.frameContent, QStringList() << styleFrameBackgroundNormal << styleFrameBackgroundHover, styleFrameBackgroundSelected);
        ReplaceStyle(ui_.labelName, QStringList() << styleNameLabelColorNormal << styleNameLabelColorHover, styleNameLabelColorSelected);
        ReplaceStyle(ui_.labelCompany, styleCompanyLabelColorNormal, styleCompanyLabelColorSelected);
    }
}

void RocketServerWidget::SetZoom(bool zoom)
{
    if (focused || selected)
        return;

    if (zoom && !zoomed_)
    {            
        zoomed_ = true;
        
        ReplaceStyle(ui_.frameContent, QStringList() << styleFrameBackgroundNormal << styleFrameBackgroundSelected, styleFrameBackgroundHover);
        ReplaceStyle(ui_.labelName, QStringList() << styleNameLabelColorNormal << styleNameLabelColorSelected, styleNameLabelColorHover);
    }
    else if (!zoom && zoomed_)
    {
        zoomed_ = false;
        
        ReplaceStyle(ui_.frameContent, QStringList() << styleFrameBackgroundHover << styleFrameBackgroundSelected, styleFrameBackgroundNormal);       
        ReplaceStyle(ui_.labelName, QStringList() << styleNameLabelColorHover << styleNameLabelColorSelected, styleNameLabelColorNormal);
    }
}

bool RocketServerWidget::ReplaceStyle(QWidget *widget, const QStringList &targetStyles, const QString &replacementStyle)
{
    foreach(const QString &targetStyle, targetStyles)
    {
        bool replaced = ReplaceStyle(widget, targetStyle, replacementStyle);
        if (replaced)
            return true;
    }
    return false;
}

bool RocketServerWidget::ReplaceStyle(QWidget *widget, const QString &targetStyle, const QString &replacementStyle)
{
    if (!widget) 
        return false;

    QString s = widget->styleSheet();
    if (s.indexOf(targetStyle) == 0)
    {
        widget->setStyleSheet(s.replace(targetStyle, replacementStyle));
        return true;
    }
    return false;
}

void RocketServerWidget::OnProfilePictureLoaded(AssetPtr asset)
{
    SetProfileImage(asset ? asset->DiskSource() : "");
    if (asset && framework_->Asset()->GetAsset(asset->Name()).get())
        framework_->Asset()->ForgetAsset(asset, false);
}

void RocketServerWidget::SetProfileImage(const QString &path)
{
    if (!path.isEmpty() && QFile::exists(path))
    {
        QPixmap loadedIcon(path);
        if (!loadedIcon.isNull() && (loadedIcon.width() < 50 || loadedIcon.height() < 50))
            loadedIcon = loadedIcon.scaled(QSize(50,50), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (!loadedIcon.isNull())
        {
            ui_.labelIcon->setPixmap(loadedIcon);
            return;
        }
    }
    
    // Fallback
    ui_.labelIcon->setPixmap(QPixmap(":/images/server-anon.png"));
}

void RocketServerWidget::CheckTooltip(bool show)
{
    if (show)
        emit HoverChange(this, true);
    else
        emit HoverChange(this, false);
}

void RocketServerWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverEnterEvent(event);
    SetZoom(true);
    CheckTooltip(true);
}
void RocketServerWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverMoveEvent(event);
    SetZoom(true);
    CheckTooltip(true);
}

void RocketServerWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverLeaveEvent(event);
    SetZoom(false);
    CheckTooltip(false);
}

void RocketServerWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsProxyWidget::mousePressEvent(event);

    if (!focused)
        emit FocusRequest(this);
    else
        emit UnfocusRequest(this);
}

// RocketPromotionWidget

RocketPromotionWidget::RocketPromotionWidget(Framework *framework, const Meshmoon::PromotedServer &inData) :
    RocketSceneWidget(framework),
    data(inData),
    focused(false),
    promoting(false)
{
    ui_.setupUi(widget_);

    // Change size animation to opacity
    opacityAnim_ = new QPropertyAnimation(this, "opacity", this);
    opacityAnim_->setEasingCurve(QEasingCurve::OutCubic);
    opacityAnim_->setDuration(500);

    timer.setSingleShot(true);

    connect(opacityAnim_, SIGNAL(finished()), SLOT(OnPromotionOpacityAnimationFinished()));
    connect(&timer, SIGNAL(timeout()), SIGNAL(Timeout()));
    
    setOpacity(0.0);
    hide();
}

RocketPromotionWidget::~RocketPromotionWidget()
{
    if (timer.isActive())
        timer.stop();
}

void RocketPromotionWidget::SetGeometry(const QSizeF &size, const QPointF &pos)
{       
    widget_->setMinimumSize(size.toSize());
    widget_->setMaximumSize(size.toSize());
    setGeometry(QRectF(pos, size));
}

void RocketPromotionWidget::RestartTimer()
{
    if (timer.isActive())
        timer.stop();
    timer.start(data.durationMsec);
}

void RocketPromotionWidget::Show()
{
    SetPromoting(true);
        
    LoadRequiredImage();
    
    if (IsHiddenOrHiding())
    {
        if (isVisible())
            hide();

        opacityAnim_->stop();
        opacityAnim_->setDirection(QAbstractAnimation::Forward);
        opacityAnim_->setStartValue(opacity());
        
        setVisible(true); // resets opacity

        opacityAnim_->setEndValue(1.0);
        opacityAnim_->start();
    }
    if (!timer.isActive())
        timer.start(data.durationMsec);
}

void RocketPromotionWidget::Animate(const QSizeF &newSize, const QPointF &newPos, qreal newOpacity, 
                                    const QRectF &sceneRect, RocketAnimations::Animation anim)
{
    widget_->setMaximumSize(newSize.toSize());

    RocketSceneWidget::Animate(newSize, newPos, newOpacity, sceneRect, anim);
}

void RocketPromotionWidget::Hide(RocketAnimations::Animation anim, const QRectF & /*sceneRect*/)
{
    SetPromoting(false);

    if (isVisible())
    {
        if (anim != RocketAnimations::NoAnimation)
        {
            // Opacity anim
            opacityAnim_->stop();
            opacityAnim_->setDirection(QAbstractAnimation::Backward);
            opacityAnim_->setStartValue(0.0);
            opacityAnim_->setEndValue(opacity());
            opacityAnim_->start();
        }
        else
        {
            hide();
            setOpacity(0.0);
        }
    }
    if (timer.isActive())
        timer.stop();
}

void RocketPromotionWidget::AboutToBeShown()
{
    LoadRequiredImage();
}

void RocketPromotionWidget::SetFocused(bool focused_)
{
    if (focused == focused_)
        return;

    focused = focused_;
    if (focused && largeImage_.isNull()) 
        LoadRequiredImage();

    ui_.labelImage->setPixmap(focused ? largeImage_ : smallImage_);
    ui_.labelImage->setCursor(focused ? Qt::ArrowCursor : Qt::PointingHandCursor);

    if (focused && timer.isActive())
        timer.stop();
}

void RocketPromotionWidget::SetPromoting(bool promoting_)
{
    if (promoting != promoting_)
        promoting = promoting_;
}

void RocketPromotionWidget::OnPromotionOpacityAnimationFinished()
{
    if (opacityAnim_->direction() == QAbstractAnimation::Backward)
    {
        hide();
        setOpacity(0.0);
    }
}

void RocketPromotionWidget::OnSmallImageCompleted(AssetPtr asset)
{
    QString diskSource = asset->DiskSource();
    if (asset && framework_->Asset()->GetAsset(asset->Name()).get())
        framework_->Asset()->ForgetAsset(asset, false);
    LoadSmallImage(diskSource);
    
    if (promoting)
        Show();
}

void RocketPromotionWidget::OnLargeImageCompleted(AssetPtr asset)
{
    QString diskSource = asset->DiskSource();
    if (asset && framework_->Asset()->GetAsset(asset->Name()).get())
        framework_->Asset()->ForgetAsset(asset, false);
    LoadLargeImage(diskSource);
}

void RocketPromotionWidget::LoadRequiredImage()
{
    if (!focused && smallImage_.isNull() && !data.smallPicture.trimmed().isEmpty())
    {
        QString diskSource = framework_->Asset()->Cache()->FindInCache(data.smallPicture);
        if (diskSource.isEmpty())
        {
            AssetTransferPtr smallTransfer = framework_->Asset()->RequestAsset(data.smallPicture, "Binary");
            connect(smallTransfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnSmallImageCompleted(AssetPtr)));
            connect(smallTransfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(LoadSmallImage()));
        }
        else
            LoadSmallImage(diskSource);
    }
    if (focused && largeImage_.isNull() && data.type == "world" && !data.largePicture.trimmed().isEmpty())
    {
        QString diskSource = framework_->Asset()->Cache()->FindInCache(data.largePicture);
        if (diskSource.isEmpty())
        {
            AssetTransferPtr largeTransfer = framework_->Asset()->RequestAsset(data.largePicture, "Binary");
            connect(largeTransfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnLargeImageCompleted(AssetPtr)));
            connect(largeTransfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(LoadLargeImage()));
        }
        else
            LoadLargeImage(diskSource);
    }
}

void RocketPromotionWidget::LoadSmallImage(const QString &path)
{
    if (path.isEmpty())
    {
        LogError("Small promotion image path is empty for " + data.name + ". Transfer failed?");
        return;
    }

    smallImage_ = QPixmap(path);
    if (smallImage_.isNull())
        LogWarning("Failed to create small promotion picture for " + data.name);
    else
    {
        if (smallImage_.width() > 960)
        {
            LogWarning("Small promotion picture wider than 960x200, scaling down: " + data.name);
            smallImage_ = smallImage_.scaledToWidth(960, Qt::SmoothTransformation);
        }
        if (smallImage_.height() > 200)
        {
            LogWarning("Small promotion picture higher than 960x200, scaling down: " + data.name);
            smallImage_ = smallImage_.scaledToHeight(200, Qt::SmoothTransformation);
        }

        if (!focused)
        {
            ui_.labelImage->setPixmap(smallImage_);
            ui_.labelImage->setCursor(Qt::PointingHandCursor);
        }
    }
}

void RocketPromotionWidget::LoadLargeImage(const QString &path)
{
    if (path.isEmpty())
    {
        LogError("Large promotion image path is empty for " + data.name + ". Transfer failed?");
        return;
    }
    
    largeImage_ = QPixmap(path);
    if (largeImage_.isNull())
        LogWarning("Failed to create large promotion picture for " + data.name);
    else
    {
        if (largeImage_.width() > 960)
        {
            LogWarning("Large promotion picture wider than 960x600, scaling down: " + data.name);
            largeImage_ = largeImage_.scaledToWidth(960, Qt::SmoothTransformation);
        }
        if (largeImage_.height() > 600)
        {
            LogWarning("Large promotion picture higher than 960x600, scaling down: " + data.name);
            largeImage_ = largeImage_.scaledToHeight(600, Qt::SmoothTransformation);
        }

        if (focused)
        {
            ui_.labelImage->setPixmap(largeImage_);
            ui_.labelImage->setCursor(Qt::ArrowCursor);
        }
    }
}

void RocketPromotionWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsProxyWidget::mousePressEvent(event);
    if (!focused)
    {
        opacityAnim_->stop();
        setOpacity(1.0);
        emit Pressed(this);
    }
}

// RocketLobbyActionButton

RocketLobbyActionButton::RocketLobbyActionButton(Framework *framework_, Direction direction) :
    RocketSceneWidget(framework_),
    direction_(direction),
    mode_(Unknown)
{
    widget_->setObjectName("widget");
    widget_->setLayout(new QVBoxLayout(widget_));
    widget_->layout()->setContentsMargins(0,0,0,0);
    widget_->layout()->setSpacing(0);
       
    button_ = new QPushButton(widget_);
    button_->setObjectName("button");
    button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    widget_->layout()->addWidget(button_);
    
    SetArrowLook();

    connect(button_, SIGNAL(clicked()), SIGNAL(Pressed()));
    
    setZValue(zValue() - 5);
    hide();
}

RocketLobbyActionButton::~RocketLobbyActionButton()
{
}

void RocketLobbyActionButton::SetArrowLook()
{
    button_->setText("");
    if (mode_ != Arrow)
    {
        mode_ = Arrow;
        widget_->setMaximumSize(QSize(32,32));
        widget_->setStyleSheet(QString("QWidget#widget { background-color: transparent; } QPushButton#button { transparent; \
           border: 0px; background-position: %1 center; background-image: url(\"%2\"); } QPushButton#button:hover { background-image: url(\"%3\"); } ")
           .arg(direction_ == Left ? "left" : "right").arg(direction_ == Left ? ":/images/icon-previous.png" : ":/images/icon-next.png")
           .arg(direction_ == Left ? ":/images/icon-previous-hover.png" : ":/images/icon-next-hover.png"));
    }
}

void RocketLobbyActionButton::SetButtonLook(const QString &message)
{
    button_->setText(message);
    if (mode_ != Button)
    {
        mode_ = Button;
        widget_->setMaximumSize(QSize(16000,16000));
        widget_->setStyleSheet(QString("QWidget#widget { background-color: transparent; } QPushButton#button { background-color: rgba(255, 255, 255, 255); \
            border: 1px solid rgb(100, 100, 100); border-radius: 3px; font-family: \"Arial\"; font-size: 22px; color: rgb(91, 91, 91); } \
            QPushButton#button:hover { background-color: rgba(255, 255, 255, 200); color: black; } \
            QPushButton#button:pressed { background-color: rgba(255, 255, 255, 100); }"));
    }
}

QString RocketLobbyActionButton::GetButtonText()
{
    return button_->text();
}

// RocketLoaderWidget

RocketLoaderWidget::RocketLoaderWidget(Framework *framework) :
    QGraphicsObject(0),
    framework_(framework),
    step_(0),
    opacity_(0),
    timerId_(-1),
    buttonCount_(2.0),
    sizeChanged_(true),
    isPromoted(false),
    percent_(-1.0)
{
    serverData_.Reset();
    
    movie_ = new QMovie(":/images/loader-big.gif", "GIF");
    movie_->setCacheMode(QMovie::CacheAll);

    font_.setFamily("Arial");
    font_.setPointSize(16);
    font_.setBold(true);

    percentFont_.setFamily("Arial");
    percentFont_.setPointSize(80);
    percentFont_.setBold(true);

    setZValue(505);
    hide();
}

RocketLoaderWidget::~RocketLoaderWidget()
{
    StopAnimation();
    SAFE_DELETE(movie_);
}

void RocketLoaderWidget::StartAnimation()
{
    if (timerId_ == -1)
    {
        timerId_ = startTimer(40);
        step_ = 0;
        opacity_ = 0;
        percent_ = -1.0;
        movie_->start();
    }
}

void RocketLoaderWidget::StopAnimation()
{
    if (timerId_ != -1)
    {
        killTimer(timerId_);
        timerId_ = -1;
        isPromoted = false;
        percent_ = -1.0;
        movie_->stop();
    }
}

void RocketLoaderWidget::Show()
{
    if (!isVisible())
    {
        sizeChanged_ = true;
        buttonCount_ = 2.0;
        show();
    }
}

void RocketLoaderWidget::Hide(bool takeScreenShot)
{
    if (isVisible())
        hide();
    StopAnimation();

    // We will take a screenshot for load screen purposes after 30 seconds.
    // The delay is here because a) we don't want to lag the hide animation of
    // the portal widget (happens when this is called) b) We want the world to load
    // completely (or as much as it can in 30 seconds) and possibly let the user 
    // move a bit to get some variation from the startup position.
    if (takeScreenShot && serverData_.CanProcessShots())
        QTimer::singleShot(30000, this, SLOT(StoreServerScreenshot()));
}

void RocketLoaderWidget::LoadServerScreenshot()
{
    if (serverData_.CanProcessShots())
    {
        serverImage_ = QPixmap::fromImage(serverData_.LoadScreenShot(framework_));
        sizeChanged_ = true;
    }
}

void RocketLoaderWidget::StoreServerScreenshot()
{
    if (serverData_.CanProcessShots())
        serverData_.StoreScreenShot(framework_);
}

void RocketLoaderWidget::SetServer(const Meshmoon::Server &serverData)
{
    ResetServer();

    serverData_ = serverData;
    LoadServerScreenshot();
}

void RocketLoaderWidget::SetMessage(const QString &message)
{
    message_ = message;
}

void RocketLoaderWidget::SetGeometry(const QSizeF &size, const QPointF &pos)
{
    if (size_ != size)
    {
        prepareGeometryChange();
        size_ = size;
        sizeChanged_ = true;
    }
    setPos(pos);
}

void RocketLoaderWidget::ResetServer()
{
    serverData_.Reset();
    message_ = "";
    serverImage_ = QPixmap();
    buffer_ = QPixmap();
}

QRectF RocketLoaderWidget::boundingRect() const
{
    return QRectF(0, 0, size_.width(), size_.height());
}

void RocketLoaderWidget::timerEvent(QTimerEvent * /*event*/)
{
    if (step_ < 200)
        step_++;
    else
        step_ = 0;
    if (opacity_ < 200)
        opacity_ += 10;
    update(QRectF(0, 0, size_.width(), size_.height()));
}

void RocketLoaderWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/)
{    
    int buttonHeight = 62;
    int margin = 9;

    QRectF rect(0, 0, size_.width(), size_.height());
    QRectF imageRect(0, buttonHeight+margin, size_.width(), size_.height()-(buttonHeight+margin));
    QSizeF pieSize = imageRect.height() > imageRect.width() ? QSizeF(imageRect.width()/1.2, imageRect.width()/1.2) : QSizeF(imageRect.height()/1.2, imageRect.height()/1.2);
    QRectF textRect(0,0,(rect.width() - margin)/buttonCount_, buttonHeight);

    if (!serverImage_.isNull())
    {
        if (sizeChanged_ || buffer_.isNull())
        {
            QSize renderSize = imageRect.size().toSize();
            QSize imageSize = serverImage_.size();

            qreal xFactor = (qreal)imageSize.width() / (qreal)renderSize.width();
            qreal yFactor = (qreal)imageSize.height() / (qreal)renderSize.height();
            if (xFactor > yFactor)
            {
                buffer_ = serverImage_.scaledToHeight(renderSize.height(), Qt::SmoothTransformation);
                slideRect_ = QRectF(buffer_.width()/2.0 - renderSize.width()/2.0, 0, renderSize.width(), renderSize.height());
            }
            else
            {
                buffer_ = serverImage_.scaledToWidth(renderSize.width(), Qt::SmoothTransformation);
                slideRect_ = QRectF(0, buffer_.height()/2.0 - renderSize.height()/2.0, renderSize.width(), renderSize.height());
            }
            sizeChanged_ = false;
        }

        painter->drawPixmap(imageRect, buffer_, slideRect_);
        painter->setPen(QColor(183,185,184));
        painter->setBrush(Qt::transparent);
        painter->drawRect(imageRect);
    }
    
    if (!message_.isEmpty())
    {
        QTextOption textOption(Qt::AlignCenter);
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter->setPen(QColor(90,90,90));
        painter->setFont(font_);
        painter->drawText(textRect, message_, textOption);
    }
    
    if (movie_->state() == QMovie::Running)
    {
        QPixmap currentFrame = movie_->currentPixmap();
        if (currentFrame.width() > pieSize.toSize().width() || currentFrame.height() > pieSize.toSize().height())
            currentFrame = currentFrame.scaled(pieSize.toSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            
        if (!serverImage_.isNull() || isPromoted)
        {
            painter->setPen(Qt::transparent);
            painter->setBrush(QColor(255,255,255,opacity_));
            painter->drawRect(imageRect);
        }
        painter->drawPixmap(imageRect.center() - QPointF(currentFrame.width()/2.0, currentFrame.height()/2.0), currentFrame, currentFrame.rect());
    }
    
    if (percent_ > 0.0)
    {
        int percentAlpha = static_cast<int>(percent_ / 100.0 * 255) + 50;
        if (percentAlpha > 255) percentAlpha = 255;

        painter->setPen(QColor(242,154,41,percentAlpha));
        painter->setFont(percentFont_);

        QPointF imageCenter = imageRect.center();
        QRectF percentRect = QRectF(imageCenter.x()-(percent_ < 10.0 ? 175 : 148), imageCenter.y()-100, 200, 200);
        painter->drawText(percentRect, Qt::AlignRight|Qt::AlignVCenter, QString::number(Floor(percent_)));

        QFont f = painter->font();
        f.setPointSize(36);
        painter->setFont(f);
        painter->setPen(QColor(90,90,90,percentAlpha));

        percentRect.translate(200, -18);
        painter->drawText(percentRect, Qt::AlignLeft|Qt::AlignVCenter, "%");
    }
}

void RocketLoaderWidget::SetButtonCount( int buttonCount )
{
    buttonCount_ = (qreal)buttonCount;
}

void RocketLoaderWidget::SetCompletion(qreal percent)
{
    // Don't allow to go back in progress while loading world.
    if (percent > percent_)
    {
        percent_ = percent;
        if (percent_ > 100.0)
            percent_ = 100.0;
    }
}

// RocketServerFilter

RocketServerFilter::RocketServerFilter(RocketLobby *controller) :
    QTextEdit("", 0),
    controller_(controller),
    sortCriteriaButton_(0),
    sortCriteriaMenu_(0),
    emptyTermFired_(false),
    helpText_("<span style=\"color:grey;\">" + tr("Search") + "...</span>")
{
    ReadConfig();

    setObjectName("filterLineEdit");
    
    // Try to fix mac font alignment on written text. Suggestions render to center.
    setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
    
    setAcceptRichText(true);
    setTabChangesFocus(false);
    setText(helpText_);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::ClickFocus);

    setMinimumHeight(35);
    setMaximumHeight(35);
    setMinimumWidth(150);
    setMaximumWidth(960);
    
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(this, SIGNAL(textChanged()), SLOT(OnTextChanged()));

    orderDelayTimer_.setSingleShot(true);
    connect(&orderDelayTimer_, SIGNAL(timeout()), SLOT(OnOrderServers()));
    
    focusDelayTimer_.setSingleShot(true);
    connect(&focusDelayTimer_, SIGNAL(timeout()), SLOT(OnCheckFocus()));
        
    // Sort criterial configuration
    sortCriteriaButton_ = new QPushButton(Meshmoon::ServerScore::SortCriteriaToString(currentSortCriterial_), this);
    sortCriteriaButton_->setObjectName("sortCriteriaButton");
    sortCriteriaButton_->setCursor(Qt::PointingHandCursor);
    sortCriteriaButton_->setToolTip("World Sorting Criteria");
    connect(sortCriteriaButton_, SIGNAL(clicked()), SLOT(OnShowSortingConfigMenu()));

    sortCriteriaMenu_ = new QMenu();
    sortCriteriaMenu_->setStyleSheet("QMenu::item:disabled { color: rgb(188, 99, 22); }");
    
    CreateSortCriterialTitle("Sorting");
    for (int i=0; i<static_cast<int>(Meshmoon::ServerScore::SortCriterialNumItems); i++)
    {
        QAction *act = sortCriteriaMenu_->addAction(Meshmoon::ServerScore::SortCriteriaToString(static_cast<Meshmoon::ServerScore::SortCriteria>(i)));
        act->setProperty("scoreCriterialIndex", i);
        act->setCheckable(true);
        act->setChecked(false);
    }

    // Admin and active world configuration
    CreateSortCriterialTitle("Ordering");
    CreateSortCriterialAction("My spaces first", "favorAdministratedWorlds", "Shows spaces that you administer first in the space listing", favorAdministratedWorlds_);
    CreateSortCriterialAction("My Meshmoon Education Program spaces first", "favorMEPWorlds", "Shows MEP spaces that you are a part of first in the space listing", favorMEPWorlds_);
    CreateSortCriterialAction("Spaces with users first", "favorActiveWorlds", "Shows worlds that currently have users in them first in the world listing", favorActiveWorlds_);

    CreateSortCriterialTitle("Show");
    CreateSortCriterialAction("Private worlds", "showPrivateWorlds", "Shows private worlds I have access to in the world listing", showPrivateWorlds_);
    CreateSortCriterialAction("Worlds under maintenance", "showMaintenanceWorlds", "Shows worlds that are currently under maintenance in the world listing", showMaintenanceWorlds_);
    
    connect(sortCriteriaMenu_, SIGNAL(triggered(QAction*)), SLOT(OnSortConfigChanged(QAction*)));
    
    sortCriteriaButton_->setMinimumHeight(minimumHeight());
    sortCriteriaButton_->setMaximumHeight(maximumHeight());
    sortCriteriaButton_->setVisible(false);

    // <rant>
    // Do filter suggestion from a proxy widget that gets rendered
    // on top of this widget. It is very hard to make proper logic with
    // keeping the real input text and the suggestion after it (with a different color i might add).
    // Keeping the text cursor always in the proper position and reseting the suggestion at the right times, ugh.
    // Proxy might be a bit heavier on the rendering but this is easy to work with.
    // </rant>
    suggestLabel_ = new QLabel();
    suggestLabel_->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    suggestLabel_->setStyleSheet("QLabel { background-color: transparent; color: rgb(186, 186, 186); font-family: \"Arial\"; font-size: 22px; }");
    suggestLabel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    suggestProxy_ = new QGraphicsProxyWidget(0, Qt::Widget);
    suggestProxy_->setWidget(suggestLabel_);
    
    suggestionListProxy_ = new RocketSceneWidget(controller->GetFramework(), false);
    suggestionListProxy_->showOpacity = 1.0;
    SAFE_DELETE(suggestionListProxy_->widget_);
    
    QListWidget *listWidget = new QListWidget();
    suggestionListProxy_->widget_ = listWidget;
    suggestionListProxy_->widget_->installEventFilter(this);
    suggestionListProxy_->setZValue(suggestionListProxy_->zValue() * 2);
    suggestionListProxy_->widget_->hide();
    
    connect(listWidget, SIGNAL(itemClicked(QListWidgetItem*)), SLOT(OnSuggestionListItemSelected(QListWidgetItem*)));
    
    suggestionListProxy_->widget_->setStyleSheet(QString(
        "QListView { \
            background-color: rgba(255, 255, 255, 220); \
            font-family: \"Arial\"; \
            font-size: 22px; \
            padding: 0px; \
            margin: 0px; \
            show-decoration-selected: 1; \
            outline: none; \
            min-height: 5px; \
        }") +
        "QListView::item { \
            color: rgb(150, 150, 150); \
            min-height: 25px; \
            max-height: 25px; \
            border: 0px; \
            padding: 0px; \
            margin: 0px; \
            padding-left: 5px; \
        } \
        QListView::item:selected, ::item:selected:hover { \
            background-color: rgba(200, 200, 200, 80); \
            color: rgb(88, 88, 88); \
        } \
        QListView::item:hover { \
            color: rgb(120, 120, 120); \
        }");

    controller_->GetFramework()->Ui()->GraphicsScene()->addItem(suggestionListProxy_);
    controller_->GetFramework()->Ui()->GraphicsScene()->addItem(suggestProxy_);
    
    Hide();
}

RocketServerFilter::~RocketServerFilter()
{
    controller_ = 0;
}

QAction *RocketServerFilter::CreateSortCriterialTitle(const QString &text)
{
    if (!sortCriteriaMenu_)
        return 0;

    QAction *title = sortCriteriaMenu_->addAction(text);
    QFont titleFont("Arial");
    titleFont.setPixelSize(13);
    titleFont.setWeight(QFont::Bold);
    title->setProperty("menuGroupTitle", true);
    title->setFont(titleFont);
    title->setEnabled(false);
    return title;
}

QAction *RocketServerFilter::CreateSortCriterialAction(const QString &text, const QString &propertyName, const QString &tooltip, bool checked)
{
    if (!sortCriteriaMenu_)
        return 0;

    QAction *act = sortCriteriaMenu_->addAction(text);
    act->setProperty(qPrintable(propertyName), true);
    act->setCheckable(true);
    act->setChecked(checked);
    act->setToolTip(tooltip);
    act->setStatusTip(tooltip);
    return act;
}

void RocketServerFilter::ReadConfig()
{
    if (!controller_)
        return;

    ConfigData config("rocket-lobby", "sorting");
    currentSortCriterial_ = static_cast<Meshmoon::ServerScore::SortCriteria>(controller_->GetFramework()->Config()->DeclareSetting(config, "sortCriteriaType", 
        static_cast<int>(Meshmoon::ServerScore::TopYear)).toInt());
    favorAdministratedWorlds_ = controller_->GetFramework()->Config()->DeclareSetting(config, "favorAdministratedWorlds", true).toBool();
    favorMEPWorlds_ = controller_->GetFramework()->Config()->DeclareSetting(config, "favorMEPWorlds", true).toBool();
    favorActiveWorlds_ = controller_->GetFramework()->Config()->DeclareSetting(config, "favorActiveWorlds", true).toBool();
    showMaintenanceWorlds_ = controller_->GetFramework()->Config()->DeclareSetting(config, "showMaintenanceWorlds", true).toBool();
    showPrivateWorlds_ = controller_->GetFramework()->Config()->DeclareSetting(config, "showPrivateWorlds", true).toBool();
}

void RocketServerFilter::WriteConfig()
{
    if (!controller_)
        return;

    ConfigData config("rocket-lobby", "sorting");
    controller_->GetFramework()->Config()->Write(config, "sortCriteriaType", static_cast<int>(currentSortCriterial_));
    controller_->GetFramework()->Config()->Write(config, "favorAdministratedWorlds", favorAdministratedWorlds_);
    controller_->GetFramework()->Config()->Write(config, "favorMEPWorlds", favorMEPWorlds_);
    controller_->GetFramework()->Config()->Write(config, "favorActiveWorlds", favorAdministratedWorlds_);
    controller_->GetFramework()->Config()->Write(config, "showMaintenanceWorlds", showMaintenanceWorlds_);
    controller_->GetFramework()->Config()->Write(config, "showPrivateWorlds", showPrivateWorlds_);
}

void RocketServerFilter::SetSortCriteriaInterfaceVisible(bool visible)
{
    if (!sortCriteriaButton_)
        return;

    sortCriteriaButton_->setVisible(visible);
    if (visible)
    {
        // Highlight the UI for a sec if we are about to sort by a non-default criteria.
        if (currentSortCriterial_ != Meshmoon::ServerScore::TotalLoginCount)
        {
            sortCriteriaButton_->setStyleSheet("color: rgb(219, 137, 37);");
            QTimer::singleShot(1500, this, SLOT(OnRestoreSearchCriteriaButton()));
        }
        QTimer::singleShot(1, this, SLOT(UpdateGeometry()));
    }
}

void RocketServerFilter::UnInitialize(Framework *framework)
{
    framework->Ui()->GraphicsScene()->removeItem(suggestProxy_);
    SAFE_DELETE(suggestProxy_);
    
    framework->Ui()->GraphicsScene()->removeItem(suggestionListProxy_);
    SAFE_DELETE(suggestionListProxy_);
    
    SAFE_DELETE(sortCriteriaMenu_);
    suggestLabel_ = 0;
    controller_ = 0;
}

void RocketServerFilter::Show()
{
    if (isVisible())
        return;
    
    emptyTermFired_ = false;
    show();
    
    HideAndClearSuggestions();
    QTimer::singleShot(100, this, SLOT(UpdateGeometry()));
}

void RocketServerFilter::ShowSuggestionLabel(bool updateGeometryDelayed)
{
    if (suggestProxy_ && !suggestProxy_->isVisible())
        suggestProxy_->show();
        
    if (updateGeometryDelayed)
        QTimer::singleShot(1, this, SLOT(UpdateGeometry()));
}

void RocketServerFilter::ShowSuggestionList(bool updateGeometryDelayed)
{
    if (suggestionListProxy_ && suggestionListProxy_->IsHiddenOrHiding())
        suggestionListProxy_->Show();

    if (updateGeometryDelayed)
        QTimer::singleShot(1, this, SLOT(UpdateGeometry()));
}

void RocketServerFilter::Hide()
{
    if (isVisible())
        hide();

    emptyTermFired_ = false;
    clearFocus();
    if (suggestionListProxy_)
        suggestionListProxy_->clearFocus();

    HideSuggestionLabel(true);
    HideSuggestionList(true);
}

void RocketServerFilter::HideSuggestionLabel(bool clearText)
{
    if (suggestProxy_ && suggestProxy_->isVisible())
        suggestProxy_->hide();
    if (clearText && suggestLabel_ && !suggestLabel_->text().isEmpty())
        suggestLabel_->clear();
}

void RocketServerFilter::HideSuggestionList(bool clearItems)
{
    if (suggestionListProxy_ && !suggestionListProxy_->IsHiddenOrHiding())
        suggestionListProxy_->Hide(RocketAnimations::NoAnimation);
    if (clearItems)
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        if (list) list->clear();
    }
}

QString RocketServerFilter::GetTerm()
{
    QString term = toPlainText();
    if (IsPlaceholder(term))
        term = "";
    return term;
}

bool RocketServerFilter::IsTermValid()
{
    return !GetTerm().isEmpty();
}

void RocketServerFilter::ApplyCurrentTerm()
{
    emptyTermFired_ = false;

    HideAndClearSuggestions();
    OnTextChanged();
    
    HideSuggestionLabel();
    HideSuggestionList();
}

bool RocketServerFilter::HasOpenMenus()
{
    return (sortCriteriaMenu_ && sortCriteriaMenu_->isVisible());
}

bool RocketServerFilter::HasFocus()
{
    return (hasFocus() || (suggestionListProxy_ && suggestionListProxy_->hasFocus()));
}

void RocketServerFilter::EmitCurrentSortConfigChanged()
{
    if (sortCriteriaButton_)
    {
        sortCriteriaButton_->setStyleSheet("color: rgb(219, 137, 37);");
        QTimer::singleShot(1500, this, SLOT(OnRestoreSearchCriteriaButton()));
    }
    emit SortConfigChanged();
}

void RocketServerFilter::OnRestoreSearchCriteriaButton()
{
    if (sortCriteriaButton_)
        sortCriteriaButton_->setStyleSheet("");
}

void RocketServerFilter::OnShowSortingConfigMenu()
{
    if (!sortCriteriaMenu_)
        return;

    foreach(QAction *act, sortCriteriaMenu_->actions())
    {
        if (act->property("scoreCriterialIndex").isValid())
            act->setChecked(currentSortCriterial_ == static_cast<Meshmoon::ServerScore::SortCriteria>(act->property("scoreCriterialIndex").toInt()));
    }

    sortCriteriaMenu_->popup(QCursor::pos());
}

void RocketServerFilter::OnSortConfigChanged(QAction *act)
{
    if (!sortCriteriaButton_ || !act)
        return;
        
    if (act->property("scoreCriterialIndex").isValid())
    {
        currentSortCriterial_ = static_cast<Meshmoon::ServerScore::SortCriteria>(act->property("scoreCriterialIndex").toInt());
        sortCriteriaButton_->setText(Meshmoon::ServerScore::SortCriteriaToString(currentSortCriterial_));
    }
    else if (act->property("favorAdministratedWorlds").isValid())
        favorAdministratedWorlds_ = act->isChecked();
    else if (act->property("favorMEPWorlds").isValid())
        favorMEPWorlds_ = act->isChecked();
    else if (act->property("favorActiveWorlds").isValid())
        favorActiveWorlds_ = act->isChecked();
    else if (act->property("showPrivateWorlds").isValid())
        showPrivateWorlds_ = act->isChecked();
    else if (act->property("showMaintenanceWorlds").isValid())
        showMaintenanceWorlds_ = act->isChecked();
    else
        return;

    WriteConfig();
    
    EmitCurrentSortConfigChanged();
    QTimer::singleShot(1, this, SLOT(UpdateGeometry()));
}

bool RocketServerFilter::IsPlaceholder(const QString &term)
{
    return (term == tr("Search") + "...");
}

void RocketServerFilter::OnTextChanged()
{
    if (!controller_)
        return;

    QString term = GetTerm();

    // Make widgets decide if they are visible or not with the current term
    Meshmoon::RelevancyFilter filter(term, Qt::CaseInsensitive);
    foreach(RocketServerWidget *server, controller_->Servers())
    {
        if (!ShowPrivateWorlds() && !server->data.isPublic)
            continue;
        if (!ShowMaintenanceWorlds() && server->data.isUnderMaintainance)
            continue;
        server->FilterAndSuggest(filter);
    }

    UpdateSuggestionWidgets(filter);

    if (orderDelayTimer_.isActive())
        orderDelayTimer_.stop();
    orderDelayTimer_.start((term.isEmpty() ? 1 : 250));
}

void RocketServerFilter::OnOrderServers()
{
    if (!controller_)
        return;

    // Order servers, uses the filter information from widgets.
    // Track when we send empty term to lobby sorting, only send once in a row.
    QString term = GetTerm();
    if (emptyTermFired_ && term.isEmpty())
        return;
    controller_->OrderServers(!term.isEmpty(), !term.isEmpty() ? true : false);
    emptyTermFired_ = term.isEmpty();

    // If there is only one visible search result, hide box and select the server.
    int visibleServers = controller_->VisibleServers();
    if (visibleServers == 1 && suggestionListProxy_->isVisible())
        HideSuggestionList();
    if (visibleServers == 1 && HasFocus() && !suggestProxy_->isVisible())
        controller_->SelectFirstServer();
}

void RocketServerFilter::UpdateSuggestionWidgets(Meshmoon::RelevancyFilter &filter)
{
    if (!filter.term.isEmpty() && !IsPlaceholder(filter.term))
    {
        if (filter.HasHits())
        {
            QStringList allSuggestions = filter.AllValues();

            // We can only show direct hits in the line edit suggestions
            if (filter.HasDirectHits())
            {
                QStringList directSuggestions = filter.DirectValues();
                QString suggestion = directSuggestions.takeFirst();
                QString originalSuggestion = suggestion; 
                allSuggestions.removeAll(suggestion);

                int startIndex = suggestion.indexOf(filter.term, 0 , Qt::CaseInsensitive);
                if (startIndex == -1) startIndex = 0; // Should never happen.
                suggestion = suggestion.right(suggestion.length() - filter.term.length() - startIndex);

                // Split the term from first space to show suggestions one word at a time.
                // But if there is only one direct suggestion left, use it fully!
                if (!directSuggestions.isEmpty())
                {
                    /** We still might be able to find a common prefix with multiple spaces among the remaining direct suggestions.
                        term         = "demo sce"
                        suggestion 1 = "demo scene meshmoon";
                        suggestion 2 = "demo scene meshmoon one";
                        suggestion 3 = "demo scene meshmoon two";
                        should pick  = "demo scene meshmoon" NOT "demo scene" */
                    bool remainingSamePrefix = true;
                    foreach(const QString &remainingDirect, directSuggestions)
                    {
                        if (!remainingDirect.startsWith(originalSuggestion, Qt::CaseInsensitive))
                        {
                            remainingSamePrefix = false;
                            break;
                        }
                    }
                    if (!remainingSamePrefix && suggestion.indexOf(" ", 1) != -1)
                        suggestion = suggestion.left(suggestion.indexOf(" ", 1));
                }

                // Set suggestion to proxy.
                suggestLabel_->setText(suggestion);

                QTextCursor cur = textCursor();
                cur.movePosition(QTextCursor::End);
                setTextCursor(cur);

                ShowSuggestionLabel();
            }
            else
                HideSuggestionLabel(true);

            // Update suggestion list
            if (suggestionListProxy_)
            {
                QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
                if (list && allSuggestions.size() > 0)
                {
                    list->clear();
                    for (int si=0; si<allSuggestions.size(); si++)
                    {
                        if (si >= 4) break;
                        list->addItem(filter.GuaranteeStartsWithCase(allSuggestions[si]));
                    }
                    ShowSuggestionList();
                }
                else
                    HideSuggestionList(true);
            }

            UpdateGeometry();
        }
        else
            HideAndClearSuggestions();
    }
    else
        HideAndClearSuggestions();
}

void RocketServerFilter::UpdateGeometry()
{
    if (!controller_ || !suggestProxy_ || !suggestLabel_ || !controller_->ui_.contentFrame)
        return;
    
    int criterialButtonWidth = 0;
    if (sortCriteriaButton_ && sortCriteriaButton_->isVisible())
    {
        // Make this button fixed size depending on the text width.
        QFontMetrics m(sortCriteriaButton_->font());
        criterialButtonWidth = m.width(sortCriteriaButton_->text()) + 8 + 12;
        sortCriteriaButton_->setFixedWidth(criterialButtonWidth);
        sortCriteriaButton_->move(width() - criterialButtonWidth - 37, 0);
    }
    
    QPoint scenePos = controller_->ui_.contentFrame->pos() + pos();

    if (suggestLabel_->isVisible())
    {
        float yPosFilter = 1.5;

        // Calculate position and size for suggestion proxy. This looks weird but is correct
        // as long as the padding in the style sheet is not changed (the 5 in x).
        QRect rect = cursorRect();
        QPoint scenePosCursor = scenePos + rect.topLeft();
        scenePosCursor.setX(scenePosCursor.x() + 5 + rect.width());
        suggestProxy_->setGeometry(QRectF(scenePosCursor, QSizeF(width() - rect.x() - criterialButtonWidth - 37, height() - (rect.y()*yPosFilter))));
    }
    
    if (suggestionListProxy_->isVisible())
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        int itemCount = list ? list->count() : 0;
        if (itemCount > 0)
            suggestionListProxy_->setGeometry(QRectF(scenePos + QPoint(0, height()-3), QSizeF(width(), itemCount * 25 + 5)));
        else
            HideSuggestionList();
    }
}

void RocketServerFilter::HideAndClearSuggestions()
{
    if (!suggestProxy_ || !suggestLabel_ || !suggestionListProxy_)
        return;
    
    HideSuggestionList(true);   
    HideSuggestionLabel(true);
}

void RocketServerFilter::ApplySuggestion(bool selectFirstServerOnEmpty)
{
    if (!suggestLabel_)
        return;

    if (suggestLabel_->isVisible())
    {
        QString suggestion = suggestLabel_->text();
        if (!suggestion.isEmpty())
        {
            HideAndClearSuggestions();

            setText(toPlainText() + suggestion);
            QTextCursor cur = textCursor();
            cur.movePosition(QTextCursor::End);
            setTextCursor(cur);

            UpdateGeometry();

            // Apply sorting immediately
            if (orderDelayTimer_.isActive())
                orderDelayTimer_.stop();
            OnOrderServers();
            return;
        }
    }
    else if (selectFirstServerOnEmpty)
    {
        if (controller_ && !GetTerm().isEmpty())
            controller_->SelectFirstServer();
    }
}

void RocketServerFilter::OnSuggestionListItemSelected(QListWidgetItem *item)
{
    if (!item)
        return;
   
    QString suggestion = item->text();
    if (!suggestion.isEmpty())
    {
        HideAndClearSuggestions();

        setText(suggestion);
        QTextCursor cur = textCursor();
        cur.movePosition(QTextCursor::End);
        setTextCursor(cur);
        setFocus(Qt::TabFocusReason);

        UpdateGeometry();

        // Apply sorting immediately
        if (orderDelayTimer_.isActive())
            orderDelayTimer_.stop();
        OnOrderServers();    
    }
}

void RocketServerFilter::OnCheckFocus()
{
    if (!HasFocus())
    {
        HideSuggestionLabel();
        HideSuggestionList();
    }
}

void RocketServerFilter::focusInEvent(QFocusEvent *e)
{
    QTextEdit::focusInEvent(e);
    if (toPlainText() == tr("Search") + "...")
        setText("");
    else if (suggestLabel_ && !suggestLabel_->text().isEmpty())
        ShowSuggestionLabel(true);
        
    if (controller_)
        controller_->UnselectCurrentServer();

    focusDelayTimer_.stop();
}

void RocketServerFilter::focusOutEvent(QFocusEvent *e)
{
    QTextEdit::focusOutEvent(e);
    if (toPlainText().isEmpty())
        setText(helpText_);

    focusDelayTimer_.start(10);
}

void RocketServerFilter::FocusSearchBox()
{
    controller_->CleanInputFocus();
    if (suggestionListProxy_)
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        if (list)
            list->clearFocus();
        suggestionListProxy_->clearFocus();
    }
    
    setFocus(Qt::TabFocusReason);
}

void RocketServerFilter::FocusSuggestionBox()
{
    controller_->CleanInputFocus();
    clearFocus();
    
    if (suggestionListProxy_)
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        if (list)
            list->setFocus(Qt::TabFocusReason);
        suggestionListProxy_->setFocus(Qt::TabFocusReason);
    }
}

void RocketServerFilter::Unfocus()
{
    if (suggestionListProxy_)
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        if (list)
        {
            list->clearFocus();
            list->releaseKeyboard();
        }
        suggestionListProxy_->clearFocus();
    }
    clearFocus();
    releaseKeyboard();
    
    // Hack to unfocus the text cursor caret! Nothing else seems to work
    // This will eventually lead into  QTextControlPrivate::setBlinkingCursorEnabled(false)
    QFocusEvent fe(QEvent::FocusOut);
    QApplication::sendEvent(this, &fe);

    controller_->CleanInputFocus();
}

void RocketServerFilter::keyPressEvent(QKeyEvent *e)
{
    if (!suggestionListProxy_)
        return;

    // Enter = hide box and select first server
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
        // If the text apply timer is running, do this 
        // delayed so the server have time to be sorted!
        e->setAccepted(true);
        HideAndClearSuggestions();
        Unfocus();
        QTimer::singleShot((orderDelayTimer_.isActive() ? 250 : 1), controller_, SLOT(SelectFirstServer()));
        return;
    }
    else if (e->key() == Qt::Key_Tab)
    {
        e->setAccepted(true);
        ApplySuggestion(true);
        return;
    }
    // Down = show suggestion
    else if (e->key() == Qt::Key_Down)
    {
        QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
        if (!list)
            return;
        if (suggestionListProxy_->isVisible())
        {
            FocusSuggestionBox();
            list->setCurrentRow(0);
        }
        else if (list->count() > 0)
        {
            list->clearSelection();
            ShowSuggestionList();
        }
        e->setAccepted(true);
        return;
    }

    QTextEdit::keyPressEvent(e);
    if (e->key() == Qt::Key_End || (e->key() == Qt::Key_Right && textCursor().position() == toPlainText().length()))
        ApplySuggestion(true);
    else if (e->key() == Qt::Key_Escape)
    {
        if (suggestionListProxy_->isVisible())
        {
            HideSuggestionList();
            FocusSearchBox();
        }
        else
        {
            clear();
            if (!hasFocus() && toPlainText().isEmpty())
                setText(helpText_);
            controller_->CleanInputFocus();
        }
        e->setAccepted(true);
    }
}

bool RocketServerFilter::eventFilter(QObject *obj, QEvent *e)
{
    if (suggestionListProxy_ && suggestionListProxy_->widget_ == obj)
    {
        if (suggestionListProxy_->isVisible())
        {
            QListWidget *list = suggestionListProxy_->WidgetAs<QListWidget>();
            if (!list)
                return false;

            if (e->type() == QEvent::FocusOut)
            {
                focusDelayTimer_.start(10);
                list->clearSelection();
            }
            else if (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride)
            {
                QKeyEvent *ke = dynamic_cast<QKeyEvent*>(e);
                if (!ke)
                    return false;
                bool isTab = ke->key() == Qt::Key_Tab;
                if (!isTab && e->type() == QEvent::ShortcutOverride)
                    return false;

                if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)
                {
                    int index = list->currentRow();
                    int diff = (ke->key() == Qt::Key_Up ? -1 : 1);
                    index += diff;
                    if (index < 0)
                    {
                        // Focus the input field
                        FocusSearchBox();
                        return true;
                    }
                    if (index >= list->count()) index = list->count()-1;
                    list->setCurrentRow(index);
                    return true;
                }
                else if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
                {
                    QListWidgetItem *item = list->item(list->currentRow());
                    if (item)
                    {
                        list->setCurrentItem(item);
                        OnSuggestionListItemSelected(item);
                        return true;
                    }
                }
                else if (ke->key() == Qt::Key_Escape || isTab)
                {
                    HideSuggestionList();
                    if (e->type() == QEvent::ShortcutOverride)
                        ke->setAccepted(true);
                    else
                        FocusSearchBox();
                    return true;
                }
            }
        }
    }
    return false;
}

// RocketLicensesDialog

RocketLicensesDialog::RocketLicensesDialog(RocketPlugin *plugin)
{
    ui_.setupUi(this);
    
    if (plugin && !plugin->GetFramework()->IsHeadless())
        setWindowIcon(plugin->GetFramework()->Ui()->MainWindow()->windowIcon());

    LoadLicenses();

    connect(ui_.buttonClose, SIGNAL(clicked()), SLOT(close()), Qt::QueuedConnection);
    connect(ui_.licensesList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(OnLicenseSelected(QListWidgetItem*, QListWidgetItem*)));

    ui_.licensesList->setCurrentRow(0);
}

void RocketLicensesDialog::LoadLicenses()
{
    ui_.licensesList->clear();
    licenses_.clear();

    // Load license file paths
    QDir dir(QDir::toNativeSeparators(Application::InstallationDirectory()) + "licenses");
    foreach (const QString &file, dir.entryList(QDir::NoDotAndDotDot|QDir::Files, QDir::Name))
    {
        QFileInfo info(dir.absoluteFilePath(file));
        if (info.suffix().toLower() != "txt")
            continue;

        License l;
        l.name = info.baseName();
        l.absoluteFilePath = info.absoluteFilePath();
        l.item = new QListWidgetItem(l.name);
        licenses_ << l;

        ui_.licensesList->addItem(l.item);
    }

    // Load URL metadata
    QFile file(dir.absoluteFilePath("Urls.dat"));
    if (!file.open(QFile::ReadOnly))
        return;

    QTextStream urlsStream(&file);
    while(!urlsStream.atEnd())
    {
        QStringList parts = urlsStream.readLine().split(" ", QString::SkipEmptyParts);
        if (parts.size() < 2)
            continue;
            
        QString name = parts.takeFirst();
        for(int i=0; i<licenses_.size(); ++i)
        {
            License &l = licenses_[i];
            if (l.name.compare(name, Qt::CaseInsensitive) == 0)
            {
                l.urls = parts;
                break;
            }
        }
    }
    file.close();    
}

void RocketLicensesDialog::OnLicenseSelected(QListWidgetItem *item, QListWidgetItem* /*previous*/)
{
    ui_.licenseEditor->clear();

    for(int i=0; i<licenses_.size(); ++i)
    {
        License &l = licenses_[i];
        if (l.item && l.item == item)
        {
            // Load license
            QFile file(l.absoluteFilePath);
            if (file.open(QFile::ReadOnly))
            {
                QTextStream s(&file);
                s.setAutoDetectUnicode(true);
                ui_.licenseEditor->setPlainText(s.readAll());
                file.close();   
            }
            else
                ui_.licenseEditor->setPlainText("Failed to load license file for preview");

            // Show related urls
            if (!l.urls.isEmpty())
            {
                QStringList links;
                foreach(const QString &link, l.urls)
                {
                    QString linkText = link.trimmed();
                    if (linkText.startsWith("http://"))
                        linkText = link.mid(7);
                    else if (linkText.startsWith("https://"))
                        linkText = link.mid(8);
                    if (linkText.endsWith("/"))
                        linkText.chop(1);
                    links << QString("<a href=\"%1\">%2</a>").arg(link).arg(linkText);
                }
                ui_.descLabel->setText(links.join(" "));
            }
            else
                ui_.descLabel->setText("");
            return;
        }
    }
}

// RocketEulaDialog

RocketEulaDialog::RocketEulaDialog(RocketPlugin *plugin)
{   
    ui_.setupUi(this);
    
    setWindowTitle("Meshmoon Rocket End User License Agreement");
    if (plugin && !plugin->GetFramework()->IsHeadless())
        setWindowIcon(plugin->GetFramework()->Ui()->MainWindow()->windowIcon());

    ui_.licensesList->hide();
    ui_.frameTop->hide();
    ui_.descLabel->setText("Meshmoon Rocket End User License Agreement");

    QFile file(QDir::toNativeSeparators(Application::InstallationDirectory()) + "EULA-Meshmoon-Rocket.txt");
    if (file.open(QFile::ReadOnly))
    {
        QTextStream s(&file);
        s.setAutoDetectUnicode(true);
        ui_.licenseEditor->setPlainText(s.readAll());
        file.close();
    }
    else
        ui_.licenseEditor->setPlainText("Failed to load EULA file for preview");
        
    connect(ui_.buttonClose, SIGNAL(clicked()), SLOT(close()), Qt::QueuedConnection);
}
