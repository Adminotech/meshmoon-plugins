/*
 * Copyright (C) 2008 Remko Troncon
 */

#include "SparkleAutoUpdater.h"

#include <Cocoa/Cocoa.h>
#include <Sparkle/Sparkle.h>

#import "UpdaterDelegate.h"

#include "Framework.h"
#include "Application.h"

class SparkleAutoUpdater::Private
{
    public:
        SUUpdater* updater;
        UpdaterDelegate* delegate;
};

SparkleAutoUpdater::SparkleAutoUpdater(Framework* fw, const QString& aUrl) :
    silent(true)
{
    d = new Private;
    framework = fw;

    d->updater = [SUUpdater sharedUpdater];
    [d->updater retain];

    d->delegate = [[UpdaterDelegate alloc] init];
    [d->delegate retain];
    [d->delegate setCPPUpdater:this];
    [d->updater setDelegate:d->delegate];

    NSURL* url = [NSURL URLWithString:
        [NSString stringWithUTF8String: aUrl.toUtf8().data()]];
    [d->updater setFeedURL: url];
}

SparkleAutoUpdater::~SparkleAutoUpdater()
{
    [d->updater release];
    [d->delegate release];
    delete d;
}

void SparkleAutoUpdater::checkForUpdates(QString& parameter)
{
    // Always treat update checking as 'silent' one, if not otherwise explicitly requested
    silent = true;
    if (parameter == "/checknow")
        silent = false;

    [d->updater checkForUpdatesInBackground];
}

void SparkleAutoUpdater::KillTundra()
{
    framework->Exit();
}

bool SparkleAutoUpdater::IsTundraExiting()
{
    return framework->IsExiting();
}

bool SparkleAutoUpdater::IsSilentChecking()
{
    return silent;
}

QString SparkleAutoUpdater::GetVersion()
{
    return framework->App()->Version();
}
