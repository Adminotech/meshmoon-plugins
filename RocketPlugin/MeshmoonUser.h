/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "common/MeshmoonCommon.h"
#include "MeshmoonData.h"

#include <QObject>
#include <QList>

/// Authenticator for requesting permission level.
/** When you receive this object connect to the Completed signal for the users permission level.
    @ingroup MeshmoonRocket */
class RocketUserAuthenticator : public QObject
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    RocketUserAuthenticator(u32 _connectionId);
    
    u32 connectionId;
    void EmitCompleted(Meshmoon::PermissionLevel permissionLevel);
    /// @endcond

signals:
    /// @todo the permissionLevelString seems totally unnecessary - remove?
    void Completed(int permissionLevel, const QString &permissionLevelString);
};
Q_DECLARE_METATYPE(RocketUserAuthenticator*)

/// Provides information about currently the authenticated.
/** All third-party code should have single path for asking authentication: 
    rocket.user.RequestAuthentication().Completed.connect(this, this.onAuthenticated);
    @ingroup MeshmoonRocket */
class MeshmoonUser : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ Name)
    Q_PROPERTY(QString hash READ Hash)
    Q_PROPERTY(QString gender READ Gender)
    Q_PROPERTY(QString profilePictureUrl READ ProfilePictureUrl)
    Q_PROPERTY(QString avatarAppearanceUrl READ AvatarAppearanceUrl)
    Q_PROPERTY(u32 connectionId READ ConnectionId)

public:
    /// @cond PRIVATE
    MeshmoonUser();
    ~MeshmoonUser();

    void SetClientConnectionId(u32 connectionId);
    void SetPermissionLevel(Meshmoon::PermissionLevel permissionLevel, bool log);
    void Clear();

    friend class RocketPlugin;
    friend class MeshmoonWorld;
    /// @endcond
    
public slots:
    /// Request permission level authenticator object.
    /** Connect to the returned objects Completed signal and wait for
        it to be triggered for the results. */
    RocketUserAuthenticator *RequestAuthentication();

    /// Get name of currently authenticated user.
    QString Name() const;
    
    /// Get hash of currently authenticated user.
    QString Hash() const;
    
    /// Get gender of currently authenticated user.
    QString Gender() const;
    
    /// Get profile image url of currently authenticated user.
    QString ProfilePictureUrl() const;
    
    /// Get avatar appearance url of currently authenticated user.
    QString AvatarAppearanceUrl() const;

    /// Returns Meshmoon Education Program names that this user is part of.
    QStringList MEPNames() const;

    /// Returns Meshmoon Education Program ids that this user is part of.
    QStringList MEPIds() const;

    /// Returns Meshmoon Education Program name for id.
    QString MEPNameById(const QString &id) const;

    /// Get connection id of currently authenticated user.
    u32 ConnectionId() const;

    /// Get permission level of this user.
    Meshmoon::PermissionLevel PermissionLevel() const;

    /// Get permission level of this user as a string eg. "Basic". 
    /** Exposed for scripting so you don't have to work with enums, which can be a pain. */
    QString PermissionLevelString() const;

    /// Get permission level of this user as a number.
    /** Exposed for scripting so you don't have to work with enums, which can be a pain. */
    int PermissionLevelNumber() const;

    QString toString() { return QString("MeshmoonUser(connectionId = %1, name = %2, permissions = %3)").arg(ConnectionId()).arg(Name()).arg(PermissionLevelString()); }
    
private slots:
    RocketUserAuthenticator *Authenticator(u32 connectionId) const;
    void Process();

    void OnAuthenticated(const Meshmoon::User &userData);
    void OnAuthReset();

private:
    u32 connectionId_;
    bool levelAcquired_;
    
    Meshmoon::User userData_;
    Meshmoon::PermissionLevel permissionLevel_;
    
    QList<RocketUserAuthenticator*> authenticators_;
};
Q_DECLARE_METATYPE(MeshmoonUser*)
Q_DECLARE_METATYPE(MeshmoonUserList)
