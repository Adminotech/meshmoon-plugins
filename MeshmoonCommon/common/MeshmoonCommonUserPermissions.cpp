
#include "StableHeaders.h"

#include "common/MeshmoonCommon.h"

namespace Meshmoon
{
    UserPermissions::UserPermissions() :
        connectionId(0),
        level(Basic)
    {
    }
        
    UserPermissions::UserPermissions(uint _connectionId, const QString &_username, const QString &_userhash) :
        connectionId(_connectionId),
        level(Basic),
        username(_username),
        userhash(_userhash)
    {
    }

    void UserPermissions::Reset()
    {
        *this = UserPermissions();
    }
}
