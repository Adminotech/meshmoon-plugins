/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */
    
#include "StableHeaders.h"
#include "MeshmoonHttpRequest.h"

#include "CoreJsonUtils.h"
#include "Application.h"

#include <QFile>

MeshmoonHttpRequest::MeshmoonHttpRequest(const QString &url, const QByteArray &requestBody, const QString &contentType) :
    request(QUrl(url, QUrl::TolerantMode)),
    reply(0),
    requestBodyDevice_(0),
    method(MethodUnknown),
    autoExecuteLocationRedirects_(true)
{
    ValidateHttpScheme();

    SetRequestBody(requestBody, contentType);
    SetUserAgent(Application::FullIdentifier());
}

MeshmoonHttpRequest::~MeshmoonHttpRequest()
{
    ClearRequestBodyDevice();

    if (reply)
        reply->deleteLater();
}

void MeshmoonHttpRequest::EmitFinished()
{
    if (reply)
        emit Finished(this, ResponseStatusCode(), ResponseError());
    else
        emit Finished(this, -1, "Internal HTTP client error");

    ClearRequestBodyDevice();
}

void MeshmoonHttpRequest::EmitAuthenticationRequired(QAuthenticator *authenticator)
{
    emit AuthenticationRequired(this, authenticator);
}

void MeshmoonHttpRequest::EmitSslErrors(const QList<QSslError> &errors)
{
    emit SslErrors(this, errors);
}

QByteArray MeshmoonHttpRequest::RequestBody()
{
    return requestBody;
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetRequestBody(const QByteArray &body)
{
    return SetRequestBody(body, "");
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetRequestBody(const QByteArray &body, const QString &contentType)
{
    ClearRequestBodyDevice();

    requestBody = body;
    if (!contentType.isEmpty())
        SetRequestContentType(contentType);
    else if (RequestContentType().isEmpty())
        SetRequestContentType("application/octet-stream");
    return this;
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetRequestBodyJSON(const QVariant &obj)
{
    bool ok = false;
    QByteArray json = TundraJson::Serialize(obj, TundraJson::IndentNone, &ok);
    if (ok)
    {
        if (!json.isEmpty())
            SetRequestBody(json, "application/json");
        else
            LogError("MeshmoonHttpRequest::SetRequestBodyJSON: Object is zero bytes of data after serializing to JSON string.");
    }
    else
        LogError("MeshmoonHttpRequest::SetRequestBodyJSON: Failed to serialize given object to JSON string.");
    return this;
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetRequestBodyFromFile(const QString &filepath, const QString &contentType)
{
    QFile file(filepath);
    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            QByteArray fileContents = file.readAll();
            file.close();

            if (!fileContents.isEmpty())
            {
                /** @note This print is here for transparency, so totally silent uploads cant be done from scripts
                    by trying for files randomly eg. C:\Windows\System32\important.file without users authorization.
                    We need a security mechanism here and we need to shutdown AssetAPI::UploadAssetFromFile for the same reason. */
                LogInfo(QString("[MeshmoonHttpRequest]: Read request %1 body content from %2 (%3 bytes)")
                    .arg(UrlString()).arg(file.fileName()).arg(requestBody.size()));

                SetRequestBody(fileContents, contentType);
            }
            else
                LogError(QString("MeshmoonHttpRequest::SetRequestBodyFromFile: File %1 was empty").arg(filepath));
        }
        else
            LogError(QString("MeshmoonHttpRequest::SetRequestBodyFromFile: File %1 could not be opened for reading").arg(filepath));
    }
    else
        LogError(QString("MeshmoonHttpRequest::SetRequestBodyFromFile: File %1 does not exist").arg(filepath));
   return this;
}

QString MeshmoonHttpRequest::RequestContentType()
{
    QVariant h = request.header(QNetworkRequest::ContentTypeHeader);
    return (h.isValid() ? h.toString() : "");
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetRequestContentType(const QString &contentType)
{
    return SetRequestHeader(MeshmoonHttp::HeaderContentType, contentType);
}

QString MeshmoonHttpRequest::ResponseBodyString()
{
    return ResponseBodyBytes();
}

QByteArray MeshmoonHttpRequest::ResponseBodyBytes()
{
    // Reads JIT body data once from the reply.
    if (responseBody.isEmpty() && reply)
        responseBody = reply->readAll();
    return responseBody;
}

QVariant MeshmoonHttpRequest::ResponseJSON()
{
    return TundraJson::Parse(ResponseBodyBytes());
}

QDomDocument MeshmoonHttpRequest::ResponseXML()
{
    QDomDocument doc;
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(ResponseBodyBytes(), &errorMsg, &errorLine, &errorColumn))
    {
        LogError(QString("MeshmoonHttpRequest::ResponseXML: Parsing response bytes as XML failed: %1 at line %2 column %3.")
            .arg(errorMsg).arg(errorLine).arg(errorColumn));
        return QDomDocument();
    }
    return doc;
}

QString MeshmoonHttpRequest::ResponseStatus() const
{
    int statusCode = ResponseStatusCode();
    return QString("%1 %2").arg(statusCode).arg(StatusCodeString(statusCode));
}

int MeshmoonHttpRequest::ResponseStatusCode() const
{
    if (reply)
        return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    return 0;
}

QString MeshmoonHttpRequest::ResponseStatusCodeString() const
{
    return StatusCodeString(ResponseStatusCode());
}

QString MeshmoonHttpRequest::StatusCodeString(int status)
{
    switch(status)
    {
        case 0:         return "ErrorReadingStatusCodeFromReply";

        // 1xx
        case 100:       return "Continue";
        case 101:       return "Switching Protocols";
        case 102:       return "Processing";

        // 2xx
        case 200:       return "OK";
        case 201:       return "Created";
        case 202:       return "Accepted";
        case 203:       return "Non-Authoritative Information";
        case 204:       return "No Content";
        case 205:       return "Reset Content";
        case 206:       return "Partial Content";
        case 207:       return "Multi-Status";
        case 208:       return "Already Reported";
        case 226:       return "IM Used";

        // 3xx
        case 300:       return "Multiple Choices";
        case 301:       return "Moved Permanently";
        case 302:       return "Found";
        case 303:       return "See Other";
        case 304:       return "Not Modified";
        case 305:       return "Use Proxy";
        case 306:       return "Switch Proxy";
        case 307:       return "Temporary Redirect";
        case 308:       return "Permanent Redirect";

        // 4xx
        case 400:       return "Bad Request";
        case 401:       return "Unauthorized";
        case 402:       return "Payment Required";
        case 403:       return "Forbidden";
        case 404:       return "Not Found";
        case 405:       return "Method Not Allowed";
        case 406:       return "Not Acceptable";
        case 407:       return "Proxy Authentication Required";
        case 408:       return "Request Timeout";
        case 409:       return "Conflict";
        case 410:       return "Gone";
        // ...
        case 424:       return "Method Failure"; // WebDAV
        case 440:       return "Login Timeout";  // Microsoft
        case 444:       return "No Response";    // Nginx
        case 449:       return "Retry With";     // Microsoft
        case 451:       return "Redirect";       // Microsoft
        case 494:       return "Request Header Too Large"; // Nginx
        // continues to 499

        // 5xx
        case 500:       return "Internal Server Error";
        case 501:       return "Not Implemented";
        case 502:       return "Bad Gateway";
        case 503:       return "Service Unavailable";
        case 504:       return "Gateway Timeout";
        case 505:       return "HTTP Version Not Supported";
        case 506:       return "Variant Also Negotiates";
        case 507:       return "Insufficient Storage";
        case 508:       return "Loop Detected";
        case 509:       return "Bandwidth Limit Exceeded";
        case 510:       return "Not Extended";
    }
    return "StatusCodeStringNotRecognized";
}

QString MeshmoonHttpRequest::ResponseError()
{
    if (reply && reply->error() != QNetworkReply::NoError)
        return reply->errorString();
    return "";
}

QStringList MeshmoonHttpRequest::ResponseHeaderNames() const
{
    if (!reply)
        return QStringList();

    QStringList names;
    foreach(const QByteArray &header, reply->rawHeaderList())
        names << header;
    return names;
}

bool MeshmoonHttpRequest::HasResponseHeader(const QString &name) const
{
    if (reply)
        return reply->hasRawHeader(name.toUtf8());
    return false;
}

QNetworkReply *MeshmoonHttpRequest::Response() const
{
    return reply;
}

QNetworkReply::NetworkError MeshmoonHttpRequest::ResponseErrorEnum() const
{
    if (reply)
        return reply->error();
    return QNetworkReply::NoError;
}

QUrl MeshmoonHttpRequest::ResponseUrl() const
{
    if (reply)
        return reply->url();
    return QUrl();
}

QString MeshmoonHttpRequest::ResponseUrlString(QUrl::FormattingOption options) const
{
    if (reply)
        return reply->url().toString(options);
    return "";
}

void MeshmoonHttpRequest::IgnoreSslErrors()
{
    if (reply)
        reply->ignoreSslErrors();
    else
        LogError("MeshmoonHttpRequest::IgnoreSslErrors: Cannot ignore SSL errors. Reply is not in a valid state.");
}

QString MeshmoonHttpRequest::ResponseHeader(const QString &name) const
{
    if (!reply)
        return "";

    if (HasResponseHeader(name))
        return reply->rawHeader(name.toUtf8());
    else if (HasResponseHeader(name.toLower()))
        return reply->rawHeader(name.toLower().toUtf8());
    return "";
}

void MeshmoonHttpRequest::ValidateHttpScheme()
{
    if (request.url().scheme().isEmpty())
        SetScheme("http");
}

QString MeshmoonHttpRequest::MethodString() const
{
    return MethodToString(method);
}

QString MeshmoonHttpRequest::MethodToString(Method method)
{
    switch(method)
    {
        case MethodGet:     return "GET";
        case MethodHead:    return "HEAD";
        case MethodOptions: return "OPTIONS";
        case MethodPut:     return "PUT";
        case MethodPost:    return "POST";
        case MethodPatch:   return "PATCH";
        case MethodDelete:  return "DELETE";
        default:            return "";
    }
}

QUrl MeshmoonHttpRequest::Url() const
{
    return request.url();
}

QString MeshmoonHttpRequest::UrlString(QUrl::FormattingOption options) const
{
    return Url().toString(options);
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetUrl(const QUrl &url)
{
    request.setUrl(url);
    ValidateHttpScheme();
    return this;
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetUrlFromUserInput(const QString &url)
{
    return SetUrl(QUrl::fromUserInput(url));
}

int MeshmoonHttpRequest::Port() const
{
    return request.url().port();
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetPort(int port)
{
    QUrl url = request.url();
    url.setPort(port);
    SetUrl(url);
    return this;
}

QString MeshmoonHttpRequest::Scheme() const
{
    return request.url().scheme();
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetScheme(const QString &scheme)
{
    QUrl url = request.url();
    url.setScheme(scheme);
    SetUrl(url);
    return this;
}

QString MeshmoonHttpRequest::Host() const
{
    return request.url().host();
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetHost(const QString &host)
{
    QUrl url = request.url();
    url.setHost(host);
    SetUrl(url);
    return this;
}

QString MeshmoonHttpRequest::Path() const
{
    return request.url().path();
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetPath(const QString &path)
{
    QUrl url = request.url();
    url.setPath(path);
    SetUrl(url);
    return this;
}

QString MeshmoonHttpRequest::UserInfo() const
{
    return request.url().userInfo();
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetUserInfo(const QString &info)
{
    QUrl url = request.url();
    url.setUserInfo(info);
    SetUrl(url);
    return this;
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetQueryItem(const QString &key, const QString &value)
{
    QUrl url = request.url();
    if (url.hasQueryItem(key))
        url.removeQueryItem(key);
    url.addQueryItem(key, value);
    SetUrl(url);
    return this;
}

bool MeshmoonHttpRequest::HasRequestHeader(const QString &name) const
{
    return request.hasRawHeader(name.toUtf8());
}

MeshmoonHttpRequest* MeshmoonHttpRequest::SetRequestHeader(const QString &name, const QString &value)
{
    // Redirect well known headers to QNetworkRequest cooked headers.
    if (name.compare(MeshmoonHttp::HeaderContentLength, Qt::CaseInsensitive) == 0)
    {
        bool ok = false;
        int contentLength = value.toInt(&ok);
        if (ok)
            SetRequestHeader(QNetworkRequest::ContentLengthHeader, contentLength);
        else
            LogError(QString("MeshmoonHttpRequest::SetRequestHeader: Failed to parse '%1' header value '%2' to a number.").arg(name).arg(value));
    }
    else if (name.compare(MeshmoonHttp::HeaderContentType, Qt::CaseInsensitive) == 0)
        SetRequestHeader(QNetworkRequest::ContentTypeHeader, value);
    // No matches, set raw header
    else
        request.setRawHeader(name.toUtf8(), value.toUtf8());
    return this;
}

void MeshmoonHttpRequest::SetRequestHeader(QNetworkRequest::KnownHeaders name, const QVariant &value)
{
    request.setHeader(name, value);
}

QString MeshmoonHttpRequest::RequestHeader(const QString &name) const
{
    return request.rawHeader(name.toUtf8());
}

QString MeshmoonHttpRequest::UserAgent() const
{
    return RequestHeader(MeshmoonHttp::HeaderUserAgent);
}

MeshmoonHttpRequest *MeshmoonHttpRequest::SetUserAgent(const QString &userAgent)
{
    return SetRequestHeader(MeshmoonHttp::HeaderUserAgent, userAgent);
}

bool MeshmoonHttpRequest::AutoExecuteLocationRedirects() const
{
    return autoExecuteLocationRedirects_;
}
    
MeshmoonHttpRequest *MeshmoonHttpRequest::SetAutoExecuteLocationRedirects(bool autoExecute)
{
    autoExecuteLocationRedirects_ = autoExecute;
    return this;
}

void MeshmoonHttpRequest::AboutToBeExecuted()
{
    /** Content-Length
        @note QNetworkAccessManager::createRequest should already do this if not set.
        But might not for our custom methods. Either way do it here where we are in control. */
    if (!requestBody.isEmpty())
    {
        if (!request.header(QNetworkRequest::ContentLengthHeader).isValid())
        {
            LogDebug(QString("MeshmoonHttpRequest::AboutToBeExecuted: Setting 'Content-Length' header to '%1'. Was not set by the user.").arg(requestBody.size()));
            SetRequestHeader(QNetworkRequest::ContentLengthHeader, requestBody.size());
        }
    }
}

QIODevice *MeshmoonHttpRequest::RequestBodyDevice()
{
    if (requestBody.isEmpty())
        return 0;

    if (!requestBodyDevice_)
    {
        requestBodyDevice_ = new QBuffer(&requestBody);
        if (!requestBodyDevice_->open(QIODevice::ReadOnly))
        {
            LogError("MeshmoonHttpRequest::RequestBodyDevice: Failed to open device for reading, body wont be sent!");
            SAFE_DELETE(requestBodyDevice_);
        }
    }
    return requestBodyDevice_;
}

void MeshmoonHttpRequest::ClearRequestBodyDevice()
{
    if (!requestBodyDevice_)
        return;

    if (requestBodyDevice_->isOpen())
        requestBodyDevice_->close();
    SAFE_DELETE(requestBodyDevice_);

    requestBody = "";
}

bool MeshmoonHttpRequest::IsRedirectStatusCode() const
{
    int status = ResponseStatusCode();
    return (status == 301 || status == 302 || status == 303 || status == 307);
}
