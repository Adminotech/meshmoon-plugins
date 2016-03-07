/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonHttpRequest.h
    @brief  Represents a single HTTP request */

#pragma once

#include "MeshmoonHttpPluginFwd.h"
#include "MeshmoonHttpPluginApi.h"

#include <QObject>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>

/// @cond PRIVATE
#ifdef SetPort // Win.h -> WinSpool.h weirdness
#undef SetPort
#endif
/// @endcond

/// Represents a single HTTP request.
class MESHMOON_HTTP_API MeshmoonHttpRequest : public QObject
{
    Q_OBJECT

    // URL related properties
    Q_PROPERTY(int port READ Port WRITE SetPort)                        /**< @copydoc SetPort */
    Q_PROPERTY(QString scheme READ Scheme WRITE SetScheme)              /**< @copydoc SetScheme */
    Q_PROPERTY(QString host READ Host WRITE SetHost)                    /**< @copydoc SetHost */
    Q_PROPERTY(QString path READ Path WRITE SetPath)                    /**< @copydoc SetPath */
    Q_PROPERTY(QString userInfo READ UserInfo WRITE SetUserInfo)        /**< @copydoc SetUserInfo */
    Q_PROPERTY(QString userAgent READ UserAgent WRITE SetUserAgent)     /**< @copydoc SetUserAgent */

    // Request related properties
    Q_PROPERTY(QString method READ MethodString)                                /**< @copydoc MethodString */
    Q_PROPERTY(QByteArray requestBody READ RequestBody WRITE SetRequestBody)    /**< @copydoc SetRequestBody */
    Q_PROPERTY(QString requestContentType READ RequestContentType WRITE SetRequestContentType) /**< @copydoc SetRequestContentType */
    Q_PROPERTY(bool autoExecuteLocationRedirects READ AutoExecuteLocationRedirects WRITE SetAutoExecuteLocationRedirects) /**< @copydoc SetAutoExecuteLocationRedirects */

    // Response related properties
    Q_PROPERTY(int status READ ResponseStatusCode)                      /**< @copydoc ResponseStatusCode */
    Q_PROPERTY(QString body READ ResponseBodyString)                    /**< @copydoc ResponseBodyString */
    Q_PROPERTY(QByteArray bodyBytes READ ResponseBodyBytes)             /**< @copydoc ResponseBodyBytes */
    Q_PROPERTY(QString error READ ResponseError)                        /**< @copydoc ResponseError */

public:
    explicit MeshmoonHttpRequest(const QString &url = "", const QByteArray &requestBody = "", const QString &contentType = "");
    ~MeshmoonHttpRequest();

    enum Method 
    {
        MethodUnknown,
        MethodHead,
        MethodOptions,
        MethodGet,
        MethodPut,
        MethodPatch,
        MethodPost,
        MethodDelete
    };
    Q_ENUMS(Method)

    /// HTTP request method.
    /** @note Method is only set to a valid method once this request
        has been used/created through MeshmoonHttpClient functions.
        Method cannot be set from 3rd party code. */
    QString MethodString() const;

    /// Static converter from method enum to string.
    /** @note Resulting string will be upper cased. */
    static QString MethodToString(Method method);
    
    /// Static converted from status code to its string representation.
    /** For example for status code 200 returns "OK" 
        @note All status strings might not be supported, 
        only the more common ones for HTTP traffic. */
    static QString StatusCodeString(int status);

signals:
    /// Request finished signal.
    /** @param The request itself.
        @param Status code of the response.
        @param Error message, empty if no errors. */
    void Finished(MeshmoonHttpRequest *request, int statusCode, const QString &error);

    /// Server requires authentication for a request.
    /** @see Read more at https://qt-project.org/doc/qt-4.8/qnetworkaccessmanager.html#authenticationRequired */
    void AuthenticationRequired(MeshmoonHttpRequest *request, QAuthenticator *authenticator);

    /// Request encountered SSL errors.
    /** If you wish to ignore these errors and trust the source, call IgnoreSslErrors(). */
    void SslErrors(MeshmoonHttpRequest *request, const QList<QSslError> &errors);

public slots:
    /************* URL *************/

    /// Returns URL this request is being made to.
    QUrl Url() const;

    /// Sets URL this request is being made to.
    MeshmoonHttpRequest *SetUrl(const QUrl &url);

    /// Sets URL this request is being made to.
    /** Runs the given string through http://qt-project.org/doc/qt-4.8/qurl.html#fromUserInput */
    MeshmoonHttpRequest *SetUrlFromUserInput(const QString &url);

    /// Returns URL this request is being made to.
    QString UrlString(QUrl::FormattingOption options = QUrl::None) const;
    QString toString(QUrl::FormattingOption options = QUrl::None) const { return UrlString(options); }

    /// Returns request port, -1 if no port is defined.
    int Port() const;

    /// Set request port.
    /** @see http://qt-project.org/doc/qt-4.8/qurl.html#setPort */
    MeshmoonHttpRequest *SetPort(int port);

    /// Returns request scheme.
    QString Scheme() const;

    /// Set request scheme.
    /** @see http://qt-project.org/doc/qt-4.8/qurl.html#setScheme */
    MeshmoonHttpRequest *SetScheme(const QString &scheme);

    /// Returns request host.
    QString Host() const;

    /// Sets request host.
    /** @see http://qt-project.org/doc/qt-4.8/qurl.html#setHost */
    MeshmoonHttpRequest *SetHost(const QString &host);

    /// Returns request path.
    QString Path() const;

    /// Sets request path.
    /** @see http://qt-project.org/doc/qt-4.8/qurl.html#setPath */
    MeshmoonHttpRequest *SetPath(const QString &path);

    /// Returns user info of the request URL.
    QString UserInfo() const;

    /// Sets user info of request URL.
    /** @see http://qt-project.org/doc/qt-4.8/qurl.html#setUserInfo */
    MeshmoonHttpRequest *SetUserInfo(const QString &info);

    /// Sets query item to request URL.
    /** @note Percent encoding is applied as needed. */
    MeshmoonHttpRequest *SetQueryItem(const QString &key, const QString &value);

    /************* Request *************/

    /// Request body.
    QByteArray RequestBody();

    /// Set request body and content type to be sent to the server.
    /** @param Body
        @param If not an empty string, SetRequestContentType will be called with it. 
        If you want to reset content-type to an empty value, call SetRequestContentType directly.
        @see SetRequestContentType and SetRequestHeader. */
    MeshmoonHttpRequest *SetRequestBody(const QByteArray &body, const QString &contentType);

    /** If content type is not yet set, it will be set to "application/octet-stream".
        @see SetRequestContentType and SetRequestHeader.
        @overload */
    MeshmoonHttpRequest *SetRequestBody(const QByteArray &body);

    /// Serialize @c obj as the request body.
    /** Content-Type header will be forced to "application/json".
        @see SetRequestContentType and SetRequestHeader. */
    MeshmoonHttpRequest *SetRequestBodyJSON(const QVariant &obj);

    /// Sets request body and content type from a file
    /** @note This is not safe. We need to authorize uploads from the users, so scripts cannot upload arbitrary
        filepath from the users disk. As this can be done totally hidden from the user.
        @param Path to the file that should be read.
        @param If empty string and content type is not yet set, it will be set to "application/octet-stream".
        @see SetRequestContentType and SetRequestHeader. */
    MeshmoonHttpRequest *SetRequestBodyFromFile(const QString &filepath, const QString &contentType);

    /// Request content type.
    QString RequestContentType();

    /// Set request content type to be sent to the server. Sets 'content-type' header to @c contentType.
    MeshmoonHttpRequest *SetRequestContentType(const QString &contentType);

    /// Sets @c name header to @c value.
    MeshmoonHttpRequest *SetRequestHeader(const QString &name, const QString &value);

    /// Returns current header value for @c name.
    QString RequestHeader(const QString &name) const;

    /// Checks if a header is set.
    bool HasRequestHeader(const QString &name) const;

    /// Returns user agent from request headers.
    /** @note Default user agent is value of Application::FullIdentifier().
        Set explicitly for every request if this is relevant to your HTTP server. */
    QString UserAgent() const;

    /// Sets user agent to request headers.
    MeshmoonHttpRequest *SetUserAgent(const QString &userAgent);
    
    /// Returns if this request auto executes server redirects. 
    /** @see SetAutoExecuteLocationRedirects */
    bool AutoExecuteLocationRedirects() const;
    
    /// If this request should auto execute to server redirects. Default is true.
    /** If server responds with '301 Moved Permanently', '302 Found', '303 See Other' or '307 Temporary Redirect'
        with a valid 'Location' header URL to the new resource, auto execute before emitting Finished signal for this request. 
        @note The spec says eg. 301 should not be automatically redirected, if you don't want this call this function with false
        to handle it manually in your code. */
    MeshmoonHttpRequest *SetAutoExecuteLocationRedirects(bool autoExecute);

    /************* Response *************/

    /// Response body received from the server as a string. @see ResponseBodyBytes.
    QString ResponseBodyString();

    /// Response body as bytes. @see ResponseBodyString.
    QByteArray ResponseBodyBytes();

    /// Response body as QVariant represented JSON object.
    QVariant ResponseJSON();
    
    /// Response body as QDocument represented XML object.
    QDomDocument ResponseXML();

    /// Response status code received from the server.
    int ResponseStatusCode() const;

    /// Response status code string received from the server.
    /** For example for status code 200 returns "OK" 
        @note All status strings might not be supported, 
        only the more common ones for HTTP traffic. */
    QString ResponseStatusCodeString() const;

    /// Response status code and its string description. eg. "200 OK".
    /** @see ResponseStatusCode and ResponseStatusCodeString. */
    QString ResponseStatus() const;

    /// Response error, empty string if no errors.
    QString ResponseError();

    /// Returns all response header names sent by the server.
    QStringList ResponseHeaderNames() const;

    /// Response header value for @c name.
    /** @note Both case sensitive and lower cased name will be queried. */
    QString ResponseHeader(const QString &name) const;

    /// Returns if a response header was sent by the server.
    bool HasResponseHeader(const QString &name) const;
    
    /// Ignore encountered SSL errors.
    /** Should be called when handling the OnSslErrors signal, if you wish to ignore these errors.
        @note It is good practice to prompt the user before ignoring the errors. */
    void IgnoreSslErrors();

    /// Returns the underlying raw QNetworkReply ptr.
    /** @note Exposed for very special cases, prefer using the MeshmoonHttpRequest API. */
    QNetworkReply *Response() const;

    /// Reply error enum.
    QNetworkReply::NetworkError ResponseErrorEnum() const;

    /// Returns reply URL.
    QUrl ResponseUrl() const;
    
    /// Returns reply URL string.
    QString ResponseUrlString(QUrl::FormattingOption options = QUrl::None) const;

private:
    void EmitFinished();
    void EmitAuthenticationRequired(QAuthenticator *authenticator);
    void EmitSslErrors(const QList<QSslError> &errors);

    void ValidateHttpScheme();

    /// Private overload to set QNetworkRequest cooked headers.
    void SetRequestHeader(QNetworkRequest::KnownHeaders name, const QVariant &value);

    /// This request is about to be executed to the network.
    /** Should execute any last second preparations, like setting content length header. */
    void AboutToBeExecuted();

    /// Device that is opened to the request body bytes. Null if no request body is set.
    /** @note Will be closed once the reply finishes. */
    QIODevice *RequestBodyDevice();

    /// Closes body device and resets body QByteArray.
    void ClearRequestBodyDevice();
    
    /// Returns if response status code falls into the auto redirect execute umbrella.
    bool IsRedirectStatusCode() const;

    Method method;
    
    QNetworkRequest request;
    QNetworkReply *reply;
    
    QByteArray requestBody;
    QByteArray responseBody;
    
    QBuffer *requestBodyDevice_;
    
    bool autoExecuteLocationRedirects_;

    friend class MeshmoonHttpClient;
};
Q_DECLARE_METATYPE(MeshmoonHttpRequest*)
