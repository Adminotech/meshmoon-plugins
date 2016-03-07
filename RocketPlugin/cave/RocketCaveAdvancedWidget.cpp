/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketCaveAdvancedWidget.h"

#include "ConfigAPI.h"
#include "UiAPI.h"
#include "UiMainWindow.h"

#include "MemoryLeakCheck.h"

RocketCaveAdvancedWidget::RocketCaveAdvancedWidget(Framework *framework):
    framework_(framework),
    proxy_(0)
{
    ui_.setupUi(this);

    setWindowFlags(Qt::WindowStaysOnTopHint | Qt::Tool);

    //proxy_ = framework_->Ui()->GraphicsScene()->addWidget(this, Qt::Dialog);
    //proxy_->setPos(100, 100);
    hide();

    connect(ui_.customMatrix, SIGNAL(toggled(bool)), this, SLOT(UseCustomViews(bool)));

    connect(ui_.editView1, SIGNAL(clicked()), this, SLOT(EditView1()));
    connect(ui_.editView2, SIGNAL(clicked()), this, SLOT(EditView2()));
    connect(ui_.editView3, SIGNAL(clicked()), this, SLOT(EditView3()));
    connect(ui_.editView4, SIGNAL(clicked()), this, SLOT(EditView4()));

    connect(ui_.updateEyePositionButton, SIGNAL(clicked()), this, SLOT(UpdateEyePosition()));

    editView_ = new RocketCaveAdvancedEditViewWidget(framework_);

    connect(editView_, SIGNAL(ViewUpdated()), this, SLOT(OnUpdateView()));

    currentView = 0;

    ConfigData cave("rocketcave", "main");
    ui_.customMatrix->setChecked(framework_->Config()->Read(cave, "custom").toBool());
}

RocketCaveAdvancedWidget::~RocketCaveAdvancedWidget()
{
    if(!framework_)
        return;

    delete editView_;
    editView_ = 0;

    framework_->Ui()->GraphicsScene()->removeItem(proxy_);
    proxy_ = 0;
}

void RocketCaveAdvancedWidget::Show()
{
    if(!isVisible())
        show();
}

void RocketCaveAdvancedWidget::UseCustom()
{
    if(ui_.customMatrix->isChecked())
        UpdateAllViews();
    else
        emit SetUseDefaultViews();
}

void RocketCaveAdvancedWidget::UpdateAllViews()
{
    currentView = 0;
    LoadAndShowView();
    editView_->UpdateVectors(0.0);
    OnUpdateView();

    currentView = 1;
    LoadAndShowView();
    editView_->UpdateVectors(0.0);
    OnUpdateView();

    currentView = 2;
    LoadAndShowView();
    editView_->UpdateVectors(0.0);
    OnUpdateView();

    currentView = 3;
    LoadAndShowView();
    editView_->UpdateVectors(0.0);
    OnUpdateView();

    editView_->hide();
}

void RocketCaveAdvancedWidget::UseCustomViews(bool checked)
{
    if(checked)
    {
        UpdateAllViews();
    }
    else
    {
        emit SetUseDefaultViews();
    }

    ConfigData cave("rocketcave", "main");
    framework_->Config()->Write(cave, "custom", checked);
}

QString RocketCaveAdvancedWidget::ViewToQString()
{
    QString result;
    result = QString::number(editView_->top_left.x()) + " " + QString::number(editView_->top_left.y()) + " " + QString::number(editView_->top_left.z()) + " ";
    result += QString::number(editView_->bottom_left.x()) + " " + QString::number(editView_->bottom_left.y()) + " " + QString::number(editView_->bottom_left.z()) + " ";
    result += QString::number(editView_->bottom_right.x()) + " " + QString::number(editView_->bottom_right.y()) + " " + QString::number(editView_->bottom_right.z());

    return result;
}

void RocketCaveAdvancedWidget::OnUpdateView()
{
    // Save settings to config
    ConfigData cave("rocketcave", "advanced");
    framework_->Config()->Write(cave, "view" + QString::number(currentView + 1), ViewToQString());

    if(!ui_.customMatrix->isChecked())
        return;

    emit SetViewDimensions(currentView, editView_->top_left, editView_->bottom_left, editView_->bottom_right);
}

/// @todo value is unused - Matias fix this
void RocketCaveAdvancedWidget::UpdateEyePosition()
{
    QVector3D eye_pos;
    eye_pos.setX(ui_.eyeX->value());
    eye_pos.setY(ui_.eyeY->value());
    eye_pos.setZ(ui_.eyeZ->value());

    emit SetEyePosition(eye_pos);
}

void RocketCaveAdvancedWidget::EditView1()
{
    currentView = 0;
    LoadAndShowView();
}

void RocketCaveAdvancedWidget::EditView2()
{
    currentView = 1;
    LoadAndShowView();
}

void RocketCaveAdvancedWidget::EditView3()
{
    currentView = 2;
    LoadAndShowView();
}

void RocketCaveAdvancedWidget::EditView4()
{
    currentView = 3;
    LoadAndShowView();
}

void RocketCaveAdvancedWidget::LoadAndShowView()
{
    ConfigData cave("rocketcave", "advanced");
    QString view = framework_->Config()->Read(cave, "view" + QString::number(currentView + 1)).toString();

    editView_->ViewFromQString(view);
    editView_->Show();
}
