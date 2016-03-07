/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MeshmoonWorld.h"
#include "MeshmoonUser.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "Application.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include "MsgClientJoined.h"
#include "MsgClientLeft.h"

#include "MemoryLeakCheck.h"

MeshmoonWorld::MeshmoonWorld(RocketPlugin *plugin) :
    plugin_(plugin)
{
    Clear();
}

MeshmoonWorld::~MeshmoonWorld()
{
    Clear();
}

QString MeshmoonWorld::Name() const
{
    return name_;
}

QString MeshmoonWorld::Id() const
{
    return id_;
}

QString MeshmoonWorld::MEPName() const
{
    return mepName_;
}

QString MeshmoonWorld::MEPId() const
{
    return mepId_;
}

bool MeshmoonWorld::IsPublic() const
{
    return public_;
}

MeshmoonUserList MeshmoonWorld::Users() const
{
    return users_;
}

MeshmoonUser *MeshmoonWorld::User(u32 connectionId) const
{
    for(int i=0; i<users_.length(); ++i)
        if (users_[i]->ConnectionId() == connectionId)
            return users_[i];
    return 0;
}

void MeshmoonWorld::Clear()
{
    name_ = "";
    id_ = "";
    mepName_ = "";
    mepId_ = "";
    public_ = false;
    
    foreach(MeshmoonUser *user, users_)
        SAFE_DELETE(user);
    users_.clear();
    
    SetWindowTitle(Application::FullIdentifier());
}

void MeshmoonWorld::SetServer(const Meshmoon::Server &server)
{
    name_ = server.name;
    id_ = server.txmlId;
    mepName_ = server.mep.IsValid() ? server.mep.name : "";
    mepId_ = server.mep.IsValid() ? server.mep.id : "";
    public_ = server.isPublic;
    
    if (!name_.isEmpty())
        SetWindowTitle(Application::FullIdentifier() + " - " + name_);
}

void MeshmoonWorld::SetWindowTitle(const QString &title)
{
    if (!plugin_->GetFramework()->IsHeadless() && plugin_->GetFramework()->Ui()->MainWindow())
        plugin_->GetFramework()->Ui()->MainWindow()->setWindowTitle(title);
}

void MeshmoonWorld::ClientJoined(const char* data, size_t numBytes)
{
    MsgClientJoined msg(data, numBytes);
    RemoveUser(msg.userID);
    
    
    MeshmoonUser *user = new MeshmoonUser();
    user->connectionId_ = msg.userID;
    user->userData_.name = msg.username;
    
    users_ << user;
    emit UserJoined(user);
}

void MeshmoonWorld::ClientLeft(const char* data, size_t numBytes)
{
    MsgClientLeft msg(data, numBytes);
    MeshmoonUser *user = User(msg.userID);
    if (user)
        emit UserLeft(user);
    RemoveUser(msg.userID);
}

void MeshmoonWorld::RemoveUser(u32 connectionId)
{
    for(int i=0; i<users_.length(); ++i)
    {
        MeshmoonUser *user = users_[i];
        if (!user || user->ConnectionId() == connectionId)
        {
            SAFE_DELETE(user);
            users_.removeAt(i);
            i--;
        }
    }
}
