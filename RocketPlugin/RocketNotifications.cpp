/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "RocketNotifications.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "LoggingFunctions.h"

#include "UiAPI.h"
#include "UiMainWindow.h"

#include "AssetAPI.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "Math/MathFunc.h"

#include <QGraphicsScene>

// RocketNotificationWidget

RocketNotificationWidget::RocketNotificationWidget(Framework *framework) :
    RocketSceneWidget(framework),
    closeTimer_(0),
    msecsAutoHide_(0)
{
    setZValue(1500);

    ui_.setupUi(widget_);   
    ui_.labelIcon->hide();
    connect(ui_.buttonClose, SIGNAL(clicked()), SLOT(Close()), Qt::QueuedConnection);
    
    originalStyle_ = widget_->styleSheet();

    SetInfoTheme();
    hide();
}

RocketNotificationWidget::~RocketNotificationWidget()
{
    if (closeTimer_)
        SAFE_DELETE(closeTimer_);
}

void RocketNotificationWidget::showEvent(QShowEvent *e)
{
    RocketSceneWidget::showEvent(e);

    if (msecsAutoHide_ > 0)
        SetAutoHide(msecsAutoHide_);
}

void RocketNotificationWidget::Close()
{
    HideAndClose(RocketAnimations::AnimateFade, framework_->Ui()->GraphicsScene()->sceneRect());
}

void RocketNotificationWidget::SetAutoHide(int msecs)
{
    // We might still be in hidden state when invoked from showEvent()
    // so check msecsAutoHide_.
    if (isVisible() || msecsAutoHide_ > 0)
    {
        if (!closeTimer_)
        {
            closeTimer_ = new QTimer();
            closeTimer_->setSingleShot(true);
            connect(closeTimer_, SIGNAL(timeout()), this, SLOT(Close()), Qt::QueuedConnection);
        }

        if (closeTimer_->isActive())
            closeTimer_->stop();
        closeTimer_->start(msecs);
    }
    else
        msecsAutoHide_ = msecs;
}

void RocketNotificationWidget::StopAutoHide()
{
    if (closeTimer_ && closeTimer_->isActive())
        closeTimer_->stop();
}

void RocketNotificationWidget::SetInfoTheme()
{
    widget_->setStyleSheet("QWidget#RocketNotificationWidget { border-bottom: 1px solid rgb(182,186,192); background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(230, 230, 230, 255), stop:1 rgba(245, 246, 247, 255)); }"
        + originalStyle_);
}

void RocketNotificationWidget::SetWarningTheme()
{
    widget_->setStyleSheet("QWidget#RocketNotificationWidget { border-bottom: 1px solid rgb(182,186,192); background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 241, 180, 255), stop:1 rgba(250, 231, 148, 255)); }"
        + originalStyle_);
}

void RocketNotificationWidget::SetErrorTheme()
{
    QString style = QString(originalStyle_);
    style.replace("url(:/images/icon-checked.png)", "url(:/images/icon-checked-white.png)", Qt::CaseInsensitive);
    style.replace("url(:/images/icon-unchecked.png)", "url(:/images/icon-unchecked-white.png)", Qt::CaseInsensitive);
    style.replace("color: rgb(80,80,80);", "color: white;", Qt::CaseInsensitive);
    style.replace("color: rgb(50,50,50);", "color: white;", Qt::CaseInsensitive);
    
    widget_->setStyleSheet("QWidget#RocketNotificationWidget { border-bottom: 1px solid rgb(182,186,192); background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(255, 104, 99, 255), stop:1 rgba(250, 77, 71, 255)); }"
        + style);
}

void RocketNotificationWidget::SetMessage(const QString &message)
{
    ui_.labelText->setText(message);
}

void RocketNotificationWidget::SetPixmap(const QString &pixmapResource)
{
    if (pixmapResource.trimmed().isEmpty())
    {
        if (ui_.labelIcon->isVisible())
            ui_.labelIcon->hide();
        return;
    }
    if (!pixmapResource.startsWith("http", Qt::CaseInsensitive))
    {
        QPixmap pixmap(pixmapResource);
        if (!pixmap.isNull())
        {
            if (pixmap.width() > 32 || pixmap.height() > 32)
                pixmap = pixmap.scaled(QSize(32,32), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            ui_.labelIcon->setPixmap(pixmap);
        }
        ui_.labelIcon->setVisible(!pixmap.isNull());
    }
    else
    {
        AssetTransferPtr transfer = framework_->Asset()->RequestAsset(pixmapResource, "Binary");
        connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnPixmapLoaded(AssetPtr)));
    }
}

void RocketNotificationWidget::OnPixmapLoaded(AssetPtr asset)
{
    SetPixmap(asset->DiskSource());
    framework_->Asset()->ForgetAsset(asset, false);
}

QPushButton *RocketNotificationWidget::AddButton(bool closeNotificationOnPress, const QString &text)
{
    QPushButton *button = new QPushButton(text, widget_);
    ui_.buttonLayout->addWidget(button);
    if (closeNotificationOnPress)
        connect(button, SIGNAL(clicked()), this, SLOT(Close()), Qt::QueuedConnection); // Queued connection so the code that added the button gets clicked signal first!
    return button;
}

QCheckBox *RocketNotificationWidget::AddCheckBox(const QString &text, bool checked)
{
    QCheckBox *checkbox = new QCheckBox(text, widget_);
    checkbox->setChecked(checked);
    ui_.buttonLayout->addWidget(checkbox);
    return checkbox;
}

QRadioButton *RocketNotificationWidget::AddRadioButton(const QString &text, bool checked)
{
    QRadioButton *radioButton = new QRadioButton(text, widget_);
    radioButton->setAutoExclusive(true);
    radioButton->setChecked(checked);
    ui_.buttonLayout->addWidget(radioButton);
    return radioButton;
}

// RocketNotifications

RocketNotifications::RocketNotifications(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework())
{
    connect(plugin_, SIGNAL(SceneResized(const QRectF&)), SLOT(OnSceneResized(const QRectF&)));

    dialogButtonStyle_ = 
"QPushButton { \
    background-color: transparent; \
    color: rgb(80, 80, 80); \
    border: 1px solid grey; \
    border: 0px; \
    border-radius: 3px; \
    font-family: 'Arial'; \
    font-size: 13px; \
    font-weight: bold; \
    min-height: 25px; \
    max-height: 25px; \
    min-width: 75px; \
    padding-left: 5px; \
    padding-right: 5px; } \
QPushButton:hover { \
    background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(242, 244, 243, 255), stop:1 rgba(255, 255, 255, 255)); \
    color: rgb(60, 60, 60); \
    border: 1px solid grey; } \
QPushButton:pressed { \
    background-color: rgba(200, 200, 200); \
    border: 1px solid grey; } \
QPushButton:default { \
    color: rgb(25,25,25); }";
}

RocketNotifications::~RocketNotifications()
{
    CloseNotifications();
}

void RocketNotifications::CloseNotifications()
{
    if (notifications_.size() > 0)
    {
        foreach(RocketNotificationWidget *notification, notifications_)
        {
            if (notification)
            {
                notification->close();
                if (notification->scene())
                    framework_->Ui()->GraphicsScene()->removeItem(notification);
            }
            SAFE_DELETE_LATER(notification);
        }
        notifications_.clear();
        activeNotification_ = 0;
    }
}

QMessageBox *RocketNotifications::ShowSplashDialog(const QString &message, const QString &pixmapPath, QMessageBox::StandardButtons buttons,
                                                   QMessageBox::StandardButton defaultButton, QWidget *parent, bool autoPositionAndShow)
{
    return ShowSplashDialog(message, (!pixmapPath.isEmpty() ? QPixmap(pixmapPath) : QPixmap()), buttons, defaultButton, parent, autoPositionAndShow);
}

QMessageBox *RocketNotifications::ShowSplashDialog(const QString &message, const QPixmap &pixmap, QMessageBox::StandardButtons buttons,
                                                   QMessageBox::StandardButton defaultButton, QWidget *parent, bool autoPositionAndShow)
{
    QMessageBox *dialog = new QMessageBox(parent);
    
    // Dim foreground automatically if no parent is set
    if (!parent)
    {
        DimForeground();
        connect(dialog, SIGNAL(destroyed(QObject*)), this, SLOT(ClearForeground()));
    }

    dialog->setAttribute(Qt::WA_DeleteOnClose, true);   
    dialog->setWindowModality(parent != 0 ? Qt::WindowModal : Qt::ApplicationModal);
    dialog->setWindowFlags(parent != 0 ? Qt::Tool : Qt::SplashScreen);
    dialog->setWindowTitle(" ");
    dialog->setModal(true);
    dialog->setStyleSheet(dialogButtonStyle_);

    QFont dialogFont("Arial");
    dialogFont.setPixelSize(14);
    dialog->setFont(dialogFont);
    dialog->setText(message);

    dialog->setStandardButtons(buttons);
    if (defaultButton != QMessageBox::NoButton)
        dialog->setDefaultButton(defaultButton);

    if (!pixmap.isNull())
        dialog->setIconPixmap(pixmap);

    if (autoPositionAndShow)
    {
        dialog->show();
        dialog->setFocus(Qt::ActiveWindowFocusReason);
        dialog->activateWindow();

        if (!parent)
        {
            CenterToMainWindow(dialog);

            dialog->setWindowOpacity(0.0);

            QPropertyAnimation *showAnim = new QPropertyAnimation(dialog, "windowOpacity", dialog);
            showAnim->setStartValue(0.0);
            showAnim->setEndValue(1.0);
            showAnim->setDuration(300);
            showAnim->setEasingCurve(QEasingCurve(QEasingCurve::InOutQuad)); 
            showAnim->start();
        }
        else
            CenterToWindow(parent, dialog);
    }

    foreach (QAbstractButton *button, dialog->buttons())
    {
        QMessageBox::ButtonRole role = dialog->buttonRole(button);
        if (role == QMessageBox::AcceptRole || role == QMessageBox::YesRole || role == QMessageBox::ApplyRole)
        {
            button->setIcon(QIcon(":/images/icon-check.png"));
            button->setIconSize(QSize(16,16));
        }
        else if (role == QMessageBox::RejectRole || role == QMessageBox::NoRole)
        {
            button->setIcon(QIcon(":/images/icon-cross.png"));
            button->setIconSize(QSize(16,16));
        }
    }

    return dialog;
}

QMessageBox::StandardButton RocketNotifications::ExecSplashDialog(const QString &message, const QString &pixmapPath, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    return ExecSplashDialog(message, (!pixmapPath.isEmpty() ? QPixmap(pixmapPath) : QPixmap()), buttons, defaultButton);
}

QMessageBox::StandardButton RocketNotifications::ExecSplashDialog(const QString &message, const QPixmap &pixmap, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    QPixmap emptyIcon(16,16);
    emptyIcon.fill(Qt::transparent);

    QMessageBox dialog;
    dialog.setWindowTitle(" ");
    dialog.setWindowIcon(QIcon(emptyIcon));
    dialog.setStyleSheet(dialogButtonStyle_);

    QFont dialogFont("Arial");
    dialogFont.setPixelSize(12);
    dialog.setFont(dialogFont);
    dialog.setText(message);
    if (!pixmap.isNull())
        dialog.setIconPixmap(pixmap);

    dialog.setStandardButtons(buttons);
    if (defaultButton != QMessageBox::NoButton)
        dialog.setDefaultButton(defaultButton);
        
    foreach (QAbstractButton *button, dialog.buttons())
    {
        QMessageBox::ButtonRole role = dialog.buttonRole(button);
        if (role == QMessageBox::AcceptRole || role == QMessageBox::YesRole || role == QMessageBox::ApplyRole)
        {
            button->setIcon(QIcon(":/images/icon-check.png"));
            button->setIconSize(QSize(16,16));
        }
        else if (role == QMessageBox::RejectRole || role == QMessageBox::NoRole)
        {
            button->setIcon(QIcon(":/images/icon-cross.png"));
            button->setIconSize(QSize(16,16));
        }
    }
    
    DimForeground();
    dialog.setModal(true);
    int ret = dialog.exec();
    ClearForeground();
    if (!ret)
        return QMessageBox::Cancel;

    return dialog.standardButton(dialog.clickedButton());
}

RocketNotificationWidget *RocketNotifications::ShowNotification(const QString &message, const QString &pixmapResource)
{
    RocketNotificationWidget *notification = new RocketNotificationWidget(framework_);
    notification->setProperty("isCurrent", false);
    notification->SetMessage(message);
    notification->SetPixmap(pixmapResource);
    notification->hide();
    connect(notification, SIGNAL(Closed()), SLOT(ProcessNextNotification()), Qt::QueuedConnection);

    notifications_.push_back(notification);
    QTimer::singleShot(1, this, SLOT(ProcessNextNotification()));
    return notification;
}

void RocketNotifications::SetForeground(const QBrush &brush)
{
    if (framework_ && framework_->Ui()->GraphicsScene())
    {
        framework_->Ui()->GraphicsScene()->setForegroundBrush(brush);
        framework_->Ui()->GraphicsScene()->update();
    }
}

bool RocketNotifications::HasForeground(const QBrush &brush) const
{
    if (framework_ && framework_->Ui()->GraphicsScene())
        return (framework_->Ui()->GraphicsScene()->foregroundBrush() == brush);
    return false;
}

bool RocketNotifications::IsForegroundDimmed() const
{
    return HasForeground(QColor(23,23,23,75));
}

void RocketNotifications::DimForeground()
{
    SetForeground(QColor(23,23,23,75));
}

void RocketNotifications::ClearForeground()
{
    SetForeground(Qt::NoBrush);
}

void RocketNotifications::CenterToMainWindow(QWidget *widget)
{
    if (widget)
    {
        QPointF centerPos = MainWindowCenter();
        widget->move(centerPos.x() - (widget->width()/2), centerPos.y() - (widget->height()/2));
    }
}

void RocketNotifications::CenterToWindow(QWidget *window, QWidget *widget)
{
    if (!window || !widget)
        return;

    QWidget *topLevelWindow = TopMostParent(window);
    QPoint centerPos = QRect(topLevelWindow->pos(), topLevelWindow->rect().size()).center();
    if (centerPos.x() >= 0 && centerPos.y() >= 0)
        widget->move(centerPos.x() - (widget->width()/2), centerPos.y() - (widget->height()/2));
}

QRect RocketNotifications::MainWindowRect() const
{
    if (framework_ && framework_->Ui()->MainWindow())
    {
        QPoint mainWindowPos = framework_->Ui()->MainWindow()->pos();
        QSize mainWindowSize = framework_->Ui()->MainWindow()->rect().size();
        return QRect(mainWindowPos, mainWindowSize);
    }
    return QRect();
}

void RocketNotifications::EnsureGeometryWithinDesktop(QRect &rect)
{
    int xMax = UiMainWindow::DesktopWidth(),
        yMax = UiMainWindow::DesktopHeight();
        
    if (rect.width() > xMax)
        rect.setWidth(xMax);
    if (rect.height() > yMax)
        rect.setHeight(yMax);
        
    const int padding = 10;
    const int w = Max(rect.width(), 100);
    const int h = Max(rect.height(), 100);
    if (rect.topLeft().x() + w > xMax)
        rect.moveTo(xMax - w - padding, rect.topLeft().y());
    if (rect.topLeft().y() + h > yMax)
        rect.moveTo(rect.topLeft().x(), yMax - h - padding);
    if (rect.topLeft().x() < 0)
        rect.moveTo(padding, rect.topLeft().y());
    if (rect.topLeft().y() < 0)
        rect.moveTo(rect.topLeft().x(), padding);
}

QPoint RocketNotifications::MainWindowCenter() const
{
    return MainWindowRect().center();
}

QWidget *RocketNotifications::TopMostParent(QWidget *widget)
{
    QWidget *topLevelWidget = widget;
    while (topLevelWidget && topLevelWidget->parentWidget())
    {
        Qt::WindowType t = topLevelWidget->windowType();
        if (t == Qt::Window || t == Qt::Dialog || t == Qt::Tool)
            break;
        topLevelWidget = topLevelWidget->parentWidget();
    }
    return topLevelWidget;
}

void RocketNotifications::OnSceneResized(const QRectF &rect)
{
    if (activeNotification_)
        activeNotification_->resize(rect.width(), activeNotification_->size().height());
}

RocketNotificationWidgetList RocketNotifications::Notifications()
{
    return notifications_;
}

void RocketNotifications::ProcessNextNotification()
{
    // Check that no other notification is visible.
    foreach(RocketNotificationWidget *notification, notifications_)
        if (notification && notification->isVisible())
            return;

    // Delete the previous notification.
    if (activeNotification_)
    {
        for (int i=0; i<notifications_.size(); ++i)
        {
            RocketNotificationWidget *notification = notifications_[i];
            if (activeNotification_ == notification)
            {
                if (notification->scene())
                    framework_->Ui()->GraphicsScene()->removeItem(notification);
                SAFE_DELETE_LATER(notification);
                notifications_.removeAt(i);
                break;
            }
        }
    }
    if (notifications_.empty())
        return;

    RocketNotificationWidget *notification = notifications_.first();
    if (!notification)
    {
        notifications_.removeFirst();
        ProcessNextNotification();
        return;
    }
    notification->setProperty("isCurrent", true);

    activeNotification_ = notification;
    framework_->Ui()->GraphicsScene()->addItem(notification);
    
    QRectF sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
    notification->resize(sceneRect.width(), notification->size().height());
    notification->Animate(notification->size(), QPointF(0,0), -1.0, sceneRect, RocketAnimations::AnimateDown);
}
