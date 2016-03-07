/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */
    
#include "StableHeaders.h"

#include "MeshmoonHttpClient.h"
#include "MeshmoonHttpRequest.h"

#include "Framework.h"
#include "Application.h"

MeshmoonHttpClient::MeshmoonHttpClient(Framework *framework) :
    LC("[MeshmoonHttpClient]: "),
    framework_(framework),
    http_(new QNetworkAccessManager(this)),
    maxOngoingRequests_(8)
{
    connect(http_, SIGNAL(finished(QNetworkReply*)), this, SLOT(OnFinished(QNetworkReply*)));
    connect(http_, SIGNAL(authenticationRequired(QNetworkReply*, QAuthenticator*)), 
        this, SLOT(OnAuthenticationRequired(QNetworkReply*, QAuthenticator*)));
    connect(http_, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)), 
        this, SLOT(OnSslErrors(QNetworkReply*, const QList<QSslError>&)));
}

MeshmoonHttpClient::~MeshmoonHttpClient()
{
}

int MeshmoonHttpClient::MaxOngoingRequests() const
{
    return maxOngoingRequests_;
}

void MeshmoonHttpClient::SetMaxOngoingRequests(int maxOngoingRequests)
{
    if (maxOngoingRequests < 1)
        maxOngoingRequests = 1;
    maxOngoingRequests_ = maxOngoingRequests;
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Get(const QString &url)
{
    return GetRaw(MAKE_SHARED(MeshmoonHttpRequest, url));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::GetRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodGet, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Head(const QString &url)
{
    return HeadRaw(MAKE_SHARED(MeshmoonHttpRequest, url));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::HeadRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodHead, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Options(const QString &url)
{
    return OptionsRaw(MAKE_SHARED(MeshmoonHttpRequest, url));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::OptionsRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodOptions, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Post(const QString &url, const QByteArray &body, const QString &contentType)
{
    return PostRaw(MAKE_SHARED(MeshmoonHttpRequest, url, body, contentType));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::PostRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodPost, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Put(const QString &url, const QByteArray &body, const QString &contentType)
{
    return PutRaw(MAKE_SHARED(MeshmoonHttpRequest, url, body, contentType));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::PutRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodPut, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Patch(const QString &url, const QByteArray &body, const QString &contentType)
{
    return PatchRaw(MAKE_SHARED(MeshmoonHttpRequest, url, body, contentType));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::PatchRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodPatch, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Delete(const QString &url)
{
    return DeleteRaw(MAKE_SHARED(MeshmoonHttpRequest, url));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::DeleteRaw(MeshmoonHttpRequestPtr request)
{
    return Add(MeshmoonHttpRequest::MethodDelete, request);
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::Add(MeshmoonHttpRequest::Method method, MeshmoonHttpRequestPtr request)
{
    request->method = method;
    pending_ << request;

    /** We want a delay here so all data can be filled into 
        the request before execution, eg. headers, url manipulations. */
    ExecuteNext(1);
    return request;
}

void MeshmoonHttpClient::ExecuteNext(int msecDelay)
{
    if (pending_.isEmpty())
        return;

    if (msecDelay <= 0)
    {
        if (ongoing_.size() < MaxOngoingRequests())
        {
            MeshmoonHttpRequestPtr request = pending_.takeFirst();
            request->AboutToBeExecuted();
            switch(request->method)
            {
                case MeshmoonHttpRequest::MethodGet:
                {
                    request->reply = http_->get(request->request);
                    break;
                }
                case MeshmoonHttpRequest::MethodPost:
                {
                    request->reply = http_->post(request->request, request->RequestBodyDevice());
                    break;
                }
                case MeshmoonHttpRequest::MethodPut:
                {
                    request->reply = http_->put(request->request, request->RequestBodyDevice());
                    break;
                }
                case MeshmoonHttpRequest::MethodDelete:
                {
                    request->reply = http_->deleteResource(request->request);
                    break;
                }
                case MeshmoonHttpRequest::MethodHead:
                {
                    request->reply = http_->head(request->request);
                    break;
                }
                case MeshmoonHttpRequest::MethodOptions:
                case MeshmoonHttpRequest::MethodPatch:
                {
                    request->reply = http_->sendCustomRequest(request->request, request->MethodString().toAscii(), request->RequestBodyDevice());
                    break;
                }
                default:
                {
                    LogDebug(LC + QString("Not supported %1 %2").arg(request->MethodString()).arg(request->UrlString()));
                    break;
                }
            }
            if (request->reply)
            {
                ongoing_ << request;
                if (IsLogChannelEnabled(LogChannelDebug))
                    LogDebug(LC + QString("Executing %1 %2").arg(request->MethodString()).arg(request->UrlString()));
            }
            else
                request.reset();

            ExecuteNext();
        }
    }
    else
        QTimer::singleShot(msecDelay, this, SLOT(ExecuteNext()));
}

void MeshmoonHttpClient::OnFinished(QNetworkReply *reply)
{
    for(int i=0, len=ongoing_.size(); i<len; ++i)
    {
        MeshmoonHttpRequestPtr request = ongoing_[i];
        if (request->reply == reply)
        {
            ongoing_.removeAt(i);

            // Handle auto redirects. All of these should return the 'Location' header to the new resource URL.
            int status = request->ResponseStatusCode();
            if (request->AutoExecuteLocationRedirects() && request->IsRedirectStatusCode())
            {
                QString redirectUrl = request->ResponseHeader("Location");
                if (!redirectUrl.isEmpty())
                {
                    if (IsLogChannelEnabled(LogChannelDebug))
                    {
                        LogDebug(LC + QString("Auto executing location redirect %1 %2 from %3 to %4")
                            .arg(status).arg(request->ResponseStatusCodeString())
                            .arg(request->UrlString()).arg(redirectUrl));
                    }

                    // Set new request URL
                    request->SetUrl(QUrl(redirectUrl));
                    request->reply->deleteLater();
                    request->reply = 0;

                    // Reset body to same content (underlying QIODevice* reset)
                    QByteArray requestBody = request->requestBody;
                    if (!requestBody.isEmpty())
                        request->SetRequestBody(requestBody);

                    Add(request->method, request);
                    return;
                }
                else
                    LogWarning("");
            }

            if (IsLogChannelEnabled(LogChannelDebug))
            {
                LogDebug(LC + QString("Completed %1 %2 %3 %4").arg(status).arg(request->ResponseStatusCodeString())
                    .arg(request->MethodString()).arg(request->UrlString()));
            }

            // Emit completion and release our shared ptr. Memory will be released once all refs are gone.
            // Usually there is no need to keep response once processed but it is possible that this is desired.
            request->EmitFinished();
            request.reset();
            break;
        }
    }
    ExecuteNext();
}

void MeshmoonHttpClient::OnAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    MeshmoonHttpRequestPtr request = OngoingRequestForReply(reply);
    if (request)
    {
        emit AuthenticationRequired(request, authenticator);
        request->EmitAuthenticationRequired(authenticator);
    }
    else
        LogError(LC + QString("OnAuthenticationRequired: Failed to find request for Qt reply: %1").arg(reply->url().toString()));
}

void MeshmoonHttpClient::OnSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    MeshmoonHttpRequestPtr request = OngoingRequestForReply(reply);
    if (request)
    {
        emit SslErrors(request, errors);
        request->EmitSslErrors(errors);
    }
    else
        LogError(LC + QString("OnSslErrors: Failed to find request for Qt reply: %1").arg(reply->url().toString()));
}

MeshmoonHttpRequestPtr MeshmoonHttpClient::OngoingRequestForReply(QNetworkReply *reply) const
{
    for(int i=0, len=ongoing_.size(); i<len; ++i)
    {
        MeshmoonHttpRequestPtr request = ongoing_[i];
        if (request.get() && request->reply == reply)
            return request;
    }
    return MeshmoonHttpRequestPtr();
}