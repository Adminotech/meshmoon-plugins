/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketLobbyWidget.h"
#include "RocketLobbyWidgets.h"
#include "RocketPlugin.h"
#include "RocketMenu.h"
#include "RocketSettings.h"
#include "RocketNotifications.h"
#include "RocketOfflineLobby.h"
#include "utils/RocketAnimations.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"

#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiProxyWidget.h"

#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "IAsset.h"

#include "InputAPI.h"
#include "InputContext.h"

#include "Algorithm/Random/LCG.h"
#include "ConsoleAPI.h"
#include "ConsoleWidget.h"

#include <QGraphicsScene>
#include <QGraphicsWebView>
#include <QFile>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QDesktopServices>
#include <QMovie>
#include <QWebPage>
#include <QWebFrame>
#include <QNetworkReply>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QMessageBox>
#include <QMenu>
#include <QAction>

#include "MemoryLeakCheck.h"

RocketLobby::RocketLobby(RocketPlugin *plugin) :
    QWidget(0),
    framework_(plugin->GetFramework()),
    plugin_(plugin),
    anims_(new RocketAnimationEngine(this)),
    proxy_(0), 
    filter_(0), 
    loader_(0), 
    rocketLogo_(0),
    leftActionButton_(0),
    rightActionButton_(0), 
    loaderAnimation_(0),
    currentPage_(0),
    previousPage_(0),
    currentItemsPerWidth_(1),
    currentNewsIndex_(-1),
    state_(Portal),
    showNewsOverride_(true)
{
    // Menu items   
    actDisconnect_ = new QAction(QIcon(":/images/icon-disconnect.png"), tr("Disconnect"), this);
    actEditProfile_ = new QAction(QIcon(":/images/icon-profile.png"), tr("Edit Profile"), this);
    actDirectLogin_ = new QAction(QIcon(":/images/icon-connect.png"), tr("Direct Login"), this);

    actDisconnect_->setVisible(false);
    actEditProfile_->setVisible(false);
    
    plugin_->Menu()->AddAction(actDirectLogin_, tr("Session"));
    plugin_->Menu()->AddAction(actEditProfile_, tr("Session"));
    plugin_->Menu()->AddAction(actDisconnect_, tr("Session"));
    
    connect(plugin_, SIGNAL(ConnectionStateChanged(bool)), SLOT(OnConnectionStateChanged(bool)));
    connect(plugin_->Settings(), SIGNAL(SettingsApplied(const ClientSettings*)), this, SLOT(OnSettingsApplied(const ClientSettings*)));

    connect(actDisconnect_, SIGNAL(triggered()), SIGNAL(DisconnectRequest()));
    connect(actDirectLogin_, SIGNAL(triggered()), SLOT(OnDirectLoginPressed()));
    connect(actEditProfile_, SIGNAL(triggered()), SLOT(OnEditProfilePressed()));
    
    // Setup own ui
    ui_.setupUi(this);
    proxy_ = framework_->Ui()->AddWidgetToScene(this, Qt::Widget);
    
    // Rocket logo
    QRectF sceneRect = framework_->Ui()->GraphicsScene()->sceneRect();
    rocketLogo_ = new QLabel(ui_.contentFrame);
    rocketLogo_->setFixedSize(146, 150);
    rocketLogo_->setPixmap(QPixmap(":/images/logo-rocket-light.png"));
    rocketLogo_->setStyleSheet("QLabel { background-color: transparent; }");
    rocketLogo_->move(5, sceneRect.height() - 100 - 150);
    rocketLogo_->show();

    filter_ = new RocketServerFilter(this);
    ui_.filterLayout->insertWidget(1, filter_);
    connect(filter_, SIGNAL(SortConfigChanged()), SLOT(OrderServers()));
        
    leftActionButton_ = new RocketLobbyActionButton(framework_, RocketLobbyActionButton::Left);
    rightActionButton_ = new RocketLobbyActionButton(framework_, RocketLobbyActionButton::Right);
    framework_->Ui()->GraphicsScene()->addItem(leftActionButton_);
    framework_->Ui()->GraphicsScene()->addItem(rightActionButton_);
    
    loader_ = new RocketLoaderWidget(framework_);
    framework_->Ui()->GraphicsScene()->addItem(loader_);

    connect(plugin_, SIGNAL(SceneResized(const QRectF&)), this, SLOT(OnSceneResized(const QRectF&)));
    connect(proxy_, SIGNAL(Visible(bool)), SLOT(OnVisibilityChange(bool)));
    connect(ui_.buttonLogout, SIGNAL(clicked()), SLOT(OnLogoutPressed()));
    connect(ui_.buttonEditSettings, SIGNAL(clicked()), SIGNAL(EditSettingsRequest()));
    connect(ui_.buttonEditProfile, SIGNAL(clicked()), SLOT(OnEditProfilePressed()));
    connect(ui_.buttonEditAvatar, SIGNAL(clicked()), SIGNAL(OpenAvatarEditorRequest()));
    connect(ui_.buttonPresis, SIGNAL(clicked()), SIGNAL(OpenPresisRequest()));
    connect(ui_.buttonRefreshServers, SIGNAL(clicked()), SIGNAL(UpdateServersRequest()));
    connect(ui_.buttonExitRocket, SIGNAL(clicked()), framework_, SLOT(Exit()), Qt::QueuedConnection);
    connect(leftActionButton_, SIGNAL(Pressed()), SLOT(LeftActionPressed()));
    connect(rightActionButton_, SIGNAL(Pressed()), SLOT(RightActionPressed()));

    ui_.labelProfile->setVisible(false);
    ui_.buttonLogout->setVisible(false);
    ui_.buttonEditSettings->setVisible(false);
    ui_.buttonEditProfile->setVisible(false);
    ui_.buttonEditAvatar->setVisible(false);
    ui_.buttonRefreshServers->setVisible(false);
    ui_.onlineUsersLabelValue->setVisible(false);
    ui_.totalLoginsLabelValue->setVisible(false);
    ui_.hostedScenesLabelValue->setVisible(false);
    ui_.onlineUsersLabel->setVisible(false);
    ui_.totalLoginsLabel->setVisible(false);
    ui_.hostedScenesLabel->setVisible(false);
    ui_.labelFreeInfo->setVisible(false);
    ui_.frameNews->setVisible(false);
    ui_.labelNewsTitle->setVisible(false);
    ui_.labelNewsMessage->setVisible(false);
    ui_.buttonExitRocket->setVisible(false);
    
    connect(ui_.buttonMeshmoonLogo, SIGNAL(clicked()), SLOT(OnMeshmoonLogoPressed()));

    leftActionButton_->Hide();
    rightActionButton_->Hide();
    filter_->Hide();
   
    // Animations init
    anims_->Initialize(proxy_, false, true, true, false, 750);
    anims_->HookCompleted(this, SLOT(OnTransformAnimationsFinished(RocketAnimationEngine*, RocketAnimations::Direction)));
    
    // Timers    
    newsTimer_.setSingleShot(true);
    filterTimer_.setSingleShot(true);
    connect(&newsTimer_, SIGNAL(timeout()), SLOT(NewsTimeout()));
    connect(&filterTimer_, SIGNAL(timeout()), SLOT(ApplyCurrentFilterAndSorting()));
    
    // Input
    if (framework_->Input()->TopLevelInputContext())
        connect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), SLOT(OnKeyPressed(KeyEvent*)));
}

RocketLobby::~RocketLobby()
{
    rocketLogo_ = 0;
    
    ClearWebview();
    ClearServers();
    ClearPromoted();
    ClearNews();

    filter_->UnInitialize(framework_);
    
    if (proxy_)
    {
        framework_->Ui()->RemoveWidgetFromScene(proxy_);
        proxy_ = 0;
    }
    if (leftActionButton_)
    {
        framework_->Ui()->GraphicsScene()->removeItem(leftActionButton_);
        SAFE_DELETE(leftActionButton_);
    }
    if (rightActionButton_)
    {
        framework_->Ui()->GraphicsScene()->removeItem(rightActionButton_);
        SAFE_DELETE(rightActionButton_);
    }
    if (loader_)
    {
        framework_->Ui()->GraphicsScene()->removeItem(loader_);
        SAFE_DELETE(loader_);
    }
}

void RocketLobby::Show(bool animate)
{
    if (state_ != Portal)
        state_ = Portal;

    CleanInputFocus();
    if (isVisible())
        return;

    ClearWebview();
    ClearServers();
    ClearPromoted();
    ClearNews();
    
    loader_->Hide();
    filter_->Hide();
    
    actEditProfile_->setVisible(true);
    actDirectLogin_->setVisible(true);

    show();

    if (animate)
    {
        anims_->Stop();
        anims_->SetDirection(QAbstractAnimation::Forward);
        anims_->scale->setStartValue(0.0);
        anims_->scale->setEndValue(1.0);
        anims_->pos->setStartValue(framework_->Ui()->GraphicsScene()->sceneRect().center());
        anims_->pos->setEndValue(QPointF(0.0, 0.0));
        anims_->Start();
    }
    else
    {
        setGeometry(framework_->Ui()->GraphicsScene()->sceneRect().toRect());
        if (proxy_)
        {
            proxy_->setPos(0,0);
            proxy_->setScale(1.0);
        }
    }
}

void RocketLobby::StartLoader(const QString &message, bool showLeft, bool showRight, bool showUserInformation)
{
    state_ = Loader;
    
    loader_->Show();
    loader_->ResetServer();
    loader_->SetMessage(message);
    loader_->StartAnimation();
    filter_->Hide();
    
    if (leftActionButton_)
    {
        leftActionButton_->SetButtonLook("");
        leftActionButton_->setVisible(showLeft);
    }
    if (rightActionButton_)
    {
        rightActionButton_->SetButtonLook(tr("Cancel Login"));
        rightActionButton_->setVisible(showRight);
    }
    if (showLeft && showRight)
        loader_->SetButtonCount(2);
    else
        loader_->SetButtonCount(1);
        
    // Only show the name and image of user when showUserInformation
    if (showUserInformation)
    {
        ui_.labelUsername->show();
        ui_.labelProfile->show();

        ui_.buttonLogout->setVisible(false);
        ui_.buttonEditSettings->setVisible(false);
        ui_.buttonEditProfile->setVisible(false);
        ui_.buttonEditAvatar->setVisible(false);
        ui_.buttonPresis->setVisible(false);
        ui_.buttonRefreshServers->setVisible(false);
        ui_.buttonExitRocket->setVisible(false);
    }

    if (ui_.labelUsername->text().trimmed().isEmpty())
        ui_.buttonPresis->setVisible(false);
            
    OnSceneResized(framework_->Ui()->GraphicsScene()->sceneRect());
}

RocketLoaderWidget *RocketLobby::GetLoader() const
{
    return loader_;
}

void RocketLobby::OnConnectionStateChanged(bool connectedToServer)
{
    if (actDisconnect_)
        actDisconnect_->setVisible(connectedToServer);
}

void RocketLobby::Hide(bool animate)
{
    if (!isVisible() || anims_->engine->state() == QAbstractAnimation::Running)
        return;

    if (animate)
    {
        anims_->engine->stop();
        anims_->engine->setDirection(QAbstractAnimation::Backward);
        anims_->scale->setEndValue(1.0);
        anims_->scale->setStartValue(1.0);
        anims_->pos->setEndValue(proxy_->pos());
        anims_->pos->setStartValue(proxy_->pos() - QPointF(width(), 0));
        anims_->engine->start();
    }
    else
        hide();

    loader_->Hide(true);
    filter_->Hide();
    leftActionButton_->Hide();
    rightActionButton_->Hide();
    actEditProfile_->setVisible(false);
    actDirectLogin_->setVisible(false);

    foreach(RocketServerWidget *server, serverWidgets_)
    {
        server->SetFocused(false);
        server->Hide();
    }
    foreach(RocketPromotionWidget *promoted, promotedWidgets_)
    {
        promoted->SetFocused(false);
        promoted->Hide();
    }
    
    emit PlaySoundRequest("minimize");
}

bool RocketLobby::IsLoaderVisible() const
{
    if (loader_)
        return loader_->isVisible();
    return false;
}

void RocketLobby::ShowCenterContent()
{
    state_ = Portal;
    
    if (NumServers() == 0)
        emit UpdateServersRequest();
    if (NumPromoted() == 0)
        emit UpdatePromotedRequest();
    if (NumServers() > 0 && NumPromoted() > 0)
        ApplyCurrentFilterAndSorting();

    actEditProfile_->setVisible(true);
    actDirectLogin_->setVisible(true);
    
    CleanInputFocus();
}

void RocketLobby::HideCenterContent()
{
    state_ = NoCenterContent;
    
    ClearWebview();
    if (filter_)
        filter_->Hide();
    if (loader_)
        loader_->Hide();

    if (leftActionButton_)
        leftActionButton_->Hide(RocketAnimations::AnimateFade);
    if (rightActionButton_)
        rightActionButton_->Hide(RocketAnimations::AnimateFade);

    foreach(RocketPromotionWidget *promoted, promotedWidgets_)
        promoted->Hide(RocketAnimations::AnimateFade);
    foreach(RocketServerWidget *server, serverWidgets_)
        server->Hide(RocketAnimations::AnimateFade);
}

QString RocketLobby::GetFocusedImagePath()
{
    RocketServerWidget *focused = GetFocusedServer();
    if (!focused && GetFocusedPromo())
        focused = GetServerForPromoted(GetFocusedPromo());
    
    QString imageUrl;
    if (focused)
    {
        if (!focused->data.txmlPictureUrl.isEmpty())
            imageUrl = focused->data.txmlPictureUrl;
        else if (!focused->data.companyPictureUrl.isEmpty())
            imageUrl = focused->data.companyPictureUrl;
    }
    return imageUrl.isEmpty() ? imageUrl : framework_->Asset()->GetAssetCache()->FindInCache(imageUrl);
}

Meshmoon::Server RocketLobby::GetFocusedServerInfo()
{
    RocketServerWidget *focused = GetFocusedServer();
    if (!focused && GetFocusedPromo())
        focused = GetServerForPromoted(GetFocusedPromo());

    if (focused)
        return focused->data;
    return Meshmoon::Server();
}

Meshmoon::ServerOnlineStats RocketLobby::GetFocusedServerOnlineStats()
{
    RocketServerWidget *focused = GetFocusedServer();
    if (!focused && GetFocusedPromo())
        focused = GetServerForPromoted(GetFocusedPromo());

    if (focused)
        return focused->stats;
    return Meshmoon::ServerOnlineStats();
}

void RocketLobby::EmitLogoutRequest()
{
    emit LogoutRequest();
}

void RocketLobby::CleanInputFocus()
{
    clearFocus();
    ui_.contentFrame->setFocus(Qt::MouseFocusReason);
    ui_.contentFrame->setFocus(Qt::TabFocusReason);
}

void RocketLobby::OnTransformAnimationsFinished(RocketAnimationEngine *engine, RocketAnimations::Direction direction)
{
    if (direction == RocketAnimations::Backward)
    {
        ClearWebview();
        ClearServers();
        ClearPromoted();
        ClearNews();
        hide();
    }
}

void RocketLobby::OnMeshmoonLogoPressed()
{
    QDesktopServices::openUrl(QUrl("http://www.meshmoon.com"));
}

void RocketLobby::OnKeyPressed(KeyEvent *event)
{
    if (!isVisible())
        return;

    // Ignore if console visible and has focus.
    const ConsoleWidget &console = *framework_->Console()->Widget();
    if (console.isVisible() && (console.LineEdit()->hasFocus() || console.TextEdit()->hasFocus()))
        return;

    // Promotion, server or login loader focused
    if (state_ == Server || state_ == Promotion || state_ == Loader)
    {
        if (event->keyCode == Qt::Key_Return || event->keyCode == Qt::Key_Enter)
        {
            LeftActionPressed();
            event->Suppress();
        }
        else if (event->keyCode == Qt::Key_Escape || event->keyCode == Qt::Key_Backspace)
        {
            RightActionPressed();
            event->Suppress();
        }
    }
    // Lobby portal focused
    else if (state_ == Portal)
    {
        if (event->keyCode == Qt::Key_F5)
        {
            UpdateServersRequest();
            event->Suppress();
        }
        else
        {
            const bool filterHasFocus = filter_ && filter_->isVisible() && filter_->HasFocus();
            if (!filterHasFocus)
            {
                switch(event->keyCode)
                {
                case Qt::Key_Tab:
                    UnselectCurrentServer();
                    filter_->FocusSearchBox();
                    event->Suppress();
                    break;
                case Qt::Key_Left:
                    SelectPreviousServer();
                    event->Suppress();
                    break;
                case Qt::Key_Right:
                    SelectNextServer();
                    event->Suppress();
                    break;
                case Qt::Key_Up:
                    SelectAboveServer();
                    break;
                case Qt::Key_Down:
                    SelectBelowServer();
                    event->Suppress();
                    break;
                case Qt::Key_Return:
                case Qt::Key_Enter:
                    {
                        RocketServerWidget *selectedWidget = GetSelectedServer();
                        if (selectedWidget)
                            FocusServer(selectedWidget);
                        else
                            SelectFirstServer();
                        event->Suppress();
                        break;
                    }
                case Qt::Key_Escape:
                    if (GetSelectedServer())
                    {
                        UnselectCurrentServer();
                        event->Suppress();
                        break;
                    }
                default:
                    if (ui_.contentFrame->hasFocus())
                    {
                        // Ignore function keys
                        if (event->keyCode >= Qt::Key_F1 && event->keyCode <= Qt::Key_F12)
                            return;

                        // Handle special keys
                        if (event->keyCode == Qt::Key_PageUp)
                        {
                            if (leftActionButton_ && !leftActionButton_->IsHiddenOrHiding())
                                LeftActionPressed();
                            event->Suppress();
                        }
                        else if (event->keyCode == Qt::Key_PageDown)
                        {
                            if (rightActionButton_ && !rightActionButton_->IsHiddenOrHiding())
                                RightActionPressed();
                            event->Suppress();
                        }
                        else if (event->keyCode == Qt::Key_Home)
                        {
                            OrderServers(true, true);
                            event->Suppress();
                        }

                        if (!event->handled && (event->modifiers == Qt::NoModifier || event->modifiers == Qt::ShiftModifier))
                        {
                            UnselectCurrentServer();
                            filter_->setFocus(Qt::TabFocusReason);
                            if (event->qtEvent)
                            {
                                event->qtEvent->setAccepted(false);
                                QApplication::sendEvent(filter_, event->qtEvent);
                            }
                            event->Suppress();
                        }
                    }
                    break;
                }
            }
        }
    }
}

void RocketLobby::OnDirectLoginPressed()
{
    OnDirectLoginDestroy();

    // New custom dialog
    dialogDirectConnect_ = new QDialog();
    dialogDirectConnect_->setAttribute(Qt::WA_DeleteOnClose, true);
    dialogDirectConnect_->setObjectName("directLoginWidget");
    dialogDirectConnect_->setWindowFlags(Qt::SplashScreen);
    dialogDirectConnect_->setWindowTitle(" ");
    dialogDirectConnect_->setWindowIcon(QIcon(":/images/icon-connect.png"));
    dialogDirectConnect_->setWindowModality(Qt::ApplicationModal);    
    dialogDirectConnect_->setStyleSheet("QWidget { font-family: \"Arial\"; font-size: 12px; } QWidget#directLoginWidget { border: 1px solid grey; background-color: \
        qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 rgba(232, 234, 233, 255), stop:1 rgba(245, 246, 247, 255)); } \
        QLabel#errorLabel { color: red; } QLabel { color: rgb(43,43,43); }");
    
    QVBoxLayout *mainVertical = new QVBoxLayout();    

    QLabel *titleLabel = new QLabel("Direct Login");
    titleLabel->setObjectName("directLoginTitle");
    titleLabel->setStyleSheet("QLabel#directLoginTitle { font-size: 19px; font-weight: bold; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainVertical->addWidget(titleLabel);

    // Grid with input fields
    QLabel *titleServer = new QLabel("Host", dialogDirectConnect_);
    QLabel *titleUsername = new QLabel("Username", dialogDirectConnect_);
    QLabel *titlePassword = new QLabel("Password", dialogDirectConnect_);

    QLineEdit *lineServer = new QLineEdit(dialogDirectConnect_);
    QLineEdit *lineUsername = new QLineEdit(dialogDirectConnect_);
    QLineEdit *linePassword = new QLineEdit(dialogDirectConnect_);

    lineServer->setObjectName("lineServer");
    lineServer->setText("localhost");
    lineUsername->setObjectName("lineUsername");
    linePassword->setObjectName("linePassword");
    linePassword->setEchoMode(QLineEdit::NoEcho);

    // If authenticated, help with the username field
    if (ui_.labelUsername->isVisible() && !ui_.labelUsername->text().isEmpty() && ui_.labelUsername->text() != "Authenticating User")
        lineUsername->setText(ui_.labelUsername->text());

    QGridLayout *grid = new QGridLayout();
    grid->addWidget(titleServer, 0, 0);
    grid->addWidget(lineServer, 0, 1);
    grid->addWidget(titleUsername, 1, 0);
    grid->addWidget(lineUsername, 1, 1);
    grid->addWidget(titlePassword, 2, 0);
    grid->addWidget(linePassword, 2, 1);

    // Radio buttons
    QRadioButton *checkTcp = new QRadioButton("TCP", dialogDirectConnect_);
    QRadioButton *checkUdp = new QRadioButton("UDP", dialogDirectConnect_);
    checkTcp->setAutoExclusive(true);
    checkUdp->setAutoExclusive(true);
    checkUdp->setChecked(true);
    checkUdp->setObjectName("checkUdp");
    checkTcp->setObjectName("checkTcp");

    QHBoxLayout *checkLayout = new QHBoxLayout();
    checkLayout->addWidget(checkUdp);
    checkLayout->addWidget(checkTcp);
    checkLayout->addSpacerItem(new QSpacerItem(1,1, QSizePolicy::Expanding));
    grid->addLayout(checkLayout, 3, 1);

    mainVertical->addLayout(grid);

    // Error label and buttons
    QLabel *errorLabel = new QLabel(dialogDirectConnect_);
    errorLabel->setObjectName("errorLabel");
    errorLabel->setAlignment(Qt::AlignBottom|Qt::AlignRight);
    errorLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainVertical->addWidget(errorLabel);

    QPushButton *buttonConnect = new QPushButton(tr("Connect"), dialogDirectConnect_);
    buttonConnect->setDefault(true);
    QPushButton *buttonCancel = new QPushButton(tr("Cancel"), dialogDirectConnect_);
    buttonCancel->setAutoDefault(false);

    connect(lineServer, SIGNAL(returnPressed()), SLOT(OnDirectLoginConnect()));
    connect(lineUsername, SIGNAL(returnPressed()), SLOT(OnDirectLoginConnect()));
    connect(linePassword, SIGNAL(returnPressed()), SLOT(OnDirectLoginConnect()));
    connect(buttonConnect, SIGNAL(clicked()), SLOT(OnDirectLoginConnect()));
    connect(buttonCancel, SIGNAL(clicked()), dialogDirectConnect_, SLOT(close()));
    connect(dialogDirectConnect_, SIGNAL(destroyed(QObject*)), plugin_->Notifications(), SLOT(ClearForeground()));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Horizontal);
    buttonBox->addButton(buttonConnect, QDialogButtonBox::ActionRole);
    buttonBox->addButton(buttonCancel, QDialogButtonBox::ActionRole);
    mainVertical->addWidget(buttonBox);

    dialogDirectConnect_->setLayout(mainVertical);
    dialogDirectConnect_->setMinimumWidth(400);
    dialogDirectConnect_->show();

    plugin_->Notifications()->CenterToMainWindow(dialogDirectConnect_);
    plugin_->Notifications()->DimForeground();
}

void RocketLobby::OnDirectLoginConnect()
{
    if (!dialogDirectConnect_)
    {
        LogError("OnDirectLoginConnect: Direct Login dialog null, cannot proceed!");
        return;
    }
    QLineEdit *lineServer = dialogDirectConnect_->findChild<QLineEdit*>("lineServer");
    QLineEdit *lineUsername = dialogDirectConnect_->findChild<QLineEdit*>("lineUsername");
    QLineEdit *linePassword = dialogDirectConnect_->findChild<QLineEdit*>("linePassword");
    QLabel *errorLabel = dialogDirectConnect_->findChild<QLabel*>("errorLabel");
    QRadioButton *checkTcp = dialogDirectConnect_->findChild<QRadioButton*>("checkTcp");
    if (!lineServer || !lineUsername || !errorLabel || !linePassword || !checkTcp)
    {
        LogError("OnDirectLoginConnect: Not all child widgets could be found, cannot proceed!");
        return;
    }
    
    int port = 2345;
    QString server = lineServer->text().trimmed();
    QString username = lineUsername->text();
    QString password = linePassword->text();
    QString protocol = checkTcp->isChecked() ? "tcp" : "udp";
    if (server.isEmpty() || server == ":")
    {
        errorLabel->setText("Host cannot be empty");
        return;  
    }
    if (server.count(":") > 1)
    {
        errorLabel->setText(tr("Syntax error: Multiple \":\" characters in host"));
        return;
    }
    if (server.count(":") == 1)
    {
        QStringList split = server.split(":");
        if (split.size() > 1)
        {
            server = split.at(0);
            port = split.at(1).toInt();
        }
        else
        {
            errorLabel->setText(tr("Syntax error: Failed to parse port from host"));
            return;
        }
    }    
    dialogDirectConnect_->close();
    
    ClearWebview();
    ClearServers();
    ClearPromoted();
    ClearNews();
    StartLoader(tr("Connecting to %1:%2").arg(server).arg(port));

    emit DirectLoginRequest(server, port, username, password, protocol);
}

void RocketLobby::OnDirectLoginDestroy()
{
    if (dialogDirectConnect_)
        SAFE_DELETE(dialogDirectConnect_);
}

void RocketLobby::OnLogoutPressed()
{
    if (state_ != Webpage)
        emit LogoutRequest();
}

void RocketLobby::OnEditProfilePressed()
{
    if (state_ != Webpage)
        emit EditProfileRequest();
}

void RocketLobby::UpdateLogin(const QString &message, bool stopLoadAnimation)
{
    if (loader_)
    {
        loader_->SetMessage(message);
        if (stopLoadAnimation)
            loader_->StopAnimation();
        else if (!loader_->IsAnimating())
            loader_->StartAnimation();
    }
}

void RocketLobby::OnVisibilityChange(bool visible)
{
    if (visible)
        OnSceneResized(framework_->Ui()->GraphicsScene()->sceneRect());
    else
    {
        leftActionButton_->Hide();
        rightActionButton_->Hide();
        loader_->Hide();
        
        foreach(RocketServerWidget *server, serverWidgets_)
            server->Hide();
        foreach(RocketPromotionWidget *promoted, promotedWidgets_)
            promoted->Hide();
    }
}

void RocketLobby::OnWebpageLoadStarted(QGraphicsWebView *webview, const QUrl &requestedUrl)
{
    ClearWebview();
    ClearServers();
    ClearPromoted();
    ClearNews();
    
    if (!webview)
    {
        LogError("OnAuthStarted: Web browser widget is null, cannot proceed!");
        return;
    }
        
    state_ = Webpage;
    webview_ = webview;
    
    if (webview_ && webview_->page() && webview_->page()->mainFrame())
    {        
        webview_->page()->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);
        webview_->page()->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
        
        QNetworkAccessManager *networkManager = webview_->page()->networkAccessManager();
        if (networkManager)
        {
            connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnWebviewReplyFinished(QNetworkReply*)), Qt::UniqueConnection);
            connect(networkManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError>&)), this, SLOT(OnWebviewSslErrors(QNetworkReply *, const QList<QSslError>&)), Qt::UniqueConnection);
        }
    }
    
    // Init loader if this is first time coming here.
    if (!loaderAnimation_)
    {
        loaderAnimation_ = new QMovie(":/images/loader.gif", "GIF", this);
        ui_.labelLoader->setMovie(loaderAnimation_);
        loaderAnimation_->setCacheMode(QMovie::CacheAll);
    }
    if (loaderAnimation_ && loaderAnimation_->isValid())
    {
        ui_.labelLoader->show();
        loaderAnimation_->start();
        
        connect(webview_, SIGNAL(loadStarted()), this, SLOT(OnWebviewLoadStarted()), Qt::UniqueConnection);
        connect(webview_, SIGNAL(loadFinished(bool)), this, SLOT(OnWebviewLoadCompleted(bool)), Qt::UniqueConnection);
    }
    else
        ui_.labelLoader->hide();
    
    framework_->Ui()->GraphicsScene()->addItem(webview_);
    webview_->hide();
    
    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    QSizeF size(rect.width() - ui_.rightFrame->width() , rect.height());
    QPointF bottomPos(0, rect.height());
    QPointF pos(0, 0);

    webview_->setPos(bottomPos);
    webview_->resize(size);
    
    QPropertyAnimation *anim = new QPropertyAnimation(webview_, "pos", webview_); 
    anim->setDuration(500);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(bottomPos);
    anim->setEndValue(pos);
    
    // Bit of a hack to go around first startup with auth screen visible scaling issues.
    QTimer::singleShot(500, this, SLOT(OnAuthShow()));
    QTimer::singleShot(505, anim, SLOT(start()));
    
    if (requestedUrl != Meshmoon::UrlEditProfile)
    {
        QFont font(ui_.labelUsername->font());
        font.setPointSize(16);
        font.setBold(true);
        ui_.labelUsername->setAlignment(Qt::AlignCenter);
        ui_.labelUsername->setFont(font);
        
        ui_.labelUsername->setText(tr("Authenticating User"));
        ui_.labelProfile->setVisible(false);
    }
    else
        ui_.labelProfile->setVisible(true);
    
    ui_.buttonLogout->setVisible(false);
    ui_.buttonEditSettings->setVisible(false);
    ui_.buttonEditProfile->setVisible(false);
    ui_.buttonEditAvatar->setVisible(false);
    ui_.buttonPresis->setVisible(false);
    ui_.buttonRefreshServers->setVisible(false);
    ui_.buttonExitRocket->setVisible(false);
    actEditProfile_->setVisible(false);
    
    // Hide all ui as browser is about to show.
    leftActionButton_->Hide();
    rightActionButton_->Hide();
    loader_->Hide();
    filter_->Hide();
}

void RocketLobby::OnAuthShow()
{
    if (webview_)
        webview_->show();
}

void RocketLobby::OnAuthenticated(const Meshmoon::User &user)
{
    state_ = Portal;

    ClearWebview();
    loader_->Hide();
    
    ui_.buttonLogout->setVisible(true);
    ui_.buttonEditSettings->setVisible(true);
    ui_.buttonEditProfile->setVisible(true);
    ui_.buttonEditAvatar->setVisible(true);
    ui_.buttonPresis->setVisible(true);
    ui_.buttonRefreshServers->setVisible(true);
#ifdef Q_OS_WIN
    // Only windows menubar is hidden in --ogrecapturetopwindow, OSX has it on top outside the window rect.
    ui_.buttonExitRocket->setVisible((framework_->Ui()->MainWindow() && framework_->Ui()->MainWindow()->menuBar() && !framework_->Ui()->MainWindow()->menuBar()->isVisible()));
#endif
    actEditProfile_->setVisible(true);
    actDirectLogin_->setVisible(true);
    
    SetUserInformation(user);
}

void RocketLobby::SetUserInformation(const Meshmoon::User &user)
{
    user_ = user;

    // Reduce font size if a long username. Kind of works as a hack...
    QFont font(ui_.labelUsername->font());
    font.setPixelSize(22);
    font.setBold(true);
    if (user.name.length() >= 13)
    {
        if (user.name.length() <= 15)
            font.setPixelSize(font.pixelSize() - 4);
        else if (user.name.length() <= 20)
            font.setPixelSize(font.pixelSize() - 6);
        else if (user.name.length() <= 23)
            font.setPixelSize(font.pixelSize() - 8);
        else if (user.name.length() <= 26)
            font.setPixelSize(font.pixelSize() - 10);
        else if (user.name.length() > 26)
            font.setPixelSize(font.pixelSize() - 14);
    }
    ui_.labelUsername->setFont(font);

    // Set username and show ui elements
    ui_.labelUsername->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    ui_.labelUsername->setText(user.name);

    if (!user.pictureUrl.isEmpty() && user.pictureUrl.isValid())
    {
        /// @todo This is a bug, we never refresh profile image if its in cache! 
        /// Make config that writes last fetched time and do it periodically, 
        /// or just always do the HEAD check via RequestAsset?
        QString diskSource = framework_->Asset()->Cache()->FindInCache(user.pictureUrl.toString());
        if (diskSource.isEmpty())
        {
            AssetTransferPtr transfer = framework_->Asset()->RequestAsset(user.pictureUrl.toString(), "Binary");
            connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnProfilePictureLoaded(AssetPtr)));
            connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(SetProfileImage()));
        }
        else
            SetProfileImage(diskSource);
    }
    else
        SetProfileImage();
}

void RocketLobby::OnProfilePictureLoaded(AssetPtr asset)
{
    SetProfileImage(asset ? asset->DiskSource() : "");
    if (asset) framework_->Asset()->ForgetAsset(asset->Name(), false);
}

void RocketLobby::SetProfileImage(const QString &path)
{
    ui_.labelProfile->setVisible(true);
    
    if (!path.isEmpty() && QFile::exists(path))
    {
        QPixmap loadedIcon(path);
        if (loadedIcon.width() < 50 || loadedIcon.height() < 50)
            loadedIcon = loadedIcon.scaled(QSize(50,50), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (!loadedIcon.isNull())
        {
            ui_.labelProfile->setPixmap(loadedIcon);
            return;
        }
    }
    
    // Fallback
    ui_.labelProfile->setPixmap(QPixmap(":/images/profile-anon.png"));
}

void RocketLobby::LeftActionPressed()
{
    if (state_ == Portal)
    {
        if (leftActionButton_->opacity() < 0.9)
            return;
        previousPage_ = currentPage_;
        currentPage_--;
        if (currentPage_ < 0)
            currentPage_ = 0;
        OrderServers(false, true);
    }
    else if (state_ == Server)
    {
        RocketServerWidget *server = GetFocusedServer();
        if (server)
        {
            if (leftActionButton_->GetButtonText().indexOf(tr("Login to")) == 0)
            {
                leftActionButton_->SetButtonLook("");
                rightActionButton_->SetButtonLook(tr("Cancel Login"));
                ConnectToServer(server);
                
                emit PlaySoundRequest("click2");
            }
        }
    }
    else if (state_ == Promotion)
    {
        RocketPromotionWidget *promoted = GetFocusedPromo();
        if (promoted)
        {
            if (leftActionButton_->GetButtonText().indexOf(tr("Login to")) == 0)
            {
                leftActionButton_->SetButtonLook("");
                rightActionButton_->SetButtonLook(tr("Cancel Login"));
                ConnectToServer(promoted);
                
                emit PlaySoundRequest("click2");
            }
        }
    }
}

void RocketLobby::RightActionPressed()
{
    if (state_ == Portal)
    {
        if (rightActionButton_->opacity() < 0.9)
            return;
        previousPage_ = currentPage_;
        currentPage_++;
        OrderServers(false, true);
    }
    else if (state_ == Server)
    {
        RocketServerWidget *server = GetFocusedServer();
        if (server)
        {
            if (rightActionButton_->GetButtonText() == tr("Back"))
                UnfocusServer(server);
            else
                CancelConnection(server);
        }
    }
    else if (state_ == Promotion)
    {
        RocketPromotionWidget *promoted = GetFocusedPromo();
        if (promoted)
        {
            if (rightActionButton_->GetButtonText() == tr("Back"))
                UnfocusPromoted(promoted);
            else
                CancelConnection(promoted);
        }
    }
    else if (state_ == Loader)
    {
        if (rightActionButton_->GetButtonText() == tr("Cancel Login"))
            emit CancelRequest(Meshmoon::Server());
    }
}

RocketServerWidget *RocketLobby::ServerById(const QString &id) const
{
    if (id.isEmpty())
        return 0;
    foreach(RocketServerWidget *server, serverWidgets_)
        if (server && server->data.txmlId.compare(id, Qt::CaseSensitive) == 0)
            return server;
    return 0;
}

RocketServerWidget *RocketLobby::GetFocusedServer() const
{
    foreach(RocketServerWidget *server, serverWidgets_)
        if (server && server->focused)
            return server;
    return 0;
}

RocketServerWidget *RocketLobby::GetSelectedServer() const
{
    foreach(RocketServerWidget *server, serverWidgets_)
        if (server && server->selected)
            return server;
    return 0;
}

RocketPromotionWidget *RocketLobby::GetFocusedPromo() const
{
    foreach(RocketPromotionWidget *promo, promotedWidgets_)
        if (promo && promo->focused)
            return promo;
    return 0;
}

void RocketLobby::OnWebviewLoadStarted()
{
    if (loaderAnimation_ && loaderAnimation_->state() != QMovie::Running)
        loaderAnimation_->start();
    if (!ui_.labelLoader->isVisible())
        ui_.labelLoader->show();
}

void RocketLobby::OnWebviewLoadCompleted(bool success)
{
    if (loaderAnimation_ && loaderAnimation_->state() != QMovie::NotRunning)
        loaderAnimation_->stop();
    if (ui_.labelLoader->isVisible())
        ui_.labelLoader->hide();
        
    if (!success && webview_ && webview_->page() && webview_->page()->mainFrame())
    {
        if (webview_->page()->mainFrame()->requestedUrl() == Meshmoon::UrlLogin)
        {
            if (plugin_->OfflineLobby())
                plugin_->OfflineLobby()->Show();
            if (ui_.labelUsername)
                ui_.labelUsername->setText("Offline");
        }
    }
}

void RocketLobby::OnWebviewReplyFinished(QNetworkReply *reply)
{
    if (reply && reply->url().toString().startsWith(Meshmoon::UrlLogin.toString()))
    {
        if (reply->error() == QNetworkReply::NoError)
        {
            if (plugin_->OfflineLobby())
                plugin_->OfflineLobby()->Hide();
        }
        else
            LogDebug("[Adminotech]: Load error \"" + reply->errorString() + "\" while loading " + reply->url().toString());
    }
}

void RocketLobby::OnWebviewSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
}

void RocketLobby::OnSettingsApplied(const ClientSettings *settings)
{
    if (!settings || !proxy_) 
        return;

    // Stats
    if (ui_.frameStats->isVisible() != settings->showHostingStats)
        ui_.frameStats->setVisible(settings->showHostingStats);

    // News
    if (showNewsOverride_ != settings->showHostingNews)
    {
        showNewsOverride_ = settings->showHostingNews;
        NewsTimeout();
    }
}

void RocketLobby::ClearWebview()
{
    if (webview_)
    {
        framework_->Ui()->GraphicsScene()->removeItem(webview_);
        
        // Targeted disconnects.
        disconnect(webview_, SIGNAL(loadStarted()), this, SLOT(OnWebviewLoadStarted()));
        disconnect(webview_, SIGNAL(loadFinished(bool)), this, SLOT(OnWebviewLoadCompleted(bool)));
        
        QNetworkAccessManager *networkManager = webview_->page() ? webview_->page()->networkAccessManager() : 0;
        if (networkManager)
        {
            disconnect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnWebviewReplyFinished(QNetworkReply*)));
            disconnect(networkManager, SIGNAL(sslErrors(QNetworkReply *, const QList<QSslError>&)), this, SLOT(OnWebviewSslErrors(QNetworkReply *, const QList<QSslError>&)));
        }
        
        // Generic disconnects to be double sure.
        disconnect(this, SLOT(OnWebviewLoadStarted()));
        disconnect(this, SLOT(OnWebviewLoadCompleted(bool)));
        disconnect(this, SLOT(OnWebviewReplyFinished(QNetworkReply*)));
        disconnect(this, SLOT(OnWebviewSslErrors(QNetworkReply *, const QList<QSslError>&)));
        
        // Null the ptr, we are not responsible for deleting this object, AdminotechBackend is.
        webview_ = 0;
    }
    OnWebviewLoadCompleted(true);
}

void RocketLobby::SetServers(ServerWidgetList servers, bool showServers)
{
    if (!framework_ || !framework_->Ui() || !framework_->Ui()->GraphicsScene())
    {
        LogError("RocketLobby: Cannot set servers, Framework/UI not ready.");
        return;
    }
    
    ClearServers();
    CleanInputFocus();

    state_ = Portal;
    
    // Add widgets to scene
    foreach(RocketServerWidget* server, servers)
    {
        if (!server)
        {
            LogWarning("RocketLobby: Found a null server widget, ignoring.");
            continue;
        }

        framework_->Ui()->GraphicsScene()->addItem(server);
        if (!isVisible() || !showServers)
            server->hide();
        
        connect(server, SIGNAL(FocusRequest(RocketServerWidget*)), SLOT(FocusServer(RocketServerWidget*)), Qt::QueuedConnection);
        connect(server, SIGNAL(UnfocusRequest(RocketServerWidget*)), SLOT(UnfocusServer(RocketServerWidget*)), Qt::QueuedConnection);
        connect(server, SIGNAL(ConnectRequest(RocketSceneWidget*)), SLOT(ConnectToServer(RocketSceneWidget*)), Qt::UniqueConnection);
        connect(server, SIGNAL(CancelRequest(RocketSceneWidget*)), SLOT(CancelConnection(RocketSceneWidget*)), Qt::UniqueConnection);
        connect(server, SIGNAL(HoverChange(RocketServerWidget*, bool)), SLOT(HoverChangeServer(RocketServerWidget*, bool)), Qt::UniqueConnection);
        
        serverWidgets_ << server;
    }
    servers.clear();
    
    ApplyServerScores();
    
    if (showServers)
        ApplyCurrentFilterAndSortingDelayed();
}

void RocketLobby::ApplyCurrentFilterAndSortingDelayed(int msec)
{
    if (filterTimer_.isActive())
        filterTimer_.stop();
    filterTimer_.start(msec);
}

void RocketLobby::ApplyCurrentFilterAndSorting()
{
    if (filterTimer_.isActive())
        filterTimer_.stop();

    // Apply current filter text to the new servers.
    if (!isVisible())
        return;

    if (filter_)
        filter_->Show();
    if (filter_ && filter_->IsTermValid())
        filter_->ApplyCurrentTerm();
    else
        OrderServers(true, false);        
}

void RocketLobby::SetPromoted(PromotedWidgetList promoted, bool showPromoted)
{
    if (!framework_ || !framework_->Ui() || !framework_->Ui()->GraphicsScene())
    {
        LogError("RocketLobby: Cannot set promoted, Framework/UI not ready.");
        return;
    }
    if (promoted.isEmpty())
        return;

    /** If all these widgets already exist don't do anything.
        This can be the case when startup cached promotions were shown
        and the data has not changed (a later 304 response from backend) */
    if (promotedWidgets_.size() > 0 && promotedWidgets_.size() == promoted.size())
    {
        bool allExist = true;
        foreach(RocketPromotionWidget* promo, promoted)
        {
            bool found = false;
            foreach(RocketPromotionWidget* existingPromo, promotedWidgets_)
            {
                if (existingPromo && existingPromo->data.Matches(promo->data))
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                allExist = false;
                break;
            }
        }
        if (allExist)
        {
            foreach(RocketPromotionWidget* promo, promoted)
                SAFE_DELETE_LATER(promo);
            return;
        }
    }

    ClearPromoted();
    state_ = Portal;

    // Add widgets to scene
    foreach(RocketPromotionWidget* promo, promoted)
    {
        if (!promo)
        {
            LogWarning("RocketLobby: Found a null promoted widget, ignoring.");
            continue;
        }

        framework_->Ui()->GraphicsScene()->addItem(promo);
        if (!isVisible() || !showPromoted)
            promo->hide();
            
        connect(promo, SIGNAL(Timeout()), SLOT(PromotedTimeout()), Qt::UniqueConnection);
        connect(promo, SIGNAL(Pressed(RocketPromotionWidget*)), SLOT(FocusPromoted(RocketPromotionWidget*)), Qt::QueuedConnection);
        promotedWidgets_ << promo;
    }
    promoted.clear();

    if (showPromoted)
        ApplyCurrentFilterAndSorting();
}

void RocketLobby::SetNews(Meshmoon::NewsList news)
{
    if (!isVisible())
        return;

    news_ = news;
    currentNewsIndex_ = -1;
    
    bool showNews = !news_.isEmpty();
    ui_.frameNews->setVisible(showNews);
    ui_.labelNewsTitle->setVisible(showNews);
    ui_.labelNewsTitle->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);
    ui_.labelNewsTitle->setOpenExternalLinks(true);
    ui_.labelNewsMessage->setVisible(showNews);
    ui_.labelNewsMessage->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);
    ui_.labelNewsMessage->setOpenExternalLinks(true);
    
    if (showNews)
        NewsTimeout();
}

void RocketLobby::ClearNews()
{
    news_.clear();
    if (newsTimer_.isActive())
        newsTimer_.stop();
    NewsTimeout();
}

void RocketLobby::NewsTimeout()
{       
    if (news_.isEmpty() || !showNewsOverride_)
    {
        ui_.frameNews->setVisible(false);
        ui_.labelNewsTitle->setVisible(false);
        ui_.labelNewsMessage->setVisible(false);
        return;
    }
        
    if (!ui_.frameNews->isVisible())
        ui_.frameNews->setVisible(true);
    if (!ui_.labelNewsTitle->isVisible())
        ui_.labelNewsTitle->setVisible(true);
    if (!ui_.labelNewsMessage->isVisible())
        ui_.labelNewsMessage->setVisible(true);
    
    currentNewsIndex_++;
    if (currentNewsIndex_ >= news_.size())
        currentNewsIndex_ = 0;
    
    Meshmoon::NewsItem nextNews = news_.at(currentNewsIndex_);
    ui_.labelNewsTitle->clear();
    ui_.labelNewsTitle->setTextFormat(Qt::RichText);
    ui_.labelNewsTitle->setText(nextNews.title);
    ui_.labelNewsMessage->clear();
    ui_.labelNewsMessage->setTextFormat(Qt::RichText);
    ui_.labelNewsMessage->setText(nextNews.message);
    
    QString styleSheet = nextNews.important ? "QLabel { color: rgb(210,0,0); }" : "QLabel { color: rgb(45,45,45); }";
    ui_.labelNewsTitle->setStyleSheet(styleSheet);
    ui_.labelNewsMessage->setStyleSheet(styleSheet);
    
    if (news_.size() > 1)
        newsTimer_.start(30000);
}

void RocketLobby::SetStats(const Meshmoon::Stats &stats)
{
    ui_.onlineUsersLabelValue->show();
    ui_.totalLoginsLabelValue->show();
    ui_.hostedScenesLabelValue->show();
    ui_.onlineUsersLabel->show();
    ui_.totalLoginsLabel->show();
    ui_.hostedScenesLabel->show();
    
    QString loginCountStr = (stats.uniqueLoginCount != 0 ? QString::number(stats.uniqueLoginCount) : "-");
    if (stats.uniqueLoginCount > 1000000)
        loginCountStr = loginCountStr.left(1) + " " + loginCountStr.mid(1,3) + " " + loginCountStr.mid(4);
    else if (stats.uniqueLoginCount > 100000)
        loginCountStr = loginCountStr.left(3) + " " + loginCountStr.mid(3);

    ui_.totalLoginsLabelValue->setText(loginCountStr);
    ui_.hostedScenesLabelValue->setText(stats.hostedSceneCount != 0 ? QString::number(stats.hostedSceneCount) : "-");
}

void RocketLobby::SetOnlineStats(const Meshmoon::OnlineStats &stats)
{
    /// @note The widget is still shown when stats arrive. It will have '0' until this function is called.
    ui_.onlineUsersLabelValue->setText(QString::number(stats.onlineUserCount));

    // Reset current stats, new data has landed.
    foreach(RocketServerWidget *server, serverWidgets_)
        if (server)
            server->ResetOnlineStats();

    // The servers referenced by this object are guaranteed to be known at this point.
    // Note that there might be id:s that we don't have permission to see, so we might not find each server.
    foreach(const Meshmoon::ServerOnlineStats &serverStats, stats.spaceUsers)
    {
        RocketServerWidget *server = ServerById(serverStats.id);
        if (server)
            server->SetOnlineStats(serverStats);
    }

    /** Execute filtering right away. The information flow order is:
            servers -> scores -> stats
        We don't want to unnecessarily shuffle the UI for each event.
        If everything goes smoothly the first two have delayed filtering
        and hopefully it did not have time yet to trigger. If this
        online stats takes a long time a server ordering happens once
        before and once now that the user data is here.
    */
    /** @todo Only do this if the first page of servers is activated. Works fine now,
        but if we start polling the online users ~every 60 seconds, we don't want to mess
        with the view if user is on another page (without filter terms) */
    ApplyCurrentFilterAndSorting();
}

void RocketLobby::SetScores(const Meshmoon::ServerScoreMap &scores)
{
    scores_ = scores;
    
    // Show the score sorting UI.
    filter_->SetSortCriteriaInterfaceVisible(true);
    
    ApplyServerScores();
    ApplyCurrentFilterAndSortingDelayed();
}

bool RocketLobby::HasServerScores() const
{
    return !scores_.isEmpty();
}

void RocketLobby::ApplyServerScores()
{
    if (!HasServerScores())
        return;
        
    foreach(RocketServerWidget *server, serverWidgets_)
    {
        if (server && scores_.contains(server->data.txmlId))
            server->score = scores_[server->data.txmlId];
    }
}

void RocketLobby::ClearServers()
{
    if (!framework_ || !framework_->Ui() || !framework_->Ui()->GraphicsScene())
    {
        LogError("RocketLobby: Cannot clear servers, Framework/UI not ready.");
        return;
    }
    
    foreach(RocketServerWidget *server, serverWidgets_)
    {
        if (server)
        {            
            framework_->Ui()->GraphicsScene()->removeItem(server);
            SAFE_DELETE(server);
        }
    }
    serverWidgets_.clear();
}

void RocketLobby::ClearPromoted()
{
    if (!framework_ || !framework_->Ui() || !framework_->Ui()->GraphicsScene())
    {
        LogError("RocketLobby: Cannot clear promoted, Framework/UI not ready.");
        return;
    }

    foreach(RocketPromotionWidget *promo, promotedWidgets_)
    {
        if (promo)
        {            
            framework_->Ui()->GraphicsScene()->removeItem(promo);
            SAFE_DELETE(promo);
        }
    }
    promotedWidgets_.clear();
}

void RocketLobby::OnSceneResized(const QRectF &rect)
{
    if (!isVisible())
        return;
    
    setGeometry(0, 0, rect.width(), rect.height());

    if (rocketLogo_)
    {
        int y = rect.height() - 100 - rocketLogo_->height();
        if (y < 0)
            rocketLogo_->hide();
        else
        {
            if (!rocketLogo_->isVisible())
                rocketLogo_->show();
            rocketLogo_->move(5, y);
        }
    }
    
    if (state_ == Portal)
        OrderServers();
    else if (state_ == Server)
        FocusServer();
    else if (state_ == Promotion)
        FocusPromoted();
    else if (state_ == Loader)
        FocusLoader();
    else if (state_ == Webpage && webview_ && webview_->isVisible())
    {
        webview_->setPos(ui_.contentFrame->pos());
        webview_->resize(ui_.contentFrame->size());
    }
    if (filter_ && filter_->isVisible())
        filter_->UpdateGeometry();
}

void RocketLobby::FocusServer(RocketServerWidget *server)
{
    if (!server)
        server = GetFocusedServer();
    if (!server)
    {
        LogWarning("Could not find server to focus into!");
        SetFreeText("");
        return;
    }
    if (state_ != Server)
    {
        state_ = Server;
        SetFreeText(server);
    }
        
    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    rect.setWidth(rect.width() - ui_.rightFrame->width());
    
    // Hide filter and promo items. If a matching promo item is found mark it down.
    foreach(RocketPromotionWidget *promotedIter, promotedWidgets_)
        promotedIter->Hide();
    RocketPromotionWidget *promoted = GetPromotedForServer(server);

    filter_->Hide();
    
    // Hide and unfocus other servers if needed.  
    foreach(RocketServerWidget *iter, serverWidgets_)
    {
        if (iter == server)
        {
            if (!iter->focused)
            {
                iter->setProperty("wasSelected", iter->selected);
                iter->SetFocused(true);
                iter->Hide();

                leftActionButton_->SetButtonLook(tr("Login to ") + iter->data.name);
                rightActionButton_->SetButtonLook(tr("Back"));

                // Hide so they will be animated to their place.
                leftActionButton_->Hide();
                rightActionButton_->Hide();
                
                if (promoted)
                {
                    if (promoted->promoting)
                        promoted->Show();
                    else
                    {
                        promoted->Show();
                        promoted->SetPromoting(false);
                    }
                    promoted->SetFocused(true);
                }
            }
                
            // Calculate render information
            int outerMargin = 9;

            qreal itemWidth = qMin<qreal>(960.0, rect.width() - (2*outerMargin));
            qreal itemHeight = iter->size().height();
            qreal x = qMax<qreal>(outerMargin, (rect.width()/2.0) - (itemWidth/2.0));
            qreal y = ui_.contentFrame->pos().y() + outerMargin;

            qreal buttonWidth = (itemWidth - outerMargin) / 2.0;
            QPointF buttonsPos(x, y + itemHeight + outerMargin);
            
            // Update focus animation
            iter->Animate(QSizeF(itemWidth, itemHeight), QPointF(x,y), -1.0, rect, RocketAnimations::AnimateDown);
            if (promoted)
            {
                promoted->Animate(QSizeF(itemWidth, qMin<qreal>(600.0, rect.height() - (outerMargin*2) - (itemHeight + outerMargin)*2)), 
                    buttonsPos + QPointF(0, itemHeight + outerMargin), -1.0, rect);
            }

            // Update action buttons
            leftActionButton_->Animate(QSizeF(buttonWidth, itemHeight), buttonsPos, -1.0, rect, RocketAnimations::AnimateDown);
            rightActionButton_->Animate(QSizeF(buttonWidth, itemHeight), buttonsPos + QPointF(buttonWidth + outerMargin, 0), -1.0, rect, RocketAnimations::AnimateDown);
            
            // Update loader widget
            if (!loader_->isVisible())
            {
                promoted ? loader_->ResetServer() : loader_->SetServer(iter->data);
                loader_->Show();
            }
            if (!promoted)
                loader_->SetGeometry(QSizeF(itemWidth, rect.height() - itemHeight - (3*outerMargin)), buttonsPos);
            else
                loader_->SetGeometry(QSizeF(itemWidth, qMin<qreal>((itemHeight + outerMargin) + 600.0, rect.height() - outerMargin*2 - (itemHeight + outerMargin))), buttonsPos);
        }
        else
        {
            iter->SetFocused(false);
            iter->Hide();
        }
    }
}

void RocketLobby::UnfocusServer(RocketServerWidget *server)
{
    state_ = Portal;
    if (server)
    {
        server->SetFocused(false);
        server->Hide();
    }
    foreach(RocketPromotionWidget *promoted, promotedWidgets_)
    {
        promoted->SetFocused(false);
        promoted->Hide();
    }
    
    bool wasSelected = server ? server->property("wasSelected").toBool() : false;

    filter_->Show();
    loader_->Hide();
    
    leftActionButton_->SetArrowLook();
    leftActionButton_->Hide();
    rightActionButton_->SetArrowLook();
    rightActionButton_->Hide();
    
    OrderServers();
    
    if (server && server->isVisible() && wasSelected)
        server->SetSelected(true);
    
    CleanInputFocus();
}

void RocketLobby::PromotedTimeout()
{    
    // Don't show the next promo if we are not showing the server listing.
    if (state_ != Portal)
    {
        foreach(RocketPromotionWidget *promoted, promotedWidgets_)
            promoted->Hide();
        return;
    }

    int iPromo = -1;
    for(int i=0; i<promotedWidgets_.size(); ++i)
    {
        if (promotedWidgets_[i]->promoting)
        {
            iPromo = i;
            break;
        }
    }
    if (iPromo == -1)
    {
        LogWarning("Could not find current promoted item, cannot change promo!");
        return;
    }
    QRectF geometry; 
    if (promotedWidgets_[iPromo])
    {
        // Hack to not rotate promoted if any menu item is visible. As the switch will hide the menu.
        if ((plugin_->Menu() && plugin_->Menu()->HasOpenMenus()) || filter_->HasOpenMenus())
        {
            promotedWidgets_[iPromo]->RestartTimer();
            return;
        }

        geometry = promotedWidgets_[iPromo]->geometry();
        promotedWidgets_[iPromo]->Hide(RocketAnimations::AnimateFade);
    }
    if (iPromo >= (promotedWidgets_.size()-1))
        iPromo = 0;
    else
        iPromo++;
    
    // Show the promo
    if (promotedWidgets_[iPromo])
    {
        if (!geometry.isNull())
            promotedWidgets_[iPromo]->SetGeometry(geometry.size(), geometry.topLeft());
        promotedWidgets_[iPromo]->Show();
    }
    
    // Inform the next promo about a pending show
    iPromo++;
    if (iPromo >= promotedWidgets_.size())
        iPromo = 0;
    if (promotedWidgets_[iPromo])
        promotedWidgets_[iPromo]->AboutToBeShown();
}

void RocketLobby::FocusPromoted(RocketPromotionWidget *promoted)
{
    if (!promoted)
        promoted = GetFocusedPromo();
    if (!promoted)
    {
        LogWarning("Could not find promoted to focus into!");
        SetFreeText("");
        return;
    }
    
    // Already animating to hide, don't act on the click
    if (promoted->IsHiddenOrHiding())
        return;
    
    // Web url promotion
    if (promoted->data.type == "web")
    {
        SetFreeText("");
        QDesktopServices::openUrl(promoted->data.webUrl);
        return;
    }
    // Rocket action
    if (promoted->data.type == "action")
    {
        SetFreeText("");
        if (plugin_->Menu() && plugin_->Menu()->SessionMenu())
        {
            foreach(QAction *act, plugin_->Menu()->SessionMenu()->actions())
            {
                if (act && act->text().trimmed().toLower() == promoted->data.actionText.trimmed().toLower())
                {
                    act->trigger();
                    break;
                }
            }
        }
        return;
    }
    
    if (state_ != Promotion)
    {
        state_ = Promotion;
        SetFreeText(GetServerForPromoted(promoted));
    }

    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    rect.setWidth(rect.width() - ui_.rightFrame->width());

    // Hide and unfocus other servers if needed.  
    foreach(RocketPromotionWidget *iter, promotedWidgets_)
    {
        if (iter == promoted)
        {
            if (!iter->focused)
            {
                iter->SetFocused(true);
                iter->SetPromoting(true);
                
                leftActionButton_->SetButtonLook(tr("Login to ") + iter->data.name);                
                rightActionButton_->SetButtonLook(tr("Back"));

                // Hide so they will be animated to their place.
                leftActionButton_->Hide();
                rightActionButton_->Hide();
            }

            // Calculate render information
            int outerMargin = 9;
            int x = outerMargin;
            int y = ui_.contentFrame->pos().y() + outerMargin;
            
            qreal promoWidth = qMin<qreal>(960.0, rect.width() - (2*outerMargin));
            qreal promoX = qMax<qreal>(x, (rect.width()/2.0) - (promoWidth/2.0));
            qreal promoY = y + 62 + outerMargin;

            QSizeF buttonSize((promoWidth-outerMargin)/2.0, 62);
            QPointF buttonsPos(promoX, y);

            // Update focus animation
            iter->Animate(QSizeF(promoWidth, qMin<qreal>(600.0, rect.height() - (outerMargin*2) - 62 - outerMargin)), QPointF(promoX, promoY), -1.0, rect);

            // Update action buttons
            leftActionButton_->Animate(buttonSize, buttonsPos, -1.0, rect, RocketAnimations::AnimateDown);
            rightActionButton_->Animate(buttonSize, buttonsPos + QPointF(buttonSize.width() + outerMargin, 0), -1.0, rect, RocketAnimations::AnimateDown);

            // Update loader widget
            if (!loader_->isVisible())
            {
                loader_->ResetServer();
                loader_->Show();
            }
            loader_->SetGeometry(QSizeF(promoWidth, qMin<qreal>(62 + outerMargin + 600.0, rect.height() - outerMargin*2)), QPointF(promoX, y));
        }
        else
        {
            iter->SetFocused(false);
            iter->Hide();
        }
    }

    // Hide filter and promo items
    foreach(RocketServerWidget *server, serverWidgets_)
    {
        if (server->focused)
            server->focused = false;
        server->Hide();
    }
    filter_->Hide();
}

void RocketLobby::UnfocusPromoted(RocketPromotionWidget *promoted)
{
    state_ = Portal;
    
    foreach(RocketPromotionWidget *promo, promotedWidgets_)
    {
        promo->SetFocused(false);
        if (promo != promoted)
            promo->Hide();
    }
    promoted->Show();
    
    filter_->Show();
    loader_->Hide();

    leftActionButton_->SetArrowLook();
    leftActionButton_->Hide();
    rightActionButton_->SetArrowLook();
    rightActionButton_->Hide();
    
    OrderServers();
}

void RocketLobby::FocusLoader()
{
    if (state_ != Loader)
        return;
    if (!loader_ || !leftActionButton_ || !rightActionButton_)
    {
        qDebug() << "FocusLoader: Loader or action buttons null, cannot continue!";
        return;
    }
        
    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    rect.setWidth(rect.width() - ui_.rightFrame->width());
    
    // Calculate render information
    int outerMargin = 9;

    qreal itemWidth = qMin<qreal>(960.0, rect.width() - (2*outerMargin));
    qreal itemHeight = 62.0;
    qreal x = qMax<qreal>(outerMargin, (rect.width()/2.0) - (itemWidth/2.0));
    qreal y = ui_.contentFrame->pos().y() + outerMargin;
    qreal buttonWidth = (itemWidth - outerMargin) / (leftActionButton_->isVisible() && rightActionButton_->isVisible() ? 2.0 : 1.0);
    
    // Update action buttons
    if (leftActionButton_->isVisible())
        leftActionButton_->Animate(QSizeF(buttonWidth, itemHeight), QPointF(x,y), -1.0, rect, RocketAnimations::AnimateDown);
    if (rightActionButton_->isVisible())
        rightActionButton_->Animate(QSizeF(buttonWidth, itemHeight), QPointF(x,y) + QPointF(buttonWidth + outerMargin, 0), -1.0, rect, RocketAnimations::AnimateDown);

    // Update loader widget
    if (!loader_->isVisible())
    {
        loader_->ResetServer();
        loader_->Show();
    }
    loader_->SetGeometry(QSizeF(itemWidth, rect.height() - itemHeight - (3*outerMargin)), QPointF(x,y));
}

void RocketLobby::ConnectToServer(RocketSceneWidget *server)
{
    if (!server)
        return;

    loader_->SetMessage(tr("Requesting server information..."));
    loader_->StartAnimation();
    
    RocketServerWidget *server1 = dynamic_cast<RocketServerWidget*>(server);
    if (server1)
    {
        if (GetPromotedForServer(server1))
            loader_->isPromoted = true;
        emit StartupRequest(server1->data);
    }
    else 
    {
        RocketPromotionWidget *server2 = dynamic_cast<RocketPromotionWidget*>(server);
        if (server2)
        {
            loader_->isPromoted = true;
            emit StartupRequest(server2->data.ToServer());
        }
    }
}

void RocketLobby::CancelConnection(RocketSceneWidget *server)
{
    if (!server)
        return;
        
    RocketServerWidget *server1 = dynamic_cast<RocketServerWidget*>(server);
    if (server1)
        emit CancelRequest(server1->data);
    else 
    {
        RocketPromotionWidget *server2 = dynamic_cast<RocketPromotionWidget*>(server);
        if (server2)
            emit CancelRequest(server2->data.ToServer());
    }
}

void RocketLobby::HoverChangeServer(RocketServerWidget *server, bool hovering)
{
    if (state_ == Portal)
        SetFreeText(hovering ? server : 0);
}

void RocketLobby::SetFreeText(RocketServerWidget *server)
{
    if (server && !server->stats.userNames.isEmpty())
    {
        SetFreeText(QString("<span style=\"color: black; font-size: 19px;\"><b>Users<b></span><br>") +
            "<span style=\"color: grey; font-size: 19px;\">" + server->stats.userNames.join("<br>") + "</span>");
    }
    else
        SetFreeText("");
}

void RocketLobby::SetFreeText(const QString &text)
{
    if (text.isEmpty())
    {
        if (ui_.labelFreeInfo->isVisible())
        {
            ui_.labelFreeInfo->setText(text);
            ui_.labelFreeInfo->hide();
        }
    }
    else
    {
        if (!ui_.labelFreeInfo->isVisible())
            ui_.labelFreeInfo->show();
        ui_.labelFreeInfo->setText(text);
    }
}

struct ServerImportanceComparer : public std::binary_function<Entity*, Entity*, bool>
{
    Meshmoon::User user;
    Meshmoon::ServerScore::SortCriteria sortCriterial;
    bool administratedWorldsFirst;
    bool activeWorldsFirst;
    bool mepWorldsFirst;

    ServerImportanceComparer(const Meshmoon::User &user_, Meshmoon::ServerScore::SortCriteria sortCriterial_, bool administratedWorldsFirst_,
                             bool mepWorldsFirst_, bool activeWorldsFirst_) :
        user(user_),
        sortCriterial(sortCriterial_),
        administratedWorldsFirst(administratedWorldsFirst_),
        mepWorldsFirst(mepWorldsFirst_),
        activeWorldsFirst(activeWorldsFirst_)
    {
    }

    inline bool operator()(const RocketServerWidget* s1, const RocketServerWidget* s2)
    {
        // Administrated by current user go first
        if (administratedWorldsFirst)
        {
            if (s1->data.isAdministratedByCurrentUser || s2->data.isAdministratedByCurrentUser)
            {
                // One is administrated, the other is not
                if (s1->data.isAdministratedByCurrentUser && !s2->data.isAdministratedByCurrentUser)
                    return true;
                else if (!s1->data.isAdministratedByCurrentUser && s2->data.isAdministratedByCurrentUser)
                    return false;
                // Both are administrated by the user, let fall the criteria sort
            }
        }

        // MEP that current user is part of go first
        if (mepWorldsFirst && !user.meps.isEmpty())
        {
            if ((s1->data.mep.IsValid() || s2->data.mep.IsValid()) && (user.IsInMep(s1->data.mep) || user.IsInMep(s2->data.mep)))
            {
                // One is MEP, the other is not
                if (s1->data.mep.IsValid() && !s2->data.mep.IsValid() && user.IsInMep(s1->data.mep))
                    return true;
                else if (!s1->data.mep.IsValid() && s2->data.mep.IsValid() && user.IsInMep(s2->data.mep))
                    return false;
                // Both are MEP by the user, let fall the criteria sort
            }
        }

        // Currently worlds with logged in user count always goes first to the sort.
        if (activeWorldsFirst)
        {
            if (s1->stats.userCount > 0 || s2->stats.userCount > 0)
            {
                if (s1->stats.userCount > s2->stats.userCount)
                    return true;
                else if (s1->stats.userCount == s2->stats.userCount)
                    return (s1->data.name.compare(s2->data.name, Qt::CaseInsensitive) < 0);
                return false;
            }
        }

        // Login count data is always available.
        if (sortCriterial == Meshmoon::ServerScore::TotalLoginCount)
            return CompareMember(s1->data.loginCount, s2->data.loginCount, &s1->data.name, &s2->data.name);
        else if (sortCriterial == Meshmoon::ServerScore::AlphabeticalByName)
            return (s1->data.name.compare(s2->data.name, Qt::CaseInsensitive) < 0);
        else if (sortCriterial == Meshmoon::ServerScore::AlphabeticalByAccount)
            return (s1->data.company.compare(s2->data.company, Qt::CaseInsensitive) < 0);

        // If both of the servers do not have score data, order alphabetically.
        // This is the case when two worlds that have zero all time logins are compared.
        if (!s1->score.HasData() && !s2->score.HasData())
            return CompareMember(0, 0, &s1->data.name, &s2->data.name);

        if (sortCriterial == Meshmoon::ServerScore::TopDay)
            return CompareMember(s1->score.DayCumulative(), s2->score.DayCumulative(), &s1->data.name, &s2->data.name);
        else if (sortCriterial == Meshmoon::ServerScore::TopWeek)
            return CompareMember(s1->score.WeekCumulative(), s2->score.WeekCumulative(), &s1->data.name, &s2->data.name); 
        else if (sortCriterial == Meshmoon::ServerScore::TopMonth)
            return CompareMember(s1->score.MonthCumulative(), s2->score.MonthCumulative(), &s1->data.name, &s2->data.name);
        else if (sortCriterial == Meshmoon::ServerScore::TopThreeMonth)
            return CompareMember(s1->score.ThreeMonthsCulumative(), s2->score.ThreeMonthsCulumative(), &s1->data.name, &s2->data.name);
        else if (sortCriterial == Meshmoon::ServerScore::TopSixMonth)
            return CompareMember(s1->score.SixMonthsCumulative(), s2->score.SixMonthsCumulative(), &s1->data.name, &s2->data.name);
        else if (sortCriterial == Meshmoon::ServerScore::TopYear)
            return CompareMember(s1->score.YearCumulative(), s2->score.YearCumulative(), &s1->data.name, &s2->data.name);
        else
            qDebug() << "ServerImportanceComparer: Unknown sort criteria " << sortCriterial << Meshmoon::ServerScore::SortCriteriaToString(sortCriterial);
        return false;
    }
    
    template<typename T>
    bool CompareMember(const T &m1, const T &m2, const QString *name1, const QString *name2) const
    {
        // Greater.
        if (m1 > m2)
            return true;
        // Equals, order alphabetically.
        else if (m1 == m2)
            return (name1->compare(*name2, Qt::CaseInsensitive) < 0);
        // Less.
        return false;
    }
};

void RocketLobby::OrderServers(bool resetPage, bool animateHide)
{   
    sortedVisibleServers_.clear();
    
    if (state_ != Portal)
        return;
    SetFreeText("");

    if (serverWidgets_.isEmpty())
    {
        if (ui_.labelCenterInfo->isVisible())
            ui_.labelCenterInfo->setVisible(false);
        return;
    }

    if (resetPage)
        currentPage_ = 0;

    int outerMargin = 9;
    int margin = 0;

    // Calculate render information
    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    rect.setWidth(rect.width() - ui_.rightFrame->width());
    qreal widthCheck = (960.0 + 2*outerMargin);
    bool buttonsFitOnSides = rect.width() > (widthCheck + (2*32) + (2*outerMargin));
    if (rect.width() > widthCheck)
    {
        QPointF fixupPos((rect.width() / 2.0) - (widthCheck / 2.0), 0);
        QSizeF fixupSize(widthCheck, rect.height());
        rect = QRectF(fixupPos, fixupSize);
    }
    QSizeF size = rect.size();

    int itemsPerWidth = size.toSize().width() / 300;
    if (itemsPerWidth < 1)
        itemsPerWidth = 1;

    int y = rect.y() + outerMargin;
    int x = rect.x() + outerMargin;

    // Find active promoted item.
    RocketPromotionWidget *promoted = 0;
    for(int ipSort=0; ipSort<promotedWidgets_.size(); ++ipSort)
    {
        RocketPromotionWidget *promoIter = promotedWidgets_[ipSort];
        if (promoIter)
        {
            // No item can have click focus if we are here.
            if (promoIter->focused)
                promoIter->SetFocused(false);
            if (!promoted && promoIter->promoting)
                promoted = promoIter;
        }
    }

    // No promoted selected yet, pick random index.
    if (!promoted && !promotedWidgets_.isEmpty())
    {
        LCG randomGenerator;
        int randomIndex = randomGenerator.Int(0, promotedWidgets_.size()-1);
        if (randomIndex < promotedWidgets_.size())
            promoted = promotedWidgets_[randomIndex];
        else
            promoted = promotedWidgets_.first();
    }
    if (promoted)
    {
        qreal promoWidth = qMin<qreal>(960.0, rect.width() - (2*outerMargin));
        qreal promoX = qMax<qreal>(x, (rect.width()/2.0) - (promoWidth/2.0));

        ui_.filterPusher->changeSize(1, 200 + outerMargin, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui_.contentFrame->layout()->update();

        promoted->SetGeometry(QSizeF(promoWidth, 200.0), QPointF(promoX, y));
        promoted->Show();

        y += 200.0 + outerMargin + 35 + outerMargin;
    }
    else
    {
        ui_.filterPusher->changeSize(1, 1, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui_.contentFrame->layout()->update();

        y += 35 + outerMargin;
    }

    int width = (size.width() - (outerMargin*2) - ((itemsPerWidth-1)*margin)) / itemsPerWidth;
    int height = size.height() - y - outerMargin;

    // Calculate how many lines we want before next/previous buttons if there are too many servers.
    int itemsPerHeight = height / 62;
    if (itemsPerHeight < 1)
        itemsPerHeight = 1;
    // If space is too narrow, make room for buttons on the bottom.
    if (!buttonsFitOnSides)
    {
        itemsPerHeight = (height - 32 - outerMargin*0.5) / 62;
        if (itemsPerHeight < 1)
            itemsPerHeight = 1;
    }
    if (height > 62)
        height = 62;

    // Remove servers with UI selections
    QVector<RocketServerWidget*> orderedServers;
    for(int iSort=0; iSort<serverWidgets_.size(); ++iSort)
    {
        RocketServerWidget *sorting = serverWidgets_[iSort];
        if (!sorting)
            continue;
        sorting->SetFocused(false);

        if (sorting->filtered)
        {
            sorting->Hide();
            continue;
        }
        if (!filter_->ShowPrivateWorlds() && !sorting->data.isPublic)
        {
            sorting->Hide();
            continue;
        }
        if (!filter_->ShowMaintenanceWorlds() && sorting->data.isUnderMaintainance)
        {
            sorting->Hide();
            continue;
        }
        orderedServers << sorting;
    }

    // Sort servers by admin status, importance with user and login count.
    ServerImportanceComparer comparer(user_, HasServerScores() ? filter_->CurrentSortCriteria() : Meshmoon::ServerScore::TotalLoginCount,
        filter_->FavorAdministratedWorlds(), filter_->FavorMEPWorlds(), filter_->FavorActiveWorlds());
    qSort(orderedServers.begin(), orderedServers.end(), comparer);

    // Show first page if no paging is needed.
    if ((orderedServers.size() <= itemsPerWidth * itemsPerHeight) && currentPage_ > 0)
        currentPage_ = 0;

    int fromIndex = currentPage_ == 0 ? 0 : currentPage_ * (itemsPerWidth * itemsPerHeight);
    int toIndex = fromIndex + itemsPerWidth * itemsPerHeight;

    if (orderedServers.isEmpty())
    {
        fromIndex = 0;
        toIndex = 0;
    }
    else
    {
        // Out of range, we are in a too high page count for our window size. Hop to the first page.
        if (fromIndex >= orderedServers.size())
        {
            previousPage_ = 1;
            currentPage_ = 0;
            OrderServers(false, true);
            return;
        }
        if (toIndex >= orderedServers.size())
            toIndex = orderedServers.size();
    }

    int itemStartY = y;
    itemsPerHeight = ((toIndex - fromIndex) / itemsPerWidth) + ((toIndex - fromIndex) % itemsPerWidth == 0 ? 0 : 1);

    // Render
    int indexTotal = 0;
    int indexVisible = 0;
    int indexRow = 1;
    int index = 0;
    for(int iIter=0; iIter<orderedServers.size(); ++iIter)
    {
        RocketServerWidget *server = orderedServers[iIter];
        if (!server)
            continue;
        server->SetFocused(false);

        // Check if the item is in out page
        indexTotal++;
        if (indexTotal <= fromIndex)
        {
            server->Hide(RocketAnimations::AnimateFade, animateHide ? rect : QRectF());
            continue;
        }
        else if (indexTotal > toIndex)
        {
            server->Hide(RocketAnimations::AnimateFade, animateHide ? rect : QRectF());
            continue;
        }
        indexVisible++;
        index++;
        
        // Store the visible sorted servers
        sortedVisibleServers_ << server;

        // Move to next vertical server position
        if (itemsPerWidth < index)
        {
            y += margin + height;
            x = rect.x() + outerMargin;
            
            indexRow++;
            index = 1;
        }

        // Animate to new size and position
        server->Animate(QSizeF(width, height), QPointF(x, y), -1.0, rect, RocketAnimations::AnimateFade);
        
        // Update grid position.
        server->UpdateGridPosition(indexRow, index, itemsPerHeight, itemsPerWidth, indexTotal == toIndex);
        
        // If the last line is not full, go back and adjust grip pos for stylesheets to match.
        if (indexTotal == toIndex && index < itemsPerWidth)
        {
            for(int goForward=1; index<itemsPerWidth; goForward++)
            {
                index++;
                
                int lookupIndex = iIter - itemsPerWidth + goForward;
                if (lookupIndex < 0 || lookupIndex >= orderedServers.size())
                    break;
                
                RocketServerWidget *backupServer = orderedServers[lookupIndex];
                if (backupServer && backupServer->isVisible())
                    backupServer->UpdateGridPosition(indexRow-1, index, itemsPerHeight-1, itemsPerWidth, index == itemsPerWidth);
            }
        }
        
        // Move to next horizontal server position
        x += margin + width;
    }
    y += outerMargin + height;
    x = rect.x() + outerMargin;

    // Update the next/previous buttons
    if (fromIndex > 0)
    {
        leftActionButton_->SetArrowLook();
        if (buttonsFitOnSides)
        {
            leftActionButton_->Animate(QSizeF(32, 32), QPointF(x - 32 - outerMargin, ((y - outerMargin - height - itemStartY) / 2.0) + itemStartY + 16),
                -1.0, rect, RocketAnimations::AnimateFade);
        }
        else
            leftActionButton_->Animate(QSizeF(32, 32), QPointF(x, y), -1.0, rect, RocketAnimations::AnimateFade);
    }
    else if (leftActionButton_->isVisible())
        leftActionButton_->Hide(RocketAnimations::AnimateFade, rect);
    if (toIndex < orderedServers.size())
    {
        int moveRight = width * itemsPerWidth;
        rightActionButton_->SetArrowLook();
        if (buttonsFitOnSides)
        {
            rightActionButton_->Animate(QSizeF(32, 32), QPointF(x + moveRight + outerMargin, ((y - outerMargin - height - itemStartY) / 2.0) + itemStartY + 16),
                -1.0, rect, RocketAnimations::AnimateFade);
        }
        else
            rightActionButton_->Animate(QSizeF(32, 32), QPointF(x + moveRight - 32, y), -1.0, rect, RocketAnimations::AnimateFade);
    }
    else if (rightActionButton_->isVisible())
        rightActionButton_->Hide(RocketAnimations::AnimateFade, rect);

    if (loader_->isVisible())
        loader_->Hide();

    // If everything is filtered out, show a message about it.
    if (orderedServers.isEmpty())
    {
        ui_.labelCenterInfo->setVisible(true);
        ui_.labelCenterInfo->setText(tr("No results with") +" \"" + filter_->GetTerm() + "\""); // todo: parse out the generated part when its there!
    }
    else if (ui_.labelCenterInfo->isVisible())
        ui_.labelCenterInfo->setVisible(false);
        
    currentItemsPerWidth_ = itemsPerWidth;
}

RocketServerWidget *RocketLobby::GetServerForPromoted(RocketPromotionWidget *promoted) const
{
    if (!promoted)
        return 0;

    foreach(RocketServerWidget *serverIter, serverWidgets_)
        if (serverIter && promoted->data.Equals(serverIter->data))
            return serverIter;
    return 0;
}

RocketPromotionWidget *RocketLobby::GetPromotedForServer(RocketServerWidget *server) const
{
    if (!server)
        return 0;

    foreach(RocketPromotionWidget *promotedIter, promotedWidgets_)
        if (promotedIter && promotedIter->data.Equals(server->data))
            return promotedIter;
    return 0;
}

int RocketLobby::VisibleServers() const
{
    int visible = 0;
    foreach(const RocketServerWidget *server, serverWidgets_)
    {
        if (server && !server->IsHiddenOrHiding())
            visible++;
    }
    return visible;
}

void RocketLobby::SelectFirstServer()
{
    if (!sortedVisibleServers_.isEmpty())
    {
        // Make sure the text filter is not focuced
        if (filter_ && filter_->HasFocus())
        {
            filter_->HideAndClearSuggestions();
            filter_->Unfocus();
        }
        SelectServer(sortedVisibleServers_.first());
    }
}

void RocketLobby::SelectNextServer()
{
    if (currentItemsPerWidth_ <= 0)
        return;

    for(int i=0; i<sortedVisibleServers_.size(); i++)
    {
        QPointer<RocketServerWidget> &server = sortedVisibleServers_[i];
        if (!server->selected)
            continue;
        UnselectServer(server);

        int currentLineOffset = i % currentItemsPerWidth_;
        if (currentLineOffset + 1 < currentItemsPerWidth_ && i+1 < sortedVisibleServers_.size())
            SelectServer(sortedVisibleServers_[i+1]);
        else
        {
            if (rightActionButton_ && !rightActionButton_->IsHiddenOrHiding())
            {
                RightActionPressed();
                i -= (currentItemsPerWidth_-1);
                SelectServer(i >= 0 && i < sortedVisibleServers_.size() ? sortedVisibleServers_[i] : sortedVisibleServers_.last());
            }
            else
                server->SetSelected(true);
        }
        return;
    }
    
    // None selected, grab the first one
    SelectFirstServer();
}

void RocketLobby::SelectPreviousServer()
{
    if (currentItemsPerWidth_ <= 0)
        return;

    for(int i=0; i<sortedVisibleServers_.size(); i++)
    {
        QPointer<RocketServerWidget> &server = sortedVisibleServers_[i];
        if (!server->selected)
            continue;
        UnselectServer(server);

        int indexLineOffset = i % currentItemsPerWidth_;
        if (indexLineOffset > 0 && i-1 >= 0)
            SelectServer(sortedVisibleServers_[i-1]);
        else
        {
            if (leftActionButton_ && !leftActionButton_->IsHiddenOrHiding())
            {
                LeftActionPressed();
                i += (currentItemsPerWidth_-1);
                SelectServer(i < sortedVisibleServers_.size() ? sortedVisibleServers_[i] : sortedVisibleServers_.last());
            }
            else
                server->SetSelected(true);
        }
        return;
    }
    
    // None selected, grab the first one
    SelectFirstServer();
}

void RocketLobby::SelectAboveServer()
{
    if (currentItemsPerWidth_ <= 0)
        return;
        
    for(int i=0; i<sortedVisibleServers_.size(); i++)
    {
        QPointer<RocketServerWidget> &server = sortedVisibleServers_[i];
        if (!server->selected)
            continue;

        i -= currentItemsPerWidth_; // to loop: if (i<0) i = sortedVisibleServers_.size() + i;
        if (i >= 0)
        {
            UnselectServer(server);
            SelectServer(sortedVisibleServers_[i]);
        }
        return;
    }

    // None selected, grab the first one
    SelectFirstServer();
}

void RocketLobby::SelectBelowServer()
{
    if (currentItemsPerWidth_ <= 0)
        return;
        
    for(int i=0; i<sortedVisibleServers_.size(); i++)
    {
        QPointer<RocketServerWidget> &server = sortedVisibleServers_[i];
        if (!server->selected)
            continue;

        i += currentItemsPerWidth_; // to loop: sortedVisibleServers_[(i+currentItemsPerWidth_) % sortedVisibleServers_.size()]
        if (i < sortedVisibleServers_.size())
        {
            UnselectServer(server);
            SelectServer(sortedVisibleServers_[i]);
        }
        return;
    }

    // None selected, grab the first one
    SelectFirstServer();
}

void RocketLobby::UnselectCurrentServer()
{
    foreach(RocketServerWidget *server, serverWidgets_)
        if (server && server->selected)
            UnselectServer(server);
}

void RocketLobby::SelectServer(RocketServerWidget *server)
{
    if (state_ == Portal && server)
        server->SetSelected(true);
}

void RocketLobby::UnselectServer(RocketServerWidget *server)
{
    if (state_ == Portal && server)
        server->SetSelected(false);
}

void RocketLobby::FocusFirstServer()
{
    if (sortedVisibleServers_.isEmpty())
        return;
    QPointer<RocketServerWidget> first = sortedVisibleServers_.first();
    if (!first.isNull())
    {
        first->setFocus(Qt::MouseFocusReason);
        FocusServer(first);
    }
}
