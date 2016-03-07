/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

#pragma once

#include "RocketFwd.h"
#include "AssetFwd.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

/// Represents an %Meshmoon asset. 
/** Used for example in Meshmoon asset library, storage, build mode. */
class MeshmoonAsset : public QObject
{
    Q_OBJECT

public:
    enum Type
    {
        Unknown = 0,
        All,
        Mesh,
        Material,
        Texture
    };

    /// Create from library source.
    MeshmoonAsset(const MeshmoonLibrarySource &_source, const QString &_name, const QString &relativeRef, Type _type = Unknown, QObject *parent = 0);

    /// Create from asset ref with display name.
    MeshmoonAsset(const QString &_name, const QString &_assetRef, Type _type = Unknown, QObject *parent = 0);
    
    /// Create from asset ref.
    MeshmoonAsset(const QString &_assetRef, Type _type = Unknown, QObject *parent = 0);

    /// Returns the corresponding asset from AssetAPI. Returns null if not loaded.
    AssetPtr Asset(AssetAPI *assetAPI) const;
    
    /// Returns asset reference.
    ::AssetReference AssetReference() const;

    /// Returns asset type.
    QString ResourceType() const;
    
    /// Returns if this asset originated from a Meshmoon library.
    bool IsLibraryRef() const;
    
    /// Returns if this asset originated from the current scenes Meshmoon storage.
    bool IsStorageRef(MeshmoonStorage *storage) const;

    /// Does this asset contain a certain description tag.
    bool ContainsTag(const QString &tag) const;

    /// Returns the S3 region variation URL for this asset.
    QString S3RegionVariationURL() const;

    /// Returns the S3 region variation for originalRef.
    static QString S3RegionVariationURL(QString originalRef);

    Type type;

    QString name;
    QString filename;
    QString assetRef;
    QString description;
    QString previewImageUrl;

    QStringList tags;
    QVariantMap metadata;

    MeshmoonLibrarySource source;
};
Q_DECLARE_METATYPE(MeshmoonAsset*)
