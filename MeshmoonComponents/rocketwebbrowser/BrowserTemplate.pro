#-------------------------------------------------
#
# Project created by QtCreator 2013-01-29T22:01:58
#
#-------------------------------------------------

QT       += core network
QT       -= gui

TARGET = RocketWebBrowser
CONFIG   += console
CONFIG   -= app_bundle

DEFINES += ADMINO_CEF_ENABLED
DEFINES += ADMINO_CEF_IPC_ENABLED
DEFINES += ROCKET_WEB_BROWSER
DEFINES += ROCKET_BROWSER_CONSOLE
QMAKE_CXXFLAGS += -fvisibility=hidden
INCLUDEPATH += CEF_DIR_HERE
INCLUDEPATH += CEF_DIR_HERE/include

LIBS += -LCEF_DIR_HERE/Release -lcef
LIBS += -LCEF_DIR_HERE/xcodebuild/Release -lcef_dll_wrapper

TEMPLATE = app


SOURCES += main.cpp \
    BrowserNetwork.cpp \
    RocketWebBrowser.cpp \
    CefIntegration.cpp

HEADERS += \
    BrowserNetworkMessages.h \
    BrowserNetwork.h \
    RocketWebBrowser.h \
    CefIntegration.h
