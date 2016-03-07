/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "Framework.h"

#include "ui_RocketCaveAdvancedWidget.h"

#include "RocketCaveAdvancedEditViewWidget.h"

#include <QWidget>
#include <QDebug>
#include <QApplication>
#include <QDesktopWidget>
#include <QVector3D>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>

class RocketCaveAdvancedWidget : public QWidget
{
    Q_OBJECT

public:
    RocketCaveAdvancedWidget(Framework *framework);
    ~RocketCaveAdvancedWidget();

    Ui::RocketCaveAdvancedWidget ui_;

signals:
    void SetViewDimensions(int view, QVector3D top_left, QVector3D bottom_left, QVector3D bottom_right);
    void SetUseDefaultViews();

    void SetEyePosition(QVector3D eye_position);

public slots:
    void Show();

    void UseCustom();

private slots:
    void UseCustomViews(bool checked);

    void UpdateEyePosition();

    void EditView1();
    void EditView2();
    void EditView3();
    void EditView4();

    void LoadAndShowView();

    void OnUpdateView();

    QString ViewToQString();

    void UpdateAllViews();

private:
    int currentView;

    Framework *framework_;

    QGraphicsProxyWidget *proxy_;

    RocketCaveAdvancedEditViewWidget *editView_;
};
