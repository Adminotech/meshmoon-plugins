/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketCaveAdvancedEditViewWidget.h"

#include "UiAPI.h"

#include "MemoryLeakCheck.h"

RocketCaveAdvancedEditViewWidget::RocketCaveAdvancedEditViewWidget(Framework *framework)
{
    framework_ = framework;

    ui_.setupUi(this);
    proxy_ = framework_->Ui()->GraphicsScene()->addWidget(this, Qt::Dialog);
    proxy_->setPos(100, 100);
    hide();

    connect(ui_.topLeftX, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.topLeftY, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.topLeftZ, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));

    connect(ui_.bottomLeftX, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.bottomLeftY, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.bottomLeftZ, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));

    connect(ui_.bottomRightX, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.bottomRightY, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
    connect(ui_.bottomRightZ, SIGNAL(valueChanged(double)), this, SLOT(UpdateVectors(double)));
}

RocketCaveAdvancedEditViewWidget::~RocketCaveAdvancedEditViewWidget()
{
    if(!framework_)
        return;

    framework_->Ui()->GraphicsScene()->removeItem(proxy_);
    proxy_ = 0;
}

void RocketCaveAdvancedEditViewWidget::Show()
{
    if(!isVisible())
        show();
}

void RocketCaveAdvancedEditViewWidget::ViewFromQString(QString view)
{
    QStringList values = view.split(" ");
    if(values.size() == 9)
    {
        // Top left
        ui_.topLeftX->setValue(values[0].toDouble());
        ui_.topLeftY->setValue(values[1].toDouble());
        ui_.topLeftZ->setValue(values[2].toDouble());

        // Bottom left
        ui_.bottomLeftX->setValue(values[3].toDouble());
        ui_.bottomLeftY->setValue(values[4].toDouble());
        ui_.bottomLeftZ->setValue(values[5].toDouble());

        // Bottom right
        ui_.bottomRightX->setValue(values[6].toDouble());
        ui_.bottomRightY->setValue(values[7].toDouble());
        ui_.bottomRightZ->setValue(values[8].toDouble());
    }
    else
    {
        // Top left
        ui_.topLeftX->setValue(0);
        ui_.topLeftY->setValue(0);
        ui_.topLeftZ->setValue(0);

        // Bottom left
        ui_.bottomLeftX->setValue(0);
        ui_.bottomLeftY->setValue(0);
        ui_.bottomLeftZ->setValue(0);

        // Bottom right
        ui_.bottomRightX->setValue(0);
        ui_.bottomRightY->setValue(0);
        ui_.bottomRightZ->setValue(0);
    }
}
/// @todo value is unused - Matias fix this
void RocketCaveAdvancedEditViewWidget::UpdateVectors(double /*value*/)
{
    /* Top left point */
    top_left.setX(ui_.topLeftX->value());
    top_left.setY(ui_.topLeftY->value());
    top_left.setZ(ui_.topLeftZ->value());

    /* Bottom left point */
    bottom_left.setX(ui_.bottomLeftX->value());
    bottom_left.setY(ui_.bottomLeftY->value());
    bottom_left.setZ(ui_.bottomLeftZ->value());

    /* Bottom right point */
    bottom_right.setX(ui_.bottomRightX->value());
    bottom_right.setY(ui_.bottomRightY->value());
    bottom_right.setZ(ui_.bottomRightZ->value());

    emit ViewUpdated();
}
