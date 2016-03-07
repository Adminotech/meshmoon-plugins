/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "AssetFwd.h"

#include "RocketSceneWidget.h"
#include "common/MeshmoonCommon.h"

#include "ui_RocketLayersWidget.h"

#include <QPushButton>
#include <QLabel>

/// @cond PRIVATE
class RocketLayerElementWidget : public QWidget
{
    Q_OBJECT

public:
    RocketLayerElementWidget(Framework *framework, uint id_, bool visible_, const QString &name, const QString &iconUrl);
    ~RocketLayerElementWidget();

    uint id;
    bool visible;

    void UpdateState(bool visiblem, bool emitSignal);
    
public slots:
    void UpdateState(const Meshmoon::SceneLayer &layer);
    
private slots:
    void OnVisibilityButtonPressed(bool checked);
    void OnIconDownloadCompleted(AssetPtr asset);
    
signals:
    void VisibilityChanged(uint layerId, bool visible);
    
private:    
    Framework *framework_;

    QLabel *iconLabel_;
    QPushButton *buttonVisibility_;
    QString styleChecked_;
    QString styleUnchecked_;
};
typedef QList<RocketLayerElementWidget*> RocketLayerElementWidgetList;
/// @endcond

/// Provides UI for layer management.
/** @ingroup MeshmoonRocket */
class RocketLayersWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    explicit RocketLayersWidget(RocketPlugin *plugin);
    ~RocketLayersWidget();

    void UpdateTaskbarEntry(RocketTaskbar *taskbar, bool hasLayers);

    /// Removes layer from the layout.
    void RemoveLayer(const Meshmoon::SceneLayer &layer);

    /// Update widget with given layers.
    void UpdateState(const Meshmoon::SceneLayerList &layers);

    /// Clears all layers content.
    void ClearLayers();

    /// RocketSceneWidget override.
    void Hide(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF &sceneRect = QRectF());

public slots:
    /// Toggles visibility.
    void ToggleVisibility();

    /// Show widget.
    void Show();

private slots:
    void OnWindowResized(const QRectF &rect);
    void OnVisibilityChangeRequest(uint layerId, bool visible);
    
    void HideAllLayers();
    void DownloadAllLayers();

private:
    RocketPlugin *plugin_;

    Ui::RocketLayersWidget ui_;
    RocketLayerElementWidgetList elements_;
    
    QAction *layersToggleAction_;
};
Q_DECLARE_METATYPE(RocketLayersWidget*)
