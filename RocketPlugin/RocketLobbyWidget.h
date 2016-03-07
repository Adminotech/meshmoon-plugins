/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "utils/RocketAnimationsFwd.h"
#include "UiFwd.h"
#include "AssetFwd.h"
#include "InputFwd.h"
#include "MeshmoonData.h"

#include "ui_RocketLobbyWidget.h"

#include <QTimer>
#include <QPointer>
#include <QSslError>
#include <QPointer>

class QMovie;
class QMessageBox;

// RocketLobby

class RocketLobby : public QWidget
{
Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketLobby(RocketPlugin *plugin);
    virtual ~RocketLobby();
    
    friend class RocketPlugin;

    /// @endcond

    enum InterfaceState
    {
        Portal,
        Webpage,
        Server,
        Promotion,
        Loader,
        NoCenterContent
    };
    
    InterfaceState State() const { return state_; }

    void StartLoader(const QString &message = QString(), bool showLeft = true, bool showRight = true, bool showUserInformation = false);
    RocketLoaderWidget *GetLoader() const;

    void SetServers(ServerWidgetList servers, bool showServers = true);
    void SetPromoted(PromotedWidgetList promoted, bool showPromoted = true);
    void SetNews(Meshmoon::NewsList news);
    void SetStats(const Meshmoon::Stats &stats);
    void SetOnlineStats(const Meshmoon::OnlineStats &stats);
    void SetScores(const Meshmoon::ServerScoreMap &scores);
    void SetUserInformation(const Meshmoon::User &user);
    
    void UpdateLogin(const QString &message, bool stopLoadAnimation = false);

    void EmitLogoutRequest();

public slots:
    void Show(bool animate = true);
    void Hide(bool animate = true);

    void ShowCenterContent();
    void HideCenterContent();

    void OrderServers(bool resetPage = false, bool animateHide = false);

    void OnWebpageLoadStarted(QGraphicsWebView *webview, const QUrl &requestedUrl);
    void OnAuthShow();
    
    bool IsLoaderVisible() const;
    bool HasServerScores() const;

    int VisibleServers() const;

    ServerWidgetList Servers() { return serverWidgets_; }
    PromotedWidgetList Promoted() { return promotedWidgets_; }
    const PromotedWidgetList &PromotedRef() { return promotedWidgets_; }
    Framework *GetFramework() const { return framework_; }

    int NumServers() const { return serverWidgets_.size(); }
    int NumPromoted() const { return promotedWidgets_.size(); }

    void SelectFirstServer();
    void SelectNextServer();
    void SelectPreviousServer();
    void SelectAboveServer();
    void SelectBelowServer();
    void UnselectCurrentServer();

    void FocusFirstServer();

    RocketServerWidget *ServerById(const QString &id) const;
    RocketServerWidget *GetFocusedServer() const;
    RocketServerWidget *GetSelectedServer() const;
    RocketServerWidget *GetServerForPromoted(RocketPromotionWidget *promoted) const;
    RocketPromotionWidget *GetFocusedPromo() const;
    RocketPromotionWidget *GetPromotedForServer(RocketServerWidget *server) const;

    RocketLobbyActionButton *LeftActionButton() const { return leftActionButton_; }
    RocketLobbyActionButton *RightActionButton() const { return rightActionButton_; }

    QString GetFocusedImagePath();
    Meshmoon::Server GetFocusedServerInfo();
    Meshmoon::ServerOnlineStats GetFocusedServerOnlineStats();

    void CleanInputFocus();
    
private slots:
    void OnAuthenticated(const Meshmoon::User &user);
    void OnConnectionStateChanged(bool connectedToServer);
    void OnVisibilityChange(bool visible);
    void OnProfilePictureLoaded(AssetPtr asset);
    void OnSceneResized(const QRectF &rect);
    void OnTransformAnimationsFinished(RocketAnimationEngine *engine, RocketAnimations::Direction direction);
    void OnMeshmoonLogoPressed();
    void OnKeyPressed(KeyEvent *event);
    
    void OnDirectLoginPressed();
    void OnDirectLoginConnect();
    void OnDirectLoginDestroy();
    
    void OnLogoutPressed();
    void OnEditProfilePressed();

    void SetProfileImage(const QString &path = QString());
    
    void LeftActionPressed();
    void RightActionPressed();
    
    void ClearServers();
    void ClearPromoted();
    void ClearWebview();
    void ClearNews();

    void ApplyServerScores();
    void ApplyCurrentFilterAndSortingDelayed(int msec = 200);
    void ApplyCurrentFilterAndSorting();
    
    void FocusServer(RocketServerWidget *server = 0);
    void UnfocusServer(RocketServerWidget *server);
    
    void SelectServer(RocketServerWidget *server);
    void UnselectServer(RocketServerWidget *server);
    
    void PromotedTimeout();
    void NewsTimeout();
    
    void FocusPromoted(RocketPromotionWidget *promoted = 0);
    void UnfocusPromoted(RocketPromotionWidget *server);
    
    void FocusLoader();
    
    void ConnectToServer(RocketSceneWidget *server);
    void CancelConnection(RocketSceneWidget *server);
    void HoverChangeServer(RocketServerWidget *server, bool hovering);
    
    void SetFreeText(RocketServerWidget *server);
    void SetFreeText(const QString &text);
    
    void OnWebviewLoadStarted();
    void OnWebviewLoadCompleted(bool success);
    void OnWebviewReplyFinished(QNetworkReply *reply);
    void OnWebviewSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    
    void OnSettingsApplied(const ClientSettings *settings);

signals:
    void DisconnectRequest();
    void LogoutRequest();
    void UpdateServersRequest();
    void UpdatePromotedRequest();
    void EditSettingsRequest();
    void OpenAvatarEditorRequest();
    void OpenPresisRequest();
    void DirectLoginRequest(const QString &host, unsigned short port, const QString &username, const QString &password, const QString &protocol);
    void EditProfileRequest();
    void PlaySoundRequest(const QString &soundName);
    
    void StartupRequest(const Meshmoon::Server &server);
    void CancelRequest(const Meshmoon::Server &server);
    
protected:
    friend class RocketServerFilter;
    Ui::RocketLobbyWidget ui_;

private:
    Framework *framework_;
    RocketPlugin *plugin_;
    
    UiProxyWidget *proxy_;
    RocketServerFilter *filter_;
    QPointer<QGraphicsWebView> webview_;
    QLabel *rocketLogo_;
    
    Meshmoon::User user_;
    Meshmoon::NewsList news_;
    Meshmoon::ServerScoreMap scores_;
    
    RocketAnimationEngine *anims_;
    
    ServerWidgetList serverWidgets_;
    PromotedWidgetList promotedWidgets_;

    QTimer newsTimer_;
    QTimer filterTimer_;
    
    RocketLoaderWidget *loader_;
    RocketLobbyActionButton *leftActionButton_;
    RocketLobbyActionButton *rightActionButton_;

    int currentPage_;
    int previousPage_;
    int currentItemsPerWidth_;
    int currentNewsIndex_;
    
    bool showNewsOverride_;
    
    InterfaceState state_;
    
    QMovie *loaderAnimation_;
    
    QAction *actDirectLogin_;
    QAction *actDisconnect_;
    QAction *actEditProfile_;
    
    QPointer<QDialog> dialogDirectConnect_;
    QList<QPointer<RocketServerWidget> > sortedVisibleServers_;
};
