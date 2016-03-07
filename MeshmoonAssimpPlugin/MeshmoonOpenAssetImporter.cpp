/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @note This code is based on OgreAssimp. For the latest info, see http://code.google.com/p/ogreassimp/
    Copyright (c) 2011 Jacob 'jacmoe' Moen. Licensed under the MIT license

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MeshmoonOpenAssetImporter.h"

#include "LoggingFunctions.h"
#include "Math/MathFunc.h"
#include "OgreMeshAsset.h"
#include "OgreSkeletonAsset.h"
#include "OgreMaterialAsset.h"
#include "TextureAsset.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "Profiler.h"
#include "FrameAPI.h"

#include <assimp/DefaultLogger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <Ogre.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>
#include <QTextStream>
#include <QDomDocument>

#include <kNet/PolledTimer.h>

#include "MemoryLeakCheck.h"

const QString MeshmoonOpenAssetImporter::LC("[MeshmoonOpenAssetImport]: ");

MeshmoonOpenAssetImporter::MeshmoonOpenAssetImporter(AssetAPI *assetApi) :
    framework_(assetApi->GetFramework()),
    assetAPI_(assetApi),
    meshCreated(false),
    completedEmitted(false),
    depsResolved_(false),
    texCount(0),
    mSubMeshCount(0),
    msBoneCount(0),
    scene(0)
{
    debugLoggingEnabled_ = IsLogChannelEnabled(LogChannelDebug);
}

MeshmoonOpenAssetImporter::~MeshmoonOpenAssetImporter()
{
}

bool MeshmoonOpenAssetImporter::LoadTexture(const QString &textureRef, const QString &destOgreMaterialName, const QString &destTextureUnitName)
{
    PROFILE(MeshmoonOpenAssetImport_LoadTexture)

    // Loads the texture from http address
    if (textureRef.startsWith("http://", Qt::CaseInsensitive) || textureRef.startsWith("https://", Qt::CaseInsensitive) || textureRef.startsWith("local://", Qt::CaseInsensitive))
    {
        AssetTransferPtr transPtr = assetAPI_->RequestAsset(textureRef, "Texture");
        if (!transPtr)
        {
            LogError(LC + "Failed to request texture " + textureRef);
            return false;
        }

        connect(transPtr.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(OnTextureLoaded(AssetPtr)), Qt::UniqueConnection);
        connect(transPtr.get(), SIGNAL(Failed(IAssetTransfer*, QString)),this, SLOT(OnTextureLoadFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);
        
        TextureReceiver receiver;
        receiver.textureRef = textureRef;
        receiver.materialName = destOgreMaterialName;
        receiver.textureUnitName = destTextureUnitName;
        textureReceivers_ << receiver;
        return true;
    }
    // Loads the texture from file
    else
    {
        AssetPtr textureAsset = assetAPI_->FindAsset(textureRef);
        if (!textureAsset.get())
        {
            textureAsset = assetAPI_->CreateNewAsset("Texture", textureRef);
            if (!textureAsset.get())
            {
                LogError(LC + "Failed to create texture asset " + textureRef);
                return false;
            }
            textureAsset->SetDiskSource(textureRef);
            if (!textureAsset->LoadFromFile(textureRef))
            {
                LogError(LC + "Failed to load texture from disk source " + textureAsset->DiskSource());
                return false;
            }
        }
        if (textureAsset.get())
        {
            SetTexture(textureAsset, destOgreMaterialName, destTextureUnitName);
            return true;
        }
    }
    return false;
}

void MeshmoonOpenAssetImporter::SetTexture(AssetPtr asset, const QString &destOgreMaterialName, const QString &destTextureUnitName)
{
    PROFILE(MeshmoonOpenAssetImport_SetTexture)
    
    TextureAsset *textureAsset = dynamic_cast<TextureAsset*>(asset.get());
    if (!textureAsset)
    {
        LogError(LC + "SetTexture called with non TextureAsset AssetPtr.");
        return;
    }
    if (!textureAsset->IsLoaded())
    {
        LogError(LC + "SetTexture called with TextureAsset that is not loaded.");
        return;
    }

    QString textureRef = textureAsset->Name();
    std::string ogreTextureName = textureAsset->ogreTexture->getName();
    
    // Find receivers or create from input params
    QList<TextureReceiver> receivers;
    if (destOgreMaterialName.isEmpty() || destTextureUnitName.isEmpty())
    {
        foreach(const TextureReceiver &receiver, textureReceivers_)
            if (receiver.textureRef.compare(textureRef, Qt::CaseSensitive) == 0)
                receivers << receiver;
    }
    else
    {
        TextureReceiver receiver;
        receiver.textureRef = textureRef;
        receiver.materialName = destOgreMaterialName;
        receiver.textureUnitName = destTextureUnitName;
        receivers << receiver;
    }
    
    foreach(const TextureReceiver &receiver, receivers)
    {
        Ogre::MaterialPtr ogreMaterial = Ogre::MaterialManager::getSingleton().getByName(receiver.materialName.toStdString());
        if (!ogreMaterial.get() || !ogreMaterial->getTechnique(0) || !ogreMaterial->getTechnique(0)->getPass(0))
        {
            LogError(LC + QString("SetTexture: Material %1 in invalid state. Cannot apply texture %2 to texture unit %3")
                .arg(receiver.materialName).arg(receiver.textureRef).arg(receiver.textureUnitName));
            continue;
        }
        Ogre::Pass *ogrePass = ogreMaterial->getTechnique(0)->getPass(0);
        Ogre::TextureUnitState *tu = ogrePass->getTextureUnitState(receiver.textureUnitName.toStdString());
        if (tu)
        {
            tu->setTextureName(ogreTextureName);

            // Remove this specific receiver for this texture
            RemoveTextureReceiver(receiver);
            
            // Check if this material is no longer waiting for texture and load it.
            if (NumPendingMaterialTextures(receiver.materialName) == 0)
            {
                ogreMaterial->load();
                if (debugLoggingEnabled_)
                    LogDebug(LC + QString("All textures loaded to %1. Loading material now.").arg(receiver.materialName));
            }
        }
        else
            LogError(LC + QString("SetTexture: Failed to find texture unit %1 from %2").arg(receiver.textureUnitName).arg(receiver.materialName));
    }

    // Remove all remaining receivers. Also checks completion of all texture loads.
    RemoveTextureReceivers(textureRef);
}

void MeshmoonOpenAssetImporter::OnTextureLoaded(AssetPtr asset)
{
    if (asset.get())
        SetTexture(asset);
    else
        LogError(LC + "OnTextureLoaded asset ptr null!");
}

void MeshmoonOpenAssetImporter::OnTextureLoadFailed(IAssetTransfer* assetTransfer, QString reason)
{
    // This error is already logged by AssetAPI, no need to repeat here.
    RemoveTextureReceivers(assetTransfer->SourceUrl());
}

uint MeshmoonOpenAssetImporter::NumPendingMaterialTextures(const QString &destOgreMaterialName)
{
    uint num = 0;
    for (int i=0; i<textureReceivers_.size(); ++i)
    {
        const TextureReceiver &receiver = textureReceivers_[i];
        if (receiver.materialName.compare(destOgreMaterialName, Qt::CaseSensitive) == 0)
            num++;
    }
    return num;
}

void MeshmoonOpenAssetImporter::RemoveTextureReceivers(const QString &textureRef)
{
    for (int i=0; i<textureReceivers_.size(); ++i)
    {
        const TextureReceiver &receiver = textureReceivers_[i];
        if (receiver.textureRef.compare(textureRef, Qt::CaseSensitive) == 0)
        {
            textureReceivers_.removeAt(i);
            i--;
        }
    }
    CheckCompletion();
}

void MeshmoonOpenAssetImporter::RemoveTextureReceiver(const TextureReceiver &receiver)
{
    for (int i=0; i<textureReceivers_.size(); ++i)
    {
        const TextureReceiver &receiverIter = textureReceivers_[i];
        if (receiverIter.Matches(receiver))
        {
            textureReceivers_.removeAt(i);
            i--;
        }
    }
}

bool MeshmoonOpenAssetImporter::TextureReceiver::Matches(const TextureReceiver &other) const
{
    return Matches(other.textureRef, other.materialName, other.textureUnitName);
}

bool MeshmoonOpenAssetImporter::TextureReceiver::Matches(const QString &ref, const QString &mat, const QString &tu) const
{
    return (textureRef.compare(ref, Qt::CaseSensitive) == 0 && materialName.compare(mat, Qt::CaseSensitive) == 0 && textureUnitName.compare(tu, Qt::CaseSensitive) == 0);
}

static aiMatrix4x4 UpdateAnimationFuncWithTime(const aiNodeAnim *channel, double currentTime, double duration, aiMatrix4x4 &mat)
{
    PROFILE(MeshmoonOpenAssetImport_UpdateAnimationFuncWithTime)
    // calculate the transformations for each animation channel
    // get the bone/node which is affected by this animation channel
    if (!channel)
        return mat;

    // ******** Position *****
    aiVector3D presentPosition(0, 0, 0);
    if (channel->mNumPositionKeys > 0)
    {
        // Look for present frame number. Search from last position if time is after the last time, else from beginning
        // Should be much quicker than always looking from start for the average use case.
        unsigned int frame = 0;
        while(frame < channel->mNumPositionKeys - 1)
        {
            if(currentTime < channel->mPositionKeys[frame+1].mTime)
                break;
            frame++;
        }

        // interpolate between this frame's value and next frame's value
        unsigned int nextFrame = (frame + 1) % channel->mNumPositionKeys;
        const aiVectorKey& key = channel->mPositionKeys[frame];
        const aiVectorKey& nextKey = channel->mPositionKeys[nextFrame];
        double diffTime = nextKey.mTime - key.mTime;

        if(diffTime < 0.0)
            diffTime += duration;
        if(diffTime > 0)
        {
            float factor = float((currentTime - key.mTime) / diffTime);
            presentPosition = key.mValue + (nextKey.mValue - key.mValue) * factor;
        }
        else
        {
            presentPosition = key.mValue;
        }
    }

    // ******** Rotation *********
    aiQuaternion presentRotation(1, 0, 0, 0);
    if(channel->mNumRotationKeys > 0)
    {
        unsigned int frame = 0;
        while(frame < channel->mNumRotationKeys - 1)
        {
            if(currentTime < channel->mRotationKeys[frame+1].mTime)
                break;
            frame++;
        }

        // interpolate between this frame's value and next frame's value
        unsigned int nextFrame = (frame + 1) % channel->mNumRotationKeys;
        const aiQuatKey& key = channel->mRotationKeys[frame];
        const aiQuatKey& nextKey = channel->mRotationKeys[nextFrame];
        double diffTime = nextKey.mTime - key.mTime;
        if(diffTime < 0.0)
            diffTime += duration;
        if(diffTime > 0)
        {
            float factor = float((currentTime - key.mTime) / diffTime);
            aiQuaternion::Interpolate(presentRotation, key.mValue, nextKey.mValue, factor);
        } 
        else
        {
            presentRotation = key.mValue;
        }
    }

    // ******** Scaling **********
    aiVector3D presentScaling(1, 1, 1);
    if (channel->mNumScalingKeys > 0)
    {
        unsigned int frame = 0;
        while(frame < channel->mNumScalingKeys - 1)
        {
            if(currentTime < channel->mScalingKeys[frame+1].mTime)
                break;
            frame++;
        }
        presentScaling = channel->mScalingKeys[frame].mValue;
    }

    // build a transformation matrix from it
    mat = aiMatrix4x4(presentRotation.GetMatrix());
    mat.a1 *= presentScaling.x; mat.b1 *= presentScaling.x; mat.c1 *= presentScaling.x;
    mat.a2 *= presentScaling.y; mat.b2 *= presentScaling.y; mat.c2 *= presentScaling.y;
    mat.a3 *= presentScaling.z; mat.b3 *= presentScaling.z; mat.c3 *= presentScaling.z;
    mat.a4 = presentPosition.x; mat.b4 = presentPosition.y; mat.c4 = presentPosition.z;

    return mat;
}

static aiMatrix4x4 UpdateAnimationFunc(const aiScene * scene, aiNodeAnim * pchannel, Ogre::Real val, float duration, float &ticks, aiMatrix4x4 & mat)
{
    PROFILE(MeshmoonOpenAssetImport_UpdateAnimationFunc)
    double currentTime = fmod((float)val * 1.f, duration);
    ticks = fmod((float)val * 1.f, duration);

    // calculate the transformations for each animation channel
    // get the bone/node which is affected by this animation channel
    const aiNodeAnim* channel = pchannel;

    // ******** Position *****
    aiVector3D presentPosition(0, 0, 0);
    if (channel->mNumPositionKeys > 0)
    {
        // Look for present frame number. Search from last position if time is after the last time, else from beginning
        // Should be much quicker than always looking from start for the average use case.
        unsigned int frame = 0;
        while(frame < channel->mNumPositionKeys - 1)
        {
            if(currentTime < channel->mPositionKeys[frame+1].mTime)
                break;
            frame++;
        }

        // interpolate between this frame's value and next frame's value
        unsigned int nextFrame = (frame + 1) % channel->mNumPositionKeys;
        const aiVectorKey& key = channel->mPositionKeys[frame];
        const aiVectorKey& nextKey = channel->mPositionKeys[nextFrame];
        double diffTime = nextKey.mTime - key.mTime;
        if(diffTime < 0.0)
            diffTime += duration;
        if(diffTime > 0)
        {
            float factor = float((currentTime - key.mTime) / diffTime);
            presentPosition = key.mValue + (nextKey.mValue - key.mValue) * factor;
        }
        else
        {
            presentPosition = key.mValue;
        }
    }

    // ******** Rotation *********
    aiQuaternion presentRotation(1, 0, 0, 0);
    if(channel->mNumRotationKeys > 0)
    {
        unsigned int frame = 0;
        while(frame < channel->mNumRotationKeys - 1)
        {
            if(currentTime < channel->mRotationKeys[frame+1].mTime)
                break;
            frame++;
        }

        // interpolate between this frame's value and next frame's value
        unsigned int nextFrame = (frame + 1) % channel->mNumRotationKeys;
        const aiQuatKey& key = channel->mRotationKeys[frame];
        const aiQuatKey& nextKey = channel->mRotationKeys[nextFrame];
        double diffTime = nextKey.mTime - key.mTime;
        if(diffTime < 0.0)
            diffTime += duration;
        if(diffTime > 0)
        {
            float factor = float((currentTime - key.mTime) / diffTime);
            aiQuaternion::Interpolate(presentRotation, key.mValue, nextKey.mValue, factor);
        } 
        else
        {
            presentRotation = key.mValue;
        }
    }

    // ******** Scaling **********
    aiVector3D presentScaling(1, 1, 1);
    if (channel->mNumScalingKeys > 0)
    {
        unsigned int frame = 0;
        while(frame < channel->mNumScalingKeys - 1)
        {
            if(currentTime < channel->mScalingKeys[frame+1].mTime)
                break;
            frame++;
        }
        presentScaling = channel->mScalingKeys[frame].mValue;
    }

    // build a transformation matrix from it
    mat = aiMatrix4x4(presentRotation.GetMatrix());
    mat.a1 *= presentScaling.x; mat.b1 *= presentScaling.x; mat.c1 *= presentScaling.x;
    mat.a2 *= presentScaling.y; mat.b2 *= presentScaling.y; mat.c2 *= presentScaling.y;
    mat.a3 *= presentScaling.z; mat.b3 *= presentScaling.z; mat.c3 *= presentScaling.z;
    mat.a4 = presentPosition.x; mat.b4 = presentPosition.y; mat.c4 = presentPosition.z;

    return mat;
}

void MeshmoonOpenAssetImporter::GetBasePose(const aiScene *sc, const aiNode *nd)
{
    PROFILE(MeshmoonOpenAssetImport_GetBasePose)
    unsigned int i=0, n=0, t=0;

    // Insert current mesh's bones into boneMatrices
    for(n=0; n < nd->mNumMeshes; ++n)
    {
        const struct aiMesh* mesh = sc->mMeshes[nd->mMeshes[n]];
        if (mesh->mNumBones == 0)
        {
            LogWarning("[MeshmoonOpenAssetImport]: Zero bones in mesh " + QString(mesh->mName.data) + ", skipping.");
            continue;
        }

        // Fill boneMatrices with current bone locations
        std::vector<aiMatrix4x4> boneMatrices(mesh->mNumBones);
        for(size_t a = 0; a < mesh->mNumBones; ++a)
        {
            // find the corresponding node by again looking recursively through the node hierarchy for the same name
            aiBone* bone = mesh->mBones[a];            
            aiNode* node = bone ? sc->mRootNode->FindNode(bone->mName) : 0;
            if (!node)
            {
                LogError("PLAH");
                continue;
            }

            // start with the mesh-to-bone matrix
            boneMatrices[a] = bone->mOffsetMatrix;

            // and now append all node transformations down the parent chain until we're back at mesh coordinates again
            const aiNode* tempNode = node;
            while(tempNode)
            {
                // check your matrix multiplication order here!
                boneMatrices[a] = tempNode->mTransformation * boneMatrices[a];
                tempNode = tempNode->mParent;
            }
            mBoneDerivedTransformByName[std::string(bone->mName.data)] = boneMatrices[a];
        }

        // Loop through all vertex weights of all bones.
        // All using the results from the previous code snippet.
        std::vector<aiVector3D> resultPos(mesh->mNumVertices);
        std::vector<aiVector3D> resultNorm(mesh->mNumVertices);
        for(size_t a = 0; a < mesh->mNumBones; ++a)
        {
            const aiBone* bone = mesh->mBones[a];
            const aiMatrix4x4& posTrafo = boneMatrices[a];

            // 3x3 matrix, contains the bone matrix without the translation, only with rotation and possibly scaling
            aiMatrix3x3 normTrafo = aiMatrix3x3(posTrafo);
            for(size_t b = 0; b < bone->mNumWeights; ++b)
            {
                const aiVertexWeight& weight = bone->mWeights[b];
                size_t vertexId = weight.mVertexId;
                const aiVector3D& srcPos = mesh->mVertices[vertexId];
                const aiVector3D& srcNorm = mesh->mNormals[vertexId];

                resultPos[vertexId] += (posTrafo * srcPos) * weight.mWeight;
                resultNorm[vertexId] += (normTrafo * srcNorm) * weight.mWeight;
            }
        }

        for (t = 0; t < mesh->mNumFaces; ++t)
        {
            const struct aiFace* face = &mesh->mFaces[t];
            for(i = 0; i < face->mNumIndices; i++)        // go through all vertices in face
            {
                int vertexIndex = face->mIndices[i];    // get group index for current index

                mesh->mNormals[vertexIndex] = resultNorm[vertexIndex];
                mesh->mVertices[vertexIndex] = resultPos[vertexIndex];
            }
        }
    }

    // draw all children
    for (n = 0; n < nd->mNumChildren; n++)
        GetBasePose(sc, nd->mChildren[n]);
}

void MeshmoonOpenAssetImporter::Reset()
{
    mSkeleton.setNull();
    mBonesByName.clear();
    mBoneNodesByName.clear();
    mNodeDerivedTransformByName.clear();
    mBoneDerivedTransformByName.clear();
    boneMap.clear();
    
    mSubMeshCount = 0;
    msBoneCount = 0;

    mMaterialCode = "";
    mCustomAnimationName = "";
    
    importInfo_.Reset();
    animationInfos_.clear();

    destinationMeshAsset.reset();
    destinationSkeletonAsset.reset();

    completedEmitted = false;
    
    // Do NOT clear depsResolved_ as this (via Import() etc.)
    // is called multiple times while deps are being resolved!
}

void MeshmoonOpenAssetImporter::CheckCompletion()
{
    PROFILE(MeshmoonOpenAssetImport_CheckCompletion)
    if (meshCreated && textureReceivers_.isEmpty() && !completedEmitted)
    {
        completedEmitted = true;

        LogDebug(LC + "Emitting ImportDone() with success");
        emit ImportDone(this, true, importInfo_);

        // Special deps flags reseted only on completion
        pendingDependencies_.clear();
        depsResolved_ = false;
    }
}

bool MeshmoonOpenAssetImporter::EmitFailure(const QString &error)
{
    PROFILE(MeshmoonOpenAssetImport_EmitFailure)
    if (!error.isEmpty())
        LogError(LC + error);

    if (!completedEmitted)
    {
        completedEmitted = true;

        LogDebug(LC + "Emitting ImportDone() with failure");
        emit ImportDone(this, false, importInfo_);

        // Special deps flags reseted only on completion
        pendingDependencies_.clear();
        depsResolved_ = false;
    }
    return false;
}

void MeshmoonOpenAssetImporter::InitImportInfo(const QString &assetRef, const QString &diskSource)
{
    PROFILE(MeshmoonOpenAssetImport_InitImportInfo)
    importInfo_.Reset();
    importInfo_.importedAssetRef = assetRef;

    // .dae upAxis/exporter + custom Meshmoon .dae extensions at <meshmoon>
    if (assetRef.endsWith(".dae", Qt::CaseInsensitive) && !diskSource.isEmpty() && QFile::exists(diskSource))
    {
        QFile daeFile(diskSource);
        if (daeFile.open(QIODevice::ReadOnly))
        {
            QTextStream stream(&daeFile);
            stream.setCodec("UTF-8");           
            QDomDocument daeXml;
            if (daeXml.setContent(stream.readAll()))
            {
                bool conversionOk = false;               
                QDomElement root = daeXml.firstChildElement("COLLADA");
                QDomElement meshmoon = !root.isNull() ? root.firstChildElement("meshmoon") : QDomElement();
                
                // Meshmoon mesh transform information
                QDomNodeList meshmoonTransform = !meshmoon.isNull() ? meshmoon.elementsByTagName("meshmoon_transform") : QDomNodeList();
                if (!meshmoonTransform.isEmpty())
                {
                    QDomElement transformElement = meshmoonTransform.item(0).toElement();
                    if (meshmoonTransform.size() > 1)
                        LogWarning(LC + "More than one \"meshmoon_transform\" XML elements found, using first.");

                    // Position
                    if (transformElement.hasAttribute("position"))
                    {
                        QStringList posSplit = transformElement.attribute("position").trimmed().split(",", QString::SkipEmptyParts);
                        if (posSplit.size() >= 3)
                        {
                            conversionOk = true;
                            if (conversionOk) importInfo_.transform.pos.x = posSplit.at(0).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.pos.y = posSplit.at(1).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.pos.z = posSplit.at(2).trimmed().toFloat(&conversionOk);
                            if (!conversionOk)
                            {
                                importInfo_.transform.pos = float3::zero;
                                LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid double value(s) for \"position\" at line %1 column %2")
                                    .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                            }
                        }
                        else
                            LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid \"position\" value. Valid format: \"x,y,z\" at line %1 column %2")
                                .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                    }
                    
                    // Rotation
                    if (transformElement.hasAttribute("rotation"))
                    {
                        QStringList rotSplit = transformElement.attribute("rotation").trimmed().split(",", QString::SkipEmptyParts);
                        if (rotSplit.size() >= 3)
                        {
                            conversionOk = true;
                            if (conversionOk) importInfo_.transform.rot.x = rotSplit.at(0).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.rot.y = rotSplit.at(1).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.rot.z = rotSplit.at(2).trimmed().toFloat(&conversionOk);
                            if (!conversionOk)
                            {
                                importInfo_.transform.rot = float3::zero;
                                LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid double value(s) for \"rotation\" at line %1 column %2")
                                    .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                            }
                        }
                        else
                            LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid \"rotation\" value. Valid format: \"x,y,z\" at line %1 column %2")
                                .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                    }
                    
                    // Scale
                    if (transformElement.hasAttribute("scale"))
                    {
                        QStringList scaleSplit = transformElement.attribute("scale").trimmed().split(",", QString::SkipEmptyParts);
                        if (scaleSplit.size() >= 3)
                        {
                            conversionOk = true;
                            if (conversionOk) importInfo_.transform.scale.x = scaleSplit.at(0).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.scale.y = scaleSplit.at(1).trimmed().toFloat(&conversionOk);
                            if (conversionOk) importInfo_.transform.scale.z = scaleSplit.at(2).trimmed().toFloat(&conversionOk);
                            if (!conversionOk)
                            {
                                importInfo_.transform.scale = float3::one;
                                LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid double value(s) for \"scale\" at line %1 column %2")
                                    .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                            }
                        }
                        else
                            LogError(QString(LC + "XML element \"meshmoon_transform\" has invalid \"scale\" value. Valid format: \"x,y,z\" at line %1 column %2")
                                .arg(transformElement.lineNumber()).arg(transformElement.columnNumber()));
                    }
                }
                    
                // Meshmoon animation split information
                QDomElement animations = !meshmoon.isNull() ? meshmoon.firstChildElement("meshmoon_animations") : QDomElement();
                if (!animations.isNull())
                {
                    // Read framerate
                    uint framerate = 24;
                    if (animations.hasAttribute("framerate"))
                    {
                        bool frameIsNumber = false;
                        framerate = animations.attribute("framerate").trimmed().toUInt(&frameIsNumber);
                        if (!frameIsNumber)
                        {
                            framerate = 24;
                            LogError(LC + "XML elements \"meshmoon_animations\" attribute \"framerate\" cannot be converted to a number. Defaulting to 24. Found value: \"" + animations.attribute("framerate").trimmed() + "\".");
                        }
                    }
                    else
                        LogWarning(LC + "XML element \"meshmoon_animations\" does not define attribute \"framerate\". Defaulting to 24.");
                    
                    QDomNodeList tundraAnims = animations.elementsByTagName("meshmoon_animation");
                    for(int i=0; i<tundraAnims.size(); ++i)
                    {
                        QDomElement animElement = tundraAnims.at(i).toElement();
                        if (animElement.hasAttribute("name") && animElement.hasAttribute("start") && (animElement.hasAttribute("end") || animElement.hasAttribute("length")))
                        {
                            AnimationInfo anim; 
                            anim.framerate = framerate;
                            anim.name = animElement.attribute("name").trimmed();
                            if (anim.name.isEmpty())
                            {
                                LogError(QString(LC + "XML element \"meshmoon_animation\" has empty string value for \"name\", name must be defined, at line %1 column %2")
                                    .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                continue;
                            }
                            anim.frameStart = animElement.attribute("start").toUInt(&conversionOk);
                            if (!conversionOk)
                            {
                                LogError(QString(LC + "XML element \"meshmoon_animation\" has invalid uint value for \"start\" at line %1 column %2")
                                    .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                continue;
                            }
                            // We accept both "length" and "end" to calculate duration. If both are present prefer "end".
                            if (animElement.hasAttribute("end"))
                            {
                                uint frameEnd = animElement.attribute("end").toUInt(&conversionOk);
                                if (!conversionOk)
                                {
                                    LogError(QString(LC + "XML element \"meshmoon_animation\" has invalid uint value for \"end\" at line %1 column %2")
                                        .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                    continue;
                                }
                                if (frameEnd <= anim.frameStart)
                                {
                                    LogError(QString(LC + "Animation end frame was detected to be smaller or the same than the start frame, invalid definition, at line %1 column %2")
                                        .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                    continue;
                                }
                                anim.frameEnd = frameEnd;
                            }
                            else if (animElement.hasAttribute("length"))
                            {
                                uint lengthFrames = animElement.attribute("length").toUInt(&conversionOk);
                                if (!conversionOk)
                                {
                                    LogError(QString(LC + "XML element \"meshmoon_animation\" has invalid uint value for \"length\" at line %1 column %2")
                                        .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                    continue;
                                }
                                if (lengthFrames <= 0)
                                {
                                    LogError(QString(LC + "Animation length in frames was detected to be negative or zero, at line %1 column %2")
                                        .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                    continue;
                                }
                                anim.frameEnd = anim.frameStart + lengthFrames;
                            }
                            else
                            {
                                LogWarning(QString(LC + "XML element \"meshmoon_animation\" is missing \"end\" frame or \"length\" in frames, define at least one, at line %1 column %2")
                                    .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                                continue;
                            }
                            
                            LogDebug(LC + "Found collada file Tundra animation information: " + anim.toString());
                            animationInfos_ << anim;
                        }
                        else
                        {
                            LogWarning(QString(LC + "XML element \"meshmoon_animation\" is missing one of the needed attributes: \"name\", \"start\" and \"end\" or \"length\" at line %1 column %2")
                                .arg(animElement.lineNumber()).arg(animElement.columnNumber()));
                        }
                    }
                }
                
                // Exporter information
                QDomElement asset = !root.isNull() ? root.firstChildElement("asset") : QDomElement();
                if (!asset.isNull())
                {
                    QDomNodeList authoringTools = asset.elementsByTagName("authoring_tool");
                    if (!authoringTools.isEmpty())
                        importInfo_.exporter = authoringTools.item(0).toElement().text();
                    QDomNodeList upAxis = asset.elementsByTagName("up_axis");
                    if (!upAxis.isEmpty())
                    {
                        QString upAxisStr = upAxis.item(0).toElement().text().toLower();
                        if (upAxisStr == "z_up")
                            importInfo_.upAxis = float3::unitZ;
                        else if (upAxisStr == "y_up")
                            importInfo_.upAxis = float3::unitY;
                        else if (upAxisStr == "x_up")
                            importInfo_.upAxis = float3::unitX;
                    }
                }
            }
            daeFile.close();
        }
    }
}

int MeshmoonOpenAssetImporter::NumPendingPreLoadDependencies() const
{
    return pendingDependencies_.size();
}

int MeshmoonOpenAssetImporter::RemovePreLoadDependency(const QString &assetRef)
{
    for(int i=0; i<pendingDependencies_.size(); ++i)
    {
        if (pendingDependencies_[i].compare(assetRef, Qt::CaseSensitive) == 0)
        {
            pendingDependencies_.removeAt(i);
            i--;
        }
    }
    
    // Boot up the import
    if (pendingDependencies_.size() == 0)
    {
        LogDebug(LC + "Pre-load dependencies fetched. Starting import.");
        /// @todo This is currently only supported for Import(). See if others need it.
        Import(&waitingImportData_.data[0], waitingImportData_.data.size(), waitingImportData_.assetRef, waitingImportData_.diskSource,
            destinationMeshAsset, destinationSkeletonAsset);
        waitingImportData_.data.clear();
    }    
    return NumPendingPreLoadDependencies();
}

int MeshmoonOpenAssetImporter::ResolveAndRequestDependencies(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource)
{
    if (!depsResolved_)
    {
        depsResolved_ = true;
        
        if (assetRef.endsWith(".obj", Qt::CaseSensitive))
            ResolveAndRequestObjDependencies(data_, numBytes, assetRef, diskSource);
    }
    
    int numPending = NumPendingPreLoadDependencies();
    if (numPending > 0 && waitingImportData_.data.size() == 0)
    {
        waitingImportData_.data.insert(waitingImportData_.data.end(), data_, data_ + numBytes);
        waitingImportData_.assetRef = assetRef;
        waitingImportData_.diskSource = diskSource;
    }
    return numPending;
}

void MeshmoonOpenAssetImporter::ResolveAndRequestObjDependencies(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource)
{
    // The mtllib defines are usually right at the top of the file.
    // We don't want to check each line for huge files (I've seen 70mb .obj files)
    const int maxLinesToRead = 500;
    const QString materialDefine = "mtllib ";

    QByteArray bytes = QByteArray::fromRawData((const char*)data_, numBytes);
    QTextStream stream(&bytes, QIODevice::ReadOnly|QIODevice::Text);

    int linesRead = 0;
    while (!stream.atEnd() && linesRead < maxLinesToRead)
    {
        QString line = stream.readLine().trimmed();
        if (line.startsWith(materialDefine, Qt::CaseSensitive))
        {
            QString foundRef = line.mid(materialDefine.length());
            QString dependencyRef = ResolveDependency(assetRef, diskSource, foundRef);
            if (!dependencyRef.isEmpty())
            {
                AssetTransferPtr transPtr = assetAPI_->RequestAsset(dependencyRef, "Binary");
                if (transPtr)
                {
                    LogDebug(LC + "Requested pre-load dependency " + transPtr->SourceUrl());
                    pendingDependencies_ << transPtr->SourceUrl();

                    connect(transPtr.get(), SIGNAL(Succeeded(AssetPtr)), this, SLOT(OnPreLoadDependencyLoaded(AssetPtr)), Qt::UniqueConnection);
                    connect(transPtr.get(), SIGNAL(Failed(IAssetTransfer*, QString)), this, SLOT(OnPreLoadDependencyFailed(IAssetTransfer*, QString)), Qt::UniqueConnection);
                }
                else
                    LogError(LC + "Failed to request pre-load dependency " + dependencyRef);
            }
        }
        linesRead++;
    }
}

void MeshmoonOpenAssetImporter::OnPreLoadDependencyLoaded(AssetPtr asset)
{
    QString assetRef = asset->Name();
    // This asset can be unloaded from the system. It is not needed by Tundra.
    // It's only fetched so HTTP assets are in cache and we can provide the file to Assimp.
    assetAPI_->ForgetAsset(assetRef, false);
    
    RemovePreLoadDependency(assetRef);
}

void MeshmoonOpenAssetImporter::OnPreLoadDependencyFailed(IAssetTransfer* assetTransfer, QString reason)
{
    // This error is already logged by AssetAPI, no need to repeat here.
    RemovePreLoadDependency(assetTransfer->SourceUrl());
}

QString MeshmoonOpenAssetImporter::ResolveDependency(const QString &contextRef, const QString &parentDiskSource, const QString &depRef)
{
    QFileInfo tfi(depRef);
    QString resolvedRef = "";
    AssetAPI::AssetRefType assetRefType = AssetAPI::ParseAssetRef(contextRef);

    // URL
    if (depRef.startsWith("http:", Qt::CaseInsensitive) || depRef.startsWith("https:", Qt::CaseInsensitive))
        resolvedRef = depRef;
    // C:/path/to/my.dae and local://my.dae
    // Both are resolved using the disk source of the parent. We don't want to generate a vague local://
    // ref for the texture, as we clearly know where the parent is, so we can resolve relative refs from it.
    else if (assetRefType == AssetAPI::AssetRefLocalPath || assetRefType == AssetAPI::AssetRefLocalUrl)
    {
        if (tfi.isRelative())
            resolvedRef = QFileInfo(parentDiskSource).absoluteDir().absoluteFilePath(tfi.filePath());
        else
            resolvedRef = tfi.absoluteFilePath();

        if (QFile::exists(resolvedRef))
            resolvedRef = QDir::toNativeSeparators(resolvedRef);
        else
        {
            resolvedRef = "";
            LogWarning(LC + "Failed to find dependency from referenced disk path: " + tfi.absoluteFilePath() + " with parent: " + contextRef);
        }
    }
    // http://some.host.com/my.dae
    else if (assetRefType == AssetAPI::AssetRefExternalUrl || assetRefType == AssetAPI::AssetRefRelativePath)
    {
        if (!tfi.isAbsolute())
            resolvedRef = assetAPI_->ResolveAssetRef(contextRef, depRef);
        else
            LogWarning(LC + "Cannot resolve dependency ref from an absolute disk path: " + tfi.absoluteFilePath() + " with base: " + 
                contextRef.split("/", QString::SkipEmptyParts).last());
    }
    return resolvedRef;
};

bool MeshmoonOpenAssetImporter::Import(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, AssetPtr meshAsset, AssetPtr skeletonAsset)
{
    PROFILE(MeshmoonOpenAssetImport_Import)

    kNet::PolledTimer tTotal, t;
    tTotal.Start();
    t.Start();

    Reset();

    OgreMeshAsset *ogreMeshAsset = dynamic_cast<OgreMeshAsset*>(meshAsset.get());
    OgreSkeletonAsset *ogreSkeletonAsset = dynamic_cast<OgreSkeletonAsset*>(skeletonAsset.get());
    if (!ogreMeshAsset)
        return EmitFailure("Input mesh assets is invalid!");

    destinationMeshAsset = meshAsset;
    destinationSkeletonAsset = skeletonAsset;

    InitImportInfo(assetRef, diskSource);
    
    /** Certain assets like .obj define dependency assets like materials.
        Although Assimp will parse these and we can provide a custom IOSystem* for providing these assets.
        It is too late, as these Assimp requests are synchronous and will fail if not provided right away.
        We need to resolve and fetch pre-load dependencies before the import is done. If the data was loaded from a disk source
        the importer.ReadFile() will do this correctly. But in a hosted Meshmoon env we cannot make this assumption,
        HTTP refs must work and we must fetch deps from that context. */
    if (ResolveAndRequestDependencies(data_, numBytes, assetRef, diskSource) > 0)
    {
        LogDebug(LC + QString("Resolved dependencies in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
        return true;
    }

    Assimp::Importer importer;
    if (debugLoggingEnabled_)
        Assimp::DefaultLogger::create("assimp-meshmoon-import.log", Assimp::Logger::VERBOSE);

    // Default AI_CONFIG_PP_SLM_VERTEX_LIMIT 1000000
    // Default AI_CONFIG_PP_SLM_TRIANGLE_LIMIT 1000000
    //importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 21845);
    
    int removePrimitiveFlags = 0
                          | aiPrimitiveType_LINE
                          | aiPrimitiveType_POINT;
    
    int removeComponentFlags = 0
                          | aiComponent_CAMERAS
                          | aiComponent_LIGHTS;                        
    
    uint postProcessFlags = 0
                          //| aiProcess_OptimizeGraph //@todo Enable?
                          | aiProcess_OptimizeMeshes
                          | aiProcess_SplitLargeMeshes
                          | aiProcess_JoinIdenticalVertices
                          | aiProcess_LimitBoneWeights
                          | aiProcess_GenSmoothNormals
                          | aiProcess_Triangulate
                          | aiProcess_GenUVCoords
                          | aiProcess_FlipUVs
                          | aiProcess_FixInfacingNormals
                          | aiProcess_RemoveRedundantMaterials
                          | aiProcess_ImproveCacheLocality
                          | aiProcess_SortByPType
                          | aiProcess_FindInvalidData
                          | aiProcess_FindDegenerates;

    /** aiProcess_PreTransformVertices will remove all animations from the input data.
        It should be used when no skeleton/anims is being loaded from the assimp scene.
        When importing skeleton, the vertice transforms are performed in CreateVertexData 
        with the info gathered from ComputeNodesDerivedTransform */
    if (!ogreSkeletonAsset)
    {
        removeComponentFlags |= aiComponent_ANIMATIONS;
        postProcessFlags |= aiProcess_PreTransformVertices;
    }
    
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, removePrimitiveFlags);
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);

#include "DisableMemoryLeakCheck.h"

    // This IO handler will resolve refs to on disk files.
    importer.SetIOHandler(new MeshmoonOpenAssetImportIOSystem(framework_, assetRef, diskSource));
   
    // With asset ref
    scene = importer.ReadFile(assetRef.toStdString(), postProcessFlags);
    // From disk source
    if (!scene)
    {
        LogError(LC + "Import failed with asset ref: " + assetRef);

        QString nativeDiskSource = QDir::toNativeSeparators(diskSource);
        if (QFile::exists(nativeDiskSource))
        {
            scene = importer.ReadFile(nativeDiskSource.toStdString(), postProcessFlags);
            if (!scene)
                LogError(LC + "Import failed with disk source: " + nativeDiskSource);
        }
    }
    // From memory
    if (!scene)
    {
        QFileInfo fileInfo(diskSource);
        std::string fileTypeHint = "." + fileInfo.suffix().toStdString();
        scene = importer.ReadFileFromMemory(static_cast<const void*>(data_), numBytes, postProcessFlags, fileTypeHint.c_str());
    }
    if (!scene)
        return EmitFailure(QString("Import failed, tried from both memory and from disk. Latest error: %1").arg(importer.GetErrorString()));
            
    if (debugLoggingEnabled_)
    {
        LogDebug(LC + "Importing " + ogreMeshAsset->Name());
        LogDebug(LC + QString("Assimp import done in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
        t.Start();
    }

    std::string meshName = AssetAPI::SanitateAssetRef(ogreMeshAsset->Name()).toStdString();
    std::string skeletonName = (ogreSkeletonAsset ? AssetAPI::SanitateAssetRef(ogreSkeletonAsset->Name()).toStdString() : "");

    // Bone weights
    if (scene->HasAnimations())
        GetBasePose(scene, scene->mRootNode);
        
    // Parse all nodes to internal map and set if they are needed for the end product.
    GrabNodeNamesFromNode(scene, scene->mRootNode);
    GrabBoneNamesFromNode(scene, scene->mRootNode);
    
    /*if (colladaInfo_.IsBlender())
    {
        aiMatrix4x4 transform;
        transform.FromEulerAnglesXYZ(0, 0, 0); //DegToRad(180));
        scene->mRootNode->mTransformation = transform;
    }*/
    
    // Calculate node transforms
    ComputeNodesDerivedTransform(scene, scene->mRootNode, scene->mRootNode->mTransformation);

    LogDebug(LC + QString("Nodes and transform in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
    t.Start();
    
    if (ogreSkeletonAsset && !mBonesByName.empty())
    {
        // Create skeleton
        std::string generatedSkeletonName = "generated-" + skeletonName;

        mSkeleton = Ogre::SkeletonManager::getSingleton().create(generatedSkeletonName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true); 

        // Create bones to skeleton
        msBoneCount = 0;
        CreateBonesFromNode(scene, scene->mRootNode);
        
        // Sort out parent-child bone relations
        msBoneCount = 0;
        CreateBoneHiearchy(scene, scene->mRootNode);

        // Create animations to skeleton
        if (scene->HasAnimations())
            for(uint i = 0; i < scene->mNumAnimations; ++i)
                ParseAnimation(scene, i, scene->mAnimations[i]);

        mSkeleton->setBindingPose();
               
        /** Export to input skeleton via serializer to make it work properly.
            Animations seem to be broken if this is not done.
            @todo Find out why the above happens without spinning it to disk via the serializer. 
            I've read the load path and serializer, cant find anything obvious. */
        try
        {
            QString tempExportPath = assetAPI_->GenerateTemporaryNonexistingAssetFilename(QString::fromStdString(generatedSkeletonName));

            Ogre::SkeletonSerializer ser;
            ser.exportSkeleton(mSkeleton.getPointer(), tempExportPath.toStdString());
            
            QFile tempExportFile(tempExportPath);
            if (tempExportFile.open(QIODevice::ReadOnly))
            {
                QByteArray data = tempExportFile.readAll();
                tempExportFile.close();
                QFile::remove(tempExportPath);

                if (!data.isEmpty())
                {
                    Ogre::DataStreamPtr importData = Ogre::MemoryDataStreamPtr(new Ogre::MemoryDataStream((void*)data.data(), data.size(), false, true));   
                    mSkeleton = Ogre::SkeletonManager::getSingleton().create(skeletonName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
                    ser.importSkeleton(importData, mSkeleton.getPointer());
                    ogreSkeletonAsset->ogreSkeleton = mSkeleton;
                    
                    Ogre::SkeletonManager::getSingleton().remove(generatedSkeletonName);
                }
            }
        }
        catch(Ogre::Exception /*&ex*/) { }

        if (!ogreSkeletonAsset->ogreSkeleton.getPointer())
        {
            LogWarning(LC + "Failed to export skeleton to temp disk resource. Animations might be corrupted.");
            ogreSkeletonAsset->ogreSkeleton = mSkeleton;
        }

        LogDebug(LC + QString("Skeleton created in %1 msec").arg(t.MSecsElapsed(), 0, 'f', 4));
        t.Start();
    }

    Ogre::MeshPtr mesh = (ogreMeshAsset->ogreMesh.get() ? ogreMeshAsset->ogreMesh : Ogre::MeshManager::getSingleton().createManual(meshName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME));

    // Create submeshes to input mesh ptr.
    mSubMeshCount = 0;
    CreateSubmeshesFromNode(scene, scene->mRootNode, diskSource, ogreMeshAsset->Name(), mesh, assetRef);

    // Apply skeleton
    if (mSkeleton.get())
        mesh->_notifySkeleton(mSkeleton);

    uint numReorganized = 0;
    Ogre::Mesh::SubMeshIterator submeshIter = mesh->getSubMeshIterator();
    while (submeshIter.hasMoreElements())
    {
        Ogre::SubMesh* submesh = submeshIter.getNext();
        if (submesh->useSharedVertices)
            continue;

        // Reorganize vertex data.
#if OGRE_VERSION_MINOR >= 8 && OGRE_VERSION_MAJOR >= 1
        Ogre::VertexDeclaration *newDeclaration = submesh->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mesh->hasSkeleton(), mesh->hasVertexAnimation(), false);
#else
        Ogre::VertexDeclaration *newDeclaration = submesh->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mesh->hasSkeleton(), mesh->hasVertexAnimation());
#endif
        // Check if changed (might return itself)
        if (*newDeclaration != *(submesh->vertexData->vertexDeclaration))
        {
            // Usages don't matter here since we're only exporting
            Ogre::BufferUsageList usages;
            for (unsigned short bui = 0; bui <= newDeclaration->getMaxSource(); ++bui)
                usages.push_back(Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
            submesh->vertexData->reorganiseBuffers(newDeclaration, usages);
            numReorganized++;
        }
    }
    if (debugLoggingEnabled_)
    {
        if (numReorganized > 0)
            LogDebug(LC + QString("Reorganized %1 vertex buffers").arg(numReorganized));
        LogDebug(LC + QString("Mesh with %1 submeshes created in %2 msec").arg(mesh->getNumSubMeshes()).arg(t.MSecsElapsed(), 0, 'f', 4));
    }

    importInfo_.createdOgreMeshName = QString::fromStdString(mesh->getName());
    if (mSkeleton.get())
        importInfo_.createdOgreSkeletonName = QString::fromStdString(mSkeleton->getName());

    if (debugLoggingEnabled_)
        LogDebug(LC + QString("Import completed %1 in %2 msec").arg(importInfo_.toString()).arg(tTotal.MSecsElapsed(), 0, 'f', 4));

    meshCreated = true;
    ogreMeshAsset->ogreMesh = mesh;

#include "EnableMemoryLeakCheck.h"

    CheckCompletion();
    return true;
}

bool MeshmoonOpenAssetImporter::ImportWithExternalAnimations(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, AssetPtr meshAsset, 
                                                             AssetPtr skeletonAsset, QString animationName, QList<AnimationAssetData> animationAssets)
{
    PROFILE(MeshmoonOpenAssetImport_ImportWithExternalAnimations)
    Reset();
    
    OgreMeshAsset *ogreMeshAsset = dynamic_cast<OgreMeshAsset*>(meshAsset.get());
    OgreSkeletonAsset *ogreSkeletonAsset = dynamic_cast<OgreSkeletonAsset*>(skeletonAsset.get());
    if (!ogreMeshAsset || !ogreSkeletonAsset)
    {
        LogError(LC + "Input mesh and/or skeleton assets are invalid!");
        return false;
    }

    destinationMeshAsset = meshAsset;
    destinationSkeletonAsset = skeletonAsset;

    InitImportInfo(assetRef, diskSource);

    Assimp::Importer importer;
    if (debugLoggingEnabled_)
        Assimp::DefaultLogger::create("assimp-meshmoon-import.log", Assimp::Logger::VERBOSE);
    
    // Default AI_CONFIG_PP_SLM_VERTEX_LIMIT 1000000
    // Default AI_CONFIG_PP_SLM_TRIANGLE_LIMIT 1000000
    //importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 21845);
    
    int removePrimitiveFlags = 0
                          | aiPrimitiveType_LINE
                          | aiPrimitiveType_POINT;
    
    int removeComponentFlags = 0
                          | aiComponent_CAMERAS
                          | aiComponent_LIGHTS;                        
    
    uint postProcessFlags = 0
                          //| aiProcess_OptimizeGraph //@todo Enable?
                          | aiProcess_OptimizeMeshes
                          | aiProcess_SplitLargeMeshes
                          | aiProcess_JoinIdenticalVertices
                          | aiProcess_LimitBoneWeights
                          | aiProcess_GenSmoothNormals
                          | aiProcess_Triangulate
                          | aiProcess_GenUVCoords
                          | aiProcess_FlipUVs
                          | aiProcess_FixInfacingNormals
                          | aiProcess_RemoveRedundantMaterials
                          | aiProcess_ImproveCacheLocality
                          | aiProcess_SortByPType
                          | aiProcess_FindInvalidData
                          | aiProcess_FindDegenerates;
    
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, removePrimitiveFlags);
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);

    QFileInfo fileInfo(diskSource);
    std::string fileTypeHint = "." + fileInfo.suffix().toStdString();
    scene = importer.ReadFileFromMemory(static_cast<const void*>(data_), numBytes, postProcessFlags, fileTypeHint.c_str());

    if(!scene)
    {
        LogError(LC + "Failed to import " + assetRef + " from memory.");
        LogError(QString(LC + "Import error: ") + importer.GetErrorString());
        LogInfo(LC + "Trying to load data from file:" + diskSource);
        
        // If the importer failed to read the file from memory, try to read the file again.
        QString nativeDiskSource = QDir::toNativeSeparators(diskSource);
        scene = importer.ReadFile(nativeDiskSource.toStdString(), postProcessFlags);
        if (!scene)
        {
            LogError(LC + "Conversion failed, importer unable to load data from file:" + diskSource);
            return false;
        }
    }

    std::string meshName = AssetAPI::SanitateAssetRef(ogreMeshAsset->Name()).toStdString();
    std::string skeletonName = AssetAPI::SanitateAssetRef(ogreSkeletonAsset->Name()).toStdString();

    // Bone weights
    if (scene->HasAnimations())
        GetBasePose(scene, scene->mRootNode);
        
    // Parse all nodes to internal map and set if they are needed for the end product.
    GrabNodeNamesFromNode(scene, scene->mRootNode);
    GrabBoneNamesFromNode(scene, scene->mRootNode);
    
    /*if (colladaInfo_.IsBlender())
    {
        aiMatrix4x4 transform;
        transform.FromEulerAnglesXYZ(0, 0, 0); //DegToRad(180));
        scene->mRootNode->mTransformation = transform;
    }*/
    
    // Calculate node transforms
    ComputeNodesDerivedTransform(scene, scene->mRootNode, scene->mRootNode->mTransformation);

    if (!mBonesByName.empty())
    {
        // Create skeleton
        std::string generatedSkeletonName = "generated-" + skeletonName;

        mSkeleton = Ogre::SkeletonManager::getSingleton().create(generatedSkeletonName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true); 
        mCustomAnimationName = animationName.toStdString();

        // Create bones to skeleton
        msBoneCount = 0;
        CreateBonesFromNode(scene, scene->mRootNode);
        
        // Sort out parent-child bone relations
        msBoneCount = 0;
        CreateBoneHiearchy(scene, scene->mRootNode);

        // Create animations to skeleton
        if(scene->HasAnimations())
            for(uint i = 0; i < scene->mNumAnimations; ++i)
                ParseAnimation(scene, i, scene->mAnimations[i]);

        // Load animations from external files
        foreach(AnimationAssetData data, animationAssets)
            ImportAnimation(data.data_, data.numBytes, data.assetRef, data.diskSource, data.name);

        mSkeleton->setBindingPose();
               
        /** Export to input skeleton via serializer to make it work properly.
            Animations seem to be broken if this is not done.
            @todo Find out why the above happens without spinning it to disk via the serializer. 
            I've read the load path and serializer, cant find anything obvious. */
        try
        {
            QString tempExportPath = assetAPI_->GenerateTemporaryNonexistingAssetFilename(QString::fromStdString(generatedSkeletonName));

            Ogre::SkeletonSerializer ser;
            ser.exportSkeleton(mSkeleton.getPointer(), tempExportPath.toStdString());
            
            QFile tempExportFile(tempExportPath);
            if (tempExportFile.open(QIODevice::ReadOnly))
            {
                QByteArray data = tempExportFile.readAll();
                tempExportFile.close();
                QFile::remove(tempExportPath);
                
                if (!data.isEmpty())
                {
#include "DisableMemoryLeakCheck.h"
                    Ogre::DataStreamPtr importData = Ogre::MemoryDataStreamPtr(new Ogre::MemoryDataStream((void*)data.data(), data.size(), false, true));   
#include "EnableMemoryLeakCheck.h"
                    mSkeleton = Ogre::SkeletonManager::getSingleton().create(skeletonName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, true);
                    ser.importSkeleton(importData, mSkeleton.getPointer());
                    ogreSkeletonAsset->ogreSkeleton = mSkeleton;
                    
                    Ogre::SkeletonManager::getSingleton().remove(generatedSkeletonName);
                }
            }
        }
        catch(Ogre::Exception /*&ex*/) { }

        if (!ogreSkeletonAsset->ogreSkeleton.getPointer())
        {
            LogWarning(LC + "Failed to export skeleton to temp disk resource. Animations might be corrupted.");
            ogreSkeletonAsset->ogreSkeleton = mSkeleton;
        }
    }

    Ogre::MeshPtr mesh = (ogreMeshAsset->ogreMesh.get() ? ogreMeshAsset->ogreMesh : Ogre::MeshManager::getSingleton().createManual(meshName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME));
    
    // Create submeshes to input mesh ptr.
    mSubMeshCount = 0;
    CreateSubmeshesFromNode(scene, scene->mRootNode, diskSource, ogreMeshAsset->Name(), mesh, assetRef);

    // Apply skeleton
    if (mSkeleton.get())
        mesh->_notifySkeleton(mSkeleton);

    Ogre::Mesh::SubMeshIterator submeshIter = mesh->getSubMeshIterator();
    while (submeshIter.hasMoreElements())
    {
        Ogre::SubMesh* submesh = submeshIter.getNext();
        if (submesh->useSharedVertices)
            continue;

        // Reorganize vertex data.
#if OGRE_VERSION_MINOR >= 8 && OGRE_VERSION_MAJOR >= 1
        Ogre::VertexDeclaration *newDeclaration = submesh->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mesh->hasSkeleton(), mesh->hasVertexAnimation(), false);
#else
        Ogre::VertexDeclaration *newDeclaration = submesh->vertexData->vertexDeclaration->getAutoOrganisedDeclaration(mesh->hasSkeleton(), mesh->hasVertexAnimation());
#endif
        // Check if changed (might return itself)
        if (*newDeclaration != *(submesh->vertexData->vertexDeclaration))
        {
            // Usages don't matter here since we're only exporting
            Ogre::BufferUsageList usages;
            for (unsigned short bui = 0; bui <= newDeclaration->getMaxSource(); ++bui)
                usages.push_back(Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY);
            LogDebug(LC + QString("  - Reorganizing %1 vertex buffers").arg(usages.size()));
            submesh->vertexData->reorganiseBuffers(newDeclaration, usages);
        }
    }

    if (mesh.get())
        importInfo_.createdOgreMeshName = QString::fromStdString(mesh->getName());
    if (mSkeleton.get())
        importInfo_.createdOgreSkeletonName = QString::fromStdString(mSkeleton->getName());

    if (debugLoggingEnabled_)
        LogDebug(LC + "Import completed " + importInfo_.toString());

    meshCreated = true;
    ogreMeshAsset->ogreMesh = mesh;

    CheckCompletion();
        
    return true;
}

bool MeshmoonOpenAssetImporter::ImportAnimation(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, QString animationName)
{
    PROFILE(MeshmoonOpenAssetImport_ImportAnimation)
    if (!mSkeleton.get())
    {
        LogError(LC + "ImportAnimation called when no skeleton has been loaded yet. Use the Import functions first on " + assetRef);
        return false;
    }

    QFileInfo fileInfo(diskSource);

    Assimp::Importer importer;
    if (debugLoggingEnabled_)
        Assimp::DefaultLogger::create("assimp-meshmoon-import.log", Assimp::Logger::VERBOSE);

    // Default AI_CONFIG_PP_SLM_VERTEX_LIMIT 1000000
    // Default AI_CONFIG_PP_SLM_TRIANGLE_LIMIT 1000000
    //importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 21845);

    int removePrimitiveFlags = 0
                            | aiPrimitiveType_LINE
                            | aiPrimitiveType_POINT;

    int removeComponentFlags = 0
                            | aiComponent_CAMERAS
                            | aiComponent_LIGHTS;                        

    uint postProcessFlags = 0
                            //| aiProcess_OptimizeGraph //@todo Enable?
                            | aiProcess_OptimizeMeshes
                            | aiProcess_SplitLargeMeshes
                            | aiProcess_JoinIdenticalVertices
                            | aiProcess_LimitBoneWeights
                            | aiProcess_GenSmoothNormals
                            | aiProcess_Triangulate
                            | aiProcess_GenUVCoords
                            | aiProcess_FlipUVs
                            | aiProcess_FixInfacingNormals
                            | aiProcess_RemoveRedundantMaterials
                            | aiProcess_ImproveCacheLocality
                            | aiProcess_SortByPType
                            | aiProcess_FindInvalidData
                            | aiProcess_FindDegenerates;

    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, removePrimitiveFlags);
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, removeComponentFlags);

    std::string fileTypeHint = "." + fileInfo.suffix().toStdString();
    const aiScene *aScene = importer.ReadFileFromMemory(static_cast<const void*>(data_), numBytes, postProcessFlags, fileTypeHint.c_str());

    if(!aScene)
    {
        LogError(LC + "Failed to import " + assetRef + " from memory.");
        LogError(QString(LC + "Import error: ") + importer.GetErrorString());
        LogInfo(LC + "Trying to load data from file:" + diskSource);

        // If the importer failed to read the file from memory, try to read the file again.
        aScene = importer.ReadFile(diskSource.toStdString(), postProcessFlags);
        if (!scene)
        {
            LogError(LC + "Conversion failed, importer unable to load data from file:" + diskSource);
            return false;
        }
    }

    mCustomAnimationName = animationName.toStdString();

    // Create animations to skeleton
    if (aScene->HasAnimations())
    {
        for(uint i = 0; i < aScene->mNumAnimations; ++i)
            ParseAnimation(aScene, i, aScene->mAnimations[i]);
    }
    else
        LogWarning(LC + "ImportAnimation could not find any animations from " + assetRef);

    return true;
}

void MeshmoonOpenAssetImporter::ParseAnimation(const aiScene* mScene, int index, aiAnimation* anim)
{
    PROFILE(MeshmoonOpenAssetImport_ParseAnimation)
    if (!mSkeleton.get())
    {
        LogError(LC + "ParseAnimation called when no skeleton has been loaded yet.");
        return;
    }

    // DefBonePose a matrix that represents the local bone transform (can build from Ogre bone components)
    // PoseToKey a matrix representing the keyframe translation
    // What assimp stores aiNodeAnim IS the decomposed form of the transform (DefBonePose * PoseToKey)
    // To get PoseToKey which is what Ogre needs we'ed have to build the transform from components in
    // aiNodeAnim and then DefBonePose.Inverse() * aiNodeAnim(generated transform) will be the right transform

    Ogre::String animName;
    if (mCustomAnimationName != "")
    {
        animName = mCustomAnimationName;
        if (index >= 1)
            animName += Ogre::StringConverter::toString(index);
    }
    else
        animName = Ogre::String(anim->mName.data);
    if (animName.empty())
        animName = "animation" + Ogre::StringConverter::toString(index);
    if (mSkeleton->hasAnimation(animName))
    {
        LogWarning("Skeleton already has animation " + animName + ". Adding index to the name, final name: " + animName + Ogre::StringConverter::toString(index));
        animName += Ogre::StringConverter::toString(index);
    }

    if (animationInfos_.isEmpty())
    {
        if (debugLoggingEnabled_)
        {
            LogDebug(LC + "Animation \"" + QString::fromStdString(animName) + "\"");
            LogDebug(LC + "  Duration = " + QString::number(anim->mDuration));
            LogDebug(LC + "  Tick/sec = " + QString::number(anim->mTicksPerSecond));
            LogDebug(LC + "  Channels = " + QString::number(anim->mNumChannels));
        }
        
        Ogre::Animation* animation = mSkeleton->createAnimation(Ogre::String(animName), Ogre::Real(anim->mDuration));

        for(int i = 0; i < (int)anim->mNumChannels; ++i)
        {
            aiNodeAnim* node_anim = anim->mChannels[i];
            Ogre::String boneName = Ogre::String(node_anim->mNodeName.data);

            if (mSkeleton->hasBone(boneName))
            {
                Ogre::Bone* bone = mSkeleton->getBone(boneName);

                Ogre::Matrix4 defBonePoseInv;
                defBonePoseInv.makeInverseTransform(bone->getPosition(), bone->getScale(), bone->getOrientation());
                
                Ogre::NodeAnimationTrack* track = animation->createNodeTrack(i, bone);
                for(int g = 0; g < 300; ++g)
                {
                    float time;
                    
                    aiMatrix4x4 mat;
                    aiVector3D pos;
                    aiVector3D scale;
                    aiQuaternion rot;               
                    
                    mat = UpdateAnimationFunc(scene, node_anim, g, anim->mDuration, time, mat);
                    mat.Decompose(scale, rot, pos);

                    Ogre::TransformKeyFrame* keyframe = track->createNodeKeyFrame(Ogre::Real(time));

                    Ogre::Vector3 ogrePos(pos.x, pos.y, pos.z);
                    Ogre::Vector3 ogreScale(scale.x, scale.y, scale.z);
                    Ogre::Quaternion ogreRot(rot.w, rot.x, rot.y, rot.z);
                    
                    Ogre::Matrix4 fullTransform;
                    fullTransform.makeTransform(ogrePos, ogreScale, ogreRot);

                    Ogre::Matrix4 poseTokey = defBonePoseInv * fullTransform;
                    poseTokey.decomposition(ogrePos, ogreScale, ogreRot);

                    keyframe->setTranslate(ogrePos);
                    keyframe->setRotation(ogreRot);
                    keyframe->setScale(ogreScale);
                }
            }
        }
    }
    else
    {
        if (debugLoggingEnabled_)
        {
            LogDebug(LC + "Splitting main animation:");
            LogDebug(LC + "  Duration = " + QString::number(anim->mDuration));
            LogDebug(LC + "  Tick/sec = " + QString::number(anim->mTicksPerSecond));
            LogDebug(LC + "  Channels = " + QString::number(anim->mNumChannels));
        }

        foreach(AnimationInfo animInfo, animationInfos_)
        {
            if (debugLoggingEnabled_)
                LogDebug(LC + "Creating Ogre animation: " + animInfo.toString() + 
                    QString(" start seconds=%1 end seconds=%2 duration seconds=%3").arg(animInfo.StartInSeconds()).arg(animInfo.EndInSeconds()).arg(animInfo.DurationInSeconds()));

            animInfo.animation = mSkeleton->createAnimation(animInfo.name.toStdString(), animInfo.DurationInSeconds());
            
            Ogre::Animation* animation = animInfo.animation;
            for(int i=0; i<(int)anim->mNumChannels; ++i)
            {
                aiNodeAnim* node_anim = anim->mChannels[i];
                Ogre::String boneName = Ogre::String(node_anim->mNodeName.data);

                if (mSkeleton->hasBone(boneName))
                {
                    Ogre::Bone* bone = mSkeleton->getBone(boneName);

                    Ogre::Matrix4 defBonePoseInv;
                    defBonePoseInv.makeInverseTransform(bone->getPosition(), bone->getScale(), bone->getOrientation());

                    Ogre::NodeAnimationTrack* track = animation->createNodeTrack(i, bone);

                    double time = 0.f;
                    for(int g = 1; g < 101; g++)
                    {
                        if (g == 100)
                            time = animInfo.EndInSeconds();
                        else
                            time = animInfo.StartInSeconds() + ((animInfo.DurationInSeconds() / 100.f) * g);

                        aiMatrix4x4 mat;
                        aiVector3D pos;
                        aiVector3D scale;
                        aiQuaternion rot;

                        mat = UpdateAnimationFuncWithTime(node_anim, time, anim->mDuration, mat);
                        mat.Decompose(scale, rot, pos);

                        Ogre::TransformKeyFrame* keyframe = track->createNodeKeyFrame(time - animInfo.StartInSeconds());
                        if (!keyframe)
                        {
                            LogError(LC + "Failed to create keyframe " + QString::number(g));
                            i = (int)anim->mNumChannels;
                            break;
                        }

                        Ogre::Vector3 ogrePos(pos.x, pos.y, pos.z);
                        Ogre::Vector3 ogreScale(scale.x, scale.y, scale.z);
                        Ogre::Quaternion ogreRot(rot.w, rot.x, rot.y, rot.z);

                        Ogre::Matrix4 fullTransform;
                        fullTransform.makeTransform(ogrePos, ogreScale, ogreRot);

                        Ogre::Matrix4 poseTokey = defBonePoseInv * fullTransform;
                        poseTokey.decomposition(ogrePos, ogreScale, ogreRot);

                        keyframe->setTranslate(ogrePos);
                        keyframe->setRotation(ogreRot);
                        keyframe->setScale(ogreScale);
                    }
                }
            }
        }
    }
    
    mSkeleton->optimiseAllAnimations();
}

void MeshmoonOpenAssetImporter::MarkAllChildNodesAsNeeded(const aiNode *pNode)
{
    PROFILE(MeshmoonOpenAssetImport_MarkAllChildNodesAsNeeded)
    FlagNodeAsNeeded(pNode->mName.data);
    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; ++childIdx )
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        MarkAllChildNodesAsNeeded(pChildNode);
    }
}

void MeshmoonOpenAssetImporter::GrabNodeNamesFromNode(const aiScene* mScene, const aiNode* pNode)
{
    PROFILE(MeshmoonOpenAssetImport_GrabNodeNamesFromNode)
    BoneNode bNode;
    bNode.node = const_cast<aiNode*>(pNode);
    if (pNode->mParent != NULL)
        bNode.parent = const_cast<aiNode*>(pNode->mParent);
    boneMap.insert(std::make_pair(pNode->mName.data, bNode));
    mBoneNodesByName[pNode->mName.data] = pNode;
    
    if (debugLoggingEnabled_)
        LogDebug(LC + QString("  - Found node %1 %2").arg(mBoneNodesByName.size()-1).arg(pNode->mName.data));

    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; ++childIdx)
    {
        const aiNode *pChildNode = pNode->mChildren[childIdx];
        GrabNodeNamesFromNode(mScene, pChildNode);
    }
}


void MeshmoonOpenAssetImporter::ComputeNodesDerivedTransform(const aiScene* mScene,  const aiNode *pNode, const aiMatrix4x4 &accTransform)
{
    PROFILE(MeshmoonOpenAssetImport_ComputeNodesDerivedTransform)
    if (mNodeDerivedTransformByName.find(pNode->mName.data) == mNodeDerivedTransformByName.end())
        mNodeDerivedTransformByName[pNode->mName.data] = accTransform;

    for (uint childIdx=0; childIdx<pNode->mNumChildren; ++childIdx)
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        ComputeNodesDerivedTransform(mScene, pChildNode, accTransform * pChildNode->mTransformation);
    }
}

void MeshmoonOpenAssetImporter::CreateBonesFromNode(const aiScene* mScene,  const aiNode *pNode)
{
    PROFILE(MeshmoonOpenAssetImport_CreateBonesFromNode)
    if (!mSkeleton.get())
    {
        LogError(LC + "CreateBonesFromNode called when no skeleton has been loaded yet.");
        return;
    }

    if (IsNodeNeeded(pNode->mName.data))
    {
        aiQuaternion rot; aiVector3D pos; aiVector3D scale;       
        aiMatrix4x4 aiM = pNode->mTransformation;
        aiM.Decompose(scale, rot, pos);
        
        Ogre::Vector3 ogreScale(scale.x, scale.y, scale.z);
        Ogre::Vector3 ogrePos(pos.x, pos.y, pos.z); //(pos.x, pos.z, -pos.y);
        Ogre::Quaternion ogreRot(rot.w, rot.x, rot.y, rot.z);

        try
        {
            Ogre::Bone* bone = mSkeleton->createBone(Ogre::String(pNode->mName.data), msBoneCount);
            if (!aiM.IsIdentity())
            {
                //LogDebug("Created bone: " + bone->getName() + " with position " + Ogre::StringConverter::toString(ogrePos));
                bone->setPosition(ogrePos);
                bone->setOrientation(ogreRot);
                bone->setScale(ogreScale);
            }
        }
        catch(Ogre::Exception &ex)
        {
            LogWarning("Failed to create bone \"" + Ogre::String(pNode->mName.data) + "\" exeption: " + ex.getDescription());
            return;
        }
        msBoneCount++;
    }
    
    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; ++childIdx)
    {
        const aiNode *pChildNode = pNode->mChildren[childIdx];
        CreateBonesFromNode(mScene, pChildNode);
    }
}

void MeshmoonOpenAssetImporter::CreateBoneHiearchy(const aiScene* mScene,  const aiNode *pNode) const
{
    PROFILE(MeshmoonOpenAssetImport_CreateBoneHiearchy)
    if (!mSkeleton.get())
    {
        LogError(LC + "CreateBoneHiearchy called when no skeleton has been loaded yet.");
        return;
    }

    if (IsNodeNeeded(pNode->mName.data))
    {
        Ogre::Bone* parent = 0;
        Ogre::Bone* child = 0;
        if (pNode->mParent)
            if(mSkeleton->hasBone(pNode->mParent->mName.data))
                parent = mSkeleton->getBone(pNode->mParent->mName.data);
        if(mSkeleton->hasBone(pNode->mName.data))
            child = mSkeleton->getBone(pNode->mName.data);
        if(parent && child)
            parent->addChild(child);
    }
    
    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; childIdx++)
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        CreateBoneHiearchy(mScene, pChildNode);
    }
}

void MeshmoonOpenAssetImporter::FlagNodeAsNeeded(const char* name)
{
    PROFILE(MeshmoonOpenAssetImport_FlagNodeAsNeeded)
    BoneMapType::iterator iter = boneMap.find(Ogre::String(name));
    if(iter != boneMap.end())
        iter->second.isNeeded = true;
    else
        LogWarning("Flagging as needed failed to find bone: " + std::string(name));
}

bool MeshmoonOpenAssetImporter::IsNodeNeeded(const char* name) const
{
    PROFILE(MeshmoonOpenAssetImport_IsNodeNeeded)
    BoneMapType::const_iterator iter = boneMap.find(Ogre::String(name));
    if(iter != boneMap.end())
        return iter->second.isNeeded;
    return false;
}

void MeshmoonOpenAssetImporter::GrabBoneNamesFromNode(const aiScene* mScene,  const aiNode *pNode)
{
    PROFILE(MeshmoonOpenAssetImport_GrabBoneNamesFromNode)
    if (pNode->mNumMeshes > 0)
    {
        for(uint idx=0; idx<pNode->mNumMeshes; ++idx)
        {
            aiMesh *pAIMesh = mScene->mMeshes[pNode->mMeshes[idx]];

            if (pAIMesh->HasBones())
            {
                for(Ogre::uint32 i=0; i < pAIMesh->mNumBones; ++i)
                {
                    aiBone *pAIBone = pAIMesh->mBones[i];
                    if (NULL != pAIBone)
                    {
                        mBonesByName[pAIBone->mName.data] = pAIBone;

                        // flag this node and all parents of this node as needed, until we reach the node holding the mesh, or the parent.
                        aiNode* node = mScene->mRootNode->FindNode(pAIBone->mName.data);
                        while(node)
                        {
                            if(node->mName.data == pNode->mName.data)
                            {
                                FlagNodeAsNeeded(node->mName.data);
                                break;
                            }
                            if(node->mName.data == pNode->mParent->mName.data)
                            {
                                FlagNodeAsNeeded(node->mName.data);
                                break;
                            }

                            // Not a root node, flag this as needed and continue to the parent
                            FlagNodeAsNeeded(node->mName.data);
                            node = node->mParent;
                        }

                        // Flag all children of this node as needed
                        node = mScene->mRootNode->FindNode(pAIBone->mName.data);
                        MarkAllChildNodesAsNeeded(node);
                    }
                }
            }
        }
    }

    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; childIdx++)
    {
        const aiNode *pChildNode = pNode->mChildren[childIdx];
        GrabBoneNamesFromNode(mScene, pChildNode);
    }
}

Ogre::MaterialPtr MeshmoonOpenAssetImporter::CreateVertexColorMaterial(const QString &materialRef, const aiMaterial *mat)
{
    PROFILE(MeshmoonOpenAssetImport_CreateVertexColorMaterial)

    const Ogre::String matName = materialRef.toStdString();
    const Ogre::String sanitatedMatName = AssetAPI::SanitateAssetRef(matName);
    
    Ogre::MaterialPtr ogreMaterial = Ogre::MaterialManager::getSingletonPtr()->create(
        sanitatedMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, false);

    if (ogreMaterial->getNumTechniques() == 0)
        ogreMaterial->createTechnique();
    if (ogreMaterial->getTechnique(0) && ogreMaterial->getTechnique(0)->getNumPasses() == 0)
        ogreMaterial->getTechnique(0)->createPass();

    Ogre::Pass *ogrePass = (ogreMaterial->getTechnique(0) ? ogreMaterial->getTechnique(0)->getPass(0) : 0);
    if (!ogrePass)
    {
        LogError(LC + "Failed to create technique 0 and pass 0 to Ogre::Materialin CreateVertexColorMaterial()");
        return Ogre::MaterialPtr();
    }
    
    LogWarning(LC + QString("Created vertex color material %1. Currently this will not render correctly in Meshmoon Rocket").arg(matName.c_str()));

    ogreMaterial->setCullingMode(Ogre::CULL_NONE);
    ogrePass->setVertexColourTracking(Ogre::TVC_AMBIENT|Ogre::TVC_DIFFUSE|Ogre::TVC_SPECULAR);
    return ogreMaterial;
}

inline Ogre::ColourValue AssimpToOgreColor(const aiColor4D &assimpColor)
{
    return Ogre::ColourValue(assimpColor.r, assimpColor.g, assimpColor.b, assimpColor.a);
}

void DumpColor(const QString &name, const Ogre::ColourValue &c)
{
    if (IsLogChannelEnabled(LogChannelDebug))
    {
        LogDebug(QString("[MeshmoonOpenAssetImport]:   - %1  %2 %3 %4 %5")
            .arg(name, -9)
            .arg(c.r, 0, 'f', 4)
            .arg(c.g, 0, 'f', 4)
            .arg(c.b, 0, 'f', 4)
            .arg(c.a, 0, 'f', 4));
    }
}

QString AssimpTextureTypeTextureUnitName(const aiTextureType &t)
{
    switch(t)
    {
        case aiTextureType_DIFFUSE:     return "diffuseMap";
        case aiTextureType_SPECULAR:    return "specularMap";
        case aiTextureType_NORMALS:     return "normalMap";
        case aiTextureType_HEIGHT:      return "normalMapFromAssimpHeight";
        case aiTextureType_LIGHTMAP:    return "lightMap";
    }
    return "";
}

QString AssimpTextureTypeToString(const aiTextureType &t)
{
    switch(t)
    {
        case aiTextureType_UNKNOWN:         return "aiTextureType_UNKNOWN";
        case aiTextureType_NONE:            return "aiTextureType_NONE";

        case aiTextureType_DIFFUSE:         return "aiTextureType_DIFFUSE";
        case aiTextureType_SPECULAR:        return "aiTextureType_SPECULAR";
        case aiTextureType_NORMALS:         return "aiTextureType_NORMALS";
        case aiTextureType_LIGHTMAP:        return "aiTextureType_LIGHTMAP";
        case aiTextureType_AMBIENT:         return "aiTextureType_AMBIENT";
        case aiTextureType_EMISSIVE:        return "aiTextureType_EMISSIVE";
        case aiTextureType_HEIGHT:          return "aiTextureType_HEIGHT";
        case aiTextureType_SHININESS:       return "aiTextureType_SHININESS";
        case aiTextureType_OPACITY:         return "aiTextureType_OPACITY";
        case aiTextureType_REFLECTION:      return "aiTextureType_REFLECTION";
        case aiTextureType_DISPLACEMENT:    return "aiTextureType_DISPLACEMENT";
    }
    return QString("Unknown_aiTextureType_%1").arg(static_cast<int>(t));
}

QString AssimpTextureOpToString(const aiTextureOp &op)
{
    switch(op)
    {
        case aiTextureOp_Multiply:  return "aiTextureOp_Multiply";
	    case aiTextureOp_Add:       return "aiTextureOp_Add";
	    case aiTextureOp_Subtract:  return "aiTextureOp_Subtract";
	    case aiTextureOp_Divide:    return "aiTextureOp_Divide";
	    case aiTextureOp_SmoothAdd: return "aiTextureOp_SmoothAdd";
	    case aiTextureOp_SignedAdd: return "aiTextureOp_SignedAdd";
    }
    return QString("Unknown_aiTextureOp_%1").arg(static_cast<int>(op));
}

QString AssimpTextureMapModeToString(const aiTextureMapMode &mode)
{
    switch(mode)
    {
        case aiTextureMapMode_Wrap:     return "aiTextureMapMode_Wrap";
	    case aiTextureMapMode_Clamp:    return "aiTextureMapMode_Clamp";
	    case aiTextureMapMode_Decal:    return "aiTextureMapMode_Decal";
	    case aiTextureMapMode_Mirror:   return "aiTextureMapMode_Mirror";
    }
    return QString("Unknown_aiTextureMapMode_%1").arg(static_cast<int>(mode));
}

Ogre::TextureUnitState::TextureAddressingMode ToOgreTextureAddressingMode(const aiTextureMapMode &mode)
{
    switch(mode)
    {
        case aiTextureMapMode_Wrap:     return Ogre::TextureUnitState::TAM_WRAP;
	    case aiTextureMapMode_Clamp:    return Ogre::TextureUnitState::TAM_CLAMP;
	    case aiTextureMapMode_Decal:    return Ogre::TextureUnitState::TAM_BORDER;
	    case aiTextureMapMode_Mirror:   return Ogre::TextureUnitState::TAM_MIRROR;
    }
    return Ogre::TextureUnitState::TAM_WRAP;
}

void ClampOgreColor(Ogre::ColourValue &c, float min = 0.0f, float max = 1.0f)
{
    if (IsLogChannelEnabled(LogChannelDebug) && (c.r < min || c.g < min || c.b < min))
        LogDebug(QString("Clamping color to min %1").arg(min, 0, 'f', 2));

    c.r = Clamp<float>(c.r, min, max);
    c.g = Clamp<float>(c.g, min, max);
    c.b = Clamp<float>(c.b, min, max);
    c.a = Clamp<float>(c.a, min, max);
}

Ogre::MaterialPtr MeshmoonOpenAssetImporter::CreateMaterial(const QString &materialRef, const aiMaterial *mat, const QString &meshFileDiskSource, 
                                                            const QString &meshFileName, const QString &assetRef)
{
    PROFILE(MeshmoonOpenAssetImport_CreateMaterial)
    
    const Ogre::String matName = materialRef.toStdString();
    const Ogre::String sanitatedMatName = AssetAPI::SanitateAssetRef(matName);

    Ogre::MaterialPtr ogreMaterial = Ogre::MaterialManager::getSingletonPtr()->create(
        sanitatedMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, false);
    
    if (ogreMaterial->getNumTechniques() == 0)
        ogreMaterial->createTechnique();
    if (ogreMaterial->getTechnique(0) && ogreMaterial->getTechnique(0)->getNumPasses() == 0)
        ogreMaterial->getTechnique(0)->createPass();
        
    Ogre::Pass *ogrePass = (ogreMaterial->getTechnique(0) ? ogreMaterial->getTechnique(0)->getPass(0) : 0);
    if (!ogrePass)
    {
        LogError(LC + "Failed to create technique 0 and pass 0 to Ogre::Material in CreateMaterial()");
        return Ogre::MaterialPtr();
    }

    if (debugLoggingEnabled_)
        LogDebug(LC + QString("Created material %1").arg(materialRef));

    aiColor4D assimpColor;
    Ogre::ColourValue ogreColor;
    
    float tempFloat = 0.0f;
    int tempInt = 0;

    // Ambient
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &assimpColor) == AI_SUCCESS)
    {
        /** Certain assets seem to prefer black (0,0,0) ambient light. 
            In Ogre this will make a textured meshes pretty much black.
            Make a special case out of 0,0,0 -> 1,1,1 so it looks good in Ogre.
            If not zero, clamp to 0.5 anyways. */
        ogreColor = AssimpToOgreColor(assimpColor);
        if (EqualAbs(ogreColor.r, 0.0f) && EqualAbs(ogreColor.g, 0.0f) && EqualAbs(ogreColor.b, 0.0f))
            ogreColor = Ogre::ColourValue(1.0f, 1.0f, 1.0f, 1.0f);
        else
            ClampOgreColor(ogreColor, 0.5f);
        ogrePass->setAmbient(ogreColor);
        DumpColor("Ambient", ogreColor);
    }
    // Diffuse
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &assimpColor) == AI_SUCCESS)
    {
        ogreColor = AssimpToOgreColor(assimpColor);
        ogrePass->setDiffuse(ogreColor);
        DumpColor("Diffuse", ogreColor);
    }
    // Specular
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &assimpColor) == AI_SUCCESS)
    {
        ogreColor = AssimpToOgreColor(assimpColor);
        ogrePass->setSpecular(ogreColor);
        DumpColor("Specular", ogreColor);
    }
    // Emissive
    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &assimpColor) == AI_SUCCESS)
    {
        ogreColor = AssimpToOgreColor(assimpColor);
        ogrePass->setSelfIllumination(ogreColor);
        DumpColor("Emissive", ogreColor);
    }
    // Shininess
    if (aiGetMaterialFloat(mat, AI_MATKEY_SHININESS, &tempFloat) == AI_SUCCESS)
    {
        /** @todo AI_MATKEY_SHININESS -> specularHardness
                  AI_MATKEY_SHININESS_STRENGTH -> specularPower */
        ogrePass->setShininess(tempFloat);
        if (debugLoggingEnabled_)
            LogDebug(LC + QString("  - Shininess %1").arg(tempFloat));
    }
    /// @todo Evaluate if any exporter (that we expect and test) sets this property and how it looks.
    if (aiGetMaterialInteger(mat, AI_MATKEY_TWOSIDED, &tempInt) == AI_SUCCESS)
    {
        if (tempInt > 0)
        {
            ogreMaterial->setCullingMode(Ogre::CULL_NONE);
            if (debugLoggingEnabled_)
                LogDebug(LC + QString("  - Two sided rendering %1 with Ogre::CULL_NONE").arg(tempInt));
        }
    }

    // In debug mode log all available textures
    if (debugLoggingEnabled_)
    {
            QList<aiTextureType> allTextureTypes;
            allTextureTypes << aiTextureType_DIFFUSE << aiTextureType_NORMALS << aiTextureType_SPECULAR
                            << aiTextureType_LIGHTMAP << aiTextureType_AMBIENT << aiTextureType_EMISSIVE
                            << aiTextureType_HEIGHT << aiTextureType_SHININESS << aiTextureType_OPACITY
                            << aiTextureType_DISPLACEMENT << aiTextureType_REFLECTION << aiTextureType_UNKNOWN;

            LogDebug(LC + "  Available Assimp textures");
            foreach(const aiTextureType &textureType, allTextureTypes)
            {
                uint numTexturesType = mat->GetTextureCount(textureType);
                if (numTexturesType == 0)
                    continue;
                LogDebug(LC + QString("    %1 %2")
                    .arg(numTexturesType)
                    .arg(AssimpTextureTypeToString(textureType)));
            }
    }

    QList<aiTextureType> createdTextureTypes;
    QList<aiTextureType> queryTextureTypes;
    queryTextureTypes << aiTextureType_DIFFUSE
                      << (mat->GetTextureCount(aiTextureType_NORMALS) == 0 && mat->GetTextureCount(aiTextureType_HEIGHT) > 0
                             ? aiTextureType_HEIGHT : aiTextureType_NORMALS)
                      << aiTextureType_SPECULAR
                      << aiTextureType_LIGHTMAP;

    // Textures
    foreach(const aiTextureType &textureType, queryTextureTypes)
    {
        uint numTexturesType = mat->GetTextureCount(textureType);
        if (numTexturesType == 0)
            continue;
        else if (numTexturesType > 1)
            LogWarning(LC + QString("Assimp material has more than one texture of type %1").arg(AssimpTextureTypeToString(textureType)));
            
        QString textureUnitName = AssimpTextureTypeTextureUnitName(textureType);

        // Texture properties
        aiString assimpTextureRef;
        aiTextureMapping mapping = aiTextureMapping_UV;
        uint uvIndex = 0;
        float blend	= 1.0f;
        aiTextureOp colorOp = aiTextureOp_Multiply;         // Ogre::LBX_MODULATE
        aiTextureMapMode mapMode = aiTextureMapMode_Wrap;   // Ogre::TextureUnitState::TAM_WRAP

        if (mat->GetTexture(textureType, 0, &assimpTextureRef, &mapping, &uvIndex, &blend, &colorOp, &mapMode) != AI_SUCCESS)
        {
            LogError(LC + QString("Failed to query %1 texture from Assimp material").arg(textureUnitName));
            continue;
        }

        if (debugLoggingEnabled_)
            LogDebug(LC + QString("  Loading %1").arg(textureUnitName));
        
        // aiProcess_GenUVCoords should ensure we never get anything but UV mapping.
        // Warn but don't create an error
        if (mapping != aiTextureMapping_UV)
            LogWarning(LC + QString("  - Texture mapping is not UV based: aiTextureMapping %1").arg(static_cast<int>(mapping)));
                
        QString texturePath = QDir::fromNativeSeparators(QString::fromStdString(assimpTextureRef.data).trimmed());
        if (texturePath.startsWith("./"))
            texturePath = texturePath.mid(2);
        else if (texturePath.startsWith(".") || texturePath.startsWith("/"))
            texturePath = texturePath.mid(1);

        QFileInfo tfi(texturePath);
        QString textureRef = "";
        AssetAPI::AssetRefType assetRefType = AssetAPI::ParseAssetRef(assetRef);

        // Texture is a URL
        if (texturePath.startsWith("http:", Qt::CaseInsensitive) || texturePath.startsWith("https:", Qt::CaseInsensitive))
            textureRef = texturePath;
        // C:/path/to/my.dae and local://my.dae
        // Both are resolved using the disk source of the mesh. We don't want to generate a vague local://
        // ref for the texture, as we clearly know where the parent is, so we can resolve relative refs from it.
        else if (assetRefType == AssetAPI::AssetRefLocalPath || assetRefType == AssetAPI::AssetRefLocalUrl)
        {
            if (tfi.isRelative())
                textureRef = QFileInfo(meshFileDiskSource).absoluteDir().absoluteFilePath(tfi.filePath());
            else
                textureRef = tfi.absoluteFilePath();

            if (QFile::exists(textureRef))
                textureRef = QDir::toNativeSeparators(textureRef);
            else
            {
                textureRef = "";
                LogWarning(LC + "Failed to find texture from referenced disk path: " + tfi.absoluteFilePath() + " with parent: " + assetRef);
            }
        }
        // http://some.host.com/my.dae
        else if (assetRefType == AssetAPI::AssetRefExternalUrl || assetRefType == AssetAPI::AssetRefRelativePath)
        {
            if (!tfi.isAbsolute())
                textureRef = assetAPI_->ResolveAssetRef(assetRef, texturePath);
            else
                LogWarning(LC + "Cannot resolve texture ref from an absolute disk path: " + tfi.absoluteFilePath() + " with base: " + 
                    assetRef.split("/", QString::SkipEmptyParts).last());
        }

        // Fetch texture if it looks to be valid. Otherwise load the material without the texture.
        if (!textureRef.isEmpty())
        {
            if (debugLoggingEnabled_)
            {
                LogDebug(LC + QString("  - Original ref:"));
                LogDebug(LC + QString("    %1").arg(texturePath));
                LogDebug(LC + QString("  - Resolved ref:"));
                LogDebug(LC + QString("    %1").arg(textureRef));
                LogDebug(LC + QString("  - UV: %1").arg(uvIndex));
                LogDebug(LC + QString("  - Adressing: %1").arg(AssimpTextureMapModeToString(mapMode)));
                LogDebug(LC + QString("  - Op: %1 (not applied for shader)").arg(AssimpTextureOpToString(colorOp)));
            }
            
            Ogre::TextureUnitState *tu = ogrePass->createTextureUnitState();
            if (!tu)
            {
                LogError(LC + "Failed to create texture unit to pass");
                continue;
            }

            // @note Color op has no effect in non fixed pipeline so its not applied.
            tu->setName(textureUnitName.toStdString());
            tu->setTextureCoordSet(uvIndex);
            tu->setTextureAddressingMode(ToOgreTextureAddressingMode(mapMode));

            createdTextureTypes << textureType;

            // Returns true if this is a synchronous load (non disk asset)
            LoadTexture(textureRef, QString::fromStdString(ogreMaterial->getName()), QString::fromStdString(tu->getName()));
        }
    }
    
    // Set Meshmoon shader
    if (!createdTextureTypes.isEmpty())
    {
        QString shaderName = "meshmoon/";

        /** Build shader name now that the texture unit has been created.
            This also must follow the correct Meshmoon shader texture unit indexing:
            meshmoon/DiffuseNormalMapSpecularMapLightMap */
        if (createdTextureTypes.contains(aiTextureType_DIFFUSE))
            shaderName += "Diffuse";
        if (createdTextureTypes.contains(aiTextureType_NORMALS) || createdTextureTypes.contains(aiTextureType_HEIGHT))
            shaderName += "NormalMap";
        if (createdTextureTypes.contains(aiTextureType_SPECULAR))
            shaderName += "SpecularMap";
        if (createdTextureTypes.contains(aiTextureType_LIGHTMAP))
            shaderName += "LightMap";

        LogDebug(LC + "  Setting Meshmoon shader: " + shaderName);
        ogrePass->setVertexProgram(shaderName.toStdString() + "VP");
        ogrePass->setFragmentProgram(shaderName.toStdString() + "FP");
    }

    // Load material now if there are no pending textures
    if (NumPendingMaterialTextures(QString::fromStdString(ogreMaterial->getName())) == 0 && !ogreMaterial->isLoaded())
    {
        if (debugLoggingEnabled_)
            LogDebug(LC + "  - No pending texture dependencies, loading material now");
        ogreMaterial->load();
    }

    return ogreMaterial;
}

bool MeshmoonOpenAssetImporter::CreateVertexData(const Ogre::String& name, const aiNode* pNode, const aiMesh *mesh, 
                                                 Ogre::SubMesh* submesh, Ogre::AxisAlignedBox& mAAB)
{
    PROFILE(MeshmoonOpenAssetImport_CreateVertexData)
    // If animated all submeshes must have bone weights.
    if (mSkeleton.get() && !mBonesByName.empty() && !mesh->HasBones())
    {
        LogError(LC + "Skipping Mesh " + QString(mesh->mName.data) + " with no bone weights");
        return false;
    }
    /*if (mNodeDerivedTransformByName.find(pNode->mName.data) == mNodeDerivedTransformByName.end())
    {
        LogError(LC + "Skipping Mesh " + QString(mesh->mName.data) + " failed to find derived node transform");
        return false;
    }*/
    
    float wallClockStart = (debugLoggingEnabled_ ? framework_->Frame()->WallClockTime() : 0.0f);
    
    bool hasPos = mesh->HasPositions();
    bool hasNormals = mesh->HasNormals();
    bool hasTangentsAndBitangents = mesh->HasTangentsAndBitangents();
    uint numTextureCoordinates = mesh->GetNumUVChannels();
    uint numVertexColorChannels = mesh->GetNumColorChannels();

    if (!hasPos)
    {
        LogError(LC + "Skipping Mesh " + QString(mesh->mName.data) + " with no vertices");
        return false;
    }
    
    // We must create the vertex data, indicating how many vertices there will be
    submesh->useSharedVertices = false;
#include "DisableMemoryLeakCheck.h"
    submesh->vertexData = new Ogre::VertexData();
#include "EnableMemoryLeakCheck.h"
    submesh->vertexData->vertexStart = 0;
    submesh->vertexData->vertexCount = mesh->mNumVertices;
    Ogre::VertexData *data = submesh->vertexData;
    
    if (debugLoggingEnabled_)
        LogDebug(LC + QString("  - Creating vertex buffer with %1 vertexes").arg(data->vertexCount));

    /** Build vertex declaration. 
        @todo We should refactor this function as per Ogre documentation:
        * VertexElements should be added in the following order, and the order of the
          elements within a shared buffer should be as follows:
          position, blending weights, normals, diffuse colors, specular colors,
          texture coordinates (in order, with no gaps)
        * You must not have unused gaps in your buffers which are not referenced
          by any VertexElement
        * You must not cause the buffer & offset settings of 2 VertexElements to overlap */
    Ogre::VertexDeclaration* decl = data->vertexDeclaration;
    
    static const ushort SOURCE_POS_NORMAL = 0;
    static const ushort SOURCE_TEX_COORD_TANGENT_BINORMAL = 1;
    static const ushort SOURCE_VERTEX_COLORS = 2;

    // Position and normals
    size_t offset = 0;
    offset += decl->addElement(SOURCE_POS_NORMAL, offset, Ogre::VET_FLOAT3, Ogre::VES_POSITION).getSize();
    if (hasNormals)
        decl->addElement(SOURCE_POS_NORMAL, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL);

    // Texture coordinates
    offset = 0;
    for (uint i=0, len=numTextureCoordinates; i<len; ++i)
    {
        if (mesh->HasTextureCoords(i))
        {
            if (mesh->mNumUVComponents[i] == 3)
                offset += decl->addElement(SOURCE_TEX_COORD_TANGENT_BINORMAL, offset, Ogre::VET_FLOAT3, Ogre::VES_TEXTURE_COORDINATES, static_cast<ushort>(i)).getSize();
            else
                offset += decl->addElement(SOURCE_TEX_COORD_TANGENT_BINORMAL, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, static_cast<ushort>(i)).getSize();
        }
    }

    // Tangent and binormal
    if (hasTangentsAndBitangents)
    {
        offset += decl->addElement(SOURCE_TEX_COORD_TANGENT_BINORMAL, offset, Ogre::VET_FLOAT3, Ogre::VES_TANGENT).getSize();
        decl->addElement(1, offset, Ogre::VET_FLOAT3, Ogre::VES_BINORMAL);
    }

    // Vertex colors
    offset = 0;
    for (uint i=0, len=numVertexColorChannels; i<len; ++i)
    {
        if (mesh->HasVertexColors(i))
            offset +=  decl->addElement(SOURCE_VERTEX_COLORS, offset, Ogre::VET_COLOUR, Ogre::VES_DIFFUSE, static_cast<ushort>(i)).getSize();
    }

    // Write vertex positions and normals
    Ogre::HardwareVertexBufferSharedPtr vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
            decl->getVertexSize(SOURCE_POS_NORMAL), data->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);
    float *vbData = static_cast<float*>(vbuf->lock(Ogre::HardwareBuffer::HBL_WRITE_ONLY));

    offset = 0;
    if (data->vertexCount > 0)
    {
        //aiMatrix4x4 nodeMatrix = mNodeDerivedTransformByName.find(pNode->mName.data)->second;

        aiVector3D vecAssimp;
        Ogre::Vector3 vecOgre;
        for (size_t vi=0, viLen=data->vertexCount; vi<viLen; ++vi)
        {
            vecAssimp.x = mesh->mVertices[vi].x;
            vecAssimp.y = mesh->mVertices[vi].y;
            vecAssimp.z = mesh->mVertices[vi].z;
            //vecAssimp *= nodeMatrix;

            vbData[offset++] = vecAssimp.x;
            vbData[offset++] = vecAssimp.y;
            vbData[offset++] = vecAssimp.z;

            // Update parent mesh bounding box
            vecOgre.x = vecAssimp.x; vecOgre.y = vecAssimp.y; vecOgre.z = vecAssimp.z;
            mAAB.merge(vecOgre);

            if (hasNormals)
            {
                vecAssimp.x = mesh->mNormals[vi].x;
                vecAssimp.y = mesh->mNormals[vi].y;
                vecAssimp.z = mesh->mNormals[vi].z;
                //vecAssimp *= nodeMatrix;

                vbData[offset++] = vecAssimp.x;
                vbData[offset++] = vecAssimp.y;
                vbData[offset++] = vecAssimp.z;
            }
        }
    }

    vbuf->unlock();
    data->vertexBufferBinding->setBinding(SOURCE_POS_NORMAL, vbuf);

    // Write texture coordinates, tangets and bitangents
    if (numTextureCoordinates > 0)
    {
        if (debugLoggingEnabled_)
        {
            LogDebug(LC + QString("  - Creating %1 texture coords").arg(numTextureCoordinates));
            if (hasTangentsAndBitangents)
                LogDebug(LC + "    with tangents and bitangents");
        }

        vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
                decl->getVertexSize(SOURCE_TEX_COORD_TANGENT_BINORMAL), data->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);
        vbData = static_cast<float*>(vbuf->lock(Ogre::HardwareBuffer::HBL_WRITE_ONLY));

        offset = 0;
        for (size_t vi=0, viLen=data->vertexCount; vi<viLen; ++vi)
        {
            for (uint tci=0, tciLen=numTextureCoordinates; tci<tciLen; ++tci)
            {
                const aiVector3D &coord = mesh->mTextureCoords[tci][vi];
                vbData[offset++] = coord.x;
                vbData[offset++] = coord.y;
                if (mesh->mNumUVComponents[tci] >= 3)
                    vbData[offset++] = coord.z;
            }
            if (hasTangentsAndBitangents)
            {
                const aiVector3D &tangent = mesh->mTangents[vi];
                vbData[offset++] = tangent.x;
                vbData[offset++] = tangent.y;
                vbData[offset++] = tangent.z;

                const aiVector3D &bitangent = mesh->mBitangents[vi];
                vbData[offset++] = bitangent.x;
                vbData[offset++] = bitangent.y;
                vbData[offset++] = bitangent.z;
            }
        }

        vbuf->unlock();
        data->vertexBufferBinding->setBinding(SOURCE_TEX_COORD_TANGENT_BINORMAL, vbuf);
    }

    // Write vertex colors
    /// @todo Disable vertex colors? We don't event have a shader (rex or meshmoon) that would render these!
    if (numVertexColorChannels > 0)
    {
        Ogre::RenderSystem *renderSystem = Ogre::Root::getSingleton().getRenderSystem();
        if (!renderSystem)
        {
            LogError(LC + "Failed to get current render system for vertex color conversions!");
            return false;
        }

        if (debugLoggingEnabled_)
            LogDebug(LC + "  - Creating vertex colors");

        Ogre::HardwareVertexBufferSharedPtr vbufColor = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
                decl->getVertexSize(SOURCE_VERTEX_COLORS), data->vertexCount, Ogre::HardwareBuffer::HBU_STATIC);
        /// @todo Should do float and track offset from it? Not sure if this works correctly
        Ogre::RGBA* pVertexColor = static_cast<Ogre::RGBA*>(vbufColor->lock(Ogre::HardwareBuffer::HBL_WRITE_ONLY));
        
        for (size_t vi=0, viLen=data->vertexCount; vi<viLen; ++vi)
        {
            for (uint vci=0, vciLen=numVertexColorChannels; vci<vciLen; ++vci)
            {
                const aiColor4D &assimpColor = mesh->mColors[vci][vi];
                const Ogre::ColourValue ogreColor(assimpColor.r, assimpColor.g, assimpColor.b, assimpColor.a);
                renderSystem->convertColourValue(ogreColor, &pVertexColor[vi]);
            }
        }
        vbufColor->unlock();
        data->vertexBufferBinding->setBinding(SOURCE_VERTEX_COLORS, vbufColor);
        
        /** @todo Probably not needed as we are doing a exact construction without caps already?
            If done, should be done as last in this function, originally code was here! */
        //data->closeGapsInBindings();
    }

    // Write index buffer
    size_t numIndices = mesh->mNumFaces * 3;
    // @todo Verify if I understood this correctly. If max index is <= 65535 we can save space and write with uint16 to the GPU buffer.
    Ogre::HardwareIndexBuffer::IndexType indexType = (numIndices <= 65535 ? Ogre::HardwareIndexBuffer::IT_16BIT : Ogre::HardwareIndexBuffer::IT_32BIT);
    
    if (debugLoggingEnabled_)
        LogDebug(LC + QString("  - Creating index buffer with %1 indices (%2 index type)")
            .arg(numIndices).arg(indexType == Ogre::HardwareIndexBuffer::IT_16BIT ? "16bit" : "32bit"));

    Ogre::HardwareIndexBufferSharedPtr ibuf = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
            indexType, numIndices, Ogre::HardwareBuffer::HBU_STATIC);

    offset = 0;
    if (indexType == Ogre::HardwareIndexBuffer::IT_16BIT)
    {
        Ogre::uint16 *idxData = static_cast<Ogre::uint16*>(ibuf->lock(Ogre::HardwareBuffer::HBL_WRITE_ONLY));
        for (uint fi=0, fiLen=mesh->mNumFaces; fi<fiLen; ++fi)
        {
            const aiFace &face = mesh->mFaces[fi];
            idxData[offset++] = static_cast<Ogre::uint16>(face.mIndices[0]);
            idxData[offset++] = static_cast<Ogre::uint16>(face.mIndices[1]);
            idxData[offset++] = static_cast<Ogre::uint16>(face.mIndices[2]);
        }
    }
    else
    {
        Ogre::uint32 *idxData = static_cast<Ogre::uint32*>(ibuf->lock(Ogre::HardwareBuffer::HBL_WRITE_ONLY));
        for (uint fi=0, fiLen=mesh->mNumFaces; fi<fiLen; ++fi)
        {
            const aiFace &face = mesh->mFaces[fi];
            idxData[offset++] = static_cast<Ogre::uint32>(face.mIndices[0]);
            idxData[offset++] = static_cast<Ogre::uint32>(face.mIndices[1]);
            idxData[offset++] = static_cast<Ogre::uint32>(face.mIndices[2]);
        }
    }
    ibuf->unlock();

    // Set index data to mesh
    submesh->indexData->indexBuffer = ibuf;
    submesh->indexData->indexCount = numIndices;
    submesh->indexData->indexStart = 0;

    // Bone weights
    if (mesh->HasBones() && mSkeleton.get())
    {
        if (debugLoggingEnabled_)
            LogDebug(LC + "  - Creating bone weights");

        for(Ogre::uint32 i=0; i < mesh->mNumBones; i++ )
        {
            aiBone *pAIBone = mesh->mBones[i];
            if (pAIBone != 0)
            {
                Ogre::String bname = pAIBone->mName.data;
                for (Ogre::uint32 weightIdx = 0; weightIdx < pAIBone->mNumWeights; weightIdx++)
                {
                    aiVertexWeight aiWeight = pAIBone->mWeights[weightIdx];

                    Ogre::VertexBoneAssignment vba;
                    vba.vertexIndex = aiWeight.mVertexId;
                    vba.boneIndex = mSkeleton->getBone(bname)->getHandle();
                    vba.weight = aiWeight.mWeight;

                    submesh->addBoneAssignment(vba);
                }
            }
        }
        submesh->_compileBoneAssignments();
    }
    
    if (debugLoggingEnabled_)
        LogDebug(LC + QString("Done in %1 seconds").arg(framework_->Frame()->WallClockTime() - wallClockStart, 0, 'f', 2));

    return true;
}

void MeshmoonOpenAssetImporter::CreateSubmeshesFromNode(const aiScene* mScene,  const aiNode *pNode, const QString &meshFileDiskSource, 
                                                        const QString &meshFileName, Ogre::MeshPtr destinationsMesh, const QString &assetRef)
{        
    PROFILE(MeshmoonOpenAssetImport_CreateSubmeshesFromNode)
    if (pNode->mNumMeshes > 0)
    {
        Ogre::AxisAlignedBox mAAB = destinationsMesh->getBounds();

        for (uint idx=0; idx<pNode->mNumMeshes; ++idx)
        {
            aiMesh *pAIMesh = mScene->mMeshes[pNode->mMeshes[idx]];
            const aiMaterial *pAIMaterial = mScene->mMaterials[pAIMesh->mMaterialIndex];
            
            // Read or generate material name
            QString materialRef;
            aiString assimpMaterialName;

            if (aiGetMaterialString(pAIMaterial, AI_MATKEY_NAME, &assimpMaterialName) == aiReturn_SUCCESS)
                materialRef = meshFileName + "." + AssetAPI::SanitateAssetRef(QString::fromStdString(assimpMaterialName.data).replace(" ", "_")) + ".material";
            else
                materialRef = QString(meshFileName + "." + QString::number(pAIMesh->mMaterialIndex) + ".material");

            if (materialRef.startsWith("local://", Qt::CaseInsensitive) || materialRef.startsWith("https://", Qt::CaseInsensitive))
                materialRef = "generated://" + materialRef.mid(8);
            else if (materialRef.startsWith("http://", Qt::CaseInsensitive))
                materialRef = "generated://" + materialRef.mid(7);

            OgreMaterialAsset *materialAsset = framework_->Asset()->FindAsset<OgreMaterialAsset>(materialRef).get();

            // This material is not loaded yet. Create new material to AssetAPI 
            // and generate the Ogre::MaterialPtr to it manually.
            if (!materialAsset)
            {
                AssetPtr assetPtr = assetAPI_->CreateNewAsset("OgreMaterial", materialRef);
                if (assetPtr.get())
                {
                    materialAsset = dynamic_cast<OgreMaterialAsset*>(assetPtr.get());
                    if (materialAsset)
                    {
                        /// @todo Don't create vertex color materials, we don't support rendering these in our shaders (maybe Ogre fixed pipeline does)
                        if (pAIMesh->GetNumColorChannels() > 0)
                            materialAsset->ogreMaterial = CreateVertexColorMaterial(materialRef, pAIMaterial);
                        else
                            materialAsset->ogreMaterial = CreateMaterial(materialRef, pAIMaterial, meshFileDiskSource, meshFileName, assetRef);
                    }
                }
            }

            // Store the Tundra asset reference. We are going to pass these in the ImportDone signal.
            // We cannot make the material assignments here, we don't have enough information from the EC_Mesh etc. for it.
            if (materialAsset && materialAsset->IsLoaded())
                importInfo_.materials[mSubMeshCount] = materialAsset->Name();
            mSubMeshCount++;

            Ogre::String submeshName = Ogre::String(pNode->mName.data) + "_" + Ogre::StringConverter::toString(idx);
            Ogre::SubMesh* submesh = destinationsMesh->createSubMesh(submeshName);
            
            if (debugLoggingEnabled_)
                LogDebug(LC + QString("Creating Ogre SubMesh %1 %2").arg(mSubMeshCount-1).arg(QString::fromStdString(submeshName)));

            if (!CreateVertexData(Ogre::String(pNode->mName.data), pNode, pAIMesh, submesh, mAAB))
            {
                destinationsMesh->destroySubMesh(submeshName);
                continue;
            }

            // Set a default empty material from Tundra media to initialise.
            // Later logic will apply the above created material.
            submesh->setMaterialName("LitTextured");
        }

        // Update the parent bounding box. Updated in CreateVertexData.
        destinationsMesh->_setBounds(mAAB);
        destinationsMesh->_setBoundingSphereRadius((mAAB.getMaximum()- mAAB.getMinimum()).length()/2.0);
    }

    // Traverse all child nodes of the current node instance
    for (uint childIdx=0; childIdx<pNode->mNumChildren; childIdx++)
    {
        const aiNode *pChildNode = pNode->mChildren[ childIdx ];
        CreateSubmeshesFromNode(mScene, pChildNode, meshFileDiskSource, meshFileName, destinationsMesh, assetRef);
    }
}

// MeshmoonOpenAssetImportIOSystem

MeshmoonOpenAssetImportIOSystem::MeshmoonOpenAssetImportIOSystem(Framework *framework, const QString &contextRef, const QString &contextDiskSource) :
    Assimp::IOSystem(),
    framework_(framework),
    contextRef_(contextRef),
    contextDiskSource_(contextDiskSource)
{
}

MeshmoonOpenAssetImportIOSystem::~MeshmoonOpenAssetImportIOSystem()
{
}

QString MeshmoonOpenAssetImportIOSystem::PathForReference(const QString &ref) const
{
    if (ref.startsWith("local://", Qt::CaseInsensitive) && ref == contextRef_)
        return contextDiskSource_;

    QFileInfo fi(ref);
    if (fi.isAbsolute() && !ref.startsWith("http://") && !ref.startsWith("https://"))
        return fi.absoluteFilePath();
    else
    {
        QString resolvedFromContext = framework_->Asset()->ResolveAssetRef(contextRef_, ref);
        if (resolvedFromContext.startsWith("http://") || resolvedFromContext.startsWith("https://"))
            resolvedFromContext = framework_->Asset()->Cache()->GetDiskSourceByRef(resolvedFromContext);
        else if (resolvedFromContext.startsWith("local://", Qt::CaseInsensitive))
        {
            QString localBase = contextRef_.mid(0, contextRef_.lastIndexOf("/")+1);
            QString remainder = resolvedFromContext.startsWith(localBase, Qt::CaseInsensitive) ? resolvedFromContext.mid(localBase.length()) : resolvedFromContext.mid(8);
            return QFileInfo(contextDiskSource_).absoluteDir().absoluteFilePath(remainder);
        }
        return QDir::fromNativeSeparators(resolvedFromContext);
    }
}

bool MeshmoonOpenAssetImportIOSystem::Exists(const char* pFile) const
{
    QString path = PathForReference(pFile);
    return QFile::exists(path);
} 

char MeshmoonOpenAssetImportIOSystem::getOsSeparator() const
{
    return QDir::separator().toAscii();
}

Assimp::IOStream* MeshmoonOpenAssetImportIOSystem::Open(const char* pFile, const char* pMode)
{
    // Variations: "wb", "w", "wt", "rb", "r", "rt".
    /// @todo Do we really want text mode here with 't'. It will transform \r\n to \n. Not sure if assimp expects that on windows!
    QString modeStr(pMode);
    QIODevice::OpenMode mode = modeStr.contains("r") ? QIODevice::ReadOnly : QIODevice::WriteOnly;
    if (modeStr.contains("t"))
        mode |= QIODevice::Text;

#include "DisableMemoryLeakCheck.h"
    MeshmoonOpenAssetImportIOStream *file = new MeshmoonOpenAssetImportIOStream(PathForReference(pFile), mode);
#include "EnableMemoryLeakCheck.h"
    if (!file->IsOpen())
        SAFE_DELETE(file);
    return file;
}

void MeshmoonOpenAssetImportIOSystem::Close(Assimp::IOStream* pFile)
{
    SAFE_DELETE(pFile);
}

bool MeshmoonOpenAssetImportIOSystem::ComparePaths(const char* one, const char* second) const
{
    return (PathForReference(one).compare(PathForReference(second), Qt::CaseInsensitive) == 0);
}

// MeshmoonOpenAssetImportIOStream

MeshmoonOpenAssetImportIOStream::MeshmoonOpenAssetImportIOStream(const QString path, QIODevice::OpenMode mode)
{
    file_ = new QFile(path);
    if (!file_->open(mode))
        SAFE_DELETE(file_);
}

MeshmoonOpenAssetImportIOStream::~MeshmoonOpenAssetImportIOStream()
{
    if (file_)
        file_->close();
    SAFE_DELETE(file_);
}

bool MeshmoonOpenAssetImportIOStream::IsOpen() const
{
    return (file_ && file_->isOpen());
}

size_t MeshmoonOpenAssetImportIOStream::Read(void* pvBuffer, size_t pSize, size_t pCount)
{
    if (!IsOpen())
        return 0;
    qint64 len = pSize * pCount;
    return static_cast<size_t>(file_->read((char*)pvBuffer, len));
}

size_t MeshmoonOpenAssetImportIOStream::Write(const void* pvBuffer, size_t pSize, size_t pCount)
{
    if (!IsOpen())
        return 0;
    qint64 len = pSize * pCount;
    return static_cast<size_t>(file_->write((const char*)pvBuffer, len));
}

aiReturn MeshmoonOpenAssetImportIOStream::Seek(size_t pOffset, aiOrigin pOrigin)
{
    if (!IsOpen())
        return aiReturn_FAILURE;
    qDebug() << "MeshmoonOpenAssetImportIOStream::Seek: Not implemented" << pOffset << pOrigin;
    return aiReturn_FAILURE;
}

size_t MeshmoonOpenAssetImportIOStream::Tell() const
{
    if (!IsOpen())
        return 0;
    return static_cast<size_t>(file_->pos());
}

size_t MeshmoonOpenAssetImportIOStream::FileSize() const
{
    if (!IsOpen())
        return 0;
    return static_cast<size_t>(file_->size());
}

void MeshmoonOpenAssetImportIOStream::Flush()
{
    if (IsOpen())
        file_->flush();
}
