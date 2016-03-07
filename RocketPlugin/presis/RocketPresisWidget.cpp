/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketPresisWidget.h"
#include "RocketPlugin.h"
#include "storage/MeshmoonStorageDialogs.h"

#include "Framework.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"

#include "MemoryLeakCheck.h"

AdminotechSlideManagerWidget::AdminotechSlideManagerWidget(RocketPlugin *plugin) :
    RocketSceneWidget(plugin->GetFramework()),
    plugin_(plugin)
{
    ui.setupUi(widget_);
    
    setAcceptDrops(true);
    widget_->setAcceptDrops(true);

    UiGraphicsView *gv = framework_->Ui()->GraphicsView();
    connect(gv, SIGNAL(DragEnterEvent(QDragEnterEvent*, QGraphicsItem*)), SLOT(OnHandleDragEnterEvent(QDragEnterEvent*, QGraphicsItem*)));
    connect(gv, SIGNAL(DragLeaveEvent(QDragLeaveEvent*)), SLOT(OnHandleDragLeaveEvent(QDragLeaveEvent*)));
    connect(gv, SIGNAL(DragMoveEvent(QDragMoveEvent*, QGraphicsItem*)), SLOT(OnHandleDragMoveEvent(QDragMoveEvent*, QGraphicsItem*)));
    connect(gv, SIGNAL(DropEvent(QDropEvent*, QGraphicsItem*)), SLOT(OnHandleDropEvent(QDropEvent*, QGraphicsItem*)));
    
    originalDropText_ = ui.NewPresentationButton->text();
    acceptSuffixes_ << ".ppt" << ".pptx" << ".pdf";
}

AdminotechSlideManagerWidget::~AdminotechSlideManagerWidget()
{
}

void AdminotechSlideManagerWidget::OnHandleDragEnterEvent(QDragEnterEvent *e, QGraphicsItem * /*widget*/)
{
    if (!isVisible())
        return;

    if (e->mimeData()->urls().isEmpty())
        return;
        
    QString filename = e->mimeData()->urls().first().toString();
    foreach(const QString suffix, acceptSuffixes_)
    {
        if (filename.toLower().endsWith(suffix))
        {
            e->acceptProposedAction();
            break;
        }
    }
}

void AdminotechSlideManagerWidget::OnHandleDragLeaveEvent(QDragLeaveEvent * /*e*/)
{
    ui.NewPresentationButton->setText(originalDropText_);
}

void AdminotechSlideManagerWidget::OnHandleDragMoveEvent(QDragMoveEvent *e, QGraphicsItem *widget)
{
    if (!isVisible())
        return;
    if (widget != this)
    {
        ui.NewPresentationButton->setText(originalDropText_);
        return;
    }    

    QPointF ownPos = widget->mapFromScene(e->pos());
    QWidget *child = this->widget_->childAt(ownPos.toPoint());
    if (child == ui.NewPresentationButton || child == ui.PresentationList || child == ui.PresentationList->viewport())
    {
        QString filename = e->mimeData()->urls().first().toString().split("/").last();
        ui.NewPresentationButton->setText("Drop " + filename + "\nto create new presentation");
        e->acceptProposedAction();
        return;
    }
    
    ui.NewPresentationButton->setText(originalDropText_);
    e->ignore();
}

void AdminotechSlideManagerWidget::OnHandleDropEvent(QDropEvent *e, QGraphicsItem * /*widget*/)
{
    ui.NewPresentationButton->setText(originalDropText_);
    
    if (!isVisible())
        return;
    
    QString filename = e->mimeData()->urls().first().toString();
    foreach(const QString suffix, acceptSuffixes_)
    {
        if (filename.toLower().endsWith(suffix))
        {
            // Strip file://(/) from the beginning.
#ifdef Q_WS_WIN
            if (filename.startsWith("file:///"))
                filename = filename.right(filename.length() - 8);
#else
            if (filename.startsWith("file://"))
                filename = filename.right(filename.length() - 7);
#endif
            e->acceptProposedAction();
            emit FileChosen(filename);
        }
    }
}

void AdminotechSlideManagerWidget::OnNewPresentation()
{
    QString path;
#ifndef __APPLE__
    path = "/";
#else
    path = QDir::homePath();
#endif

    RocketStorageFileDialog *dialog = new RocketStorageFileDialog(framework_->Ui()->MainWindow(), plugin_, tr("Select presentation file"), 
        path, QDir::NoDotAndDotDot | QDir::Files | QDir::AllDirs | QDir::NoSymLinks);
    QStringList nameFilters;
    nameFilters << "Presentation files(*.ppt *.pptx *.pdf)";
    dialog->setNameFilters(nameFilters);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setViewMode(QFileDialog::Detail);
        
    connect(dialog, SIGNAL(FilesSelected(const QStringList &, bool)), this, SLOT(OnFilesSelected(const QStringList &)));

    dialog->Open();
    dialog->setFocus(Qt::ActiveWindowFocusReason);
    dialog->activateWindow();
}

void AdminotechSlideManagerWidget::OnFilesSelected(const QStringList & filenames)
{
    QString filename = filenames[0];
    if (!filename.isEmpty())
        emit FileChosen(filename);
}

// AdminotechSlideControlsWidget

AdminotechSlideControlsWidget::AdminotechSlideControlsWidget(RocketPlugin *plugin):
    RocketSceneWidget(plugin->GetFramework()),
    plugin_(plugin)
{
    ui.setupUi(widget_);

    ui.labelSlide->setText("");
    ui.labelDuration->setText("");
}

AdminotechSlideControlsWidget::~AdminotechSlideControlsWidget()
{
}
