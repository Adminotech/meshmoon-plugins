/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "RocketSceneWidget.h"

#include "ui_RocketPresisWidget.h"
#include "ui_RocketPresisControlsWidget.h"

#include <QObject>
#include <QWidget>

class QDragEnterEvent;

// AdminotechSlideManagerWidget

class AdminotechSlideManagerWidget : public RocketSceneWidget
{
    Q_OBJECT
    
public:
    AdminotechSlideManagerWidget(RocketPlugin *plugin);
    virtual ~AdminotechSlideManagerWidget();
    
    Ui::RocketPresisWidget ui;

private slots:
    void OnHandleDragEnterEvent(QDragEnterEvent *e, QGraphicsItem *widget);
    void OnHandleDragLeaveEvent(QDragLeaveEvent *e);
    void OnHandleDragMoveEvent(QDragMoveEvent *e, QGraphicsItem *widget);
    void OnHandleDropEvent(QDropEvent *e, QGraphicsItem *widget);
    
signals:
    void FileChosen(QString filename);
    
private slots:
    void OnNewPresentation();
    void OnFilesSelected(const QStringList &filenames);

private:
    RocketPlugin *plugin_;
    QStringList acceptSuffixes_;
    QString originalDropText_;
};

// AdminotechSlideControlsWidget

class AdminotechSlideControlsWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    AdminotechSlideControlsWidget(RocketPlugin *plugin);
    virtual ~AdminotechSlideControlsWidget();

    Ui::RocketPresisControlsWidget ui;
    
private:
    RocketPlugin *plugin_;
};
