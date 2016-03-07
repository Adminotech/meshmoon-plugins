/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @note This code is based on OgreAssimp. For the latest info, see http://code.google.com/p/ogreassimp/
    Copyright (c) 2011 Jacob 'jacmoe' Moen. Licensed under the MIT license

    @file   
    @brief   */

#pragma once

#include "MeshmoonAssimpPluginApi.h"
#include "MeshmoonAssimpPluginFwd.h"

#include "OgreModuleFwd.h"

#include "CoreTypes.h"
#include "AssetFwd.h"

#include <assimp/vector3.h>
#include <assimp/matrix4x4.h>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

#include <OgreSkeleton.h>

#include <map>
#include <QString>
#include <QObject>
#include <QFile>

struct aiNode;
struct aiBone;
struct aiMesh;
struct aiMaterial;
struct aiScene;
struct aiAnimation;

/// @cond PRIVATE

struct BoneNode
{
    aiNode* node;
    aiNode* parent;
    bool isNeeded;

    BoneNode() : node(0), parent(0), isNeeded(false) {}
};

struct AnimationAssetData
{
    u8 *data_;
    size_t numBytes;

    QString assetRef;
    QString diskSource;
    QString name;
};

typedef std::map<Ogre::String, const aiBone*> BoneMap;
typedef std::map<Ogre::String, BoneNode> BoneMapType;
typedef std::map<Ogre::String, const aiNode*> BoneNodeMap;
typedef std::map<Ogre::String, aiMatrix4x4> NodeTransformMap;

class MESHMOON_ASSIMP_API MeshmoonOpenAssetImporter : public QObject
{
    Q_OBJECT

public:
    explicit MeshmoonOpenAssetImporter(AssetAPI *assetApi);
    ~MeshmoonOpenAssetImporter();
    
    /// Import mesh and skeleton assets.
    /** Emits ImportDone when done. */
    bool Import(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, AssetPtr meshAsset, AssetPtr skeletonAsset = AssetPtr());

    /// Import mesh and use external animations.
    /** Emits ImportDone when done. */
    bool ImportWithExternalAnimations(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, AssetPtr meshAsset, AssetPtr skeletonAsset, QString animationName, QList<AnimationAssetData> animationAssets);

    /// Import animation to currently imported Ogre skeleton.
    /** Does not emit ImportDone signal, returns boolean if animations were imported. 
        @note You must first call Import or ImportWithExternalAnimations that loads a valid skeleton to the importer state. */ 
    bool ImportAnimation(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource, QString animationName);

    /// Returns the destination mesh asset if known.
    AssetPtr DestinationMeshAsset() const { return destinationMeshAsset; }

    /// Returns the destination skeleton asset if known.
    AssetPtr DestinationSkeletonAsset() const { return destinationSkeletonAsset; }

signals:
    /// Emitted once the import is done. 
    /** @note This can happen inside of the Import functions or after it once all needed 
        resources have been loaded. Connect to this signal before calling Import. */
    void ImportDone(MeshmoonOpenAssetImporter *importer, bool success, const ImportInfo &info);

private slots:
    void OnTextureLoaded(AssetPtr asset);
    void OnTextureLoadFailed(IAssetTransfer* assetTransfer, QString reason);

    void OnPreLoadDependencyLoaded(AssetPtr asset);
    void OnPreLoadDependencyFailed(IAssetTransfer* assetTransfer, QString reason);

private:
    /// Check and emit completion.
    void CheckCompletion();

    // Emits failure and logs error message.
    bool EmitFailure(const QString &error);

    /// Reset and free internal data structures and memory.
    void Reset();
    
    /// Populates importInfo_.
    void InitImportInfo(const QString &assetRef, const QString &diskSource);
    
    /// Dependency related
    int ResolveAndRequestDependencies(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource);
    void ResolveAndRequestObjDependencies(const u8 *data_, size_t numBytes, const QString &assetRef, const QString &diskSource);
    
    QString ResolveDependency(const QString &contextRef, const QString &parentDiskSource, const QString &depRef);

    int NumPendingPreLoadDependencies() const;
    int RemovePreLoadDependency(const QString &assetRef);

    /// Sets texture unit to material.
    void SetTexture(AssetPtr asset, const QString &destOgreMaterialName = "", const QString &destTextureUnitName = "");

    /// Loads texture files from disk or requests them from AssetAPI.
    bool LoadTexture(const QString &textureRef, const QString &destOgreMaterialName, const QString &destTextureUnitName);
    
    /// Creates vertex data to submeshes.
    bool CreateVertexData(const Ogre::String& name, const aiNode* pNode, const aiMesh *mesh, 
                          Ogre::SubMesh* submesh, Ogre::AxisAlignedBox& mAAB);
    
    /// Generates the ogre materials.
    Ogre::MaterialPtr CreateMaterial(const QString &materialRef, const aiMaterial* mat, const QString &meshFileDiskSource, 
                                     const QString &meshFileName, const QString &assetRef = "");
    Ogre::MaterialPtr CreateVertexColorMaterial(const QString &materialRef, const aiMaterial *mat);
    Ogre::MaterialPtr CreateMaterialByScript(int index, const aiMaterial* mat);
    
    void GrabNodeNamesFromNode(const aiScene* mScene,  const aiNode* pNode);
    void GrabBoneNamesFromNode(const aiScene* mScene,  const aiNode* pNode);
   
    void GetBasePose(const aiScene *sc, const aiNode *nd);
   
    void ComputeNodesDerivedTransform(const aiScene* mScene, const aiNode *pNode, const aiMatrix4x4 &accTransform);
    void CreateBonesFromNode(const aiScene* mScene,  const aiNode* pNode);
    void CreateBoneHiearchy(const aiScene* mScene,  const aiNode *pNode) const;
    
    void CreateSubmeshesFromNode(const aiScene* mScene,  const aiNode *pNode, const QString &meshFileDiskSource, 
                                 const QString &meshFileName, Ogre::MeshPtr destinationsMesh, const QString &assetRef = "");
    
    void MarkAllChildNodesAsNeeded(const aiNode *pNode);
    void FlagNodeAsNeeded(const char* name);
    bool IsNodeNeeded(const char* name) const;
    void ParseAnimation (const aiScene* mScene, int index, aiAnimation* anim);

    const aiScene *scene;
    //int mLoaderParams;
    int texCount;
    Ogre::String mMaterialCode;
    Ogre::String mCustomAnimationName;
    Ogre::SkeletonPtr mSkeleton;

    int msBoneCount;
    int mSubMeshCount;

    Ogre::Real mTicksPerSecond;

    Framework *framework_;
    AssetAPI *assetAPI_;
    AssetPtr destinationMeshAsset;
    AssetPtr destinationSkeletonAsset;

    bool meshCreated;
    bool completedEmitted;
    bool debugLoggingEnabled_;
    
    BoneMapType boneMap;
    BoneNodeMap mBoneNodesByName;
    BoneMap mBonesByName;
    NodeTransformMap mNodeDerivedTransformByName;
    NodeTransformMap mBoneDerivedTransformByName;
    
    struct WaitingImportData
    {
        std::vector<u8> data;
        QString assetRef;
        QString diskSource;
    };
    // Data is copied to this when pre-load dependencies are fetched.
    WaitingImportData waitingImportData_;
    
    QStringList pendingDependencies_;
    bool depsResolved_;

    struct TextureReceiver
    {
        QString textureRef;
        QString materialName;
        QString textureUnitName;
        
        bool Matches(const TextureReceiver &other) const;
        bool Matches(const QString &ref, const QString &mat, const QString &tu) const;
    };
    /// @note This is not a map as a texture might have multiple material and texture unit receivers.
    QList<TextureReceiver> textureReceivers_;
    
    uint NumPendingMaterialTextures(const QString &destOgreMaterialName);
    void RemoveTextureReceivers(const QString &textureRef);
    void RemoveTextureReceiver(const TextureReceiver &receiver);
    
    ImportInfo importInfo_;
    QList<AnimationInfo> animationInfos_;
    
    static const QString LC;
};

class MESHMOON_ASSIMP_API MeshmoonOpenAssetImportIOStream : public Assimp::IOStream
{
public:
    MeshmoonOpenAssetImportIOStream(const QString path, QIODevice::OpenMode mode);
    ~MeshmoonOpenAssetImportIOStream();
    
    bool IsOpen() const;
    
	/// Assimp::IOSystem override.
    virtual size_t Read(void* pvBuffer, size_t pSize, size_t pCount);

	/// Assimp::IOSystem override.
    virtual size_t Write(const void* pvBuffer, size_t pSize, size_t pCount);

	/// Assimp::IOSystem override.
	virtual aiReturn Seek(size_t pOffset, aiOrigin pOrigin);

	/// Assimp::IOSystem override.
    virtual size_t Tell() const;

	/// Assimp::IOSystem override.
	virtual size_t FileSize() const;

	/// Assimp::IOSystem override.
	virtual void Flush();
	
private:
    QFile *file_;
};

class MESHMOON_ASSIMP_API MeshmoonOpenAssetImportIOSystem : public Assimp::IOSystem
{
public:
    MeshmoonOpenAssetImportIOSystem(Framework *framework, const QString &contextRef, const QString &contextDiskSource);
    ~MeshmoonOpenAssetImportIOSystem();

	/// Assimp::IOSystem override.
	virtual bool Exists( const char* pFile) const;

	/// Assimp::IOSystem override.
	virtual char getOsSeparator() const;

	/// Assimp::IOSystem override.
    /** Required modes: "wb", "w", "wt", "rb", "r", "rt".
        We only have to implement the read modes. */
    virtual Assimp::IOStream* Open(const char* pFile, const char* pMode = "rb");

    /// Assimp::IOSystem override.
	virtual void Close(Assimp::IOStream* pFile);
	
	/// Assimp::IOSystem override.
    virtual bool ComparePaths(const char* one, const char* second) const;
	
	QString PathForReference(const QString &ref) const;

private:
    Framework *framework_;
    
    QString contextRef_;
    QString contextDiskSource_;
};

/// @endcond
