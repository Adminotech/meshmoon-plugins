/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#ifdef WIN32
#include "windows.h"
#endif

// Disable C4100 warning coming from cef
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)
#endif
#include "cef_app.h"
#include "cef_client.h"
#include "cef_browser.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef ROCKET_CEF3_SANDBOX_ENABLED
#ifdef WIN32
#include "cef_sandbox_win.h"
#else
#error CEF3 sandbox mode can only be used on Windows
#endif
#endif

#include <QtGlobal>
#include <QtGui/qwindowdefs.h>
#include <QObject>
#include <QString>
#include <QHash>

class RocketWebBrowser;

/// @cond PRIVATE

inline QString CefToQString(const CefString &str)
{
    QString qtStr;
#if defined(CEF_STRING_TYPE_UTF16)
    qtStr = QString::fromUtf16((ushort*)str.c_str(), static_cast<int>(str.size()));
#elif defined(CEF_STRING_TYPE_UTF8)
    qtStr = QString::fromUtf8((char*)str.c_str(), static_cast<int>(str.size()));
#endif
    return qtStr;
}

// AdminotechCefClient

class AdminotechCefClient : public QObject,
                            public CefClient,
                            public CefLoadHandler,
                            public CefRequestHandler,
                            public CefRenderHandler
{
Q_OBJECT

public:
    AdminotechCefClient();
    ~AdminotechCefClient();

    static CefBrowserSettings CreateSettings();

    friend class RocketWebBrowser;

public slots:
    /// Creates a new browser for component instance.
    CefRefPtr<CefBrowser> CreateBrowser(RocketWebBrowser *parent, WId parentWindowId);
    
    /// Close the client and its browser.
    void Close();
    
    /// Load url to the browser.
    void LoadUrl(const QString &url);
    
    /// Resize the browser.
    void Resize(int width, int height);
    
    /// Invalidate view.
    void Invalidate(int x, int y, int width, int height);
    
    /// Focus browser.
    void FocusIn();
    
    /// Unfocus browser.
    void FocusOut();

public:
    // CefClient overrides
    CefRefPtr<CefLoadHandler> GetLoadHandler() { return this; }
    CefRefPtr<CefRequestHandler> GetRequestHandler() { return this; }
    CefRefPtr<CefRenderHandler> GetRenderHandler() { return this; }
    
    // CefLifeSpanHandler overrides
    void OnAfterCreated(CefRefPtr<CefBrowser> createdBrowser);

    // CefLoadHandler overrides
    void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame);
    void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode);
    bool OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& failedUrl, CefString& errorText);

    // CefRequestHandler overrides
    bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefRequest> request, CefString& redirectUrl,
        CefRefPtr<CefStreamReader>& resourceStream, CefRefPtr<CefResponse> response, int loadFlags);
    bool GetDownloadHandler(CefRefPtr<CefBrowser> browser, const CefString& mimeType, const CefString& fileName,
        int64 contentLength, CefRefPtr<CefDownloadHandler>& handler);

    // CefRenderHandler overrides
    /// @todo See what needs to be properly implemented for CEF3.
#ifdef ROCKET_CEF3_ENABLED
    bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
    bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height);
#else
    bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) const;
    bool GetScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) const;
    void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer);
#endif
    void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);
    void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect);

#ifdef ROCKET_CEF3_ENABLED
    CefRefPtr<CefBrowserHost> Host() const;
#else
    CefRefPtr<CefBrowser> Host() const;
#endif

    IMPLEMENT_REFCOUNTING(AdminotechCefClient);

protected:
    RocketWebBrowser *component_;
    CefRefPtr<CefBrowser> browser_;
};

// AdminotechCefEngine

class AdminotechCefEngine
{
public:
    AdminotechCefEngine();
    ~AdminotechCefEngine();

    void Shutdown();
    void Update();

    // Get static singleton instance.
    static AdminotechCefEngine *Instance();

    CefRefPtr<CefApp> application;
#ifdef ROCKET_CEF3_SANDBOX_ENABLED
    CefScopedSandboxInfo *sandbox;
#endif
};

/// @endcond
