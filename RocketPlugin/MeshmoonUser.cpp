/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MeshmoonUser.h"

#include "CoreDefines.h"
#include "LoggingFunctions.h"

#include <QTimer>

#include "MemoryLeakCheck.h"

// RocketUserAuthenticator

RocketUserAuthenticator::RocketUserAuthenticator(u32 _connectionId) :
    connectionId(_connectionId)
{
}

void RocketUserAuthenticator::EmitCompleted(Meshmoon::PermissionLevel permissionLevel)
{
    emit Completed(static_cast<int>(permissionLevel), PermissionLevelToString(permissionLevel));
}

// MeshmoonUser

MeshmoonUser::MeshmoonUser() :
    connectionId_(0),
    levelAcquired_(false),
    permissionLevel_(Meshmoon::Basic)
{
}

MeshmoonUser::~MeshmoonUser()
{
    Clear();
}

void MeshmoonUser::SetClientConnectionId(u32 connectionId)
{
    connectionId_ = connectionId;
    
    // If we received auth requests before connection ID was set it is -1.
    // Set it here to the authenticators so they will emit correctly.
    foreach(RocketUserAuthenticator *auth, authenticators_)
        if (auth && auth->connectionId == 0)
            auth->connectionId = connectionId_;
}

void MeshmoonUser::Clear()
{
    // Do not reset userData_ as it can be valid for numerous logins!

    connectionId_ = 0;
    levelAcquired_ = false;
    permissionLevel_ = Meshmoon::Basic;
    
    foreach(RocketUserAuthenticator *auth, authenticators_)
        SAFE_DELETE(auth);
    authenticators_.clear();
}

void MeshmoonUser::Process()
{
    if (levelAcquired_ == false)
        return;

    for(int i=0; i<authenticators_.size(); ++i)
    {
        RocketUserAuthenticator *auth = authenticators_[i];
        if (auth && auth->connectionId == connectionId_)
        {
            auth->EmitCompleted(permissionLevel_);

            authenticators_.removeAt(i);
            SAFE_DELETE(auth);
            break;
        }
    }
}

void MeshmoonUser::SetPermissionLevel(Meshmoon::PermissionLevel permissionLevel, bool log)
{
    if (log)
        LogInfo("[MeshmoonUser]: Received scene permission level " + PermissionLevelToString(permissionLevel));
    
    levelAcquired_ = true;
    permissionLevel_ = permissionLevel;

    Process();
}

QString MeshmoonUser::Name() const
{
    return userData_.name;
}

QString MeshmoonUser::Hash() const
{
    return QString(userData_.hash);
}

QString MeshmoonUser::Gender() const
{
    return userData_.gender;
}

QString MeshmoonUser::ProfilePictureUrl() const
{
    return userData_.pictureUrl.toString();
}

QString MeshmoonUser::AvatarAppearanceUrl() const
{
    return userData_.avatarAppearanceUrl.toString();
}

QStringList MeshmoonUser::MEPNames() const
{
    QStringList names;
    foreach(const Meshmoon::MEP &mep, userData_.meps)
        names << mep.name;
    return names;
}

QStringList MeshmoonUser::MEPIds() const
{
    QStringList ids;
    foreach(const Meshmoon::MEP &mep, userData_.meps)
        ids << mep.id;
    return ids;
}

QString MeshmoonUser::MEPNameById(const QString &id) const
{
    foreach(const Meshmoon::MEP &mep, userData_.meps)
        if (mep.id.compare(id, Qt::CaseSensitive) == 0)
            return mep.name;
    return "";
}

u32 MeshmoonUser::ConnectionId() const
{
    return connectionId_;
}

Meshmoon::PermissionLevel MeshmoonUser::PermissionLevel() const
{
    return permissionLevel_;
}

QString MeshmoonUser::PermissionLevelString() const
{
    return PermissionLevelToString(permissionLevel_);
}

int MeshmoonUser::PermissionLevelNumber() const
{
    return static_cast<int>(permissionLevel_);
}

RocketUserAuthenticator *MeshmoonUser::RequestAuthentication()
{  
    // Return a existing authenticator for multiple requests if already initialized.
    RocketUserAuthenticator *auth = Authenticator(connectionId_);
    if (!auth)
    {
        auth = new RocketUserAuthenticator(connectionId_);
        authenticators_ << auth;
    }
    
    // If level is known, trigger authenticator after 1msec
    if (levelAcquired_)
        QTimer::singleShot(1, this, SLOT(Process()));

    return auth;
}

RocketUserAuthenticator *MeshmoonUser::Authenticator(u32 connectionId) const
{
    foreach(RocketUserAuthenticator *auth, authenticators_)
        if (auth && auth->connectionId == connectionId)
            return auth;
    return 0;
}

void MeshmoonUser::OnAuthenticated(const Meshmoon::User &userData)
{
    userData_ = userData;
}

void MeshmoonUser::OnAuthReset()
{
    userData_.Reset();
}
