/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "CefIntegration.h"
#include "RocketWebBrowser.h"

#include <QDir>
#include <QDataStream>
#include <QTime>
#include <QHostAddress>
#include <QDebug>

// AdminotechCefClient

AdminotechCefClient::AdminotechCefClient() :
    component_(0)
{
}

AdminotechCefClient::~AdminotechCefClient()
{
    Close();
}

CefBrowserSettings AdminotechCefClient::CreateSettings()
{
    CefBrowserSettings s;

    /// @todo Figure out good CEF3 settings.
#ifdef ROCKET_CEF3_ENABLED
    s.java = STATE_ENABLED;
    s.plugins = STATE_ENABLED;
    s.background_color = CefColorSetARGB(255,255,255,255);
#else
    s.drag_drop_disabled = true;
    s.load_drops_disabled = true;
    s.history_disabled = true;          /// @todo Control via attribute?
    s.animation_frame_rate = 30;        /// @todo Control via attribute?

    s.remote_fonts_disabled = false;
    s.encoding_detector_enabled = true;
    s.image_load_disabled = false;
    s.web_security_disabled = false;
    s.xss_auditor_enabled = false;

    s.local_storage_disabled = false;
    s.databases_disabled = false;
    s.application_cache_disabled = false;
    s.page_cache_disabled = false;

    s.plugins_disabled = false;
    s.java_disabled = false;
    s.javascript_disabled = false;
    s.javascript_open_windows_disallowed = false;
    s.javascript_close_windows_disallowed = false;
    s.javascript_access_clipboard_disallowed = false;

    s.dom_paste_disabled = false;
    s.caret_browsing_enabled = false;

    s.webgl_disabled = false;
    s.accelerated_compositing_enabled = false;
    s.accelerated_layers_disabled = false;
    s.accelerated_video_disabled = false;
    s.accelerated_2d_canvas_disabled = false;
    s.accelerated_painting_disabled = false;
    s.accelerated_filters_disabled = false;
    s.accelerated_plugins_disabled = false;
    s.developer_tools_disabled = true;
    s.fullscreen_enabled = false;
#endif
    return s;
}

// Public API

CefRefPtr<CefBrowser> AdminotechCefClient::CreateBrowser(RocketWebBrowser *parent, WId parentWindowId)
{
    if (browser_.get())
        return browser_;

    component_ = parent;

    // Qt against Cocoa returns NSView* for winId()
    CefWindowInfo windowInfo;
#ifdef ROCKET_CEF3_ENABLED
    #ifdef __APPLE__
    windowInfo.SetAsWindowless(reinterpret_cast<NSView*>(parentWindowId), true);
    #else
    windowInfo.SetAsWindowless(parentWindowId, true);
    #endif
#else
    #ifdef __APPLE__
    windowInfo.SetAsOffScreen(reinterpret_cast<NSView*>(parentWindowId));
    #else
    windowInfo.SetAsOffScreen(parentWindowId);
    #endif
#endif

#ifdef ROCKET_CEF3_ENABLED
    browser_ = CefBrowserHost::CreateBrowserSync(windowInfo, static_cast<CefRefPtr<CefClient> >(this), "", AdminotechCefClient::CreateSettings(), NULL);
#else
    browser_ = CefBrowser::CreateBrowserSync(windowInfo, static_cast<CefRefPtr<CefClient> >(this), "", AdminotechCefClient::CreateSettings());
#endif
    return browser_;
}

void AdminotechCefClient::Close()
{
    browser_ = 0;
    component_ = 0;
}

#ifdef ROCKET_CEF3_ENABLED
CefRefPtr<CefBrowserHost> AdminotechCefClient::Host() const
{
    return browser_->GetHost();
}
#else
CefRefPtr<CefBrowser> AdminotechCefClient::Host() const
{
    return browser_;
}
#endif

void AdminotechCefClient::LoadUrl(const QString &url)
{
    if (browser_.get() && browser_->GetMainFrame().get())
    {
        QByteArray cefUrl = url.toUtf8();
        if (cefUrl.isEmpty())
            cefUrl = "about:blank";
        browser_->GetMainFrame()->LoadURL(cefUrl.constData());
    }
}

void AdminotechCefClient::Resize(int width, int height)
{
    if (browser_.get())
#ifdef ROCKET_CEF3_ENABLED
        browser_->GetHost()->WasResized();
#else
        browser_->SetSize(PET_VIEW, width, height);
#endif
}

void AdminotechCefClient::Invalidate(int x, int y, int width, int height)
{
    if (browser_.get())
#ifdef ROCKET_CEF3_ENABLED
        browser_->GetHost()->Invalidate(PET_VIEW);
#else
        browser_->Invalidate(CefRect(x, y, width, height));
#endif
}

void AdminotechCefClient::FocusIn()
{
    if (Host().get())
    {
        Host()->SetFocus(true);
        Host()->SendFocusEvent(true);
    }
}

void AdminotechCefClient::FocusOut()
{
    if (browser_.get())
    {
        /** @todo Check if this bug persists in CEF3 that we 
            need to send a dummy click to the top left corner. */
#ifndef ROCKET_CEF3_ENABLED
        Host()->SendMouseMoveEvent(0, 0, true);
#endif
        Host()->SendCaptureLostEvent();
        Host()->SetFocus(false);
        Host()->SendFocusEvent(false);
    }
}

// CefLoadHandler overrides

void AdminotechCefClient::OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame)
{
    if (frame->IsMain() && component_)
        component_->LoadStart(frame->GetURL());
}

void AdminotechCefClient::OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int /*httpStatusCode*/)
{
    if (frame->IsMain() && component_)
        component_->LoadEnd(frame->GetURL());
}

bool AdminotechCefClient::OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode /*errorCode*/, const CefString& /*failedUrl*/, CefString& /*errorText*/)
{
    /// @todo Process?
    return false; // Use the default error text
}

// CefRequestHandler overrides

bool AdminotechCefClient::OnBeforeResourceLoad(CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefRequest> /*request*/, CefString& /*redirectUrl*/,
                                               CefRefPtr<CefStreamReader>& /*resourceStream*/, CefRefPtr<CefResponse> response, int /*loadFlags*/)
{
    return false; // Allows to load the resource
}

bool AdminotechCefClient::GetDownloadHandler(CefRefPtr<CefBrowser> /*browser*/, const CefString& /*mimeType*/, const CefString& /*fileName*/,
                                             int64 /*contentLength*/, CefRefPtr<CefDownloadHandler>& /*handler*/)
{
    return false; /// @todo Handle file download requests
}

/// CefRenderHandler

#ifdef ROCKET_CEF3_ENABLED
bool AdminotechCefClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
#else
bool AdminotechCefClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) const
#endif
{
    if (!component_)
        return false;
        
    rect.x = 0; rect.y = 0;
    rect.width = component_->width;
    rect.height = component_->height;
    return true;
}

#ifdef ROCKET_CEF3_ENABLED
bool AdminotechCefClient::GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
#else
bool AdminotechCefClient::GetScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) const
#endif
{
    return GetViewRect(browser, rect);
}

void AdminotechCefClient::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show)
{
    if (!show && component_ && browser_.get())
    {
#ifdef ROCKET_CEF3_ENABLED
        browser_->GetHost()->Invalidate(PET_POPUP);
#else
        browser_->Invalidate(component_->popupRect);
#endif
        component_->popupRect.Set(-1,-1,0,0);
    }
}

void AdminotechCefClient::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect)
{
    if (component_)
        component_->popupRect.Set(rect.x, rect.y, rect.width, rect.height);
}

#ifdef ROCKET_CEF3_ENABLED
void AdminotechCefClient::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height)
#else
void AdminotechCefClient::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer)
#endif
{
    /// @todo Test if width/height is important to provide for blitting or not in CEF3!
    if (component_)
        component_->BlitToTexture((type == PET_POPUP), dirtyRects, buffer);
}

// AdminotechCefEngine

Q_GLOBAL_STATIC(AdminotechCefEngine, cefEngine)

AdminotechCefEngine::AdminotechCefEngine()
#ifdef ROCKET_CEF3_SANDBOX_ENABLED
    : sandbox(0)
#endif
{
    CefSettings settings;
    settings.multi_threaded_message_loop = false;

#ifndef ROCKET_CEF3_ENABLED
#ifdef Q_WS_WIN
    // ANGLE_IN_PROCESS
    //   - Better perf on HTML5 content etc.
    //   - Adds runtime deps: libEGL.dll libGLESv2.dll
    //   - Should work in mac also (prolly auto changes to DESKTOP_IN_PROCESS)
    // DESKTOP_IN_PROCESS
    //   - No enhanced speed for flash at least.
    // ANGLE_IN_PROCESS_COMMAND_BUFFER 
    //   - ???
    // DESKTOP_IN_PROCESS_COMMAND_BUFFER 
    //   - ???
    // http://magpcss.org/ceforum/viewtopic.php?f=6&t=607
    settings.graphics_implementation = ANGLE_IN_PROCESS; 
#endif
#else
    settings.windowless_rendering_enabled = true;
    settings.background_color = CefColorSetARGB(255,255,255,255);
#endif

    settings.log_severity = LOGSEVERITY_DISABLE;

    // Paths
    QString appDir = RocketWebBrowser::ApplicationDirectory();
    QString userDir = RocketWebBrowser::UserDataDirectory();
    
    QByteArray logFilePath = QString(userDir + "logs" + QDir::separator() + "rocket_cef_log.txt").toUtf8();
    QByteArray cefLocalesDirPath = QString(appDir + "data" + QDir::separator() + "cef" + QDir::separator() + "locales").toUtf8();
    QByteArray cefResourceDirPath = QString(appDir + "data" + QDir::separator() + "cef").toUtf8();
    QByteArray cacheDirPath = QString(userDir + "cefcache").toUtf8();

    cef_string_from_utf8(logFilePath.constData(), logFilePath.size(), &settings.log_file);
    cef_string_from_utf8(cefLocalesDirPath.constData(), cefLocalesDirPath.size(), &settings.locales_dir_path);
    cef_string_from_utf8(cefResourceDirPath.constData(), cefResourceDirPath.size(), &settings.resources_dir_path);
    cef_string_from_utf8(cacheDirPath.constData(), cacheDirPath.size(), &settings.cache_path);

#ifdef ROCKET_CEF3_ENABLED
    void *sandbox_info = 0;
#ifdef ROCKET_CEF3_SANDBOX_ENABLED
    sandbox = new CefScopedSandboxInfo();
    sandbox_info = sandbox->sandbox_info();
#endif
    if (sandbox_info == 0)
        settings.no_sandbox = true;

    CefMainArgs args;

    // Does not seem to be important at least on windows.
    //int exit_code = CefExecuteProcess(args, application, sandbox_info);
    //qDebug()  << "CefExecuteProcess exit code" << exit_code;

    if (!CefInitialize(args, settings, application, sandbox_info))
        qDebug() << "[CefEngine]: Failed to initialize CEF library. EC_WebBrowser based embedded browsers will not work!";
#else
    if (!CefInitialize(settings, application))
        qDebug() << "[CefEngine]: Failed to initialize CEF library. EC_WebBrowser based embedded browsers will not work!";
#endif
}

AdminotechCefEngine::~AdminotechCefEngine()
{
}

void AdminotechCefEngine::Update()
{
    CefDoMessageLoopWork();
}

void AdminotechCefEngine::Shutdown()
{
    /** This randomly blocks for a long time.
        The process is dying let the operating system handle
        freeing memory so our main app does not have to wait
        for a clean exit. */
    //CefShutdown();
    application = 0;
#ifdef ROCKET_CEF3_SANDBOX_ENABLED
    delete sandbox; sandbox = 0;
#endif
}

AdminotechCefEngine *AdminotechCefEngine::Instance()
{
    return cefEngine();
}

#ifdef ROCKET_WEBBROWSER_EXTERNAL_BUILD
#include "moc_CefIntegration.cxx"
#endif
