/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "RocketWebBrowser.h"
#include "BrowserNetwork.h"

#ifdef WIN32
#include "windows.h"
#include <ShTypes.h>
#include <ShlObj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QDebug>

static const int BYTES_PER_PIXEL = 4;

RocketWebBrowser::RocketWebBrowser(int &argc, char **argv) :
    network(0),
    client(0),
    width(1),
    height(1),
    debugging(false),
    logFile(0),
    parentProcessId(-1),
    popupRect(-1,-1,0,0),
    mouseCurrentLeft_(false),
    mouseCurrentRight_(false),
    mouseCurrentMid_(false)
{
    QString port;
    bool nextIsPort = false;
    bool nextIsId = false;
    bool nextIsPid = false;
    
    for(int i=1; i<argc; ++i)
    {
        QString param = argv[i];
        
        // Port
        if (param == "--port")
            nextIsPort = true;
        else if (nextIsPort)
        {
            port = param;
            network = new BrowserNetwork(this, param.toUShort());
            if (!network->server->isListening())
                return;
            nextIsPort = false;
        }
            
        // Identifier
        if (param == "--id")
            nextIsId = true;
        else if (nextIsId)
        {
            id = param;
            nextIsId = false;
        }

        if (param == "--pid")
            nextIsPid = true;
        else if (nextIsPid)
        {
            parentProcessId = param.toLongLong();
            nextIsPid = false;
        }

        if (param == "--debug")
            debugging = true;
    }

    if (debugging)
    {
        logFile = new QFile("./EC_WebBrowser_log_" + port + ".txt");
        if (!logFile->open(QIODevice::ReadWrite | QIODevice::Text))
        {
            delete logFile;
            logFile = 0;
        }
    }
       
    // All params present
    if (IsStarted())
    {
        memory.setKey(id);
        if (memory.attach())
        {
            // Start engine and browser
            AdminotechCefEngine::Instance();
            
            client = new AdminotechCefClient();
            client->CreateBrowser(this, 0);
             
            // Start update timer
            timerId_ = startTimer(10);

            // Start monitoring parent Rocket process
            if (parentProcessId >= 0)
            {
                connect(&mainProcessPoller_, SIGNAL(timeout()), SLOT(OnCheckMainProcessAlive()));
                mainProcessPoller_.start(5000);
            }
        }
        else
        {
            Log("ERROR - RocketWebBrowser: Failed attaching to IPC memory!");
            id = ""; // Make the program exit cleanly
        }
    }
    else
        Log("ERROR - RocketWebBrowser: Incomplete input params!");
}

RocketWebBrowser::~RocketWebBrowser()
{
    Log("~RocketWebBrowser");
    Cleanup();
}

void RocketWebBrowser::Log(const QString &message)
{
    if (logFile && logFile->isOpen())
    {
        QTextStream log(logFile);
        log << message << endl;
        log.flush();
    }
}

void RocketWebBrowser::Cleanup()
{
    Log("Cleanup");
    if (memory.isAttached())
        memory.detach();

    if (network)
        delete network;
    network = 0;

    // Deleting this will can cause errors.
    //if (client)
    //    delete client;
    client = 0;

    AdminotechCefEngine::Instance()->Shutdown();

    if (logFile)
    {
        logFile->close();
        delete logFile;
    }
    logFile = 0;
}

void RocketWebBrowser::ConnectionLost()
{
    Log("ConnectionLost");
    ExitProcess();
}

void RocketWebBrowser::ExitProcess()
{
    Log("ExitProcess");
    QCoreApplication::quit();
}

bool RocketWebBrowser::IsStarted()
{
    if (!network || id.isEmpty())
        return false;
    return true;
}

QString RocketWebBrowser::IpcId()
{
    if (!network)
        return "";
    return QString("cef_ipc_mem_%1_%2x%3").arg(network->port).arg(width).arg(height);
}

void RocketWebBrowser::BlitToTexture(bool isPopup, const std::vector<CefRect> &dirtyRects, const void* buffer)
{
    if (!memory.isAttached())
        return;

    memory.lock();
    void *dest = memory.data();

    BrowserProtocol::DirtyRectsMessage msg;

    if (isPopup)
    {
        if (popupRect.x != -1 && popupRect.y != -1)
        {
            int pixelsOverX = 0;
            int pixelsOverY = 0;
            
            /// Blitting whole known popup rect for now.
            /// @todo Fix this to blit correctly subrects of the popups to the main view.
            const int lineBytesMain = width * BYTES_PER_PIXEL;
            const int lineBytesPopup = popupRect.width * BYTES_PER_PIXEL;
            int popupBytesCopy = lineBytesPopup;
            
            // X is over?
            if ((popupRect.x + popupRect.width) > this->width)
            {
                pixelsOverX = (popupRect.x + popupRect.width) - this->width;
                popupBytesCopy = (popupRect.width - pixelsOverX) * BYTES_PER_PIXEL;
            }
            // Y is over?
            if ((popupRect.y + popupRect.height) > this->height)
                pixelsOverY = (popupRect.y + popupRect.height) - this->height;

            int copyHeight = popupRect.height - pixelsOverY;
            for (int y=0; y<copyHeight; ++y)
            {
                uint indexPopup = lineBytesPopup * y;
                uint indexMain = (lineBytesMain * (popupRect.y+y)) + (BYTES_PER_PIXEL * popupRect.x);
                memcpy((char*)dest + indexMain, (char*)buffer + indexPopup, popupBytesCopy);
            }
            
            msg.rects << QRect(popupRect.x, popupRect.y, popupRect.width - pixelsOverX, popupRect.height - pixelsOverY);
        }
    }
    else
    {
        // Optimize for full blit
        if (dirtyRects.size() == 1)
        {
            const CefRect &rect = dirtyRects[0];
            if (rect.x == 0 && rect.y == 0 && rect.width == this->width && rect.height == this->height)
            {
                memcpy(dest, buffer, memory.size());
                msg.rects << QRect(rect.x, rect.y, rect.width, rect.height);
            }
        }

        // Subrect blits (horizontal slices for now)
        if (msg.rects.size() < (int)dirtyRects.size())
        {
            for (uint i=0; i<dirtyRects.size(); ++i)
            {
                const CefRect &rect = dirtyRects[i];
                if (rect.x < 0 || rect.y < 0 || rect.height <= 0 || rect.width <= 0)
                    continue;
                    
                const int rectLineBytes = rect.width * BYTES_PER_PIXEL;
                const int lineBytes = width * BYTES_PER_PIXEL;
                
                for (int y=0; y<rect.height; ++y)
                {
                    uint index = (lineBytes * (rect.y+y)) + (rect.x * BYTES_PER_PIXEL);
                    memcpy((char*)dest + index, (char*)buffer + index, rectLineBytes);
                }

                msg.rects << QRect(rect.x, rect.y, rect.width, rect.height);
            }
        }
    }
    
    memory.unlock();

    if (network->client && msg.rects.size() > 0)
        network->client->socket->write(msg.Serialize());
}

void RocketWebBrowser::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == timerId_)
        AdminotechCefEngine::Instance()->Update();
}

void RocketWebBrowser::OnCheckMainProcessAlive()
{
#ifdef __APPLE__
    if (!kill(parentProcessId, 0))
    {
        if (debugging)
            qDebug() << QString("Status report from %1: Rocket (pid: %2) lives").arg(id).arg(parentProcessId);
        return;
    }
    else
    {
        if (debugging)
            Log("Controlling parent process had died. Exiting RocketWebBrowser.");
        ExitProcess();
    }
#endif
}

void RocketWebBrowser::LoadStart(const CefString &url)
{    
    if (network->client)
    {
        BrowserProtocol::TypedStringMessage msg(BrowserProtocol::LoadStart, CefToQString(url));
        network->client->socket->write(msg.Serialize());
    }
}

void RocketWebBrowser::LoadEnd(const CefString &url)
{
    if (network->client)
    {
        BrowserProtocol::TypedStringMessage msg(BrowserProtocol::LoadEnd, CefToQString(url));
        network->client->socket->write(msg.Serialize());
    }
}

void RocketWebBrowser::OnUrlRequest(const BrowserProtocol::UrlMessage &msg)
{
    if (client)
        client->LoadUrl(msg.url);
}

void RocketWebBrowser::OnResizeRequest(const BrowserProtocol::ResizeMessage &msg)
{
    width = msg.width;
    height = msg.height;
        
    QString id = IpcId();
    if (memory.key().compare(id, Qt::CaseInsensitive) != 0)
    {
        if (memory.isAttached())
            memory.detach();

        memory.setKey(id);
        if (!memory.attach())
        {
            qDebug() << "ERROR - RocketWebBrowser: Failed to attach to resized memory segment:" << id;
            ExitProcess();
            return;
        }
    }
        
    if (client)
        client->Resize(msg.width, msg.height);
}

void RocketWebBrowser::OnMouseMoveRequest(const BrowserProtocol::MouseMoveMessage &msg)
{
    if (!client || !client->Host().get())
        return;
#ifdef ROCKET_CEF3_ENABLED
    CefMouseEvent e;
    e.x = msg.x;
    e.y = msg.y;
    PopulateMouseButtons(e);
    client->Host()->SendMouseMoveEvent(e, false);
#else
    client->Host()->SendMouseMoveEvent(msg.x, msg.y, false);
#endif
}

void RocketWebBrowser::OnMouseButtonRequest(const BrowserProtocol::MouseButtonMessage &msg)
{
    if (!client || !client->Host().get())
        return;
#ifdef ROCKET_CEF3_ENABLED
    mouseCurrentLeft_ = msg.left;
    mouseCurrentRight_ = msg.right;
    mouseCurrentMid_ = msg.mid;

    /// @todo Right click does not release properly!

    CefMouseEvent e;
    e.x = msg.x;
    e.y = msg.y;

    CefBrowserHost::MouseButtonType clickType = PopulateMouseButtons(e);

    if (msg.pressCount != 0)
    {
        // All buttons equals mouse wheel
        if (msg.left && msg.right && msg.mid)
            client->Host()->SendMouseWheelEvent(e, 0, msg.pressCount);
        else
            client->Host()->SendMouseClickEvent(e, clickType, false, static_cast<int>(msg.pressCount));
    }
    else
        client->Host()->SendMouseClickEvent(e, clickType, true, 1);
#else
    CefBrowser::MouseButtonType clickType = MBT_LEFT;
    if (msg.right)      clickType = MBT_RIGHT;
    else if (msg.mid)   clickType = MBT_MIDDLE;

    if (msg.pressCount != 0)
    {
        // All buttons equals mouse wheel
        if (msg.left && msg.right && msg.mid)
            client->Host()->SendMouseWheelEvent(msg.x, msg.y, 0, msg.pressCount);
        else
            client->Host()->SendMouseClickEvent(msg.x, msg.y, clickType, false, static_cast<int>(msg.pressCount));
    }
    else
        client->Host()->SendMouseClickEvent(msg.x, msg.y, clickType, true, 1);
#endif
}

#ifdef ROCKET_CEF3_ENABLED
CefBrowserHost::MouseButtonType RocketWebBrowser::PopulateMouseButtons(CefMouseEvent &e)
{
    e.modifiers = EVENTFLAG_NONE;
    // All buttons equals mouse wheel scroll
    if (!mouseCurrentLeft_ || !mouseCurrentRight_ || !mouseCurrentMid_)
    {
        if (mouseCurrentLeft_)
            e.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
        if (mouseCurrentRight_)
            e.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
        if (mouseCurrentMid_)
            e.modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    }
    return (mouseCurrentRight_ ? MBT_RIGHT : (mouseCurrentMid_ ? MBT_MIDDLE : MBT_LEFT));
}
#endif

void RocketWebBrowser::OnKeyboardRequest(const BrowserProtocol::KeyboardMessage &msg)
{
    if (!client || !client->Host().get())
        return;
#ifdef ROCKET_CEF3_ENABLED
    // See cef/cefclient/cefclient_osr_widget_win.cpp

    CefKeyEvent e;
    e.is_system_key = 0;
    e.type = (msg.type == 0 ? KEYEVENT_KEYUP : (msg.type == 1 ? KEYEVENT_RAWKEYDOWN : KEYEVENT_CHAR));

#ifdef WIN32
    e.native_key_code = msg.nativeKey;
    e.windows_key_code = msg.key;
    qDebug() << "Char" << e.native_key_code << e.windows_key_code;
#elif (__APPLE__)
    e.character = msg.character;
    e.unmodified_character = msg.characterNoModifiers;
    qDebug() << "Char" << e.character << e.unmodified_character;
#endif

    e.modifiers = EVENTFLAG_NONE;
    if (msg.modifiers & Qt::ShiftModifier)
    {
        e.modifiers |= EVENTFLAG_SHIFT_DOWN;
        qDebug() << "Shift";
    }
    if (msg.modifiers & Qt::ControlModifier)
    {
        e.modifiers |= EVENTFLAG_CONTROL_DOWN;
        qDebug() << "Ctrl";
    }
    if (msg.modifiers & Qt::AltModifier)
    {
        e.modifiers |= EVENTFLAG_ALT_DOWN;
        qDebug() << "Alt";
    }
    qDebug() << "";
    
    // Send key down/up
    client->Host()->SendKeyEvent(e);

    // Send character
    /*if (e.type == KEYEVENT_KEYDOWN)
    {
        e.type = KEYEVENT_CHAR;
        client->Host()->SendKeyEvent(e);
    }*/

#else
    #ifdef WIN32
    CefKeyInfo keyInfo;
    keyInfo.sysChar = false;
    keyInfo.imeChar = false;
    keyInfo.key = msg.key;

    client->Host()->SendKeyEvent((CefBrowser::KeyType)msg.type, keyInfo, msg.modifier);
    #elif (__APPLE__)
    CefKeyInfo keyInfo;
    keyInfo.keyCode = msg.key;
    keyInfo.character = msg.character;
    keyInfo.characterNoModifiers = msg.characterNoModifiers;

    client->Host()->SendKeyEvent((CefBrowser::KeyType)msg.type, keyInfo, msg.modifier);
    #endif
#endif
}

void RocketWebBrowser::OnTypedRequest(const BrowserProtocol::TypedMessage &msg)
{
    switch (msg.type)
    {
        case BrowserProtocol::FocusIn:
        {
            if (client)
                client->FocusIn();
            break;
        }
        case BrowserProtocol::FocusOut:
        {
            if (client)
                client->FocusOut();
            break;            
        }
        case BrowserProtocol::Invalidate:
        {
            if (client)
                client->Invalidate(0, 0, width, height);
            break;
        }
        case BrowserProtocol::Close:
        {
            ExitProcess();
            break;
        }
        default:
            break;
    }
}

QString QStringFromWCharArray(const wchar_t *string, int size)
{
    QString qstr;
    if (sizeof(wchar_t) == sizeof(QChar))
        return qstr.fromUtf16((const ushort *)string, size);
    else
        return qstr.fromUcs4((uint *)string, size);
}

QString WStringToQString(const std::wstring &str)
{
    if (str.length() == 0)
        return "";
    return QStringFromWCharArray(str.data(), static_cast<int>(str.size()));
}

QString RocketWebBrowser::UserDataDirectory()
{
#ifdef WIN32
    LPITEMIDLIST pidl;
    if (SHGetFolderLocation(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, &pidl) != S_OK)
        return "";

    WCHAR str[MAX_PATH+1] = {};
    SHGetPathFromIDListW(pidl, str);
    CoTaskMemFree(pidl);

    return WStringToQString(str) + QDir::separator() + "Rocket" + QDir::separator();
#else
    char *ppath = 0;
    ppath = getenv("HOME");
    if (ppath == 0)
        ppath = "~";
    return QString(ppath) + QDir::separator() + ".Rocket" + QDir::separator();
#endif
}

QString RocketWebBrowser::ApplicationDirectory()
{
#ifdef WIN32
    WCHAR str[MAX_PATH+1] = {};
    DWORD success = GetModuleFileNameW(0, str, MAX_PATH);
    if (success == 0)
        return ".\\";
    QString qstr = WStringToQString(str);
    // The module file name also contains the name of the executable, so strip it off.
    int trailingSlash = qstr.lastIndexOf('\\');
    if (trailingSlash == -1)
        return ".\\"; // Some kind of error occurred.
    return qstr.left(trailingSlash+1); // +1 so that we return the trailing slash as well.
#elif defined(__APPLE__)
    char path[1024];
    uint32_t size = sizeof(path)-2;
    int ret = _NSGetExecutablePath(path, &size);
    if (ret == 0 && size > 0)
    {
        // The returned path also contains the executable name, so strip that off from the path name.
        QString p = path;
        int lastSlash = p.lastIndexOf("/");
        if (lastSlash != -1)
            p = p.left(lastSlash+1);
        return p;
    }
    else
        return "./"; 
#else
    return "./";
#endif
}

#ifdef ROCKET_WEBBROWSER_EXTERNAL_BUILD
#include "moc_RocketWebBrowser.cxx"
#endif
