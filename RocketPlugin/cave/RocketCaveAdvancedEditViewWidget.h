/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "ui_RocketCaveAdvancedEditViewWidget.h"

#include "RocketFwd.h"
#include "Framework.h"

#include <QVector3D>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>

class RocketCaveAdvancedEditViewWidget : public QWidget
{
    Q_OBJECT
public:
    RocketCaveAdvancedEditViewWidget(Framework *framework);
    ~RocketCaveAdvancedEditViewWidget();

    Ui::RocketCaveAdvancedEditViewWidget ui_;

    QVector3D top_left;
    QVector3D bottom_left;
    QVector3D bottom_right;

signals:
    void ViewUpdated();

public slots:
    void Show();

    void ViewFromQString(QString view);

    void UpdateVectors(double value);

private:
    Framework *framework_;
    QGraphicsProxyWidget *proxy_;
};
