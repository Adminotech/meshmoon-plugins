/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "MeshmoonHttpPluginFwd.h"
#include "common/MeshmoonCommon.h"
#include "MeshmoonData.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QDir>
#include <QUrl>
#include <QPointer>
#include <QGraphicsWebView>
#include <QNetworkRequest>
#include <QSslError>

/// @cond PRIVATE
struct RocketLauncher
{
    enum Type
    {
        Unknown = 0,
        DirectLogin,
        DirectLoginAnonymous,
        Filter,
        FilterAnonymous
    };

    RocketLauncher();
    
    bool DeserializeFrom(const QByteArray &data);
    void Reset(const QString &_userhash = "", const QString &_id = "");
    bool Matches(QNetworkReply *reply);
    
    QUrl VerifyUrl(const QString &username = "") const;
    QUrl AuthUrl() const;
    
    Type type;
    QString userhash;
    QString id;
    QStringList sceneIds;
    
    bool continueToWebpage;
};

// MeshmoonBackend

class MeshmoonBackend : public QObject
{
Q_OBJECT

public:
    friend class RocketAuthObject;

    explicit MeshmoonBackend(RocketPlugin *plugin);
    virtual ~MeshmoonBackend();
    
    Meshmoon::User user;
    RocketLauncher launcher;
    
    bool IsAuthenticated();
    QString CacheDirectory() const;

public slots:
    void Authenticate();
    void Unauthenticate();
    
    bool ProcessLoginUrl(QString loginUrl);
    
    void OpenLogin(QUrl url = QUrl());
    void OpenEditProfile();
    
    MeshmoonHttpRequestPtr Get(const QUrl &url, bool sendUserHash = true, const Meshmoon::HeaderMap &headers = Meshmoon::HeaderMap());
    bool Post(const QUrl &url, const QByteArray &data, const QByteArray &contentType = "text/plain", bool sendUserHash = true, const Meshmoon::HeaderMap &headers = Meshmoon::HeaderMap());
    bool Put(const QUrl &url, const QByteArray &data, const QByteArray &contentType = "text/plain", bool sendUserHash = true, const Meshmoon::HeaderMap &headers = Meshmoon::HeaderMap());
    
    void RequestBootData();

    bool RequestServers(bool requestUsers = true);
    bool RequestUsers();
    bool RequestScores();
    
    bool RequestPromoted();
    bool RequestNews();
    bool RequestStats();

    bool RequestStartup(const Meshmoon::Server &server);
    bool RequestStartup(const QString &sceneId);

signals:
    void WebpageLoadStarted(QGraphicsWebView *webview, const QUrl &requestedUrl);
    void AuthCompleted(const Meshmoon::User &user);
    void AuthReset();
    void Response(const QUrl &url, const QByteArray &data, int httpStatusCode = 200, const QString &error = QString());
    void FilterRequest(QStringList sceneIds);
    void LoaderMessage(const QString &message);
    void ExtraLoginQuery(Meshmoon::EncodedQueryItems queryItems);

    /// @cond PRIVATE
    void AuthReseted(); /**< @deprecated Use AuthReset instead. */
    /// @endcond

private slots:   
    void OnRequestFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error);
    void OnRequestSslErrors(MeshmoonHttpRequest *request, const QList<QSslError> &errors);
    
    void CreateWebView();
    void ResetWebView();
    void AddAuthObject();

    bool ReadCache(const QString &sourceFile, MeshmoonHttpRequest *request);
    QByteArray ReadCacheData(const QString &sourceFile);
    bool WriteCache(const QString &destFile, MeshmoonHttpRequest *request, const QByteArray &data);
    void InvalidateCache(const QString &sourceFile, bool refetch);

    void LoadCachedPromotions();
    
    void ClearCache();

protected:
    void Authenticated(const QString &hash);
    void AuthenticatedAndLogin(const QString &userhash, const QString &username, const QString &profileImageUrl, const QString &avatarUrl, const QString &sceneId);
    void AuthenticatedAndFilter(const QString &userhash, const QString &username, const QString &profileImageUrl, const QString &avatarUrl);
    
    void AddChainedRequest(MeshmoonHttpRequestPtr request, const QUrl &postUrl, bool sendUserHash);
    void ExecuteChainedRequests(MeshmoonHttpRequest *request);

private:
    RocketPlugin *plugin_;
    
    weak_ptr<MeshmoonHttpClient> client_;
    QPointer<QGraphicsWebView> webview_;
    RocketAuthObject *authObject_;

    bool debug_;
    QHash<QUrl, QTime> timers_;
    QHash<QUrl, QString> cacheResponses_;
    
    QString LC;
    QDir cacheDir_;
};

// RocketAuthObject

class RocketAuthObject : public QObject
{

Q_OBJECT

public:
    RocketAuthObject(MeshmoonBackend *backend) : 
        QObject(backend),
        backend_(backend)
    {
    }
    
public slots:
    void Authenticated(bool successfull, QString userhash, QString username, QString profileImageUrl)
    {
        if (backend_ && successfull)
            backend_->Authenticated(userhash);
    }
    
    void AuthenticatedAndLogin(bool successfull, QString userhash, QString username, QString profileImageUrl, QString avatarUrl, QString sceneId)
    {
        if (backend_ && successfull)
            backend_->AuthenticatedAndLogin(userhash, username, profileImageUrl, avatarUrl, sceneId);
    }
    
    void AuthenticatedAndFilter(bool successfull, QString userhash, QString username, QString profileImageUrl, QString avatarUrl)
    {
        if (backend_ && successfull)
            backend_->AuthenticatedAndFilter(userhash, username, profileImageUrl, avatarUrl);
    }
    
private:
    MeshmoonBackend *backend_;
};

/// @endcond
