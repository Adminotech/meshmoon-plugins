/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketBuildEditor.h
    @brief  RocketBuildContextWidget is a context UI for the Build UI. */

#pragma once

#include "RocketFwd.h"
#include "RocketSceneWidget.h"
#include "ui_RocketBuildContextWidget.h"

#include "SceneFwd.h"

#include <QToolButton>

/// Context UI for the Build UI.
class RocketBuildContextWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    explicit RocketBuildContextWidget(RocketPlugin *plugin);
    virtual ~RocketBuildContextWidget();

    QToolButton *undoButton;
    QToolButton *redoButton;

    Ui::RocketBuildContextWidget mainWidget;
    /// @endcond

public slots:   
    /// RocketSceneWidget override.
    void Show();
    
    /// RocketSceneWidget override.
    void Hide(RocketAnimations::Animation anim = RocketAnimations::NoAnimation, const QRectF &sceneRect = QRectF());

    void Resize();

private:
    void ConnectToNameChanges();
    void DisconnectFromNameChanges();

    RocketPlugin *rocket;
    EntityWeakPtr editedEntity; ///< Used only if we have single entity selected.
    
    bool isCompact_;

private slots:
    void Refresh(const EntityList &selection);
    void SetUndoButtonEnabled(bool enabled);
    void SetRedoButtonEnabled(bool enabled);
    void SetEntityName();
    void SetEntityGroup();
    /// Listens to external changes of the Name component of the edited entity.
    void UpdateEntityName(IAttribute *);
    void PopulateGroups();
};
Q_DECLARE_METATYPE(RocketBuildContextWidget*)
