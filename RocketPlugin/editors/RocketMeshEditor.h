/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "IRocketAssetEditor.h"
#include "Win.h"
#include "OgreModuleFwd.h"

#include "storage/MeshmoonStorageItem.h"

#define MATH_OGRE_INTEROP
#include "Transform.h"
#include "Color.h"
#include "Geometry/AABB.h"

#include "ui_RocketMeshEditorWidget.h"

#include <QPointer>
#include <QColorDialog>
#include <QTimer>
#include <QMenu>

/// Basic mesh viewer.
class RocketMeshEditor : public IRocketAssetEditor
{
    Q_OBJECT

public:
    /// Create editor from storage resource.
    RocketMeshEditor(RocketPlugin *plugin, MeshmoonStorageItemWidget *resource);
    
    /// Create view-only editor from a generic asset.
    RocketMeshEditor(RocketPlugin *plugin, MeshmoonAsset *resource);
    
    /// Dtor.
    ~RocketMeshEditor();

    /// Get all supported suffixes
    /** @note The suffixes wont have the first '.' prefix, as "png". */
    static QStringList SupportedSuffixes();

    /// Returns editor name.
    static QString EditorName();

private slots:
    void Initialize();
    void InitializeUi();

    void OnUpdate(float frametime);

    void RenderImage();

    QSize RenderSurfaceSize() const;

    QString UniqueMeshAssetRef();

    /// Transform
    void OnTransformChanged(Transform &value, bool apply = true);
    
    void OnRotationChanged(const float3 &value, bool apply = true);
    void OnRotationXChanged(int value, bool apply = true);
    void OnRotationYChanged(int value, bool apply = true);
    void OnRotationZChanged(int value, bool apply = true);

    /// Lights
    void OnDirectionalEnabledChanged(bool enabled);

    /// Colors
    Color ReadColorConfigOrDefault(const QString &configKey, Color defaultColor);
    void OnRestoreDefaultColor();
    void OnPickColor(QString type = "");
    void OnColorPickerClosed();
    void OnAmbientChanged(const QColor &color, bool apply = true);
    void OnBackgroundChanged(const QColor &color, bool apply = true);
    void OnDirectionalChanged(const QColor &color, bool apply = true);

    /// Rendering toggles
    void OnBoundingBoxEnabledChanged(bool enabled, bool apply = true);
    void OnWireframeEnabledChanged(bool enabled, bool apply = true);

    /// Materials
    void OnMaterialSelected(const QString &ref, AssetPtr asset);

    /// Config
    void ReadConfig();
    void WriteConfig(bool writeDirectional = true, bool writeAmbient = true, bool writeBackground = true, bool writeUiState = true);

    // Cursor
    void SetOverrideCursor(Qt::CursorShape shape);
    void RemoveOverrideCursor();

protected:
    /// QWidget override.
    void resizeEvent(QResizeEvent *e);
    
    /// QObject override.
    bool eventFilter(QObject *obj, QEvent *e);
    
    /// IRocketAssetEditor override.
    void ResourceDownloaded(QByteArray data);
    
    /// IRocketAssetEditor override.
    IRocketAssetEditor::SerializationResult ResourceData();
    
private:
    QString LC;
    
    Ui::RocketMeshEditorWidget ui_;
    QWidget *editor_;
    QPointer<QColorDialog> colorPicker_;

    RocketOgreSceneRenderer *ogreSceneRenderer_;    
    weak_ptr<OgreMeshAsset> mesh_;
    Ogre::Entity *meshEntity_;
    Ogre::SceneNode *meshNode_;
    Ogre::Light *directionalLight_;
    
    QTimer resizeTimer_;
    QPoint mousePos_;
    
    QMenu *colorMenu_;
    
    bool mouseInteraction_;
    float statsUpdateT_;
};
