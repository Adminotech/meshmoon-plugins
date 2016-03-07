/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "RocketMediaPlayer.h"

#ifdef Q_OS_WIN
#include "windows.h"
#endif

#include <string>
#include <vector>

#include <QtGlobal>
#include <QCoreApplication>
#include <QDebug>

class RocketMediaPlayerApplication : public QCoreApplication
{
public:
    RocketMediaPlayerApplication(int argc, char **argv) :
        QCoreApplication(argc, argv),
        isExiting_(false)
    {
    }

#ifdef Q_OS_WIN
    bool RocketMediaPlayerApplication::winEventFilter(MSG *msg, long *result)
    {
        if (!msg)
            return false;

        // This (WM_CLOSE) is sent on windows to a process when QProcess::terminate() is called.
        // QCoreApplication does not do anything on this message (QApplication does) so we have 
        // to handle it manually here. Close the app in a controlled manner.
        if (msg->message == WM_CLOSE || msg->message == WM_DESTROY || msg->message == WM_QUIT)
        {
            if (!isExiting_)
            {
                isExiting_ = true;
                quit();
                return true;
            }
        }

        Q_UNUSED(result);
        return false;
    }
#endif

private:
    bool isExiting_;
};

int main(int argc, char **argv)
{
    RocketMediaPlayerApplication app(argc, argv);
    RocketMediaPlayer player(argc, argv);
    if (!player.IsStarted())
        return 1;
    QObject::connect(&app, SIGNAL(aboutToQuit()), &player, SLOT(Cleanup()));
    int ret = app.exec();
    if (player.debugging)
        qDebug() << "app.exec() returned with" << ret;
    return ret;
}
