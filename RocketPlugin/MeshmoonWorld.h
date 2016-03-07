/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "MeshmoonData.h"

#include <QObject>
#include <QString>

/// Provides information about the current %Meshmoon world.
/** @ingroup MeshmoonRocket */
class MeshmoonWorld : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ Name)                      /**< @copydoc Name */
    Q_PROPERTY(QString id READ Id)                          /**< @copydoc Id */
    Q_PROPERTY(QString MEPName READ MEPName)                /**< @copydoc MEPName */
    Q_PROPERTY(QString MEPId READ MEPId)                    /**< @copydoc MEPId */
    Q_PROPERTY(bool public READ IsPublic)                   /**< @copydoc IsPublic */
    Q_PROPERTY(MeshmoonUserList users READ Users)           /**< @copydoc Users */

public:
    explicit MeshmoonWorld(RocketPlugin *plugin);
    ~MeshmoonWorld();

    /// World name.
    QString Name() const;

    /// Unique world id.
    QString Id() const;

    /// Worlds Meshmoon Education Program name.
    /** Name is empty if this world is not part of any MEP. */
    QString MEPName() const;

    /// Worlds Meshmoon Education Program id.
    /** Id is empty if this world is not part of any MEP. */
    QString MEPId() const;

    /// Returns if world is public.
    bool IsPublic() const;
    
    /// Returns the worlds users.
    /** @note The MeshmoonUser* will not contain anything but connection id, name and permissions level.
        If you need to more information about the logged in user on the client, use RocketPlugin::User(). */
    MeshmoonUserList Users() const;

    /// @cond PRIVATE
    void Clear();
    void SetServer(const Meshmoon::Server &server);

    void ClientJoined(const char* data, size_t numBytes);
    void ClientLeft(const char* data, size_t numBytes);
    /// @endcond

public slots:
    /// Returns a world user with connection id. Null ptr if not found.
    /** @note The MeshmoonUser* will not contain anything but connection id, name and permissions level.
        If you need to more information about the logged in user on the client, use RocketPlugin::User(). */
    MeshmoonUser *User(u32 connectionId) const;
    
    QString toString() { return QString("MeshmoonWorld(id = %1, name = %2, public = %3, %4 users)")
        .arg(Id()).arg(Name()).arg(IsPublic() ? "true" : "false").arg(users_.size()); }

signals:
    void UserJoined(MeshmoonUser *user);
    void UserLeft(MeshmoonUser *user);

private:
    void RemoveUser(u32 connectionId);
    void SetWindowTitle(const QString &title);

    RocketPlugin *plugin_;

    QString name_;
    QString id_;
    QString mepName_;
    QString mepId_;

    bool public_;
    MeshmoonUserList users_;
};
Q_DECLARE_METATYPE(MeshmoonWorld*)
