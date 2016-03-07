/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "AssetFwd.h"
#include "InputFwd.h"
#include "EC_Mesh.h"

#include <QDir>
#include <QPair>
#include <QEasingCurve>
#include <QMovie>

#include "Math/MathFwd.h"
#include "Geometry/Ray.h"

#include "RocketSplineCurve3D.h"

class QListWidgetItem;

// @cond PRIVATE

class RocketPresis : public QObject
{
Q_OBJECT

public:
    RocketPresis(RocketPlugin* plugin);
    ~RocketPresis();
    
    typedef QPair<QString, QString> QStringPair;
    
public slots:
    // Shows widget
    void Open();
    
    // Close widget
    void Close(bool restoreMainView = true);
    
    // Is visible
    bool IsVisible();

private slots:
    void DelayedShow();

    void OnPresentationFileChosen(const QString& filename);
    
    void OnBackendReply(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString &error);
    void ExecuteJsonQuery();

    void OnStartPresentation();
    
    void OnNextSlide();
    void OnPrevSlide();
    
    void OnRenameCurrentPresentation();
    void OnRemoveCurrentPresentation();
    
    void OnUpdate(float elapsedTime);
    void OnSceneResized(const QRectF &rect);
    
    void ShowStart();
    void ShowControls();

    void OnKeyPress(KeyEvent *e);

    void CheckAndDownloadEnvironments();
    
    void SetProgress(const QString &message = QString());
    void StopProgress();
    
    void UpdateCurrentPresentation(QListWidgetItem *current = 0, QListWidgetItem *previous = 0);
    void UpdateCurrentScene(QListWidgetItem *current = 0, QListWidgetItem *previous = 0);
    
    void ClearImage(bool presentation, bool scene);
    
private:   
    // Load presentations
    void UpdatePresentations();
    
    // Load template scenes
    void UpdateScenes();

    // Write json metadata of current presentation
    void WriteJsonMetaData(const QString &subdir, const QStringList &slides);
    
    // Create scene
    void CreateScene();
    
    // Load scene from txml
    void LoadScene();
    
    // Reset scene and free entities
    void ResetScene();
    
    // Load presentation slides
    bool LoadPresentation();
    
    // Calculate new camera transform
    void CalcCameraTransform(int slide);
    
    // Shows slide. Target specifies mesh where it will be shown
    void ShowSlide(int slide, int target);
    
    // Update slides
    void UpdateSlideTextures();
    
    // Must create custom methods for intersection testing for perspectice frustum
    bool FrustumContains(EC_Camera *temp, const AABB &aabb);
    
    // Update preview position and orientation
    void UpdatePreview();
    
    // Reset slides to first slides
    void LoadStartSlides();
    
    // Go to preview position
    void GoToPreviewPosition();

    // Get time
    void GetElapsedTime(int &minutes, int &seconds);
    
    // Check if scene is fully loaded
    bool CheckLoadingState();

    QString LC;
    QString identifier_;

    struct Data
    {
        EntityPtr camera;
        ScenePtr scene;
        
        Data();
        
        bool isSceneFullyLoaded;
        
        void Reset();
        bool IsValid();
        bool IsSceneValid();
    };
    Data data_;

    struct Presentation
    {
        // Is presentation started
        bool started;
        bool lastStarted;
        
        // Are start slides loaded
        bool startSlidesLoaded;
        
        // Are splides updated
        bool updated;
    
        QString presentation;
        QString scene;
        
        QStringList slides;
        
        QList<shared_ptr<EC_Material> > slideMaterials;
        QList<shared_ptr<EC_Mesh> > slideMeshes;
        QList<shared_ptr<EC_Placeable> > slideTransforms;

        int currentPreviewPosition;
        QList<shared_ptr<EC_Placeable> > previewTransforms;
        
        // Units per second
        float previewSpeed;
        float previewSpeedScalar;
        float previewTime;
        
        bool goToPreview;
        
        QList<EntityPtr> slideEntities;
        
        // Target position for camera
        Transform target;
        Transform start;

        bool goToSplit;
        bool loadSlides;
        
        // Current slide entity
        int currentSlide;
        int currentSlideTotal;
        
        // Current images in slides
        int **slideImages;
        
        enum Direction
        {
            Forward,
            Backward  
        };
        
        // Current direction we are moving
        Direction currentDirection;

        // Current slide image
        int currentImage;
        
        EntityPtr currentEntity;
        shared_ptr<EC_Placeable> currentEntityTransform;
        
        // Total elapsed time while animating
        float elapsedTime;
        
        // Total time that we have been in this slide
        float slideTime;
        
        bool transitioning;
        bool transitioningChanged;
        
        // Total elapsed time
        float totalElapsedTime;
        
        Presentation();
    };
    
    Presentation presentation_;
    
    struct PresentationSettings
    {
        float timeScale;
        
        bool automatic;
        float delay;
        
        bool autoPreview;
        float previewTime;
        
        PresentationSettings();
    };
    
    PresentationSettings settings_;

    struct ProcessingState
    {
        QString server;
    
        QString sceneListUrl;
        QString sceneListUrlBase;
        QList<QStringPair > assetDownloads;
        
        QString currentSceneName;
     
        bool loadingScenes;
    
        QString url;
        QString filename;
        QString id;
        QString urlType;
        QString uuid;
        
        bool processing;
        int slidecount;        
        int imagesDownloaded;
        
        ProcessingState();
    };
    
    ProcessingState state_;
    
    struct PresentationsDir
    {
        QString base;
        QString slides;
        QString scenes;
        
        QString name;
    };
    
    PresentationsDir presentationsdir_;

    QDir dir_;
    QAction *actPresentation;

    RocketPlugin *plugin_;
    Framework *framework_;
    
    AdminotechSlideManagerWidget *widget_;
    AdminotechSlideControlsWidget *controlsWidget_;
    
    QMovie *statusAnim_;
    
    // This is used to calculate smooth camera transform
    RocketSplineCurve3D curve_;
    QEasingCurve easing_;
};

// @endcond
