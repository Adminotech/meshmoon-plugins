/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MeshmoonBackend.h"
#include "RocketPlugin.h"
#include "RocketLobbyWidget.h"
#include "MeshmoonHttpPlugin.h"
#include "MeshmoonHttpClient.h"
#include "MeshmoonHttpRequest.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "CoreJsonUtils.h"
#include "LoggingFunctions.h"
#include "ConfigAPI.h"
#include "AssetAPI.h"
#include "AssetCache.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QGraphicsWebView>
#include <QWebPage>
#include <QWebFrame>

#include <QVariant>
#include <QVariantMap>
#include <QTimer>

#include <kNet/Network.h>

#include "MemoryLeakCheck.h"

static void LogNotImplemented(QString function, QString message = "")
{
    QString out = "MeshmoonBackend::" + function + " not implemented";
    if (!message.isEmpty())
        out += ": " + message;
    LogError(out);
}

MeshmoonBackend::MeshmoonBackend(RocketPlugin *plugin) :
    LC("[MeshmoonBackend]: "),
    QObject(0),
    plugin_(plugin),
    webview_(0),
    authObject_(0),
    debug_(plugin->GetFramework()->HasCommandLineParameter("--rocketDebugNetworking"))
{   
    authObject_ = new RocketAuthObject(this);

    MeshmoonHttpPlugin *httpPlugin = plugin_->GetFramework()->Module<MeshmoonHttpPlugin>();
    if (httpPlugin)
        client_ = weak_ptr<MeshmoonHttpClient>(httpPlugin->Client());
    if (client_.expired())
        LogError(LC + "MeshmoonHttpPlugin not loaded. This will disable all Meshmoon backend communication!");

    cacheDir_ = QDir::fromNativeSeparators(Application::UserDataDirectory() + "assetcache/meshmoon/backend");
    if (!cacheDir_.exists())
        cacheDir_.mkpath(cacheDir_.absolutePath());
}

MeshmoonBackend::~MeshmoonBackend()
{
    SAFE_DELETE(webview_);
}

QString MeshmoonBackend::CacheDirectory() const
{
    return GuaranteeTrailingSlash(cacheDir_.absolutePath());
}

void MeshmoonBackend::ClearCache()
{
    // Meshmoon backend cache
    QDir cacheDir(CacheDirectory());
    QFileInfoList cacheFiles = cacheDir.entryInfoList();  // Using flags here would be nice but does not seem to work
    foreach(QFileInfo fileInfo, cacheFiles)
    {
        if (fileInfo.isFile() && fileInfo.exists())
            QFile::remove(fileInfo.absoluteFilePath());
    }
}

bool MeshmoonBackend::ProcessLoginUrl(QString loginUrl)
{
    LogNotImplemented("ProcessLoginUrl");
    return false;
}

void MeshmoonBackend::Authenticate()
{
    LogNotImplemented("Authenticate");
}

bool MeshmoonBackend::IsAuthenticated()
{
    LogNotImplemented("IsAuthenticated");
    return false;
}

void MeshmoonBackend::Unauthenticate()
{
    LogNotImplemented("Unauthenticate");
}

void MeshmoonBackend::OpenLogin(QUrl url)
{
    LogNotImplemented("OpenLogin");
}

void MeshmoonBackend::OpenEditProfile()
{
    LogNotImplemented("OpenEditProfile");
}

void MeshmoonBackend::Authenticated(const QString &hash)
{
    LogNotImplemented("Authenticated");
}

void MeshmoonBackend::AuthenticatedAndLogin(const QString &userhash, const QString &username, const QString &profileImageUrl, const QString &avatarUrl, const QString &sceneId)
{
    LogNotImplemented("AuthenticatedAndLogin");
}

void MeshmoonBackend::AuthenticatedAndFilter(const QString &userhash, const QString &username, const QString &profileImageUrl, const QString &avatarUrl)
{
    LogNotImplemented("AuthenticatedAndFilter");
}

void MeshmoonBackend::AddChainedRequest(MeshmoonHttpRequestPtr request, const QUrl &postUrl, bool sendUserHash)
{
    LogNotImplemented("AddChainedRequest");
}

void MeshmoonBackend::ExecuteChainedRequests(MeshmoonHttpRequest *request)
{
    LogNotImplemented("ExecuteChainedRequests");
}

void MeshmoonBackend::RequestBootData()
{
    LogNotImplemented("RequestBootData");
}

bool MeshmoonBackend::RequestPromoted()
{
    LogNotImplemented("RequestPromoted");
    return false;
}

void MeshmoonBackend::LoadCachedPromotions()
{
    LogNotImplemented("LoadCachedPromotions");
}

bool MeshmoonBackend::RequestServers(bool chainUserRequest)
{
    LogNotImplemented("RequestServers");
    return false;
}

bool MeshmoonBackend::RequestUsers()
{
    LogNotImplemented("RequestUsers");
    return false;
}

bool MeshmoonBackend::RequestScores()
{
    LogNotImplemented("RequestScores");
    return false;
}

bool MeshmoonBackend::RequestNews()
{
    LogNotImplemented("RequestNews");
    return false;
}

bool MeshmoonBackend::RequestStats()
{
    LogNotImplemented("RequestStats");
    return false;
}

bool MeshmoonBackend::ReadCache(const QString &sourceFile, MeshmoonHttpRequest *request)
{
    LogNotImplemented("ReadCache");
    return false;
}

QByteArray MeshmoonBackend::ReadCacheData(const QString &sourceFile)
{
    LogNotImplemented("ReadCacheData");
    return QByteArray();
}

void MeshmoonBackend::InvalidateCache(const QString &sourceFile, bool refetch)
{
    LogNotImplemented("InvalidateCache");
}

bool MeshmoonBackend::WriteCache(const QString &destFile, MeshmoonHttpRequest *request, const QByteArray &data)
{
    LogNotImplemented("WriteCache");
    return false;
}

MeshmoonHttpRequestPtr MeshmoonBackend::Get(const QUrl &url, bool sendUserHash, const Meshmoon::HeaderMap &headers)
{
    LogNotImplemented("Get");
    return MeshmoonHttpRequestPtr();
}

bool MeshmoonBackend::Post(const QUrl &url, const QByteArray &data, const QByteArray &contentType, bool sendUserHash, const Meshmoon::HeaderMap &headers)
{
    LogNotImplemented("Post");
    return false;
}

bool MeshmoonBackend::Put(const QUrl &url, const QByteArray &data, const QByteArray &contentType, bool sendUserHash, const Meshmoon::HeaderMap &headers)
{
    LogNotImplemented("Put");
    return false;
}

bool MeshmoonBackend::RequestStartup(const Meshmoon::Server &server)
{
    LogNotImplemented("RequestStartup");
    return false;
}

bool MeshmoonBackend::RequestStartup(const QString &sceneId)
{
    LogNotImplemented("RequestStartup");
    return false;
}

void MeshmoonBackend::OnRequestFinished(MeshmoonHttpRequest *request, int statusCode, const QString &error)
{
    LogNotImplemented("OnRequestFinished");
}

void MeshmoonBackend::CreateWebView()
{
    LogNotImplemented("CreateWebView");
}

void MeshmoonBackend::ResetWebView()
{
    LogNotImplemented("ResetWebView");
}

void MeshmoonBackend::AddAuthObject()
{
    LogNotImplemented("AddAuthObject");
}

void MeshmoonBackend::OnRequestSslErrors(MeshmoonHttpRequest *request, const QList<QSslError> &errors)
{
    LogNotImplemented("OnRequestSslErrors");
}

// RocketLauncher

RocketLauncher::RocketLauncher()
{
    Reset();
}

bool RocketLauncher::DeserializeFrom(const QByteArray &data)
{
    return false;
}

void RocketLauncher::Reset(const QString &_userhash, const QString &_id)
{
    type = Unknown;
    userhash = _userhash;
    id = _id;
    sceneIds.clear();
    continueToWebpage = false;
}

bool RocketLauncher::Matches(QNetworkReply *reply)
{
    return false;
}

QUrl RocketLauncher::VerifyUrl(const QString &username) const
{
    return QUrl();
}

QUrl RocketLauncher::AuthUrl() const
{
    return QUrl();
}
