/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "MeshmoonData.h"
#include "storage/MeshmoonStorage.h"
#include "RocketLobbyWidgets.h"
#include "RocketPlugin.h"
#include "oculus/RocketOculusManager.h"

#include "Framework.h"
#include "CoreJsonUtils.h"
#include "Application.h"
#include "AssetAPI.h"
#include "IRenderer.h"
#include "EC_Camera.h"
#include "TundraLogicModule.h"
#include "Client.h"
#include "LoggingFunctions.h"

#include <QWidget>
#include <QVariant>
#include <QDir>
#include <QFile>

#include "MemoryLeakCheck.h"

/// @cond PRIVATE

namespace Meshmoon
{
    // MEP

    MEP DeserializeMEP(const QVariantMap &source)
    {
        MEP mep;
        if (source.contains("Id") && source.contains("Name"))
        {
            mep.id = source["Id"].toString();
            mep.name = source["Name"].toString();
        }
        return mep;
    }

    MEPList DeserializeMEPList(const QVariantList &source)
    {
        MEPList meps;
        foreach(const QVariant &var, source)
        {
            MEP mep = DeserializeMEP(var.toMap());
            if (mep.IsValid())
                meps << mep;
        }
        return meps;
    }

    // Json
    
    ServerWidgetList Json::DeserializeServers(const QByteArray &data, Framework *framework, const QStringList &filterIds)
    {
        ServerWidgetList servers;
        
        bool ok = false;
        QVariantList serverList = TundraJson::Parse(data, &ok).toList();
        if (ok)
        {
            foreach(const QVariant &serverVariant, serverList)
            {
                QVariantMap serverData = serverVariant.toMap();
                if (serverData.contains("CompanyName") && serverData.contains("TxmlName") && serverData.contains("TxmlUrl"))
                {
                    Server server;

                    // Basic information
                    server.name = serverData["TxmlName"].toString().trimmed();
                    server.company = serverData["CompanyName"].toString().trimmed();
                    server.txmlUrl = serverData["TxmlUrl"].toString();
                    server.txmlId = serverData["TxmlId"].toString();

                    // If filters are defined, we will simply ignore servers that are not in the filter list.
                    if (!filterIds.isEmpty() && !server.txmlId.isEmpty() && !filterIds.contains(server.txmlId))
                        continue;

                    // Images
                    if (serverData.contains("TxmlImageUrl"))
                        server.txmlPictureUrl = serverData["TxmlImageUrl"].toString();
                    if (serverData.contains("CompanyImageUrl"))
                        server.companyPictureUrl = serverData["CompanyImageUrl"].toString();

                    // Grouping
                    if (serverData.contains("Category"))
                        server.category = serverData["Category"].toString();
                    if (serverData.contains("Tags"))
                        server.tags =  serverData["Tags"].toStringList();

                    // Stats        
                    if (serverData.contains("Logincount"))
                        server.loginCount = serverData["Logincount"].toUInt();

                    // MEP, auto add MEP tag if in one
                    if (serverData.contains("MEP"))
                    {
                        server.mep = DeserializeMEP(serverData["MEP"].toMap());
                        if (server.mep.IsValid())
                            server.tags << "MEP";
                    }

                    // Mode
                    server.running = serverData.contains("Online") ? serverData["Online"].toBool() : false;
                    server.isPublic = serverData.contains("Public") ? serverData["Public"].toBool() : false;
                    server.isUnderMaintainance = serverData.contains("Maintenance") ? serverData["Maintenance"].toBool() : false;
                    server.isAdministratedByCurrentUser = serverData.contains("Admin") ? serverData["Admin"].toBool() : false;

                    servers << new RocketServerWidget(framework, server);
                }
                else
                    LogWarning("Server JSON does not have enough data to construct a server object.");
            }
        }
        else
            LogError("JSON parse error while deserializing servers.");
        return servers;
    }
    
    PromotedWidgetList Json::DeserializePromoted(const QByteArray &data, Framework *framework, const PromotedWidgetList &existingPromotions)
    {
        /** The use of existingPromotions looks pretty weird here, but we are saving 
            ~10-25 msec with this checking by not allocating new RocketPromotionWidgets
            if they already exist. If they have indeed changed (very rare) this function
            is slower as double parsing is performed. */
        bool allSkipped = true;

        PromotedWidgetList promoted;

        bool ok = false;
        QVariantList serverList = TundraJson::Parse(data, &ok).toList();
        if (ok)
        {
            foreach(const QVariant &serverVariant, serverList)
            {
                QVariantMap serverData = serverVariant.toMap();
                if (!serverData.empty())
                {
                    QString type = serverData.contains("Type") ? serverData["Type"].toString().toLower() : "world";
                    
                    // World promotions
                    if (type.isEmpty() || type == "world")
                    {
                        PromotedServer promo;
                        promo.type = type;
                        promo.durationMsec = (serverData.contains("Duration") ? serverData["Duration"].toInt() * 1000 : 5000);
                        promo.name = serverData["Title"].toString();
                        promo.smallPicture = serverData["SmallPicture"].toString();
                        promo.largePicture = serverData["LargePicture"].toString();
                        promo.txmlUrl = serverData["TxmlUrl"].toString();
                        
                        bool exists = false;
                        foreach(const RocketPromotionWidget *existingPromotion, existingPromotions)
                        {
                            if (existingPromotion && existingPromotion->data.Matches(promo))
                            {
                                exists = true;
                                break;
                            }
                        }
                        if (exists)
                            continue;
                        allSkipped = false;

                        promoted << new RocketPromotionWidget(framework, promo);
                    }
                    // Web URL
                    else if (type == "web")
                    {
                        PromotedServer promo;
                        promo.type = type;
                        promo.durationMsec = (serverData.contains("Duration") ? serverData["Duration"].toInt() * 1000 : 5000);
                        promo.smallPicture = serverData["SmallPicture"].toString();
                        promo.webUrl = serverData["WebUrl"].toString();
                        
                        bool exists = false;
                        foreach(const RocketPromotionWidget *existingPromotion, existingPromotions)
                        {
                            if (existingPromotion && existingPromotion->data.Matches(promo))
                            {
                                exists = true;
                                break;
                            }
                        }
                        if (exists)
                            continue;
                        allSkipped = false;
                        
                        promoted << new RocketPromotionWidget(framework, promo);
                    }
                    // Rocket action
                    else if (type == "action")
                    {
                        PromotedServer promo;
                        promo.type = type;
                        promo.durationMsec = (serverData.contains("Duration") ? serverData["Duration"].toInt() * 1000 : 5000);
                        promo.smallPicture = serverData["SmallPicture"].toString();
                        promo.actionText = serverData["ActionText"].toString();
                        
                        bool exists = false;
                        foreach(const RocketPromotionWidget *existingPromotion, existingPromotions)
                        {
                            if (existingPromotion && existingPromotion->data.Matches(promo))
                            {
                                exists = true;
                                break;
                            }
                        }
                        if (exists)
                            continue;
                        allSkipped = false;
                        
                        promoted << new RocketPromotionWidget(framework, promo);
                    }
                }
                else
                    LogWarning("Server JSON does not have enough data to construct a promotion object.");
            }
        }
        else
            LogError("JSON parse error while deserializing promoted servers.");
            
        // If any were skipped we need to parse again without giving the reference servers.
        // This is a perf penalty we are willing to pay as it happens very rarely.
        if (!allSkipped && !existingPromotions.isEmpty() && promoted.size() != existingPromotions.size())
        {
            foreach(RocketPromotionWidget *pw, promoted)
                SAFE_DELETE_LATER(pw);
            PromotedWidgetList dummy;
            return DeserializePromoted(data, framework, dummy);
        }
        return promoted;
    }
    
    ServerStartReply Json::DeserializeServerStart(const QByteArray &data)
    {
        ServerStartReply reply;
        
        bool ok = false;
        QVariantMap serverData = TundraJson::Parse(data, &ok).toMap();
        if (ok)
        {
            // Do not use QUrl::TolerantMode as it will do things to percent encoding 
            // we don't want at this point, see http://doc.trolltech.com/4.7/qurl.html#ParsingMode-enum
            reply.loginUrl = serverData.contains("LoginUrl") ? QUrl(serverData["LoginUrl"].toString(), QUrl::StrictMode) : QUrl();
            reply.started = serverData.contains("NewServer") ? serverData["NewServer"].toBool() : false;
        }
        else
            LogError("JSON parse error while deserializing server startup.");
            
        return reply;
    }
    
    void Json::DeserializeUser(const QByteArray &data, User &user)
    {
        // Our backend is sending bullshit HTML after the JSON in the body :E hack around...
        QByteArray dataClean = data;
        int jsonEndIndex = dataClean.lastIndexOf("}");
        if (jsonEndIndex != -1)
            dataClean = data.left(jsonEndIndex+1);

        bool ok = false;
        QVariantMap userData = TundraJson::Parse(dataClean, &ok).toMap();
        if (ok)
        {
            user.name = userData["Username"].toString();
            if (user.name.contains('%'))
                user.name = QUrl::fromEncoded(user.name.toUtf8()).toString();

            // Gender is a bit silly in the backend as boolean. true == male false == female.
            if (userData.contains("Gender"))
                user.gender = (userData["Gender"].toBool() ? "male" : "female");
            else
                user.gender = QString();

            // Profile picture and avatar url.
            user.pictureUrl = userData.contains("ProfileImageUrl") ? QUrl(userData["ProfileImageUrl"].toString(), QUrl::TolerantMode) : QUrl();
            user.avatarAppearanceUrl = userData.contains("AvatarUrl") ? QUrl(userData["AvatarUrl"].toString(), QUrl::TolerantMode) : QUrl();

            // MEP
            if (userData.contains("MEP"))
                user.meps = DeserializeMEPList(userData["MEP"].toList());
        }
        else
            LogError("JSON parse error while deserializing user.");
    }

    Stats Json::DeserializeStats(const QByteArray &data)
    {
        Stats stats;
        
        bool ok = false;
        QVariantMap statsData = TundraJson::Parse(data, &ok).toMap();
        if (ok)
        {
            ok = false;
            stats.uniqueLoginCount = statsData["TotalUserCount"].toUInt(&ok);
            if (!ok)
                stats.uniqueLoginCount = 0;
            ok = false;
            stats.hostedSceneCount = statsData["TxmlCount"].toUInt(&ok);
            if (!ok)
                stats.hostedSceneCount = 0;
        }
        else
            LogError("JSON parse error while deserializing stats.");
        
        return stats;
    }
    
    OnlineStats Json::DeserializeOnlineStats(const QByteArray &data)
    {
        OnlineStats stats;

        bool ok = false;
        QVariantMap statsData = TundraJson::Parse(data, &ok).toMap();
        if (ok)
        {
            ok = false;
            stats.onlineUserCount = statsData.value("OnlineUserCount", 0).toUInt(&ok);
            if (!ok)
                stats.onlineUserCount = 0;
            DeserializeServerStatsList(statsData.value("SpaceUsers", QVariantList()).toList(), stats.spaceUsers);
        }
        else
            LogError("JSON parse error while deserializing online stats.");

        return stats;
    }
    
    void Json::DeserializeServerStatsList(const QVariantList &source, ServerOnlineStatsList &dest)
    {      
        foreach(const QVariant &sourceIter, source)
        {
            if (sourceIter.type() != QVariant::Map)
                continue;

            QVariantMap serverData = sourceIter.toMap();
            ServerOnlineStats serverStats;
            serverStats.id = serverData.value("TxmlId", "").toString();
            serverStats.userNames = serverData.value("Users", QStringList()).toStringList();
            serverStats.userCount = serverStats.userNames.size();
            dest << serverStats;
        }
    }

    ServerScoreMap Json::DeserializeScores(const QByteArray &data)
    {
        ServerScoreMap scoreMap;

        bool ok = false;
        QVariantList scores = TundraJson::Parse(data, &ok).toList();
        if (ok)
        {
            foreach (const QVariant scoresIter, scores)
            {
                QVariantMap s = scoresIter.toMap();
                if (s.contains("TxmlId") && s.contains("Day") && s.contains("Week") && s.contains("Month") &&
                    s.contains("ThreeMonths") && s.contains("HalfYear") && s.contains("Year"))
                {
                    ServerScore score;
                    score.txmlId = s["TxmlId"].toString();
                    score.day = s["Day"].toUInt();
                    score.week = s["Week"].toUInt();
                    score.month = s["Month"].toUInt();
                    score.threeMonths = s["ThreeMonths"].toUInt();
                    score.sixMonths = s["HalfYear"].toUInt();
                    score.year = s["Year"].toUInt();
                    
                    scoreMap[score.txmlId] = score;
                }
            }
        }
        else
            LogError("JSON parse error while deserializing scores.");

        return scoreMap;
    }
    
    NewsList Json::DeserializeNews(const QByteArray &data)
    {
        NewsList news;
        
        bool ok = false;
        QVariantList newsList = TundraJson::Parse(data, &ok).toList();
        if (ok)
        {
            foreach(QVariant newsVariant, newsList)
            {
                QVariantMap newsData = newsVariant.toMap();
                if (newsData.contains("Title") && newsData.contains("Message"))
                {
                    NewsItem item;
                    item.title = newsData["Title"].toString();
                    item.message = newsData["Message"].toString();
                    if (newsData.contains("Type"))
                    {
                        QString type = newsData["Type"].toString().toLower();
                        if (type == "critical" || type == "important")
                            item.important = true;
                    }
                    if (!item.title.isEmpty() && !item.message.isEmpty())
                    {
                        if (item.important)
                            news.push_front(item);
                        else
                            news.push_back(item);
                    }
                }
            }
        }
        else
            LogError("JSON parse error while deserializing news.");

        return news;
    }
    
    AvatarList Json::DeserializeAvatars(const QByteArray &data)
    {
        AvatarList avatars;
        
        bool ok = false;
        QVariantMap dataMap = TundraJson::Parse(data, &ok).toMap();
        if (ok)
        {
            QVariantList avatarList = dataMap.value("Avatars").toList();
            foreach(QVariant avatarVariant, avatarList)
            {
                QVariantMap avatarData = avatarVariant.toMap();
                
                Avatar avatar;
                avatar.name = avatarData["Name"].toString();
                avatar.type = avatarData["Type"].toString();
                avatar.assetRef = avatarData["AssetRef"].toString().trimmed();
                avatar.assetRefArchive = avatarData["AssetRefArchive"].toString().trimmed();
                avatar.imageUrl = avatarData["ImageUrl"].toString().trimmed();
                if (avatarData.contains("Gender"))
                    avatar.gender = avatarData["Gender"].toString();
                if (avatarData.contains("Author"))
                    avatar.author = avatarData["Author"].toString();
                if (avatarData.contains("AuthorUrl"))
                    avatar.authorUrl = avatarData["AuthorUrl"].toString();
                
                QVariantList variations = avatarData.contains("Variations") ? avatarData["Variations"].toList() : QVariantList();
                if (!variations.isEmpty())
                    avatar.variations = DeserializeAvatarVariations(variations);

                avatars << avatar;
            }
        }
        else
            LogError("JSON parse error while deserializing avatars.");

        return avatars;
    }
    
    AvatarList Json::DeserializeAvatarVariations(const QVariantList &variationsVariant)
    {
        AvatarList variations;
        foreach(QVariant variationVariant, variationsVariant)
        {
            QVariantMap variantData = variationVariant.toMap();

            Avatar avatar;
            avatar.name = variantData["Name"].toString();
            avatar.type = variantData["Type"].toString();
            avatar.assetRef = variantData["AssetRef"].toString().trimmed();
            avatar.assetRefArchive = variantData["AssetRefArchive"].toString().trimmed();
            avatar.imageUrl = variantData["ImageUrl"].toString().trimmed();
            if (variantData.contains("Gender"))
                avatar.gender = variantData["Gender"].toString();
            if (variantData.contains("Author"))
                avatar.author = variantData["Author"].toString();
            if (variantData.contains("AuthorUrl"))
                avatar.authorUrl = variantData["AuthorUrl"].toString();
            variations << avatar;
        }
        return variations;
    }
    
    // Server
       
    void Server::StoreScreenShot(Framework *framework)
    {
        if (!CanProcessShots() || !framework)
            return;
            
        // Don't take screenshot if not connected!
        TundraLogic::TundraLogicModule *tundraModule = framework->GetModule<TundraLogic::TundraLogicModule>();
        if (!tundraModule || !tundraModule->GetClient().get() || !tundraModule->GetClient()->IsConnected())
            return;
            
        QString filePath = GetPictureFilePath();
        if (filePath.isEmpty())
            return;
            
        RocketPlugin* plugin = framework->GetModule<RocketPlugin>();

        if (plugin && framework->Renderer() && framework->Renderer()->MainCameraComponent() && !plugin->OculusManager()->IsEnabled())
        {
            QImage screenshot = framework->Renderer()->MainCameraComponent()->ToQImage(false);
            if (!screenshot.isNull())
            {
                if (QFile::exists(filePath))
                    QFile::remove(filePath);
                screenshot.save(filePath, "PNG");
            }
        }
    }
    
    QImage Server::LoadScreenShot(Framework *framework)
    {
        if (!CanProcessShots() || !framework)
            return QImage();

        QString filePath = GetPictureFilePath();
        if (!filePath.isEmpty() && QFile::exists(filePath))
            return QImage(filePath);
        return QImage();
    }
    
    QString Server::GetPictureFilePath()
    {
        if (!CanProcessShots())
            return "";

        QDir screenShotDir(QDir::fromNativeSeparators(Application::UserDataDirectory()));
        if (!screenShotDir.exists("adminotech"))
            screenShotDir.mkdir("adminotech");
        if (!screenShotDir.cd("adminotech"))
            return "";
        if (!screenShotDir.exists("screenshots"))
            screenShotDir.mkdir("screenshots");
        if (!screenShotDir.cd("screenshots"))
            return "";

        return screenShotDir.absoluteFilePath(AssetAPI::SanitateAssetRef(company).simplified().replace(" ", "_") + "_" + AssetAPI::SanitateAssetRef(name).simplified().replace(" ", "_") + ".png");
    }

    // RelevancyFilter

    RelevancyFilter::RelevancyFilter(const QString &term_, Qt::CaseSensitivity sensitivity_) :
        term(term_),
        sensitivity(sensitivity_)
    {

    }

    bool RelevancyFilter::HasDirectHits()
    {
        return !direct.isEmpty();
    }

    bool RelevancyFilter::HasIndirectHits()
    {
        return !indirect.isEmpty();
    }

    bool RelevancyFilter::HasHits()
    {
        return (HasDirectHits() || HasIndirectHits());
    }

    QStringList RelevancyFilter::DirectValues()
    {
        return direct.values();
    }

    QStringList RelevancyFilter::IndirectValues()
    {
        return indirect.values();
    }

    QStringList RelevancyFilter::AllValues()
    {
        QStringList values = DirectValues();
        values.append(IndirectValues());
        return values;
    }

    QString RelevancyFilter::GuaranteeStartsWithCase(const QString &suggestion)
    {
        if (suggestion.startsWith(term, Qt::CaseInsensitive) && !suggestion.startsWith(term, Qt::CaseSensitive))
            return term + suggestion.mid(term.length());
        else
            return suggestion;
    }
    
    bool RelevancyFilter::Match(const QString &suggestion)
    {
        // Full hit: term "my space" -> hits suggestion "my space" as direct and "this is my space" as indirect
        int index = suggestion.indexOf(term, 0, sensitivity);
        if (index != -1 && suggestion.compare(term, Qt::CaseInsensitive) != 0)
        {
            if (index == 0)
                DirectSuggestion(suggestion);
            else if (index > 0)
                IndirectSuggestion(suggestion);
        }
        // No hits, find parts: term "my space" -> hits suggestion "my nice space" etc.
        if (index == -1 && MatchIndirectParts(suggestion))
            index = 1; // Fake return index so below returns a "match".
        return (index != -1);
    }

    bool RelevancyFilter::MatchIndirect(const QString &suggestion)
    {
        // Yearly out, suggestion already added
        if (indirect.contains(suggestion.toLower()))
            return true;

        // Full hit: term "my space" -> hits suggestion "this is my space" as indirect.
        int index = suggestion.indexOf(term, 0, sensitivity);
        if (index != -1 && suggestion.compare(term, Qt::CaseInsensitive) != 0)
            IndirectSuggestion(suggestion);
        // No hits, find parts: term "my space" -> hits suggestion "my nice space" etc.
        if (index == -1 && MatchIndirectParts(suggestion))
            index = 1; // Fake return index so below returns a "match".
        return (index != -1);
    }

    bool RelevancyFilter::MatchIndirectParts(const QString &suggestion)
    {
        QString simplifiedTerm = term.simplified();
        if (simplifiedTerm.contains(" "))
        {
            QStringList parts = simplifiedTerm.split(" ", QString::SkipEmptyParts);
            if (!parts.isEmpty())
            {
                bool allHit = true;
                foreach(const QString &part, parts)
                {
                    allHit = suggestion.contains(part, Qt::CaseInsensitive);
                    if (!allHit)
                        break;
                }
                if (allHit)
                    IndirectSuggestion(suggestion);
                return allHit;
            }
        }
        return false;
    }

    bool RelevancyFilter::DirectSuggestion(const QString &suggestion)
    {
        QString key = suggestion.toLower();
        if (!direct.contains(key))
        {
            // Remove from indirect if exists
            if (indirect.contains(key))
                indirect.remove(key);
            direct[key] = suggestion;
            return true;
        }
        return false;
    }

    bool RelevancyFilter::IndirectSuggestion(const QString &suggestion)
    {
        QString key = suggestion.toLower();
        if (!indirect.contains(key))
        {
            // Only add if direct with same key does not exist
            if (!direct.contains(key))
                indirect[key] = suggestion;
            return true;
        }
        return false;
    }
}

/// @endcond
