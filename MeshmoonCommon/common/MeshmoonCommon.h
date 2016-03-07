/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   AdminotechCommon.h
    @brief  Common Meshmoon Rocket data structures and utilities. */

#pragma once

#include "MeshmoonCommonPluginApi.h"

#include "SceneFwd.h"
#include "kNetFwd.h"
#include "CoreStringUtils.h"
#include "Application.h"
#include "Math/float3.h"

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QHash>
#include <QList>
#include <QPair>
#include <QColor>
#include <QUrl>

#include <kNet/DataDeserializer.h>
#include <kNet/DataSerializer.h>

#include <vector>
#include <list>

namespace Meshmoon
{
    /// @cond PRIVATE

    typedef QPair<QByteArray, QByteArray> EncodedQueryItem;
    typedef QList<QPair<QByteArray, QByteArray> > EncodedQueryItems;
    typedef QHash<QByteArray, QByteArray> EncodedQueryMap;
    typedef QMap<QByteArray, QByteArray> HeaderMap;
    typedef QPair<QString, QString> QueryItem;
    typedef QList<QPair<QString, QString> > QueryItems;
    typedef QHash<QString, QString> QueryMap;

    namespace Theme
    {
        static QColor  LightGrey    = QColor(221,221,221);
        static QColor  DarkGrey     = QColor(31,31,31);
        static QColor  Blue         = QColor(8,149,195);
        static QColor  Orange       = QColor(243,154,41);

        static QString LightGreyStr = "rgb(221,221,221)";
        static QString DarkGreyStr  = "rgb(31,31,31)";
        static QString BlueStr      = "rgb(8,149,195)";
        static QString OrangeStr    = "rgb(243,154,41)";
        
        static QString ColorWithAlpha(const QColor &color, int alpha)
        {
            if (alpha < 0) alpha = 0;
            else if (alpha > 255) alpha = 255;           
            return QString("rgba(%1,%2,%3,%4)")
                .arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
        }
    }

    enum PermissionLevel
    {
        Basic = 0,
        Elevated,
        Admin,
        SuperAdmin,
        PermissionLevelOutOfRange
    };

    MESHMOON_COMMON_API inline QString PermissionLevelToString(PermissionLevel level)
    {
        switch(level)
        {
        case Basic: return "Basic";
        case Elevated: return "Elevated";
        case Admin: return "Admin";
        case SuperAdmin: return "SuperAdmin";
        default: return "Unknown";
        }
    }

    enum DenyReason
    {
        None = 0,
        NoPermissionCreate,
        NoPermissionModify,
        AuthNotFound
    };

    MESHMOON_COMMON_API inline QString DenyReasonMessage(DenyReason reason)
    {
        switch(reason)
        {
            case None: return "Permission denied with unknown reason";
            case NoPermissionCreate: return "Permission denied to create entity";
            case NoPermissionModify: return "Permission denied to modify entity";
            case AuthNotFound: return "Not authenticated, permission denied to modify";
            default: return "Permission denied with unknown reason";
        }
    }

    namespace Version
    {
        MESHMOON_COMMON_API inline uint Major(const QString &version)
        {
            if (version.isEmpty())
                return 0;
            QStringList parts = version.trimmed().split(".");
            if (parts.size() >= 1)
                return parts[0].toInt();
            return 0;
        }

        MESHMOON_COMMON_API inline uint Minor(const QString &version)
        {
            if (version.isEmpty())
                return 0;
            QStringList parts = version.trimmed().split(".");
            if (parts.size() >= 2)
                return parts[1].toInt();
            return 0;
        }

        MESHMOON_COMMON_API inline uint MajorPatch(const QString &version)
        {
            if (version.isEmpty())
                return 0;
            QStringList parts = version.trimmed().split(".");
            if (parts.size() >= 3)
                return parts[2].toInt();
            return 0;
        }

        MESHMOON_COMMON_API inline uint MinorPatch(const QString &version)
        {
            if (version.isEmpty())
                return 0;
            QStringList parts = version.trimmed().split(".");
            if (parts.size() >= 4)
                return parts[3].toInt();
            return 0;
        }

        MESHMOON_COMMON_API inline bool EqualsOrGreater(const QString &version, uint major, uint minor, uint majorPatch, uint minorPatch)
        {
            return (Major(version) >= major && Minor(version) >= minor && 
                    MajorPatch(version) >= majorPatch && MinorPatch(version) >= minorPatch);
        }
    }

    struct MESHMOON_COMMON_API UserPermissions
    {
        u32 connectionId;
        QString username;
        QString userhash;
        PermissionLevel level;

        UserPermissions();
        UserPermissions(uint _connectionId, const QString &_username, const QString &_userhash);

        void Reset();
    };
    typedef std::list<UserPermissions> UserPermissionsList;
    
    /// @endcond

    /// Meshmoon scene layer
    struct MESHMOON_COMMON_API SceneLayer
    {
        QString name;        ///< Layer name. Read-only from script.
        QString iconUrl;     ///< URL to layer icon. Read-only from script.
        u32 id;              ///< Layer id. Read-only from script.
        bool visible;        ///< Current visibility. Read-write from script.
        bool defaultVisible; ///< Layers default/starting visiblity. Read-only from script.

        QUrl txmlUrl;
        QByteArray sceneData;
        float3 centerPosition;

        bool downloaded;
        bool loaded;

        QList<EntityWeakPtr> entities;

        SceneLayer();
        SceneLayer(uint _id, bool _visible, const QString &_name, const QString &_icon);
        
        bool IsValid() const;
        void Reset();

        QString ToString() const;
        QString toString() const;
    };
    /// Meshmoon scene layer list.
    typedef QList<Meshmoon::SceneLayer> SceneLayerList;
    
    /// Meshmoon layer
    /** @todo Replace Meshmoon::SceneLayer with this class cleanly.
        Main thing when porting is to check that script exposure works in RocketPlugin and the server. */
    class MESHMOON_COMMON_API Layer : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(u32 id READ Id)
        Q_PROPERTY(QString name READ Name)
        Q_PROPERTY(QString iconUrl READ IconUrl)
        Q_PROPERTY(QString url READ Url)
        Q_PROPERTY(QString storageBaseUrl READ StorageBaseUrl)

        Q_PROPERTY(bool visible READ IsVisible)
        Q_PROPERTY(bool defaultVisible READ IsDefaultVisible)
        Q_PROPERTY(float3 position READ Position)

    public:
        Layer();

        /// @cond PRIVATE
        void Set(u32 id, const QString &name, const QString &iconUrl, const QString &url, bool defaultVisible, float3 position = float3::zero);
        void SetId(u32 id);
        void SetPosition(float3 position);
        void SetSceneData(const QByteArray &data);
        
        Meshmoon::SceneLayer ToSceneLayer(u32 id) const;

        QString PresignedUrl() const;
        /// @endcond

    public slots:
        u32 Id() const;
        QString Name() const;
        QString IconUrl() const;
        QString Url() const;
        QString StorageBaseUrl() const;
        QString StorageRef(QString path) const;

        bool IsVisible() const;
        bool IsDefaultVisible() const;
        float3 Position() const;
        
        bool IsValid() const;
        QString toString() const;

    private:
        u32 id_;
        QString name_;
        QString iconUrl_;
        QString url_;
        QString urlPresigned_;
        QString baseUrlStorage_;
        
        bool visible_;
        bool defaultVisible_;
        float3 position_;
        
        bool sceneLoaded_;
        QByteArray sceneData_;
    };
    /// Meshmoon space list.
    typedef QList<Meshmoon::Layer*> LayerList;

    /// Meshmoon space
    class MESHMOON_COMMON_API Space : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QString id READ Id)
        Q_PROPERTY(QString name READ Name)
        Q_PROPERTY(QString iconUrl READ IconUrl)
        Q_PROPERTY(QString url READ Url)
        Q_PROPERTY(QString storageBaseUrl READ StorageBaseUrl)

        Q_PROPERTY(LayerList layers READ Layers)
        
    public:
        Space();
        ~Space();
        
        /// @cond PRIVATE
        void Set(const QString &id, const QString &name, const QString &iconUrl, const QString &url, Meshmoon::LayerList layers);        
        void SetSceneData(const QByteArray &data);
        
        Meshmoon::SceneLayer ToSceneLayer(u32 id) const;
        
        QString PresignedUrl() const;
        /// @endcond

    public slots:
        QString Id() const;
        QString Name() const;
        QString IconUrl() const;
        QString Url() const;
        QString StorageBaseUrl() const;
        QString StorageRef(QString path) const;
        
        Meshmoon::LayerList Layers() const;
        Meshmoon::Layer *LayerById(u32 id) const;
        Meshmoon::Layer *LayerByName(const QString &name) const;
        Meshmoon::Layer *LayerByIndex(int index) const;
        int NumLayers() const;

        bool IsValid() const;
        QString toString() const;

    private:
        QString id_;
        QString name_;
        QString iconUrl_;
        QString url_;
        QString urlPresigned_;
        QString baseUrlStorage_;
        Meshmoon::LayerList layers_;
        
        bool sceneLoaded_;
        QByteArray sceneData_;
    };
    /// Meshmoon space list.
    typedef QList<Meshmoon::Space*> SpaceList;
   
    /// @cond PRIVATE
    
    namespace Network
    {
        static const bool defaultReliable = true;
        static const bool defaultInOrder = true;
        static const u32 defaultPriority = 100;

        /// Network message informing that a asset ref should be reloaded from source.
        struct MESHMOON_COMMON_API MsgAssetReload
        {
            MsgAssetReload();
            MsgAssetReload(const char *data, size_t numBytes);

            void InitToDefault();
            
            enum { messageID = 400 };
            static inline const char * const Name() { return "AdminoAssetReload"; }

            bool reliable;
            bool inOrder;
            u32 priority;

            std::vector<QString> assetReferences;

            inline size_t Size() const
            {
                size_t stringLen = 0;
                // 2 bytes u16 string length + N bytes UTF8 QByteArray size
                for(size_t i = 0; i < assetReferences.size(); ++i)
                    stringLen += 2 + assetReferences[i].toUtf8().size()*1;
                // 1 byte u8 vector size + combined string size (with 1 byte as separators)
                return 1 + stringLen;
            }

            inline void SerializeTo(kNet::DataSerializer &dst) const
            {
                dst.Add<u8>((u8)assetReferences.size());
                for(size_t i = 0; i < assetReferences.size(); ++i)
                    WriteUtf8String(dst, assetReferences[i]);
            }

            inline void DeserializeFrom(kNet::DataDeserializer &src)
            {
                assetReferences.resize(src.Read<u8>());
                for(size_t i = 0; i < assetReferences.size(); ++i)
                    assetReferences[i] = ReadUtf8String(src);
            }
        };

        /// Network message informing about user permissions.
        struct MESHMOON_COMMON_API MsgUserPermissions
        {
            MsgUserPermissions();
            MsgUserPermissions(const char *data, size_t numBytes);

            void InitToDefault();

            enum { messageID = 401 };
            static inline const char * const Name() { return ""; }

            bool reliable;
            bool inOrder;
            u32 priority;

            inline size_t Size() const
            {
                return 0;
            }

            inline void SerializeTo(kNet::DataSerializer &dst) const
            {
            }

            inline void DeserializeFrom(kNet::DataDeserializer &src)
            {
            }
        };

        /// Network message informing about layer updates.
        struct MESHMOON_COMMON_API MsgLayerUpdate
        {
            MsgLayerUpdate();
            MsgLayerUpdate(const char *data, size_t numBytes);

            void InitToDefault();

            enum { messageID = 402 };
            static inline const char * const Name() { return "AdminoLayerCreated"; }

            bool reliable;
            bool inOrder;
            u32 priority;

            inline size_t Size() const
            {
                return 0;
            }

            inline void SerializeTo(kNet::DataSerializer &dst) const
            {
            }

            inline void DeserializeFrom(kNet::DataDeserializer &src)
            {
            }
        };
        
        /// Network message informing about user permissions.
        struct MESHMOON_COMMON_API MsgPermissionDenied
        {
            MsgPermissionDenied();
            MsgPermissionDenied(const char *data, size_t numBytes);

            void InitToDefault();

            enum { messageID = 403 };
            static inline const char * const Name() { return "AdminoPermissionDenied"; }

            bool reliable;
            bool inOrder;
            u32 priority;
            
            inline size_t Size() const
            {
                return 0;
            }

            inline void SerializeTo(kNet::DataSerializer &dst) const
            {
            }

            inline void DeserializeFrom(kNet::DataDeserializer &src)
            {
            }
        };
        
        /// Network message informing admin user about storage.
        struct MESHMOON_COMMON_API MsgStorageInformation
        {
            MsgStorageInformation();
            MsgStorageInformation(const char *data, size_t numBytes);

            void InitToDefault();

            enum { messageID = 404 };
            static inline const char * const Name() { return "AdminoStorageInformation"; }

            bool reliable;
            bool inOrder;
            u32 priority;

            inline size_t Size() const
            {
                return 0;
            }

            inline void SerializeTo(kNet::DataSerializer &dst) const
            {
            }

            inline void DeserializeFrom(kNet::DataDeserializer &src)
            {
            }
        };
    }

    /// @endcond
}

Q_DECLARE_METATYPE(Meshmoon::SceneLayer)
Q_DECLARE_METATYPE(Meshmoon::SceneLayerList)

Q_DECLARE_METATYPE(Meshmoon::LayerList)
Q_DECLARE_METATYPE(Meshmoon::SpaceList)

//#define MESHMOON_USE_DEBUG_BACKEND

namespace Meshmoon
{
    /// @cond PRIVATE

#ifndef MESHMOON_USE_DEBUG_BACKEND
    /// @todo Change this to https://services.meshmoon.com when its ready and able. Note that paths will be different to this service.
    static const QString MeshmoonServicesBaseUrl = "https://www.adminotech.com";
#else
    /// @note Put your Adminotech office local IP here
    static const QString MeshmoonServicesBaseUrl = "http://x.x.x.x:x/adminotech";
#endif

    // Auth / Profile
    static const QUrl UrlAuth             = QUrl(MeshmoonServicesBaseUrl + "/tundraauth/");
    static const QUrl UrlLogin            = QUrl(MeshmoonServicesBaseUrl + "/tundralogin/");
    static const QUrl UrlLogout           = QUrl(MeshmoonServicesBaseUrl + "/tundralogout/");
    static const QUrl UrlEditProfile      = QUrl(MeshmoonServicesBaseUrl + "/tundralogin/EditProfile.aspx");
    static const QUrl UrlLaunch           = QUrl(MeshmoonServicesBaseUrl + "/tundralaunch/");
    static const QUrl UrlLaunchVerify     = QUrl(MeshmoonServicesBaseUrl + "/tundralaunch/Verify.aspx");

    // Servers / Info
    static const QUrl UrlServers          = QUrl(MeshmoonServicesBaseUrl + "/tundrastatus/");
    static const QUrl UrlUsers            = QUrl(MeshmoonServicesBaseUrl + "/tundrausers/");
    static const QUrl UrlScores           = QUrl(MeshmoonServicesBaseUrl + "/tundrascore/");
    static const QUrl UrlStart            = QUrl(MeshmoonServicesBaseUrl + "/tundrastart/");
    static const QUrl UrlStats            = QUrl(MeshmoonServicesBaseUrl + "/tundrastats/");
    static const QUrl UrlNews             = QUrl(MeshmoonServicesBaseUrl + "/tundranews/");

    // Avatar
    static const QUrl UrlAvatarListing    = QUrl(MeshmoonServicesBaseUrl + "/avatar/Avatars.ashx");
    static const QUrl UrlAvatarSet        = QUrl(MeshmoonServicesBaseUrl + "/tundraavatar/");

    // Reporting
    static const QUrl UrlSupportRequest   = QUrl(MeshmoonServicesBaseUrl + "/tundrareport/");

    static const QUrl UrlPromoted         = QUrl("http://meshmoon.data.s3.amazonaws.com/promotions/meshmoon-promotions.json");
    static const QUrl UrlAvatarScene      = QUrl("http://meshmoon.data.s3.amazonaws.com/avatars/avatar-editor/avatar-editor.txml");

    /// @endcond
}
