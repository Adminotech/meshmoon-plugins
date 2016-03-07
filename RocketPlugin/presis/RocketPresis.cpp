/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketPresis.h"
#include "RocketPresisWidget.h"
#include "RocketNotifications.h"

#include "RocketPlugin.h"
#include "RocketMenu.h"
#include "MeshmoonBackend.h"
#include "RocketTaskbar.h"
#include "MeshmoonUser.h"

#include "Framework.h"
#include "CoreJsonUtils.h"
#include "Profiler.h"
#include "LoggingFunctions.h"
#include "UiMainWindow.h"
#include "SceneAPI.h"
#include "AssetAPI.h"
#include "UiAPI.h"
#include "FrameAPI.h"
#include "InputAPI.h"

#include "Scene.h"
#include "Entity.h"

#include "OgreWorld.h"

#include "EC_Placeable.h"
#include "EC_Camera.h"
#include "EC_Mesh.h"
#include "EC_Material.h"
#include "EC_Name.h"

#include "Geometry/Line.h"
#include "Math/float2.h"
#include "Geometry/Plane.h"

#include "OgreMaterialAsset.h"

#include <QNetworkAccessManager>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QList>
#include <QGraphicsScene>
#include <QMenu>
#include <QPixmap>
#include <QTimer>
#include <QMenuBar>
#include <QDesktopServices>

#include <algorithm>

#include "MemoryLeakCheck.h"

typedef shared_ptr<EC_Placeable> PlaceablePtr;
typedef shared_ptr<EC_Camera> CameraPtr;

static void LogNotImplemented(QString function, QString message = "")
{
    QString out = "RocketPresis::" + function + " not implemented";
    if (!message.isEmpty())
        out += ": " + message;
    LogError(out);
}

RocketPresis::RocketPresis(RocketPlugin *plugin) :
    plugin_(plugin),
    framework_(plugin->GetFramework()),
    LC("[RocketPresis]: "),
    identifier_("RocketPresis"),
    actPresentation(0),
    widget_(0),
    controlsWidget_(0),
    statusAnim_(0),
    easing_(QEasingCurve::InOutQuad)
{
    // Add menu item to access presentation editor
    actPresentation = new QAction(QIcon(":/images/icon-image.png"), tr("Meshmoon Presis"), this);
    connect(actPresentation, SIGNAL(triggered()), SLOT(Open()));
    plugin_->Menu()->AddAction(actPresentation, tr("Utilities"));
}

RocketPresis::~RocketPresis()
{
    Close();
}

RocketPresis::ProcessingState::ProcessingState() :
    processing(false),
    slidecount(0),
    imagesDownloaded(0),
    loadingScenes(false)
{
}

RocketPresis::Presentation::Presentation() :
    startSlidesLoaded(false),
    slideImages(false),
    currentDirection(Forward),
    started(false),
    lastStarted(false),
    updated(false),
    currentSlide(-1),
    currentSlideTotal(0),
    currentImage(0),
    goToSplit(false),
    loadSlides(false),
    elapsedTime(0.0f),
    slideTime(0.0f),
    currentPreviewPosition(0),
    previewSpeed(1.0f),
    previewTime(0.0f),
    previewSpeedScalar(1.0f),
    goToPreview(false),
    transitioning(false),
    transitioningChanged(false),
    totalElapsedTime(0)
{
}

RocketPresis::PresentationSettings::PresentationSettings() :
    timeScale(5.0f),
    automatic(false),
    delay(3.0f),
    autoPreview(false),
    previewTime(5.0f)
{
}

void RocketPresis::UpdatePresentations()
{
    if (!widget_)
        return;
        
    widget_->ui.PresentationList->clear();
    widget_->ui.buttonRename->setEnabled(false);
    widget_->ui.buttonRemove->setEnabled(false);
    
    // Lookup subdirs from the slides location
    dir_ = QDir(presentationsdir_.slides);
    QStringList dirs = dir_.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(const QString &dir, dirs)
        if (QFile::exists(dir_.absoluteFilePath(dir + "/metadata.json")))
            widget_->ui.PresentationList->addItem(dir);

    if (widget_->ui.PresentationList->count() > 0)
        widget_->ui.PresentationList->setCurrentRow(0, QItemSelectionModel::SelectCurrent);
    else
    {
        QListWidgetItem *emptyItem = new QListWidgetItem();
        emptyItem->setText("No presentations found - Create a new one to get started");
        emptyItem->setData(Qt::ToolTipRole, "helptip");
        widget_->ui.PresentationList->addItem(emptyItem);
    }
        
    UpdateCurrentPresentation();
}

void RocketPresis::OnRenameCurrentPresentation()
{
    if (!widget_)
        return;

    QListWidgetItem *current = 0;
    QList<QListWidgetItem*> selected = widget_->ui.PresentationList->selectedItems();
    if (!selected.isEmpty())
        current = selected.first();
    if (!current)
        return;
    QVariant isHelpTip = current->data(Qt::ToolTipRole);
    if (!isHelpTip.isNull() && isHelpTip.toString() == "helptip")
        return;

    QDir slidesDir(presentationsdir_.slides);
    QString dirName = current->text();
    if (dirName.isEmpty() || !slidesDir.exists(dirName))
    {
        plugin_->Notifications()->ShowSplashDialog("Failed to find on the presentation folder for '" + dirName + "'", ":/images/icon-exit.png");
        return;
    }
    QString newName = QInputDialog::getText(framework_->Ui()->MainWindow(), " ", "New presentation name", QLineEdit::Normal, dirName);
    while (newName.toLower() == dirName.toLower() || slidesDir.exists(newName))
        newName = QInputDialog::getText(framework_->Ui()->MainWindow(), " ", "Presentation '" + newName + "' already exists, pick another name", QLineEdit::Normal, newName);
    if (!slidesDir.rename(dirName, newName))
    {
        plugin_->Notifications()->ShowSplashDialog("Failed to rename on the presentation folder to '" + newName + "'", ":/images/icon-exit.png");
        return;
    }
    
    UpdatePresentations();
}

void RocketPresis::OnRemoveCurrentPresentation()
{
    if (!widget_)
        return;

    QListWidgetItem *current = 0;
    QList<QListWidgetItem*> selected = widget_->ui.PresentationList->selectedItems();
    if (!selected.isEmpty())
        current = selected.first();
    if (!current)
        return;
    QVariant isHelpTip = current->data(Qt::ToolTipRole);
    if (!isHelpTip.isNull() && isHelpTip.toString() == "helptip")
        return;

    QDir slidesDir(presentationsdir_.slides);
    QString dirName = current->text();
    if (dirName.isEmpty() || !slidesDir.exists(dirName))
    {
        plugin_->Notifications()->ShowSplashDialog("Failed to find on the presentation folder for '" + dirName + "'", ":/images/icon-exit.png");
        return;
    }
    QMessageBox::StandardButton result = QMessageBox::question(framework_->Ui()->MainWindow(), "Remove Confirmation", 
        "Are you sure you want to remove presentation '" + dirName + "'?", QMessageBox::Yes|QMessageBox::Cancel);
    if (result == QMessageBox::Yes)
    {
        QDir presDir(slidesDir.absoluteFilePath(dirName));
        QFileInfoList entries = presDir.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot);
        foreach(QFileInfo entry, entries)
        {
            if (entry.isFile())
            {
                if (!presDir.remove(entry.fileName()))
                    LogWarning(LC + "Failed to remove presentation file: " + presDir.absoluteFilePath(entry.fileName()));
            }
        }
        
        if (!slidesDir.rmdir(dirName))
        {
            plugin_->Notifications()->ShowSplashDialog("Failed to remove the presentation folder of '" + dirName + "'", ":/images/icon-exit.png");
            return;
        }

        UpdatePresentations();
    }
}

void RocketPresis::UpdateCurrentPresentation(QListWidgetItem *current, QListWidgetItem * /*previous*/)
{
    if (!widget_)
        return;
    
    widget_->ui.buttonRename->setEnabled(false);
    widget_->ui.buttonRemove->setEnabled(false);
    
    if (!current)
    {
        QList<QListWidgetItem*> selected = widget_->ui.PresentationList->selectedItems();
        if (!selected.isEmpty())
            current = selected.first();
    }
    if (!current)
    {
        ClearImage(true, false);
        return;
    }
    QVariant isHelpTip = current->data(Qt::ToolTipRole);
    if (!isHelpTip.isNull() && isHelpTip.toString() == "helptip")
    {
        ClearImage(true, false);
        return;
    }
        
    QDir slidesDir(presentationsdir_.slides);
    QString dirName = current->text();
    if (dirName.isEmpty())
    {
        ClearImage(true, false);
        return;
    }
    if (!slidesDir.exists(dirName))
    {
        ClearImage(true, false);
        return;
    }
    slidesDir.cd(dirName);
    
    widget_->ui.buttonRename->setEnabled(true);
    widget_->ui.buttonRemove->setEnabled(true);
    
    QStringList filters; filters << "*.png";
    slidesDir.setNameFilters(filters);
    QStringList paths = slidesDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (paths.isEmpty())
    {
        ClearImage(true, false);
        return;
    }

    QPixmap image(slidesDir.absoluteFilePath(paths.first()));
    if (image.isNull())
    {
        ClearImage(true, false);
        return;
    }
    
    widget_->ui.labelCurrentPresentation->setPixmap(image.scaled(widget_->ui.labelCurrentPresentation->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    widget_->ui.labelCurrentPresentation->show();
}

void RocketPresis::ClearImage(bool presentation, bool scene)
{
    if (!widget_)
        return;
        
    QTextOption opt;
    opt.setAlignment(Qt::AlignCenter);
    
    if (presentation)
    {
        QPixmap pix(widget_->ui.labelCurrentPresentation->size());
        pix.fill(Qt::white);
        QPainter p(&pix);
        p.drawText(widget_->ui.labelCurrentPresentation->rect(), "No Preview Available", opt);
        p.end();
        widget_->ui.labelCurrentPresentation->setPixmap(pix);
    }
    if (scene)
    {
        QPixmap pix(widget_->ui.labelCurrentScene->size());
        pix.fill(Qt::white);
        QPainter p(&pix);
        p.drawText(widget_->ui.labelCurrentPresentation->rect(), "No Preview Available", opt);
        p.end();
        widget_->ui.labelCurrentScene->setPixmap(pix);
    }
}

void RocketPresis::UpdateScenes()
{
    if (widget_)
        widget_->ui.SceneList->clear();

    // Lookup subdirs with a .txml file from scenes location
    dir_ = QDir(presentationsdir_.scenes);
    QStringList dirs = dir_.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    QStringList filters; filters << "*.txml";
    foreach(const QString &dir, dirs)
    {
        dir_.cd(dir);
        dir_.setNameFilters(filters);
        QStringList files = dir_.entryList(QDir::Files);
        
        if (files.size() == 1)
            widget_->ui.SceneList->addItem(dir);
        else if (files.size() > 1)
            LogWarning(LC + "Scene location has more than one .txml, skipping it: " + dir_.path());
        dir_.cdUp();
    }
    
    if (widget_->ui.SceneList->count() > 0)
        widget_->ui.SceneList->setCurrentRow(0, QItemSelectionModel::SelectCurrent);
        
    UpdateCurrentScene();
}

void RocketPresis::UpdateCurrentScene(QListWidgetItem *current, QListWidgetItem * /*previous*/)
{
    if (!widget_)
        return;
    
    if (!current)
    {
        QList<QListWidgetItem*> selected = widget_->ui.SceneList->selectedItems();
        if (!selected.isEmpty())
            current = selected.first();
    }
    if (!current)
    {
        ClearImage(false, true);
        return;
    }

    QDir scenesDir(presentationsdir_.scenes);
    QString dirName = current->text();
    if (dirName.isEmpty())
    {
        ClearImage(false, true);
        return;
    }
    if (!scenesDir.exists(dirName))
    {
        ClearImage(false, true);
        return;
    }
    scenesDir.cd(dirName);

    QStringList filters; filters << "*.png";
    scenesDir.setNameFilters(filters);
    QStringList paths = scenesDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (paths.isEmpty())
    {
        ClearImage(false, true);
        return;
    }

    QPixmap image(scenesDir.absoluteFilePath(paths.first()));
    if (image.isNull())
    {
        ClearImage(false, true);
        return;
    }

    widget_->ui.labelCurrentScene->setPixmap(image.scaled(widget_->ui.labelCurrentScene->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    widget_->ui.labelCurrentScene->show();
}

void RocketPresis::OnSceneResized(const QRectF &rect)
{
    if (widget_ && widget_->isVisible())
    {
        QRectF targetRect = rect;
        targetRect.setWidth(targetRect.width() - 220);
        
        QRectF widgetRect;
        widgetRect.setHeight(targetRect.height());
        if (targetRect.width() >= 960)
        {
            widgetRect.setTopLeft(QPointF(targetRect.center().x()-480, 0));
            widgetRect.setWidth(960);
        }
        else
        {
            widgetRect.setTopLeft(QPointF(9, 0));
            widgetRect.setWidth(targetRect.width() - 18);
        }

        widget_->setGeometry(widgetRect);
    }
    
    if (controlsWidget_ && controlsWidget_->isVisible())
    {
        QPointF pos(0, rect.height()-controlsWidget_->size().height());
        controlsWidget_->setPos(pos);
        controlsWidget_->setGeometry(QRectF(pos, QSize(rect.width(), controlsWidget_->size().height())));
    }
}

void RocketPresis::ShowStart()
{
    if (controlsWidget_)
    {
        controlsWidget_->hide();
        controlsWidget_->ui.content->hide();
        controlsWidget_->ui.exitButton->hide();
        controlsWidget_->ui.startButton->show();

        QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
        QPointF pos(0, rect.height()-controlsWidget_->size().height());
        
        controlsWidget_->show();
        controlsWidget_->setPos(pos);
        controlsWidget_->hide();
        controlsWidget_->Animate(QSizeF(rect.width(), controlsWidget_->size().height()), pos, -1.0, rect, RocketAnimations::AnimateUp);
    }
}

void RocketPresis::ShowControls()
{
    if (controlsWidget_)
    {        
        controlsWidget_->ui.content->show();
        controlsWidget_->ui.exitButton->show();
        controlsWidget_->ui.startButton->hide();

        QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
        QPointF pos(0, rect.height()-controlsWidget_->size().height());

        controlsWidget_->show();
        controlsWidget_->setPos(pos);
        controlsWidget_->hide();
        controlsWidget_->Animate(QSizeF(rect.width(), controlsWidget_->size().height()), pos, -1.0, rect, RocketAnimations::AnimateUp);
    }
    
    OnNextSlide();
}

bool RocketPresis::IsVisible()
{
    if (widget_)
        return widget_->isVisible();
    return false;
}

void RocketPresis::Open()
{
    // This toggles open/close from the action.
    if (widget_)
    {
        Close();
        return;
    }
    
    if (plugin_->IsConnectedToServer())
    {
        plugin_->Notifications()->ShowSplashDialog("Cannot start Presis while connected to a online world, disconnect first.", ":/images/icon-exit.png");
        return;
    }
    
    ResetScene();
    
    dir_ = QDir(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation));
    if (!dir_.exists())
        dir_ = QDir(QDir::homePath());

    if(!dir_.exists("Meshmoon Presis"))
        dir_.mkdir("Meshmoon Presis");
    dir_.cd("Meshmoon Presis");
    presentationsdir_.base = dir_.absolutePath();
    if(!dir_.exists("Slides"))
        dir_.mkdir("Slides");
    presentationsdir_.slides = dir_.absoluteFilePath("Slides");
    if(!dir_.exists("Scenes"))
        dir_.mkdir("Scenes");
    presentationsdir_.scenes = dir_.absoluteFilePath("Scenes");

    // Create/update UI
    actPresentation->setText("Close Presis");
    curve_ = RocketSplineCurve3D();
    
    // Main widget
    widget_ = new AdminotechSlideManagerWidget(plugin_);
    framework_->Ui()->GraphicsScene()->addItem(widget_);
    widget_->hide();

    widget_->ui.frameStatus->hide();
    widget_->ui.buttonRename->setEnabled(false);
    widget_->ui.buttonRemove->setEnabled(false);
    
    // Controls
    controlsWidget_ = new AdminotechSlideControlsWidget(plugin_);         
    framework_->Ui()->GraphicsScene()->addItem(controlsWidget_);
    controlsWidget_->hide();

    connect(controlsWidget_->ui.nextButton, SIGNAL(clicked()), this, SLOT(OnNextSlide()));
    connect(controlsWidget_->ui.prevButton, SIGNAL(clicked()), this, SLOT(OnPrevSlide()));
    connect(controlsWidget_->ui.startButton, SIGNAL(clicked()), this, SLOT(ShowControls()));
    connect(controlsWidget_->ui.exitButton, SIGNAL(clicked()), this, SLOT(Close()), Qt::QueuedConnection);
    
    // Hide the rocket main content
    plugin_->HideCenterContent();

    // Update slide sets and scenes
    UpdatePresentations();
    UpdateScenes();
    CheckAndDownloadEnvironments();
    
    // Ui signals
    connect(widget_, SIGNAL(FileChosen(QString)), this, SLOT(OnPresentationFileChosen(QString)));
    connect(widget_->ui.NewPresentationButton, SIGNAL(clicked()), widget_, SLOT(OnNewPresentation()));
    connect(widget_->ui.StartButton, SIGNAL(clicked()), this, SLOT(OnStartPresentation()));
    connect(widget_->ui.ExitButton, SIGNAL(clicked()), this, SLOT(Close()), Qt::QueuedConnection);
    connect(widget_->ui.PresentationList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(UpdateCurrentPresentation(QListWidgetItem*, QListWidgetItem*)));
    connect(widget_->ui.SceneList, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), this, SLOT(UpdateCurrentScene(QListWidgetItem*, QListWidgetItem*)));
    connect(widget_->ui.buttonRename, SIGNAL(clicked()), this, SLOT(OnRenameCurrentPresentation()));
    connect(widget_->ui.buttonRemove, SIGNAL(clicked()), this, SLOT(OnRemoveCurrentPresentation()));
    connect(plugin_, SIGNAL(SceneResized(const QRectF&)), this, SLOT(OnSceneResized(const QRectF&)), Qt::UniqueConnection);

    widget_->ui.helpText->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);
    widget_->ui.helpText->setOpenExternalLinks(true);
    
    // Frame
    connect(framework_->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
    
    // Http responses
    connect(plugin_->Backend(), SIGNAL(Response(const QUrl&, const QByteArray&, int, const QString&)), this, SLOT(OnBackendReply(const QUrl&, const QByteArray&, int, const QString&)), Qt::QueuedConnection);
    
    // Key input
    connect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)), Qt::UniqueConnection);

    QTimer::singleShot(300, this, SLOT(DelayedShow()));
}

void RocketPresis::DelayedShow()
{
    if (!widget_ || widget_->isVisible())
        return;
        
    widget_->show();

    // Initial show animation
    QRectF rect = framework_->Ui()->GraphicsScene()->sceneRect();
    QRectF targetRect = rect;
    targetRect.setWidth(targetRect.width() - 220);

    QRectF widgetRect;
    widgetRect.setHeight(targetRect.height());
    if (targetRect.width() >= 960)
    {
        widgetRect.setTopLeft(QPointF(targetRect.center().x()-480, 0));
        widgetRect.setWidth(960);
    }
    else
    {
        widgetRect.setTopLeft(QPointF(9, 0));
        widgetRect.setWidth(targetRect.width() - 18);

        QString style = widget_->ui.NewPresentationButton->styleSheet();
        if (rect.width() < 950)
        {
            if (style.indexOf("background-image: url(':/images/logo-presis.png');") != -1)
            {
                style = style.replace("background-image: url(':/images/logo-presis.png');", "background-image: none;");
                style = style.replace("padding-left: 368px;", "padding-left: 10px;");
                widget_->ui.NewPresentationButton->setStyleSheet(style);
            }
        }
        else
        {
            if (style.indexOf("background-image: none;") != -1)
            {
                style = style.replace("background-image: none;", "background-image: url(':/images/logo-presis.png');");
                style = style.replace("padding-left: 10px;", "padding-left: 368px;");
                widget_->ui.NewPresentationButton->setStyleSheet(style);
            }
        }
    }

    widget_->hide();
    widget_->Animate(widgetRect.size(), widgetRect.topLeft(), -1.0, rect, RocketAnimations::AnimateUp);
}

void RocketPresis::Close(bool restoreMainView)
{
    actPresentation->setText("Meshmoon Presis");
    
    // Reset scene and unload all used assets.
    bool sceneLoaded = data_.IsValid();
    ResetScene();
    if (sceneLoaded)
        framework_->Asset()->ForgetAllAssets();
    
    if (statusAnim_)
        statusAnim_->stop();
    statusAnim_ = 0;
    
    bool hadWidgets = false;
    if (widget_)
    {
        hadWidgets = true;
        widget_->hide();
        framework_->Ui()->GraphicsScene()->removeItem(widget_);
        SAFE_DELETE(widget_);
    }
    if (controlsWidget_)
    {
        hadWidgets = true;
        controlsWidget_->hide();
        framework_->Ui()->GraphicsScene()->removeItem(controlsWidget_);
        SAFE_DELETE(controlsWidget_);
    }

    if (plugin_->Backend())
        disconnect(plugin_->Backend(), SIGNAL(Response(const QUrl&, const QByteArray&, int, const QString&)), this, SLOT(OnBackendReply(const QUrl&, const QByteArray&, int, const QString&)));
    disconnect(framework_->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPress(KeyEvent*)));
    disconnect(framework_->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
    disconnect(plugin_, SIGNAL(SceneResized(const QRectF&)), this, SLOT(OnSceneResized(const QRectF&)));
    
#ifdef Q_WS_WIN
    if (framework_->Ui()->MainWindow()->menuBar() && !framework_->Ui()->MainWindow()->menuBar()->isVisible())
        framework_->Ui()->MainWindow()->menuBar()->show();
#endif

    if (restoreMainView && hadWidgets)
        plugin_->Show(true, 1);
}

void RocketPresis::OnKeyPress(KeyEvent *e)
{
    if (!e || e->IsRepeat() || e->eventType != KeyEvent::KeyPressed)
        return;
        
    if (e->keyCode == Qt::Key_Left)
    {
        if (controlsWidget_->ui.startButton->isVisible())
            ShowControls();
        else
            OnPrevSlide();
    }
    else if (e->keyCode == Qt::Key_Right)
    {
        if (controlsWidget_->ui.startButton->isVisible())
            ShowControls();
        else
            OnNextSlide();
    }
}

void RocketPresis::SetProgress(const QString &message)
{
    if (!widget_ || !widget_->isVisible())
        return;
        
    widget_->ui.StartButton->hide();
    widget_->ui.ExitButton->hide();
    widget_->ui.frameStatus->show();
    widget_->ui.labelStatus->setText(message);
    
    if (!statusAnim_)
    {
        statusAnim_ = new QMovie(":/images/loader-big.gif", "GIF", widget_->ui.labelStatusAnimation);
        statusAnim_->setScaledSize(widget_->ui.labelStatusAnimation->size());
        statusAnim_->setCacheMode(QMovie::CacheAll);
        widget_->ui.labelStatusAnimation->setMovie(statusAnim_);
    }
    statusAnim_->start();
}

void RocketPresis::StopProgress()
{
    if (!widget_ || !widget_->isVisible())
        return;

    statusAnim_->stop();
    widget_->ui.frameStatus->hide();
    widget_->ui.StartButton->show();
    widget_->ui.ExitButton->show();
}

void RocketPresis::CheckAndDownloadEnvironments()
{
    LogNotImplemented("CheckAndDownloadEnvironments");
}

void RocketPresis::OnBackendReply(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString &error)
{
    LogNotImplemented("OnBackendReply");
}

void RocketPresis::WriteJsonMetaData(const QString &subdir, const QStringList &slides)
{
    LogNotImplemented("WriteJsonMetaData");
}

void RocketPresis::ExecuteJsonQuery()
{     
    LogNotImplemented("ExecuteJsonQuery");
}

void RocketPresis::OnPresentationFileChosen(const QString& filename)
{
    LogNotImplemented("OnPresentationFileChosen"); 
}

bool SortSlideById(const EntityPtr e1, const EntityPtr e2)
{
    QString name1 = e1->Name().toLower();
    QString name2 = e2->Name().toLower();
    
    // Slide*
    if (name1.startsWith("slide") && name2.startsWith("slide"))
    {
        bool ok1 = false, ok2 = false;
        QString id1 = name1.mid(5);
        QString id2 = name2.mid(5);
        int iid1 = id1.toInt(&ok1);
        int iid2 = id2.toInt(&ok2);
        if (ok1 && ok2)
            return (iid1 < iid2);
    }
    // PreviewPosition*
    else if (name1.startsWith("previewPosition") && name2.startsWith("previewPosition"))
    {
        bool ok1 = false, ok2 = false;
        QString id1 = name1.mid(15);
        QString id2 = name2.mid(15);
        int iid1 = id1.toInt(&ok1);
        int iid2 = id2.toInt(&ok2);
        if (ok1 && ok2)
            return (iid1 < iid2);
    }
    
    return (name1.compare(name2, Qt::CaseInsensitive) < 0 ? true : false);
}

void RocketPresis::OnStartPresentation()
{
    LogNotImplemented("OnStartPresentation"); 
}

void RocketPresis::ShowSlide(int slide, int target)
{
    LogNotImplemented("ShowSlide"); 
}

bool RocketPresis::LoadPresentation()
{
    LogNotImplemented("LoadPresentation"); 
    return false;
}

void RocketPresis::ResetScene()
{
    // Get scene name for removal
    QString sceneName = "";
    if (data_.scene)
        sceneName = data_.scene->Name();

    data_.camera.reset();
    data_.Reset();
    
    presentation_.previewTransforms.clear();
    presentation_.slideTransforms.clear();
    presentation_.slideMaterials.clear();
    presentation_.slideMeshes.clear();
    presentation_.slideEntities.clear();

    // Delete our scene
    if (!sceneName.isEmpty() && framework_->Scene()->HasScene(sceneName))
        framework_->Scene()->RemoveScene(sceneName);
    sceneName = identifier_;
    if (framework_->Scene()->HasScene(sceneName))
        framework_->Scene()->RemoveScene(sceneName);

    // Delete storages
    framework_->Asset()->RemoveAssetStorage(identifier_ + "Scenes");
    framework_->Asset()->RemoveAssetStorage(identifier_ + "Slides");
}

void RocketPresis::CreateScene()
{
    // Create scene and entities
    data_.scene = framework_->Scene()->CreateScene(identifier_, true, true);
}

void RocketPresis::LoadScene()
{
    LogNotImplemented("LoadScene"); 
}

RocketPresis::Data::Data()
{
    Reset();
}

void RocketPresis::Data::Reset()
{
    if (scene.get())
        scene.reset();
    if (camera.get())
        camera.reset();
        
    isSceneFullyLoaded = false;
}

bool RocketPresis::Data::IsValid()
{
    if(!scene.get() || !camera.get())
        return false;
    return true; 
}

bool RocketPresis::Data::IsSceneValid()
{
    if(!scene.get())
        return false;
    return true;
}

bool RocketPresis::CheckLoadingState()
{
    if (!data_.isSceneFullyLoaded && framework_->Asset()->NumCurrentTransfers() == 0)
    {
        data_.isSceneFullyLoaded = true;
        
        StopProgress();
        plugin_->Hide();
        if (widget_)
            widget_->hide();
#ifdef Q_WS_WIN
        if (framework_->Ui()->MainWindow()->menuBar())
            framework_->Ui()->MainWindow()->menuBar()->hide();
#endif
        QTimer::singleShot(300, this, SLOT(ShowStart()));
    }
    return data_.isSceneFullyLoaded;
}

void RocketPresis::CalcCameraTransform(int slide)
{
    presentation_.transitioningChanged = true;
    presentation_.elapsedTime = 0.0f;
    presentation_.goToSplit = false;
    
    if (slide >= presentation_.slideTransforms.size())
        return;

    EC_Placeable *temp = presentation_.slideTransforms[slide].get();
    if (!temp)
        return;
    
    // Camera position
    float camDist = 6.0f;
    float3 cameraPosition = temp->Position() + temp->WorldOrientation().Mul(float3(camDist, 0, 0));
    
    presentation_.start = data_.camera->GetComponent<EC_Placeable>()->transform.Get();
        
    Transform transform;
    transform.pos = cameraPosition;
    
    float4x4 rot = float4x4::LookAt(data_.scene->ForwardVector(), (temp->Position() - cameraPosition).Normalized(), data_.scene->UpVector(), data_.scene->UpVector());
   
    float angle = RadToDeg(rot.RotatePart().ToEulerYXZ().x);
    
    if (angle < -180)
        angle += 180;
    else if (angle > 180)
        angle -= 180;
   
    transform.rot = float3(0.0f, angle, 0.0f);    
    presentation_.target = transform;
    
    // If we collide to something when raycast to next slide, use split target
    OgreWorldPtr ogreWorld = data_.scene->GetWorld<OgreWorld>();
    if(ogreWorld)
    {
        float3 splitPos = float3::zero;
        float3 splitPos2 = float3::zero;
    
        float3 camPos = data_.camera->GetComponent<EC_Placeable>()->transform.Get().pos;
        
        if(presentation_.currentEntityTransform.get())
        {
            if(camPos.Distance(temp->Position()) > presentation_.currentEntityTransform->Position().Distance(temp->Position()))
            {
                // Go to split
                presentation_.goToSplit = true;

                float height = presentation_.slideEntities[0]->GetComponent<EC_Mesh>()->WorldAABB().Size().y;
                splitPos = presentation_.currentEntityTransform->Position() + float3(0, height, 0);
                
                if(splitPos.Distance(presentation_.target.pos) > splitPos.Distance(temp->Position()))
                    splitPos2 = temp->Position() + float3(0, height, 0);
                else
                    splitPos2 = splitPos.Lerp(presentation_.target.pos, 0.5f);
                
                curve_.Clear();
                curve_.AddPoint(camPos); // Starting point
                curve_.AddPoint(splitPos); // First split position
                curve_.AddPoint(splitPos2); // Second split position
                curve_.AddPoint(presentation_.target.pos); // End point
            }
            else if(camPos.Distance(temp->Position()) < camPos.Distance(cameraPosition))
            {
                presentation_.goToSplit = true;
            
                float height = presentation_.slideEntities[0]->GetComponent<EC_Mesh>()->WorldAABB().Size().y;
                splitPos2 = temp->Position() + float3(0, height, 0);
                splitPos = splitPos2.Lerp(camPos, 0.5);
            
                curve_.Clear();
                curve_.AddPoint(camPos);
                curve_.AddPoint(splitPos);
                curve_.AddPoint(splitPos2);
                curve_.AddPoint(presentation_.target.pos);
            }
        }
    }

    // Set current entity
    presentation_.currentEntity = presentation_.slideEntities[slide];
    presentation_.currentEntityTransform = presentation_.slideEntities[slide]->GetComponent<EC_Placeable>();
}

void RocketPresis::OnNextSlide()
{
    if (!data_.IsValid())
        return;

    float3 camTransform = data_.camera->GetComponent<EC_Placeable>()->Position();
    presentation_.currentDirection = Presentation::Forward;

    if (presentation_.currentSlide == -1)
    {
        presentation_.slideTime = 0.0f;
        presentation_.totalElapsedTime = 0.0f;
    }

    if (presentation_.started && presentation_.currentSlide > -1 && presentation_.slideImages[presentation_.currentSlide][1] == (presentation_.slides.size() - 1))
    {
        GoToPreviewPosition();
        return;
    }

    UpdateSlideTextures();

    presentation_.currentSlide++;
    presentation_.currentSlideTotal++;
    if (presentation_.currentSlide > (presentation_.slideMeshes.size() - 1))
        presentation_.currentSlide = 0;
        
    CalcCameraTransform(presentation_.currentSlide);
        
    presentation_.transitioning = true;
    presentation_.started = true;
    presentation_.updated = false;
    presentation_.goToPreview = false;
        
    controlsWidget_->ui.labelSlide->setText(QString("%1 of %2").arg(presentation_.currentSlideTotal).arg(presentation_.slides.size()));
}

void RocketPresis::OnPrevSlide()
{
    if (!data_.IsValid())
        return;

    float3 camTransform = data_.camera->GetComponent<EC_Placeable>()->Position();
    presentation_.currentDirection = Presentation::Backward;

    if (presentation_.currentSlide == -1)
    {
        
        presentation_.currentSlide = presentation_.slideMeshes.size() - 1;
        presentation_.slideImages[presentation_.currentSlide][1] = -1;
        presentation_.slideTime = 0.0f;
        presentation_.totalElapsedTime = 0.0f;
    }

    if (presentation_.slideImages[presentation_.currentSlide][1] == 0)
    {
        GoToPreviewPosition();     
        return;
    }

    UpdateSlideTextures();

    presentation_.currentSlide--;
    if(presentation_.currentSlide < 0)
        presentation_.currentSlide = presentation_.slideMeshes.size() - 1;

    presentation_.currentSlideTotal--;
    if (presentation_.currentSlideTotal < 0)
        presentation_.currentSlideTotal = presentation_.slides.size();
        
    CalcCameraTransform(presentation_.currentSlide);
    
    presentation_.transitioning = true;
    presentation_.started = true;
    presentation_.updated = false;
    presentation_.goToPreview = false;
    
    controlsWidget_->ui.labelSlide->setText(QString("%1 of %2").arg(presentation_.currentSlideTotal).arg(presentation_.slides.size()));
}

void RocketPresis::OnUpdate(float frameTime)
{
    PROFILE(Rocket_Presis_Update)

    if(!data_.IsValid())
        return;
    if(!CheckLoadingState())
        return;
        
    presentation_.totalElapsedTime += frameTime;
    if (presentation_.elapsedTime > 1.0f)
        presentation_.elapsedTime = 1.0f;
           
    // Set timescale for animation speed
    float frametime = frameTime * (1.0f / settings_.timeScale);
    if(!data_.camera->GetComponent<EC_Placeable>().get())
        return;
    
    if (!presentation_.started && !presentation_.goToPreview && settings_.autoPreview)
    {
        if (presentation_.slideTime > settings_.previewTime)
            OnNextSlide();
        presentation_.slideTime += frameTime;
    }
    
    // Check if we are in preview mode and update
    if (!presentation_.started && !presentation_.goToPreview)
    {
        UpdatePreview();
        presentation_.previewTime += (frameTime * presentation_.previewSpeedScalar); // Scale previewtime to correct
        return;
    }
    
    // Apply easing curve
    float elapsedEased = easing_.valueForProgress(presentation_.elapsedTime);
    if (elapsedEased > 1.0f)
        elapsedEased = 1.0f;
            
    // Split aka "jumps"
    Transform camTransform = data_.camera->GetComponent<EC_Placeable>()->transform.Get();
    if (presentation_.goToSplit)
    {
        camTransform.pos = curve_.GetPoint(elapsedEased);
        if (camTransform.pos == presentation_.target.pos)
            presentation_.goToSplit = false;
    }
    // Normal progression
    else
        camTransform.pos = float3::Lerp(presentation_.start.pos, presentation_.target.pos, elapsedEased);
     
    // If we are going to preview position
    if(camTransform == presentation_.target && presentation_.goToPreview)
    {
        LoadStartSlides();
        presentation_.slideTime = 0;
        presentation_.start = data_.camera->GetComponent<EC_Placeable>()->transform.Get();
        presentation_.goToPreview = false;
    }
       
    // If we reached our destination, update slides
    if (camTransform.pos.Equals(presentation_.target.pos) && presentation_.started && !presentation_.updated)
    {
        UpdateSlideTextures();
        presentation_.slideTime = 0;
        presentation_.transitioning = false;
        presentation_.updated = true;
    }
    
    if(settings_.automatic && presentation_.started && !presentation_.transitioning)
    {
        if(presentation_.slideTime > settings_.delay)
            OnNextSlide();
    }
       
    camTransform.rot = float3::Lerp(presentation_.start.rot, presentation_.target.rot, elapsedEased);
        
    data_.camera->GetComponent<EC_Placeable>()->transform.Set(camTransform, AttributeChange::LocalOnly);
    
    presentation_.elapsedTime += frametime;
    presentation_.slideTime += frameTime;
    
    int seconds = 0; int minutes = 0;
    GetElapsedTime(minutes, seconds);
    QString minStr = (minutes < 10 ? "0" : "") + QString::number(minutes);
    QString secStr = (seconds < 10 ? "0" : "") + QString::number(seconds);
    controlsWidget_->ui.labelDuration->setText(minStr + ":" + secStr);
}

void RocketPresis::UpdateSlideTextures()
{
    if (presentation_.currentSlide == -1)
    {
        controlsWidget_->ui.labelSlide->setText("");
        controlsWidget_->ui.labelDuration->setText("");
        return;
    }

    if (presentation_.currentDirection == Presentation::Forward)
    {
        int slideToUpdate = presentation_.currentSlide + 1;
        if(slideToUpdate > presentation_.slideEntities.size() - 1)
            slideToUpdate -= presentation_.slideEntities.size();

        // First check if we have already reached the last slide
        if(presentation_.slideImages[slideToUpdate][1] == (presentation_.slides.size() - 1) || presentation_.slideImages[presentation_.currentSlide][1] == (presentation_.slides.size() - 1))
        {
            presentation_.updated = true;
            return;
        }

        // Update slide that is 2 slides ahead
        int slideImage = presentation_.slideImages[presentation_.currentSlide][1] + 1;
        if (slideImage >= presentation_.slides.size())
            slideImage = 0;

        ShowSlide(slideImage, slideToUpdate);

        presentation_.slideImages[slideToUpdate][1] = slideImage;
        presentation_.currentImage++;
    }
    else if(presentation_.currentDirection == Presentation::Backward && presentation_.currentSlide != -1)
    {
        int slideToUpdate = presentation_.currentSlide - 1;
        if (slideToUpdate < 0)
            slideToUpdate += presentation_.slideEntities.size();

        // First check if we have already reached first slide
        // If it's zero don't do anything
        if(presentation_.slideImages[slideToUpdate][1] == 0 || presentation_.slideImages[presentation_.currentSlide][1] == 0)
        {
            presentation_.updated = true;
            return;
        }

        // Update slide that is 2 slides behind
        int slideImage = presentation_.slideImages[presentation_.currentSlide][1] - 1;
        if (slideImage < 0)
            slideImage = presentation_.slides.size() - 1;

        ShowSlide(slideImage, slideToUpdate);
        
        presentation_.slideImages[slideToUpdate][1] = slideImage;
        presentation_.currentImage--;
    }
}

void RocketPresis::UpdatePreview()
{
    if (presentation_.previewTransforms.empty())
        return;
    if (presentation_.currentPreviewPosition >= presentation_.previewTransforms.size())
        return;

    Transform camTransform = data_.camera->GetComponent<EC_Placeable>()->transform.Get();
    
    if(presentation_.previewTime > 1.0f)
        presentation_.previewTime = 1.0f;
    
    camTransform.pos = float3::Lerp(presentation_.start.pos, presentation_.previewTransforms[presentation_.currentPreviewPosition]->transform.Get().pos, presentation_.previewTime);
    camTransform.rot = float3::Lerp(presentation_.start.rot, presentation_.previewTransforms[presentation_.currentPreviewPosition]->transform.Get().rot, presentation_.previewTime);
    
    data_.camera->GetComponent<EC_Placeable>()->transform.Set(camTransform, AttributeChange::LocalOnly);
    
    if(camTransform.pos == presentation_.previewTransforms[presentation_.currentPreviewPosition]->transform.Get().pos)
    {
        presentation_.currentPreviewPosition++;
        if (presentation_.currentPreviewPosition > (presentation_.previewTransforms.size() -  1))
            presentation_.currentPreviewPosition = 0;
            
        presentation_.start = camTransform;
        presentation_.previewTime = 0;
        
        float distance = camTransform.pos.Distance(presentation_.previewTransforms[presentation_.currentPreviewPosition]->transform.Get().pos);
        presentation_.previewSpeedScalar = 1.0 / (distance / presentation_.previewSpeed);
    }
}

void RocketPresis::LoadStartSlides()
{
    /// @todo Hide slides that are over the slide image count to begin with.
    for(int i = 0; i < presentation_.slideMeshes.size(); i++)
    {
        ShowSlide(i, i);

        presentation_.slideImages[i] = new int[2];
        presentation_.slideImages[i][0] = i;
        presentation_.slideImages[i][1] = i;
    }
    presentation_.previewTime = 0;
}

void RocketPresis::GoToPreviewPosition()
{
    if (presentation_.previewTransforms.empty())
        return;

    // Go to preview position
    presentation_.start = data_.camera->GetComponent<EC_Placeable>()->transform.Get();
    presentation_.target = presentation_.previewTransforms[0]->transform.Get();
    presentation_.goToPreview = true;
    presentation_.currentSlide = -1;
    presentation_.currentSlideTotal = 0;

    controlsWidget_->ui.labelSlide->setText("Overview");
    
    // Reset timer
    presentation_.elapsedTime = 0.0f;
    presentation_.goToSplit = false;
    
    presentation_.previewTime = 0;
    presentation_.slideTime = 0;
    
    // Presentation is not started
    presentation_.started = false;
}

void RocketPresis::GetElapsedTime(int &minutes, int &seconds)
{
    minutes = floor(presentation_.totalElapsedTime / 60);
    seconds = floor(presentation_.totalElapsedTime) - (minutes * 60);
}
