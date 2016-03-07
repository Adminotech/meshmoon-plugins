/*
 * Copyright (C) 2008 Remko Troncon
 */

#ifndef AUTOUPDATER_H
#define AUTOUPDATER_H

#include <QString>

class AutoUpdater
{
	public:
		virtual ~AutoUpdater();

		virtual void checkForUpdates(QString &) = 0;
};

#endif
