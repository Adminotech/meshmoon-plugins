/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketLayersWidget.h"
#include "RocketTaskbar.h"
#include "RocketNetworking.h"
#include "RocketPlugin.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "UiAPI.h"
#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAssetTransfer.h"
#include "Client.h"

#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QAction>
#include <QFont>

#include "MemoryLeakCheck.h"

// RocketLayersWidget

RocketLayersWidget::RocketLayersWidget(RocketPlugin *plugin) :
    RocketSceneWidget(plugin->GetFramework()),
    plugin_(plugin),
    layersToggleAction_(0)
{
    ui_.setupUi(widget_);
    connect(ui_.buttonClose, SIGNAL(clicked()), SLOT(ToggleVisibility()));
    connect(ui_.hideAllButton, SIGNAL(clicked()), SLOT(HideAllLayers()));
    connect(ui_.downloadAllButton, SIGNAL(clicked()), SLOT(DownloadAllLayers()));

    hide();
}

RocketLayersWidget::~RocketLayersWidget()
{
    if (scene())
        framework_->Ui()->GraphicsScene()->removeItem(this);
}

void RocketLayersWidget::UpdateTaskbarEntry(RocketTaskbar *taskbar, bool hasLayers)
{
    if (!taskbar)
    {
        SAFE_DELETE(layersToggleAction_);
        return;
    }
        
    // Remove layers toggle action if no layers.
    if (!hasLayers && layersToggleAction_ && taskbar)
    {
        SAFE_DELETE(layersToggleAction_);
        taskbar->UpdateGeometry();
    }
    // Add if does not exists and we have layers.
    else if (hasLayers && !layersToggleAction_ && taskbar)
    {
        layersToggleAction_ = new QAction(QIcon(":/images/icon-layers-small.png"), tr("Manage Layers"), 0);
        connect(layersToggleAction_, SIGNAL(triggered()), this, SLOT(ToggleVisibility()));
        taskbar->AddAction(layersToggleAction_);
    }
}

void RocketLayersWidget::RemoveLayer(const Meshmoon::SceneLayer &layer)
{
    RocketLayerElementWidget *element = 0;
    int elementIndex = -1;
    for(int i=0; i<elements_.size(); ++i)
    {
        if (elements_[i]->id == layer.id)
        {
            elementIndex = i;
            element = elements_[i];
            break;
        }
    }
    if (element)
    {
        for(int i=0; i<ui_.layersLayout->count(); ++i)
        {
            if (ui_.layersLayout->itemAt(i)->widget() == element)
            {
                ui_.layersLayout->removeItem(ui_.layersLayout->itemAt(i));
                elements_.removeAt(elementIndex);
                SAFE_DELETE(element);
                break;
            }
        }
    }
}

void RocketLayersWidget::UpdateState(const Meshmoon::SceneLayerList &layers)
{
    for (int i=0; i<layers.size(); ++i)
    {
        const Meshmoon::SceneLayer &layer = layers[i];

        RocketLayerElementWidget *element = 0;
        foreach(RocketLayerElementWidget *e, elements_)
        {
            if (e->id == layer.id)
            {
                element = e;
                break;
            }
        }

        if (!element)
        {
            element = new RocketLayerElementWidget(framework_, layer.id, layer.visible, layer.name, layer.iconUrl);
            connect(element, SIGNAL(VisibilityChanged(uint, bool)), this, SLOT(OnVisibilityChangeRequest(uint, bool)));

            ui_.layersLayout->insertWidget(ui_.layersLayout->count()-1, element);
            elements_ << element;
        }
        else
            element->UpdateState(layer);
    }
}

void RocketLayersWidget::ClearLayers()
{
    while(!elements_.isEmpty())
    {
        RocketLayerElementWidget *element = elements_.takeFirst();
        if (!element)
            break;

        for(int i=0; i<ui_.layersLayout->count(); ++i)
        {
            if (ui_.layersLayout->itemAt(i)->widget() == element)
            {
                ui_.layersLayout->removeItem(ui_.layersLayout->itemAt(i));
                SAFE_DELETE(element);
                break;
            }
        }
    }
}

void RocketLayersWidget::ToggleVisibility()
{
    if (!isVisible())
        Show();
    else
        Hide();
}

void RocketLayersWidget::OnWindowResized(const QRectF &rect)
{
    setPos(QPointF(50, 50));
    resize(QSizeF(size().width(), rect.height()-100));
}

void RocketLayersWidget::OnVisibilityChangeRequest(uint layerId, bool visible)
{
    if (plugin_ && plugin_->Networking())
        plugin_->Networking()->SendLayerUpdateMsg(static_cast<u32>(layerId), visible);
}

void RocketLayersWidget::HideAllLayers()
{
    foreach(RocketLayerElementWidget *e, elements_)
    {
        if (e->visible)
            e->UpdateState(false, true);
    }
}

void RocketLayersWidget::DownloadAllLayers()
{
    foreach(RocketLayerElementWidget *e, elements_)
    {
        if (!e->visible)
            e->UpdateState(true, true);
    }
}

void RocketLayersWidget::Show()
{
    if (!scene())
    {
        framework_->Ui()->GraphicsScene()->addItem(this);
        if (!scene())
            return;
    }
    connect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnWindowResized(const QRectF&)), Qt::UniqueConnection);
        
    QRectF rect = scene()->sceneRect();
    Animate(QSizeF(size().width(), rect.height()-100), QPointF(50, 50), -1.0, rect, RocketAnimations::AnimateFade);   
}

void RocketLayersWidget::Hide(RocketAnimations::Animation /*anim*/, const QRectF & /*sceneRect*/)
{
    if (!scene())
        return;
    disconnect(framework_->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnWindowResized(const QRectF&)));
    // anim function parameter ignored on purpose
    RocketSceneWidget::Hide(RocketAnimations::AnimateFade, QRectF());
}

// RocketLayerElementWidget

RocketLayerElementWidget::RocketLayerElementWidget(Framework *framework, uint id_, bool visible_, const QString &name, const QString &iconUrl) :
    framework_(framework),
    id(id_),
    visible(visible_)
{
    QString hover = "QPushButton::hover { color: white; border: 1px solid rgb(104, 104, 104); }";
    styleChecked_   = QString("QPushButton { font-family: 'Arial'; font-size: 19px; min-height: 35px; max-height: 35px; border: 1px solid rgba(104, 104, 104, 150); border-radius: 3px; color: rgb(245, 245, 245); \
        background-color: qlineargradient(spread:repeat, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(56, 167, 39, 255), stop:1 rgba(16, 61, 20, 255)); } %1").arg(hover);
    styleUnchecked_ = QString("QPushButton { font-family: 'Arial'; font-size: 19px; min-height: 35px; max-height: 35px; border: 1px solid rgba(104, 104, 104, 150); border-radius: 3px; color: rgb(245, 245, 245); \
        background-color: qlineargradient(spread:repeat, x1:0, y1:0, x2:0, y2:1, stop:0 rgba(203, 186, 47, 255), stop:1 rgba(124, 108, 21, 255)); } %1").arg(hover);

    setLayout(new QHBoxLayout());
    
    QLabel *nameLabel = new QLabel(name, this);
    QFont font = nameLabel->font();
    font.setBold(true);
    nameLabel->setFont(font);
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    
    iconLabel_ = new QLabel(this);
    iconLabel_->setFixedSize(40, 40);
    iconLabel_->setStyleSheet("QLabel { border: 1px solid rgba(104, 104, 104, 150); }");
    
    buttonVisibility_ = new QPushButton(this);
    buttonVisibility_->setCheckable(true);
    buttonVisibility_->setChecked(visible);
    buttonVisibility_->setText((visible ? "Visible" : "Request Layer"));
    buttonVisibility_->setToolTip((visible ? "Click to remove layer" : "Click to download layer"));
    buttonVisibility_->setStyleSheet(buttonVisibility_->isChecked() ? styleChecked_ : styleUnchecked_);
    buttonVisibility_->setMinimumWidth(200);
    
    layout()->setContentsMargins(0, 3, 0, 3);
    layout()->setSpacing(6);
    
    layout()->addWidget(iconLabel_);
    layout()->addWidget(buttonVisibility_);
    layout()->addWidget(nameLabel);
    
    connect(buttonVisibility_, SIGNAL(toggled(bool)), SLOT(OnVisibilityButtonPressed(bool)));
    
    // Download icon
    if (!iconUrl.isEmpty())
    {
        AssetTransferPtr iconTransfer = framework->Asset()->RequestAsset(iconUrl, "Binary");
        connect(iconTransfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnIconDownloadCompleted(AssetPtr)));
    }
    else
        iconLabel_->setPixmap(QPixmap(":/images/server-anon.png").scaled(iconLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

RocketLayerElementWidget::~RocketLayerElementWidget()
{
}

void RocketLayerElementWidget::OnIconDownloadCompleted(AssetPtr asset)
{
    framework_->Asset()->ForgetAsset(asset, false);
    QString path = asset->DiskSource();
    if (!path.isEmpty() && QFile::exists(path))
    {
        QPixmap loadedIcon(path);
        if (loadedIcon.width() > iconLabel_->width() || loadedIcon.height() > iconLabel_->size().height())
            loadedIcon = loadedIcon.scaled(iconLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!loadedIcon.isNull())
            iconLabel_->setPixmap(loadedIcon);
    }
}

void RocketLayerElementWidget::OnVisibilityButtonPressed(bool checked)
{
    UpdateState(checked, true);
}

void RocketLayerElementWidget::UpdateState(const Meshmoon::SceneLayer &layer)
{
    // Only update the internal boolean in this function when data is coming from the server.
    this->visible = layer.visible;
    UpdateState(layer.visible, false);
}

void RocketLayerElementWidget::UpdateState(bool visible, bool emitSignal)
{
    buttonVisibility_->blockSignals(true);
    buttonVisibility_->setChecked(visible);
    buttonVisibility_->setText(visible ? "Visible" : "Download Layer");
    buttonVisibility_->setToolTip(visible ? "Click to remove layer" : "Click to download layer");
    buttonVisibility_->setStyleSheet(visible ? styleChecked_ : styleUnchecked_);
    buttonVisibility_->blockSignals(false);
    
    if (emitSignal)
        emit VisibilityChanged(id, visible);
}
