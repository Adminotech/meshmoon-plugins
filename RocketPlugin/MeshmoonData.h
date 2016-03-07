/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "AssetFwd.h"
#include "CoreTypes.h"
#include "AssetReference.h"

#include <QUrl>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QEasingCurve>
#include <QImage>
#include <QVariantList>
#include <QList>
#include <QPair>
#include <QHash>

/// @cond PRIVATE

namespace Meshmoon
{
    // MEP

    struct MEP
    {
        QString id;
        QString name;

        bool IsValid() const
        {
            return (!id.isEmpty() && !name.isEmpty());
        }

        QString toString() const
        {
            return QString("id=%1 name=%2").arg(id).arg(name);
        }
    };
    typedef QList<MEP> MEPList;

    // User

    struct User
    {
        u32 connectionId;

        QByteArray hash;
        QString name;
        QString gender;

        QUrl pictureUrl;
        QUrl avatarAppearanceUrl;

        MEPList meps;

        User() : connectionId(0) {}

        void Reset()
        {
            *this = User();
        }

        bool IsInMep(const MEP &other)
        {
            if (meps.isEmpty())
                return false;
            foreach (const MEP &mep, meps)
                if (mep.id.compare(other.id, Qt::CaseSensitive) == 0)
                    return true;
            return false;
        }
        
        QString toString() { return QString("name=%1 gender=%2 hash=%3 avatar=%4 picture=%5 meps=%6").arg(name).arg(gender).arg(QString(hash))
                                .arg(avatarAppearanceUrl.toString()).arg(pictureUrl.toString()).arg(meps.isEmpty() ? "false" : "true"); }
    };

    // Server

    struct Server
    {
        // Basic information
        QString name;
        QString company;
        QString txmlUrl;
        QString txmlId;

        // Images
        QString txmlPictureUrl;
        QString companyPictureUrl;

        // Grouping
        QString category;
        QStringList tags;

        // Stats
        uint loginCount;

        // MEP
        MEP mep;

        // Mode
        bool running;
        bool isPublic;
        bool isUnderMaintainance;

        // Permissions
        bool isAdministratedByCurrentUser;

        Server() :
            loginCount(0),
            running(false),
            isPublic(false),
            isUnderMaintainance(false),
            isAdministratedByCurrentUser(false)
        {
        }

        void Reset()
        {
            *this = Server();
        }

        void StoreScreenShot(Framework *framework);
        QImage LoadScreenShot(Framework *framework);
        QString GetPictureFilePath();

        bool CanProcessShots() const { return (!name.isEmpty() && !company.isEmpty()); }
    };

    // PromotedServer

    struct PromotedServer
    {
        QString type;
        uint durationMsec;

        // type == "world"
        QString name;
        QString txmlUrl;
        QString smallPicture;
        QString largePicture;

        // type == "web"
        QString webUrl;

        // type == "action"
        QString actionText;

        PromotedServer() : durationMsec(5000) {}

        bool Equals(const Server &server) const
        {
            return server.txmlUrl.compare(txmlUrl, Qt::CaseInsensitive) == 0;
        }
        
        bool Matches(const PromotedServer &other) const
        {
            if (type != other.type || durationMsec != other.durationMsec)
                return false;
            if (type == "world")
                return (name == other.name && txmlUrl == other.txmlUrl && smallPicture == other.smallPicture && largePicture == other.largePicture);
            else if (type == "web")
                return (webUrl == other.webUrl);
            else if (type == "action")
                return (actionText == other.actionText);
            return false;
        }
        
        Server ToServer() const
        {
            Server server;
            server.name = name;
            server.txmlUrl = txmlUrl;
            return server;
        }
    };

    // ServerStartReply

    struct ServerStartReply
    {
        QUrl loginUrl;
        bool started;
        
        ServerStartReply() : started(false) {}
    };

    // Stats

    struct Stats
    {
        uint uniqueLoginCount;
        uint hostedSceneCount;

        Stats() : uniqueLoginCount(0), hostedSceneCount(0) {}
    };
    
    // OnlineServerStats

    struct ServerOnlineStats
    {
        QString id;
        QStringList userNames;
        uint userCount;
        
        ServerOnlineStats() : userCount(0) {}

        void Reset()
        {
            userCount = 0;
            userNames.clear();
        }
    };
    typedef QList<ServerOnlineStats> ServerOnlineStatsList;
    
    // OnlineStats
    
    struct OnlineStats
    {
        uint onlineUserCount;
        ServerOnlineStatsList spaceUsers;

        OnlineStats() : onlineUserCount(0) {}
    };

    // ServerScore

    struct ServerScore
    {
        QString txmlId;

        uint day;
        uint week;
        uint month;
        uint threeMonths;
        uint sixMonths;
        uint year;

        enum SortCriteria
        {
            TopDay = 0,
            TopWeek,
            TopMonth,
            TopThreeMonth,
            TopSixMonth,
            TopYear,
            TotalLoginCount,
            AlphabeticalByName,
            AlphabeticalByAccount,
            SortCriterialNumItems
        };

        static QString SortCriteriaToString(SortCriteria criterial)
        {
            switch(criterial)
            {
                case TopDay: return "Trending today";
                case TopWeek: return "Trending this week";
                case TopMonth: return "Trending this month";
                case TopThreeMonth: return "Popularity last three months";
                case TopSixMonth: return "Popularity last six months";
                case TopYear: return "Popularity"; // This UI name is made more generic as its the default, we want to keep it simple
                case AlphabeticalByName: return "Alphabetically by name";
                case AlphabeticalByAccount: return "Alphabetically by account";
                case TotalLoginCount: return "All time world visitors";
                default:
                    break;
            }
            return "";
        }

        ServerScore() : day(0), week(0), month(0), threeMonths(0), sixMonths(0), year(0) {};

        bool HasData() const                  { return !txmlId.isEmpty(); }
        uint DayCumulative() const            { return day; }
        uint WeekCumulative() const           { return DayCumulative() + week; }
        uint MonthCumulative() const          { return WeekCumulative() + month; }
        uint ThreeMonthsCulumative() const    { return MonthCumulative() + threeMonths; }
        uint SixMonthsCumulative() const      { return ThreeMonthsCulumative() + sixMonths; }
        uint YearCumulative() const           { return SixMonthsCumulative() + year; }

        QString toString() const { return QString("day=%1 week=%2 month=%3 3month=%4 6month=%5 year=%6")
            .arg(DayCumulative()).arg(WeekCumulative()).arg(MonthCumulative())
            .arg(ThreeMonthsCulumative()).arg(SixMonthsCumulative()).arg(YearCumulative()); }
    };
    typedef QHash<QString, ServerScore> ServerScoreMap;

    // NewsItem

    struct NewsItem
    {
        QString title;
        QString message;
        bool important;
        
        NewsItem() : important(false) {}
    };
    typedef QList<NewsItem> NewsList;

    // Avatar

    struct Avatar
    {
        // Always there properties.
        QString name;
        QString type;
        QString assetRef;
        QString assetRefArchive;
        QString imageUrl;

        // If empty inherit from parent.
        QString gender;
        QString author;
        QString authorUrl;
        
        // Child variations
        QList<Avatar> variations;
        
        Avatar() {}
    };
    typedef QList<Avatar> AvatarList;
    
    // RelevancyFilter - Helper struct to filter relevant suggestions to a term from arbitrary strings.

    struct RelevancyFilter
    {
        QString term;
        Qt::CaseSensitivity sensitivity;
        
        QMap<QString, QString> direct;
        QMap<QString, QString> indirect;
        
        RelevancyFilter(const QString &term_, Qt::CaseSensitivity sensitivity_);
        
        bool HasDirectHits();
        bool HasIndirectHits();
        bool HasHits();
        
        QStringList DirectValues();
        QStringList IndirectValues();
        QStringList AllValues();
        
        QString GuaranteeStartsWithCase(const QString &suggestion);

        bool Match(const QString &suggestion);
        bool MatchIndirect(const QString &suggestion);
        bool MatchIndirectParts(const QString &suggestion);

        bool DirectSuggestion(const QString &suggestion);
        bool IndirectSuggestion(const QString &suggestion);
    };

    // Json

    class Json
    {
    public:
        static ServerWidgetList DeserializeServers(const QByteArray &data, Framework *framework, const QStringList &filterIds);
        static PromotedWidgetList DeserializePromoted(const QByteArray &data, Framework *framework, const PromotedWidgetList &existingPromotions);
        static ServerStartReply DeserializeServerStart(const QByteArray &data);
        static void DeserializeUser(const QByteArray &data, User &user);
        static Stats DeserializeStats(const QByteArray &data);
        static OnlineStats DeserializeOnlineStats(const QByteArray &data);
        static void DeserializeServerStatsList(const QVariantList &source, ServerOnlineStatsList &dest);
        static ServerScoreMap DeserializeScores(const QByteArray &data);
        static NewsList DeserializeNews(const QByteArray &data);
        static AvatarList DeserializeAvatars(const QByteArray &data);
        static AvatarList DeserializeAvatarVariations(const QVariantList &variationsVariant);
    };
}

/// @endcond
