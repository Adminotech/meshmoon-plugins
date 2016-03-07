/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "SceneFwd.h"
#include "CoreDefines.h"
#include "qts3/QS3Fwd.h"

#include "utils/RocketZipWorker.h"

#include "IAttribute.h"
#include "AssetAPI.h"
#include "AssetReference.h"

#include "ui_RocketSceneOptimizerWidget.h"

#include <QObject>
#include <QDir>
#include <QTime>
#include <QFileInfo>
#include <QHash>

class QFile;
class EC_Mesh;
class EC_Material;

/// @cond PRIVATE

class RocketScenePackager : public QObject
{
Q_OBJECT

public:
    explicit RocketScenePackager(RocketPlugin *plugin);
    ~RocketScenePackager();

    struct AssetFileInfo
    {
        AssetReference ref;

        QString bundleAssetRef;
        QString diskSourceAbsolutePath;
        QFileInfo diskSource;
        
        bool dontProcess;
        bool texturesProcessed;
        bool texturePostProcessed;
        
        AssetFileInfo(const QString &_ref, const QString &_type) : 
            dontProcess(false), texturesProcessed(false), texturePostProcessed(false),
            ref(_ref, _type)
        {
        }

        void SetDiskSource(const QString absoluteDiskSource)
        {
            diskSourceAbsolutePath = absoluteDiskSource;
            diskSource = QFileInfo(diskSourceAbsolutePath);
        }
    };
    
    struct TextureInfo
    {
        int iTech;
        int iPass;
        int iTexUnit;
        QString ref;

        TextureInfo() : iTech(-1), iPass(-1), iTexUnit(-1) {}
    };
    typedef QList<TextureInfo> TextureList;

    struct TextureProcessing
    {
        bool process;
        
        QString convertToFormat;
        QString mipmapGeneration;
        
        uint rescaleMode;
        uint maxSize;
        uint maxMipmaps;
        uint quality;

        uint totalProcessed;
        uint totalConverted;
        uint totalResized;
        uint totalRescaled;

        QStringList supportedInputFormats;

        TextureProcessing() :
            process(false),
            mipmapGeneration("UseSource"),
            rescaleMode(0),
            maxSize(0),
            maxMipmaps(1),
            quality(255),
            totalProcessed(0),
            totalConverted(0),
            totalResized(0),
            totalRescaled(0)
        {
        }

        void Disable() { Reset(); }
        void Reset() { *this = TextureProcessing(); }

        bool IsEnabled() const
        {
            if (!process)
                return false;
            if (convertToFormat != "")
                return true;
            if (mipmapGeneration != "" && mipmapGeneration != "UseSource")
                return true;
            if (rescaleMode != 0)
                return true;
            if (maxSize != 0)
                return true;
            return false;
        }
    };

    struct State
    {
        uint maxPackagerThreads;

        bool storageAuthenticated;
        bool waitingForAssetBundleUploads;
        bool bundleStorageUploadsOk;
        bool bundlesUploading;
        
        uint entitiesProcessed;
        uint entitiesEmptyRemoved;
        uint totalConvertedRefs;

        uint bundledMeshRefs;
        uint bundledMaterialRefs;
        uint bundledTextureRefs;
        uint bundledTextureGeneratedRefs;

        uint currentMeshBundleIndex;
        uint currentTextureBundleIndex;
        uint currentTextureGeneratedBundleIndex;
        
        qint64 totalFileSize;
        qint64 totalFileSizeMesh;
        qint64 totalFileSizeMat;
        qint64 totalFileSizeTex;
        qint64 totalFileSizeTexGen;
        
        qint64 nowFileSizeMesh;
        qint64 nowFileSizeTex;
        qint64 nowFileSizeTexGen;
        
        qint64 bundleSplitSize;
        qint64 filesCopied;
        
        uint foundMaxTexWidth;
        uint foundMaxTexHeight;
        
        bool processMeshes;
        bool processMaterials;
        bool processGeneratedMaterials;
        bool processScriptEnts;
        bool processContentToolsEnts;
        
        bool removeEmptyEntities;
        bool rewriteAssetReferences;

        QFile *logFile;
        bool logDebug;

        TextureProcessing texProcessing;
                
        QString txmlRef;
        QString txmlBaseUrl;
        QString txmlForcedBaseUrl;
        QString outputTxmlFile;
        QString bundlePrefix;
        QString textureTool;
        QDir outputDir;

        QList<AssetFileInfo*> assetsInfo;
        QHash<QString, QString> assetBaseToUuid;
        QSet<QString> rewriteUuidFilenames;
        QHash<QString, QString> oldToNewGeneraterMaterial;   
        QList<RocketZipWorker*> packagers;
        QList<QPair<AttributeWeakPtr, QString> > changedAttributes;
        QSet<QString> optimizedAssetRefs;

        QTime time;

        State() : logFile(0), storageAuthenticated(false) { Reset(); }
        
        void Reset();
        QFile *OpenLog();
    };
    
public slots:
    void Show();
    void Hide();
    
    /// Returns if the tool is running.
    bool IsRunning() const;
    
    /// Returns if the tool is stopping.
    bool IsStopping() const;
    
    /// Requests the tool to be stopped, if the tool is running.
    /** This is not a synchronous operation, we need to stop processing tools etc. over a short period of time. Connect to Stopped signals to be notified. 
        @note This will prompt a main window modal dialog to ask the user if he wants to stop, the result will be returned as a bool.
        @return True if user wanted to stop the packager, false otherwise. */
    bool RequestStop();

    QWidget *Widget() const;
    
signals:
    void Started();
    void Stopped();

private slots:
    void Start();
    bool CheckStopping();
    
    void AuthenticateStorate();
    void BackupCompleted(QS3CopyObjectResponse *response);

    void Run();
    void Stop();

    void TryStartProcessing();
    void Process();

    void EnableInterface(bool enabled);
    
    void StorageAuthCompleted();
    void StorageAuthCanceled();

    void ReadConfig();
    void SaveConfig();
    
    void StartAssetBundleUploads();
    void ReplicateChangesToServer();
    
    RocketZipWorker *StartPackager(const QString &destinationFilePath, const QString &inputDirPath, RocketZipWorker::Compression compression = RocketZipWorker::Normal);
    uint RunningPackagers() const;
    uint PendingPackagers() const;
    
    void Log(const QString &message, bool logToConsole = false, bool errorChannel = false, bool warningChannel = false, const QString &color = QString(), bool logToFile = true);
    void LogProgress(const QString &action, int progress = -1, bool updateButton = false);
    
    void OnProcessTexturesChanged();
    void OnProcessMipmapGenerationChanged();
    void OnBrowseOutputDir();
    void OnToggleLogDocking();

    void OnPackagerThreadCompleted();
    void OnAssetBundleUploadOperationProgress(QS3PutObjectResponse *response, qint64 completed, qint64 total, MeshmoonStorageOperationMonitor *operation);
    void OnAssetBundlesUploaded(MeshmoonStorageOperationMonitor *monitor);
    
    void ProcessGeneratedMaterial(Entity *ent, EC_Material *comp);
    void ProcessMeshRef(Entity *ent, EC_Mesh *comp);
    void ProcessMaterialRefs(Entity *ent, EC_Mesh *comp);
    void ProcessTextures(AssetFileInfo *materialAssetInfo, const TextureList &textures);
    
    RocketScenePackager::AssetFileInfo *GetOrCreateAssetFileInfo(const QString &ref, const QString &type, AssetAPI::AssetRefType refType, const QString &basePath, const QString &fileName, const QString &subAssetName);
    RocketScenePackager::AssetFileInfo *GetAssetFileInfo(const QString &ref);
    
    void TrimBaseStorageUrl(QString &ref);
    
    TextureList GetTextures(const QString &materialRef);
    void PostProcessTexture(AssetFileInfo *textureInfo, const QString &type);

    bool CreateOrClearWorkingDirectory();
    bool ClearDirectoryWithConfirmation(const QDir &dir);
    void RemoveAllFilesRecursive(const QDir &dir, bool onlyTxmlFromRoot = true);

protected:
    template <typename T>
    std::vector<T*> GetTargets(std::vector<shared_ptr<T> > candidates) const;
    
    bool eventFilter(QObject *obj, QEvent *e);

private:
    RocketPlugin *plugin_;
    Framework *framework_;
    
    QString LC_;
    QString contentToolsDataCompName_;
    
    SceneWeakPtr scene_;
    State state_;
    
    bool running_;
    bool stopping_;
    
    QWidget *widget_;
    QWidget *logWindow_;
    Ui::RocketSceneOptimizerWidget ui_;
};

/// @endcond
