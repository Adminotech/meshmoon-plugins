/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief  */

#include "StableHeaders.h"
#include "MeshmoonAsset.h"

#include "storage/MeshmoonStorage.h"
#include "AssetAPI.h"
#include "AssetReference.h"

#include <QFileInfo>

MeshmoonAsset::MeshmoonAsset(const MeshmoonLibrarySource &_source, const QString &_name, const QString &relativeRef, Type _type, QObject *parent) :
    QObject(parent),
    source(_source),
    name(_name),
    type(_type)
{
    assetRef = source.second;
    assetRef = assetRef.left(assetRef.lastIndexOf("/")+1);
    assetRef += (!relativeRef.startsWith("/") ? relativeRef : relativeRef.mid(1));

    filename = QFileInfo(assetRef).fileName();
}

MeshmoonAsset::MeshmoonAsset(const QString &_name, const QString &_assetRef, Type _type, QObject *parent) :
    QObject(parent),
    type(_type),
    name(_name),
    assetRef(_assetRef)
{
    filename = QFileInfo(assetRef).fileName();
}

MeshmoonAsset::MeshmoonAsset(const QString &_assetRef, Type _type, QObject *parent) :
    QObject(parent),
    type(_type),
    assetRef(_assetRef)
{
    filename = QFileInfo(assetRef).fileName();
}

AssetPtr MeshmoonAsset::Asset(AssetAPI *assetAPI) const
{
    if (!assetAPI)
        return AssetPtr();

    AssetPtr asset = assetAPI->GetAsset(assetRef);
    if (!asset)
    {
        QString s3Variation = S3RegionVariationURL();
        if (!s3Variation.isEmpty())
            asset = assetAPI->GetAsset(s3Variation);
    }
    return asset;
}

AssetReference MeshmoonAsset::AssetReference() const
{
    return ::AssetReference(assetRef, ResourceType());
}

QString MeshmoonAsset::ResourceType() const
{
    if (assetRef.endsWith(".mesh", Qt::CaseInsensitive))
        return "OgreMesh";
    else if (assetRef.endsWith(".material", Qt::CaseInsensitive))
        return "OgreMaterial";

    if (type == Mesh)
        return "OgreMesh";
    else if (type == Material)
        return "OgreMaterial";
    else if (type == Texture)
        return "Texture";
    return "";
}

bool MeshmoonAsset::IsLibraryRef() const
{
    return !source.second.trimmed().isEmpty();
}

bool MeshmoonAsset::IsStorageRef(MeshmoonStorage *storage) const
{
    if (!storage)
        return false;
    QString storageBaseUrl = storage->BaseUrl();
    if (storageBaseUrl.isEmpty())
        return false;
        
    if (assetRef.startsWith(storageBaseUrl, Qt::CaseInsensitive) || S3RegionVariationURL().startsWith(storageBaseUrl, Qt::CaseInsensitive))
        return true;
    return false;
}

QString MeshmoonAsset::S3RegionVariationURL() const
{
    return S3RegionVariationURL(assetRef);
}

QString MeshmoonAsset::S3RegionVariationURL(QString originalRef)
{
    // We need to be careful with different variations of S3 URLs and check both. Sometimes the storage gets the other variation and the default storage another.
    // http://<bucket>.s3-eu-west-1.amazonaws.com/... == http://<bucket>.s3.amazonaws.com/...
    /// @todo Handle other regions too in the future? Currently all Meshmoon asset hosting is in EU west...
    QString s3Variation = originalRef.contains(".s3-eu-west-1.", Qt::CaseInsensitive) ? ".s3-eu-west-1." : ".s3.";
    int s3VariationIndex = originalRef.indexOf(s3Variation, 0, Qt::CaseInsensitive);
    if (s3VariationIndex != -1)
        return originalRef.replace(s3VariationIndex, s3Variation.length(), (s3Variation == ".s3." ? ".s3-eu-west-1." : ".s3."));    
    return "";
}

bool MeshmoonAsset::ContainsTag(const QString &tag) const
{
    return tags.contains(tag, Qt::CaseInsensitive);
}