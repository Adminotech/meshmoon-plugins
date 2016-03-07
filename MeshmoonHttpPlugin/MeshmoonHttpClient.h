/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonHttpClient.h
    @brief  HTTP client. */

#pragma once

#include "MeshmoonHttpPluginFwd.h"
#include "MeshmoonHttpPluginApi.h"
#include "MeshmoonHttpRequest.h"

#include <QObject>

/// HTTP client API that has been designed to be flexible and easy to use from scripts.
/** @code
    // GET
    http.client.Get("http://www.service.com/my/status")
        // Set query items to the originally passed URL
        .SetQueryItem("name", "John Doe")
        .SetQueryItem("id", "52")
        // Set custom headers to be sent to the server
        .SetRequestHeader("my-custom-header", "hello world")
        // Set custom user agent
        .SetUserAgent("My Application 1.0")
    .Finished.connect(function(req, status, error) {
        console.LogInfo(req.ResponseStatus() + " for " + req.method + " to " + req.UrlString());
        if (status == 200)
        {
            // Process server response
            var responseObject = JSON.parse(req.body);
            console.LogInfo(responseObject.id + " " + responseObject.message); 
        }
    });

    // POST
    http.client.Post(
        // Request URL
        "http://www.service.com/post/json",
        // Body data bytes as QByteArray. JS strings will be auto converted.
        JSON.stringify({ name : "John Doe", id : 52, waiting : true }),
        // Content-Type defaults to 'application/octet-stream' if not defined.
        "application/json")
    .Finished.connect(function(req, status, error) {
         console.LogInfo(req.ResponseStatus() + " for " + req.method + " to " + req.UrlString());
         if (status != 200)
            console.LogError("POST failed!");
    });
    
    // Also available
    http.client.Put(url, body, contentType);    // PUT
    http.client.Patch(url, body, contentType);  // PATCH
    http.client.Head(url);                      // HEAD
    http.client.Options(url);                   // OPTIONS
    http.client.Delete(url);                    // DELETE

    var req = http.client.Put("http://none.com");
    
    // By default redirect responses will automatically be executed to the URL in the responses 'Location' header.
    // This is done for 301, 302, 303 and 307 if a valid URL is found in the 'Location' header.
    // If you wish to handle redirects yourself, do the following for each request.
    req.autoExecuteLocationRedirects = false;

    // Most of the request manipulations can also be done via object properties.
    req.host = "www.my-other-service.com";
    req.port = 8081;
    req.path = "path/to/put/handler";
    req.requestBody = "Hello world\nThis is Meshmoon HTTP client";
    req.requestContentType = "text/plain";

    req.Finished.connect(function(req, status, error) {
        console.LogInfo(req.ResponseStatus() + " for " + req.method + " to " + req.UrlString());
    });

    // Maximum requests that are executed at the same time can be configured. Default is 8. Minimum is 1.
    // Note that setting a large value will not necessarily increase the parallel request count,
    // Qt:s QNetworkAccessManager will still do its normal throttling eg. per unique host.
    http.client.maxOngoingRequests = 3;
    @endcode */
class MESHMOON_HTTP_API MeshmoonHttpClient : public QObject, public enable_shared_from_this<MeshmoonHttpClient>
{
    Q_OBJECT

    /// Maximum ongoing request limit. Default is 8. Minimum is 1.
    Q_PROPERTY(int maxOngoingRequests READ MaxOngoingRequests WRITE SetMaxOngoingRequests)

public:
    MeshmoonHttpClient(Framework *framework);
    ~MeshmoonHttpClient();

    int MaxOngoingRequests() const;
    void SetMaxOngoingRequests(int maxOngoingRequests);

    MeshmoonHttpRequestPtr GetRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr HeadRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr OptionsRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr PostRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr PutRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr PatchRaw(MeshmoonHttpRequestPtr request); /**< @overload */
    MeshmoonHttpRequestPtr DeleteRaw(MeshmoonHttpRequestPtr request); /**< @overload */

public slots:
    /// Execute GET
    /** @see https://tools.ietf.org/html/rfc2616#section-9.3 */
    MeshmoonHttpRequestPtr Get(const QString &url);

    /// Execute HEAD
    /** @see https://tools.ietf.org/html/rfc2616#section-9.4 */
    MeshmoonHttpRequestPtr Head(const QString &url);

    /// Execute OPTIONS
    /** @see https://tools.ietf.org/html/rfc2616#section-9.2 */
    MeshmoonHttpRequestPtr Options(const QString &url);

    /// Execute POST
    /** @see https://tools.ietf.org/html/rfc2616#section-9.5 */
    MeshmoonHttpRequestPtr Post(const QString &url, const QByteArray &body = "", const QString &contentType = "application/octet-stream");

    /// Execute PUT
    /** @see https://tools.ietf.org/html/rfc2616#section-9.6 */
    MeshmoonHttpRequestPtr Put(const QString &url, const QByteArray &body = "", const QString &contentType = "application/octet-stream");

    /// PATCH method
    /** @see https://tools.ietf.org/html/rfc2068 and https://tools.ietf.org/html/rfc5789 */
    MeshmoonHttpRequestPtr Patch(const QString &url, const QByteArray &body = "", const QString &contentType = "application/octet-stream");

    /// Execute DELETE
    /** @see https://tools.ietf.org/html/rfc2616#section-9.7 */
    MeshmoonHttpRequestPtr Delete(const QString &url);

signals:
    /// Server requires authentication for a request.
    /** @see Read more at https://qt-project.org/doc/qt-4.8/qnetworkaccessmanager.html#authenticationRequired */
    void AuthenticationRequired(MeshmoonHttpRequestPtr request, QAuthenticator *authenticator);

    /// Request encountered SSL errors.
    /** If you wish to ignore these errors and trust the source, call MeshmoonHttpRequest::IgnoreSslErrors(). 
        @note It is good practice to prompt the user before ignoring the errors. */
    void SslErrors(MeshmoonHttpRequestPtr request, const QList<QSslError> &errors);

private slots:
    MeshmoonHttpRequestPtr Add(MeshmoonHttpRequest::Method method, MeshmoonHttpRequestPtr request);
     
    void ExecuteNext(int msecDelay = 0);
    
    void OnFinished(QNetworkReply *reply);
    void OnAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
    void OnSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);
    
    MeshmoonHttpRequestPtr OngoingRequestForReply(QNetworkReply *reply) const;

private:
    Framework *framework_;

    QNetworkAccessManager *http_;

    MeshmoonHttpRequestList pending_;
    MeshmoonHttpRequestList ongoing_;
    
    QString LC;
    int maxOngoingRequests_;
};
Q_DECLARE_METATYPE(MeshmoonHttpClient*)
