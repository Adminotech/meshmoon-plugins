/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "BrowserNetworkMessages.h"
#include "CefIntegration.h"

#include <QtGlobal>
#include <QObject>
#include <QSharedMemory>
#include <QPoint>
#include <QFile>
#include <QTimer>

class BrowserNetwork;

class RocketWebBrowser : public QObject
{
Q_OBJECT

public:
    RocketWebBrowser(int &argc, char **argv);
    ~RocketWebBrowser();

    QString id;
    qint64 parentProcessId;
    QSharedMemory memory;
    BrowserNetwork *network;
    AdminotechCefClient *client;

    int width;
    int height;
    bool debugging;

    CefRect popupRect;

    // Guaranteed trailing slash
    static QString UserDataDirectory();
    // Guaranteed trailing slash
    static QString ApplicationDirectory();

    bool IsStarted();

    QString IpcId();

    void BlitToTexture(bool isPopup, const std::vector<CefRect> &dirtyRects, const void* buffer);

    void LoadStart(const CefString &url);
    void LoadEnd(const CefString &url);

    void Log(const QString &);
    QFile *logFile;
protected:
    void timerEvent(QTimerEvent *e);

public slots:
    void Cleanup();
    void ConnectionLost();
    void ExitProcess();

    void OnUrlRequest(const BrowserProtocol::UrlMessage &msg);
    void OnResizeRequest(const BrowserProtocol::ResizeMessage &msg);
    void OnMouseMoveRequest(const BrowserProtocol::MouseMoveMessage &msg);
    void OnMouseButtonRequest(const BrowserProtocol::MouseButtonMessage &msg);
    void OnKeyboardRequest(const BrowserProtocol::KeyboardMessage &msg);
    void OnTypedRequest(const BrowserProtocol::TypedMessage &msg);

private slots:
    void OnCheckMainProcessAlive();

private:
#ifdef ROCKET_CEF3_ENABLED
    CefBrowserHost::MouseButtonType PopulateMouseButtons(CefMouseEvent &e);
#endif
    int timerId_;
    QTimer mainProcessPoller_;

    bool mouseCurrentLeft_;
    bool mouseCurrentMid_;
    bool mouseCurrentRight_;
};
