/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonCommon.h"

namespace Meshmoon
{
    Layer::Layer() :
        id_(0),
        visible_(false),
        defaultVisible_(false),
        position_(float3::zero),
        sceneLoaded_(false)
    {
    }

    void Layer::Set(u32 id, const QString &name, const QString &iconUrl, const QString &url, bool defaultVisible, float3 position)
    {
        id_ = id;
        name_ = name;
        iconUrl_ = iconUrl;
        defaultVisible_ = defaultVisible;
        position_ = position;
        
        QUrl u = QUrl::fromEncoded(url.toUtf8(), QUrl::StrictMode);
        urlPresigned_ = u.toString(); ;
        url_ = u.toString(QUrl::RemoveQuery);

        baseUrlStorage_ = u.toString(QUrl::RemoveQuery|QUrl::StripTrailingSlash);
        baseUrlStorage_ = baseUrlStorage_.left(baseUrlStorage_.lastIndexOf("/") + 1);
    }

    void Layer::SetId(u32 id)
    {
        id_ = id;
    }

    void Layer::SetPosition(float3 position)
    {
        position_ = position;
    }
    
    void Layer::SetSceneData(const QByteArray &data)
    {
        sceneData_ = data;
    }

    Meshmoon::SceneLayer Layer::ToSceneLayer(u32 id) const
    {
        Meshmoon::SceneLayer l;
        l.id = id;
        l.name = Name();
        l.iconUrl = IconUrl();
        l.visible = IsVisible();
        l.defaultVisible = IsDefaultVisible();
        l.txmlUrl = PresignedUrl();
        l.sceneData = sceneData_;
        l.centerPosition = Position();
        l.downloaded = true;
        l.loaded = false;
        return l;
    }

    u32 Layer::Id() const
    {
        return id_;
    }

    QString Layer::Name() const
    {
        return name_;
    }

    QString Layer::IconUrl() const
    {
        return iconUrl_;
    }

    QString Layer::Url() const
    {
        return url_;
    }

    QString Layer::PresignedUrl() const
    {
        return urlPresigned_;
    }

    QString Layer::StorageBaseUrl() const
    {
        return baseUrlStorage_;
    }
    
    QString Layer::StorageRef(QString path) const
    {
        if (path.startsWith("/"))
            path = path.mid(1);
        return baseUrlStorage_ + path;
    }
    
    bool Layer::IsVisible() const
    {
        return visible_;
    }
    
    bool Layer::IsDefaultVisible() const
    {
        return defaultVisible_;
    }
    
    float3 Layer::Position() const
    {
        return position_;
    }
    
    bool Layer::IsValid() const
    {
        return (id_ > 0 && !name_.isEmpty());
    }
    
    QString Layer::toString() const
    {
        return QString("id: %1 name: %2 iconUrl: %3 url: %4 storage: %5 visible: %6 defaultVisible: %7 position: %8 valid: %9")
            .arg(id_).arg(name_)
            .arg(iconUrl_).arg(url_).arg(baseUrlStorage_)
            .arg(visible_ ? "true" : "false").arg(defaultVisible_ ? "true" : "false")
            .arg(position_.toString()).arg(IsValid() ? "true" : "false");
    }
}
