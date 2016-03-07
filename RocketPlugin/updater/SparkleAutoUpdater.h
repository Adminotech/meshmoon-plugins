/*
 * Copyright (C) 2008 Remko Troncon
 */

#ifndef SPARKLEAUTOUPDATER_H
#define SPARKLEAUTOUPDATER_H

#include <QString>

#include "AutoUpdater.h"

class Framework;

class SparkleAutoUpdater : public AutoUpdater
{
    public:
        SparkleAutoUpdater(Framework* fw, const QString& url);
        ~SparkleAutoUpdater();

        void checkForUpdates(QString& parameter);
        void KillTundra();
        bool IsTundraExiting();
        bool IsSilentChecking();
        QString GetVersion();
	
    private:
        class Private;
        Private* d;
        Framework* framework;
        bool silent;
};

#endif
