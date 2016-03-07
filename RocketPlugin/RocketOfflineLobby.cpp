/**
 @author Adminotech Ltd.
 
 Copyright Adminotech Ltd.
 All rights reserved.
 
 @file
 @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "RocketOfflineLobby.h"
#include "RocketPlugin.h"
#include "RocketLobbyWidget.h"
#include "utils/RocketAnimations.h"

#include "MeshmoonBackend.h"

#include "Framework.h"
#include "SceneAPI.h"
#include "UiAPI.h"
#include "UiGraphicsView.h"
#include "UiProxyWidget.h"

#include <QTimer>
#include <QPixmap>
#include <QMouseEvent>

RocketOfflineLobbyButton::RocketOfflineLobbyButton(const QString &title, const QString &desc, const QPixmap &icon, QWidget *parent) :
    QWidget(parent)
{
    ui_.setupUi(this);
    
    ui_.nameLabel->setText(title);
    ui_.descriptionLabel->setText(desc);
    
    ui_.iconLabel->setPixmap(icon);
}

RocketOfflineLobbyButton::~RocketOfflineLobbyButton()
{
    
}

void RocketOfflineLobbyButton::mousePressEvent(QMouseEvent *)
{
    emit Clicked();
}

RocketOfflineLobby::RocketOfflineLobby(RocketPlugin *plugin, QWidget *parent) :
    QWidget(parent),
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    animation_(new RocketAnimationEngine(this)),
    retryTimer_(0),
    retrySeconds_(3),
    actualSeconds_(retrySeconds_),
    preventShow_(false),
    opacity_(1.0)
{
    ui_.setupUi(this);
    UiProxyWidget *proxy = framework_->Ui()->AddWidgetToScene(this, Qt::Widget);
    proxy->setZValue(100000);

    setGeometry(0, 0, framework_->Ui()->GraphicsView()->width(), framework_->Ui()->GraphicsView()->height());

    retryTimer_ = new QTimer();
    retryTimer_->setInterval(1000);

    QPixmap presisImage(":/images/icon-presis.png");
    RocketOfflineLobbyButton *presisButton = new RocketOfflineLobbyButton("Presis", "Rocket's presentations tool", presisImage);
    AddButton(presisButton);

    QPixmap settingsImage(":/images/icon-settings2.png");
    RocketOfflineLobbyButton *settingsButton = new RocketOfflineLobbyButton("Settings", "Change Rocket settings", settingsImage);
    AddButton(settingsButton);

    animation_->Initialize(proxy, false, false, false, true, 500, QEasingCurve::OutCubic);
    connect(animation_->opacity, SIGNAL(finished()), SLOT(OnAnimationFinished()));

    connect(retryTimer_, SIGNAL(timeout()), this, SLOT(Countdown()));
    connect(ui_.retryButton, SIGNAL(clicked()), this, SLOT(OnRetry()));
    connect(ui_.exitButton, SIGNAL(clicked()), framework_, SLOT(Exit()), Qt::QueuedConnection);
    connect(framework_->Ui()->GraphicsView(), SIGNAL(WindowResized(int, int)), this, SLOT(OnResize(int, int)));
    connect(plugin_->Backend(), SIGNAL(AuthCompleted(const Meshmoon::User &)), this, SLOT(OnAuthenticated()));

    connect(settingsButton, SIGNAL(Clicked()), this, SLOT(Hide()));
    connect(presisButton, SIGNAL(Clicked()), this, SLOT(Hide()));
    connect(settingsButton, SIGNAL(Clicked()), plugin_->Lobby(), SIGNAL(EditSettingsRequest()));
    connect(presisButton, SIGNAL(Clicked()), plugin_->Lobby(), SIGNAL(OpenPresisRequest()));
}

RocketOfflineLobby::~RocketOfflineLobby()
{
    SAFE_DELETE(retryTimer_);
}

void RocketOfflineLobby::OnResize(int newWidth, int newHeight)
{
    setGeometry(0, 0, newWidth, newHeight);
}

void RocketOfflineLobby::AddButton(RocketOfflineLobbyButton *button)
{
    ui_.buttonHolder->addWidget(button);
}

void RocketOfflineLobby::Show(bool animate)
{
    if (preventShow_)
        return;
    if (!plugin_->Backend())
        return;
    if (plugin_->Backend()->IsAuthenticated())
        return;
    if (plugin_->GetFramework()->Scene()->MainCameraScene())
        return;

    if (!isVisible())
    {
        if (animate)
        {
            setVisible(true);
            animation_->opacity->setStartValue(0.0);
            animation_->opacity->setEndValue(opacity_);
            animation_->opacity->start();
        }
        else
            show();
    }
    if (!retryTimer_->isActive())
        retryTimer_->start();
}

void RocketOfflineLobby::Hide(bool animate)
{
    if (isVisible())
    {
        if (animate)
        {
            animation_->opacity->setStartValue(opacity_);
            animation_->opacity->setEndValue(0.0);
            animation_->opacity->start();
        }
        else
            hide();
    }

    if (retryTimer_->isActive())
    {
        retrySeconds_ = 3;
        retryTimer_->stop();
        actualSeconds_ = retrySeconds_;
    }
}

void RocketOfflineLobby::Countdown()
{
    ui_.retryLabel->setText(QString("Retrying connection in %1 seconds").arg(actualSeconds_--));
    ui_.retryButton->setEnabled(true);
    if (actualSeconds_ >= 0)
        return;

    OnRetry();
}

void RocketOfflineLobby::OnRetry()
{
    ui_.retryLabel->setText("Retrying...");
    ui_.retryButton->setDisabled(true);
    retryTimer_->stop();

    plugin_->Backend()->Authenticate();
    retrySeconds_++;
    actualSeconds_ = retrySeconds_;
}

void RocketOfflineLobby::OnAuthenticated()
{
    preventShow_ = true;
    Hide();
}

void RocketOfflineLobby::OnAnimationFinished()
{
    if (animation_->opacity->startValue() == opacity_ && animation_->opacity->endValue() == 0.0)
        setVisible(false);
}
