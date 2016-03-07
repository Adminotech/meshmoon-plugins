/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "CoreTypes.h"
#include "FrameworkFwd.h"

#include <QString>

/// @cond PRIVATE

namespace MeshmoonHttp
{
    static QString HeaderUserAgent          = "User-Agent";
    static QString HeaderContentType        = "Content-Type";
    static QString HeaderContentLength      = "Content-Length";
    static QString HeaderLastModified       = "Last-Modified";
    static QString HeaderIfModifiedSince    = "If-Modified-Since";
}

/// @endcond

class QNetworkRequest;
class QNetworkReply;
class QNetworkAccessManager;
class QNetworkDiskCache;

class MeshmoonHttpPlugin;
class MeshmoonHttpClient;
class MeshmoonHttpRequest;

typedef shared_ptr<MeshmoonHttpClient> MeshmoonHttpClientPtr;
typedef shared_ptr<MeshmoonHttpRequest> MeshmoonHttpRequestPtr;

typedef QList<MeshmoonHttpRequestPtr> MeshmoonHttpRequestList;

Q_DECLARE_METATYPE(MeshmoonHttpClientPtr)
Q_DECLARE_METATYPE(MeshmoonHttpRequestPtr)
