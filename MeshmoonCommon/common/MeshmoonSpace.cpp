/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonCommon.h"

#include <QUrl>

namespace Meshmoon
{
    Space::Space() :
        sceneLoaded_(false)
    {
    }
    
    Space::~Space()
    {
        for (int i=0, len=layers_.size(); i<len; i++)
            SAFE_DELETE(layers_[i]);
        layers_.clear();
    }

    void Space::Set(const QString &id, const QString &name, const QString &iconUrl, const QString &url, Meshmoon::LayerList layers)
    {
        id_ = id;
        name_ = name;
        iconUrl_ = iconUrl;
        layers_ = layers;
        
        QUrl u = QUrl::fromEncoded(url.toUtf8(), QUrl::StrictMode);
        urlPresigned_ = u.toString(); ;
        url_ = u.toString(QUrl::RemoveQuery);

        baseUrlStorage_ = u.toString(QUrl::RemoveQuery|QUrl::StripTrailingSlash);
        baseUrlStorage_ = baseUrlStorage_.left(baseUrlStorage_.lastIndexOf("/") + 1);
    }
    
    void Space::SetSceneData(const QByteArray &data)
    {
        sceneData_ = data;
    }
    
    Meshmoon::SceneLayer Space::ToSceneLayer(u32 id) const
    {
        Meshmoon::SceneLayer l;
        l.id = id;
        l.name = Name();
        l.iconUrl = IconUrl();
        l.visible = true;
        l.defaultVisible = true;
        l.txmlUrl = PresignedUrl();
        l.sceneData = sceneData_;
        l.centerPosition = float3::zero;
        l.downloaded = true;
        l.loaded = false;
        return l;
    }

    QString Space::Id() const
    {
        return id_;
    }

    QString Space::Name() const
    {
        return name_;
    }

    QString Space::IconUrl() const
    {
        return iconUrl_;
    }

    QString Space::Url() const
    {
        return url_;
    }

    QString Space::PresignedUrl() const
    {
        return urlPresigned_;
    }

    QString Space::StorageBaseUrl() const
    {
        return baseUrlStorage_;
    }

    QString Space::StorageRef(QString path) const
    {
        if (path.startsWith("/"))
            path = path.mid(1);
        return baseUrlStorage_ + path;
    }

    Meshmoon::LayerList Space::Layers() const
    {
        return layers_;
    }

    Meshmoon::Layer *Space::LayerById(u32 id) const
    {
        for (int i=0, len=layers_.size(); i<len; i++)
            if (layers_[i]->Id() == id)
                return layers_[i];
        return 0;
    }

    Meshmoon::Layer *Space::LayerByName(const QString &name) const
    {
        for (int i=0, len=layers_.size(); i<len; i++)
            if (layers_[i]->Name() == name)
                return layers_[i];
        return 0;
    }

    Meshmoon::Layer *Space::LayerByIndex(int index) const
    {
        if (layers_.size() > index)
            return layers_[index];
        return 0;
    }

    int Space::NumLayers() const
    {
        return layers_.size();
    }

    bool Space::IsValid() const
    {
        return (!id_.isEmpty() && !name_.isEmpty());
    }

    QString Space::toString() const
    {
        return QString("id: %1 name: %2 iconUrl: %3 url: %4 storage: %5 layers: %6 valid: %7")
            .arg(id_).arg(name_)
            .arg(iconUrl_).arg(url_)
            .arg(baseUrlStorage_).arg(layers_.size())
            .arg(IsValid() ? "true" : "false");
    }
}
