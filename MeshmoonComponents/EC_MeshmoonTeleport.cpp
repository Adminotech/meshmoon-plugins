/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "EC_MeshmoonTeleport.h"
#include "MeshmoonComponents.h"

#include "Framework.h"
#include "Scene/Scene.h"
#include "Entity.h"

#include "InputAPI.h"
#include "MouseEvent.h"
#include "UiAPI.h"
#include "UiMainWindow.h"
#include "UiGraphicsView.h"
#include "FrameAPI.h"

#include "SceneInteract.h"
#include "Renderer.h"
#include "OgreWorld.h"

#include "EC_Placeable.h"
#include "EC_Camera.h"

#include "Math/MathFunc.h"
#include "Geometry/Ray.h"

#include "TundraLogicModule.h"
#include "Server.h"

#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QApplication>

#include "MemoryLeakCheck.h"

static const QString TELEPORT_ACTION = "EC_MeshmoonTeleport_Teleport";
static const QString TELEPORT_EXECUTED_ACTION = "EC_MeshmoonTeleport_Executed";

EC_MeshmoonTeleport::EC_MeshmoonTeleport(Scene* scene) :
    IComponent(scene),
    // Attributes
    INIT_ATTRIBUTE_VALUE(triggerEntity, "Trigger Entity", EntityReference()),
    INIT_ATTRIBUTE_VALUE(destinationScene, "Destination Scene", ""),
    INIT_ATTRIBUTE_VALUE(destinationPos, "Destination Pos", float3::zero),
    INIT_ATTRIBUTE_VALUE(destinationRot, "Destination Rot", float3::zero),
    INIT_ATTRIBUTE_VALUE(destinationName, "Destination Name", ""),
    INIT_ATTRIBUTE_VALUE(confirmTeleport, "Confirm Teleportation", true),
    INIT_ATTRIBUTE_VALUE(teleportOnLeftClick, "Teleport On Left Click", false),
    INIT_ATTRIBUTE_VALUE(teleportOnProximity, "Teleport On Proximity", false),
    INIT_ATTRIBUTE_VALUE(teleportProximity, "Teleport Proximity", 5),
    // Members
    proximityDelta_(0.0f),
    proximityShowedDialog_(false),
    proximityIsOn_(false),
    locallyDisabled_(false)
{
    connect(this, SIGNAL(ParentEntitySet()), SLOT(ServerPostInit()));
}

EC_MeshmoonTeleport::~EC_MeshmoonTeleport()
{
    CloseConfirmDialog();
}

void EC_MeshmoonTeleport::AttributesChanged()
{
    if (framework->IsHeadless())
        return;

    // Mouse
    if (teleportOnLeftClick.ValueChanged())
    {
        if (framework->Input() && framework->Input()->TopLevelInputContext())
        {
            if (teleportOnLeftClick.Get())
                connect(framework->Input()->TopLevelInputContext(), SIGNAL(MouseLeftPressed(MouseEvent*)), this, SLOT(OnMouseLeftPressed(MouseEvent*)), Qt::UniqueConnection);
            else
                disconnect(framework->Input()->TopLevelInputContext(), SIGNAL(MouseLeftPressed(MouseEvent*)), this, SLOT(OnMouseLeftPressed(MouseEvent*)));
        }
    }
    // Proximity
    if (teleportOnProximity.ValueChanged())
    {
        if (framework->Frame())
        {
            bool isOn = teleportOnProximity.Get();
            if (isOn)
                connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
            else
                disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
            
            // Kind of a hack, but the server may send true to us multiple times when component is created
            // to a existing entity and below flag gets set to false, which is a problem for the UI under certain conditions.
            if (proximityIsOn_ != isOn)
            {
                proximityIsOn_ = isOn;
                proximityShowedDialog_ = false;
            }
        }
    }
}

void EC_MeshmoonTeleport::TeleportationFailed(const QString &message)
{
    if (!dialog_)
        return;
        
    ui_.labelMessage->setText(message);               
    ui_.frameContent->setStyleSheet("");

    ui_.buttonNo->setText("Close");
    ui_.buttonYes->hide();
    ui_.checkBoxAskAgain->setChecked(false);
    ui_.checkBoxAskAgain->hide();
    ui_.frameButtons->show();
    
    QVariant oldHeightVar = dialog_->property("oldheight");
    if (!oldHeightVar.isNull() && oldHeightVar.toInt() > 0)
        dialog_->setFixedHeight(oldHeightVar.toInt());
    else
        dialog_->setFixedHeight(dialog_->height() + ui_.frameButtons->height());
}

void EC_MeshmoonTeleport::TeleportNow()
{
    if (framework->IsHeadless() || !ParentEntity())
        return;        
    if (locallyDisabled_)
        return;
        
    const float3 &targetPos = destinationPos.Get();
    const float3 &targetRot = destinationRot.Get();

    // Teleport to another Meshmoon world.
    QString dest = destinationScene.Get().trimmed();
    if (!dest.isEmpty())
    {
        MeshmoonComponents *adminoComps = framework->GetModule<MeshmoonComponents>();
        if (adminoComps)
        {           
            if (dialog_)
            {
                ui_.labelMessage->setText(QString("Processing teleportation%1, please wait...").arg(destinationName.Get().trimmed().isEmpty() ? "" : " to " + destinationName.Get().trimmed()));               
                int frameHeight = ui_.frameButtons->height();
                ui_.frameButtons->hide();
                ui_.frameContent->setStyleSheet("QFrame#frameContent { border-bottom-right-radius: 3px; border-bottom-left-radius: 3px; }");
                
                dialog_->setProperty("oldheight", dialog_->height());
                dialog_->setFixedHeight(dialog_->height() - frameHeight);
            }
            
            // float3::zero will be ignored for target pos/rot in the new world.
            QString posStr, rotStr;
            if (!targetPos.IsZero())
                posStr = QString("%1:%2:%3").arg(targetPos.x).arg(targetPos.y).arg(targetPos.z);
            if (!targetRot.IsZero())
                rotStr = QString("%1:%2:%3").arg(targetRot.x).arg(targetRot.y).arg(targetRot.z);
            adminoComps->EmitTeleportRequest(dest, posStr, rotStr);
        }
        else
            CloseConfirmDialog();
    }
    // Teleport inside the current scene.
    else
    {
        CloseConfirmDialog();
        
        Entity *target = TargetEntity();
        if (!target)
            return;

        // Active camera ent or its parent is replicated eg. in 
        // the case of Avatar. The server needs to make the move.
        if (target->IsReplicated())
        {
            ParentEntity()->Exec(EntityAction::Server, TELEPORT_ACTION, QString::number(target->Id()), 
                QString("%1:%2:%3").arg(targetPos.x).arg(targetPos.y).arg(targetPos.z),
                QString("%1:%2:%3").arg(targetRot.x).arg(targetRot.y).arg(targetRot.z));
        }
        // Local target, just move it.
        else
        {
            EC_Placeable *targetP = target->GetComponent<EC_Placeable>().get();
            if (targetP)
            {
                Transform t = targetP->transform.Get();
                t.pos = targetPos;
                t.rot = targetRot;
                targetP->transform.Set(t, AttributeChange::Default);

                // Nofity interested parties locally of the executed teleport.
                QStringList actionParams;
                actionParams << QString::number(ParentEntity()->Id())
                             << QString::number(this->Id())
                             << QString::number(target->Id())
                             << QString("%1:%2:%3").arg(targetPos.x).arg(targetPos.y).arg(targetPos.z)
                             << QString("%1:%2:%3").arg(targetRot.x).arg(targetRot.y).arg(targetRot.z);

                target->Exec(EntityAction::Local, TELEPORT_EXECUTED_ACTION, actionParams);
            }
        }
    }
}

void EC_MeshmoonTeleport::ServerPostInit()
{
    TundraLogic::TundraLogicModule *tundraLogic = (framework ? framework->GetModule<TundraLogic::TundraLogicModule>() : 0);
    if (tundraLogic && tundraLogic->IsServer() && ParentEntity())
        connect(ParentEntity()->Action(TELEPORT_ACTION), SIGNAL(Triggered(QString, QString, QString, QStringList)), 
            this, SLOT(ServerTeleportNow(QString, QString, QString, QStringList)));
}

void EC_MeshmoonTeleport::ServerTeleportNow(QString targetEntId, QString posStr, QString rotStr, QStringList ignore)
{
    if (!ParentScene())
        return;

    bool ok = false;
    uint targetId = targetEntId.toUInt(&ok);
    if (!ok)
        return;
        
    Entity *ent = ParentScene()->EntityById(targetId).get();
    if (!ent)
        return;
    EC_Placeable *p = ent->GetComponent<EC_Placeable>().get();
    if (!p)
        return;
        
    Transform t = p->transform.Get();
    
    // Position
    QStringList posList = posStr.split(":", QString::SkipEmptyParts, Qt::CaseInsensitive);
    if (posList.size() >= 1)
    {
        float x = posList[0].toFloat(&ok);
        if (ok) t.pos.x = x;
    }
    if (posList.size() >= 2)
    {
        float y = posList[1].toFloat(&ok);
        if (ok) t.pos.y = y;
    }
    if (posList.size() >= 3)
    {
        float z = posList[2].toFloat(&ok);
        if (ok) t.pos.z = z;
    }
    
    // Rotation
    QStringList rotList = rotStr.split(":", QString::SkipEmptyParts, Qt::CaseInsensitive);
    if (rotList.size() >= 1)
    {
        float x = rotList[0].toFloat(&ok);
        if (ok) t.rot.x = x;
    }
    if (rotList.size() >= 2)
    {
        float y = rotList[1].toFloat(&ok);
        if (ok) t.rot.y = y;
    }
    if (rotList.size() >= 3)
    {
        float z = rotList[2].toFloat(&ok);
        if (ok) t.rot.z = z;
    }
    
    // Apply teleport to the target transform
    p->transform.Set(t, AttributeChange::Default);

    // Nofity interested parties on the server and clients of the executed teleport.
    QStringList actionParams;
    actionParams << QString::number(ParentEntity()->Id())
                 << QString::number(this->Id())
                 << targetEntId
                 << posStr
                 << rotStr;

    ent->Exec(EntityAction::Server | EntityAction::Peers, TELEPORT_EXECUTED_ACTION, actionParams);
}

void EC_MeshmoonTeleport::TeleportNowWithConfirmation()
{
    if (framework->IsHeadless())
        return;
    if (locallyDisabled_)
        return;
        
    ShowConfirmDialog();
}

void EC_MeshmoonTeleport::DisableLocally()
{    
    disconnect(framework->Input()->TopLevelInputContext(), SIGNAL(MouseLeftPressed(MouseEvent*)), this, SLOT(OnMouseLeftPressed(MouseEvent*)));
    disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
    
    locallyDisabled_ = true;
}

void EC_MeshmoonTeleport::EnableLocally()
{
    if (framework->Input() && framework->Input()->TopLevelInputContext())
    {
        if (teleportOnLeftClick.Get())
            connect(framework->Input()->TopLevelInputContext(), SIGNAL(MouseLeftPressed(MouseEvent*)), this, SLOT(OnMouseLeftPressed(MouseEvent*)), Qt::UniqueConnection);
        else
            disconnect(framework->Input()->TopLevelInputContext(), SIGNAL(MouseLeftPressed(MouseEvent*)), this, SLOT(OnMouseLeftPressed(MouseEvent*)));
    }
    if (framework->Frame())
    {
        if (teleportOnProximity.Get())
            connect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)), Qt::UniqueConnection);
        else
        {
            disconnect(framework->Frame(), SIGNAL(Updated(float)), this, SLOT(OnUpdate(float)));
            proximityShowedDialog_ = false;
        }
    }
    
    locallyDisabled_ = false;
}

void EC_MeshmoonTeleport::OnUpdate(float frametime)
{
    proximityDelta_ += frametime;
    if (proximityDelta_ < 0.5f)
        return;
    proximityDelta_ = 0.0f;

    if (teleportProximity.Get() <= 0 || !teleportOnProximity.Get())
        return;

    /// @todo Should we get distance from the entity that camera is 
    /// parented to eg. the Avatar instead of the camera ent itself
    Entity *trigger = TriggerEntity();
    if (!trigger)
        return;
    OgreWorldPtr world = ParentScene()->GetWorld<OgreWorld>();
    if (!world)
        return;
    Entity *mainCamera = world->Renderer()->MainCamera();
    if (!mainCamera)
        return;
        
    EC_Placeable *p1 = trigger->GetComponent<EC_Placeable>().get();
    EC_Placeable *p2 = mainCamera->GetComponent<EC_Placeable>().get();
    if (!p1 || !p2)
        return;

    // Go one chain down in parenting, fixes eg. camera being parented to avatar.
    // In this case we want to get the world pos of the avatar, not the camera itself.
    if (p2->ParentPlaceableComponent() != 0)
        p2 = p2->ParentPlaceableComponent();
        
    uint distance = static_cast<uint>(FloorInt(p1->WorldPosition().Distance(p2->WorldPosition())));
    if (distance <= teleportProximity.Get())
    {
        if (!confirmTeleport.Get())
            TeleportNow();
        else if (!proximityShowedDialog_)
        {
            // Don't spam the dialog. To see it again user must 
            // go out of the proximity distance and come back.
            TeleportNowWithConfirmation();
            if (!dialog_.isNull())
                proximityShowedDialog_ = true;
        }
    }
    else if (proximityShowedDialog_)
        proximityShowedDialog_ = false;
}

void EC_MeshmoonTeleport::OnMouseLeftPressed(MouseEvent *mouseEvent)
{
    if (dialog_)
        return;

    Entity *trigger = TriggerEntity();
    if (!trigger)
        return;
    OgreWorldPtr world = ParentScene()->GetWorld<OgreWorld>();
    if (!world)
        return;
    EC_Camera *mainCamera = world->Renderer()->MainCameraComponent();
    if (!mainCamera)
        return;
    if (framework->Ui()->GraphicsView()->GetVisibleItemAtCoords(mouseEvent->x, mouseEvent->y) != 0)
        return;

    float x = (float)mouseEvent->x / (float)world->Renderer()->WindowWidth();
    float y = (float)mouseEvent->y / (float)world->Renderer()->WindowHeight();

    Ray mouseRay = mainCamera->ViewportPointToRay(x, y);

    RaycastResult *result = 0;
    SceneInteract *sceneInteract = framework->GetModule<SceneInteract>();
    if (sceneInteract) /// @todo Only use common raycast is targets placeable is in layer 0?
        result = sceneInteract->CurrentMouseRaycastResult();
    else
        result = world->Raycast(mouseRay, 0xffffffff); /// @todo Get layer from targets placeable?
        
    if (result && result->entity && result->entity == trigger)
    {
        if (!confirmTeleport.Get())
            TeleportNow();
        else
            TeleportNowWithConfirmation();

        mouseEvent->Suppress();
    }
}

void EC_MeshmoonTeleport::OnKeyPressed(KeyEvent *keyEvent)
{
    if (dialog_ && keyEvent->keyCode == Qt::Key_Escape)
    {
        CloseConfirmDialog();
        keyEvent->Suppress();
    }
}

void EC_MeshmoonTeleport::ShowConfirmDialog()
{
    if (framework->IsHeadless())
        return;
    if (locallyDisabled_)
        return;
    if (!ParentScene())
        return;

    // Dialog already showing
    if (!dialog_.isNull())
        return;
        
    // Check that no other EC_MeshmoonTeleport is showing a dialog
    std::vector<shared_ptr<EC_MeshmoonTeleport> > teleports = ParentScene()->Components<EC_MeshmoonTeleport>();
    for(size_t i = 0; i < teleports.size(); ++i)
    {
        if (teleports[i]->ParentEntity() == ParentEntity())
            continue;
        if (teleports[i]->IsShowingDialog())
            return;
    }

    dialog_ = new QWidget();
    ui_.setupUi(dialog_);
    
    ui_.labelMessage->setText(QString("Do you want to launch teleport%1?").arg(destinationName.Get().trimmed().isEmpty() ? "" : " to " + destinationName.Get().trimmed()));

    connect(ui_.buttonYes, SIGNAL(clicked()), this, SLOT(TeleportNow()), Qt::QueuedConnection);
    connect(ui_.buttonNo, SIGNAL(clicked()), this, SLOT(CloseConfirmDialog()), Qt::QueuedConnection);
    connect(ui_.checkBoxAskAgain, SIGNAL(toggled(bool)), this, SLOT(SetDisableLocally(bool)));

    QGraphicsProxyWidget *proxy = framework->Ui()->GraphicsScene()->addWidget(dialog_, Qt::Widget);
    proxy->setPos(framework->Ui()->GraphicsScene()->sceneRect().center().x() - (dialog_->width()/2.0), 0);
    proxy->setZValue(1000);
    proxy->show();
    
    connect(framework->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnSceneRectChanged(const QRectF&)), Qt::UniqueConnection);
    connect(framework->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPressed(KeyEvent*)), Qt::UniqueConnection);

    if (framework->Input()->TopLevelInputContext())
        framework->Input()->TopLevelInputContext()->ReleaseAllKeys();

    framework->Input()->SceneReleaseAllKeys();
    framework->Input()->SceneReleaseMouseButtons();
    framework->Input()->SetMouseCursorVisible(true);
}

void EC_MeshmoonTeleport::CloseConfirmDialog()
{
    if (!dialog_)
        return;

    disconnect(framework->Ui()->GraphicsScene(), SIGNAL(sceneRectChanged(const QRectF&)), this, SLOT(OnSceneRectChanged(const QRectF&)));
    disconnect(framework->Input()->TopLevelInputContext(), SIGNAL(KeyPressed(KeyEvent*)), this, SLOT(OnKeyPressed(KeyEvent*)));
    
    if (locallyDisabled_)
        DisableLocally();
    else
        EnableLocally();
        
    QGraphicsProxyWidget *proxy = dialog_->graphicsProxyWidget();
    if (proxy && framework->Ui()->GraphicsScene())
        framework->Ui()->GraphicsScene()->removeItem(proxy);
        
    delete dialog_.data();
    dialog_ = 0;
    proxy = 0;
}

void EC_MeshmoonTeleport::OnSceneRectChanged(const QRectF &rect)
{
    if (dialog_ && dialog_->graphicsProxyWidget())
        dialog_->graphicsProxyWidget()->setPos(rect.center().x() - (dialog_->width()/2.0), 0);
}
void EC_MeshmoonTeleport::SetDisableLocally(bool disabled)
{
    locallyDisabled_ = disabled;
    if (dialog_ && ui_.buttonYes)
        ui_.buttonYes->setDisabled(disabled);
}

Entity *EC_MeshmoonTeleport::TriggerEntity() const
{
    if (!ParentScene())
        return 0;
    const EntityReference &triggerEnt = triggerEntity.Get();
    if (!triggerEnt.ref.trimmed().isEmpty() && !triggerEnt.IsEmpty())
        return triggerEnt.Lookup(ParentScene()).get();
    return ParentEntity();
}

Entity *EC_MeshmoonTeleport::TargetEntity() const
{
    if (!ParentScene())
        return 0;
    OgreWorldPtr world = ParentScene()->GetWorld<OgreWorld>();
    if (!world)
        return 0;
    Entity *mainCamera = world->Renderer()->MainCamera();
    if (!mainCamera)
        return 0;
    EC_Placeable *p = mainCamera->GetComponent<EC_Placeable>().get();

    // Look for the last parent that does not have a parent!
    Entity *mainCameraParent = 0;
    while (true)
    {
        if (!p)
            break;
        Entity *parent = p->ParentPlaceableEntity();
        if (!parent)
            break;
        mainCameraParent = parent;
        p = p->ParentPlaceableComponent();
    }
    return mainCameraParent ? mainCameraParent : mainCamera;
}

void EC_MeshmoonTeleport::IgnoreCurrentProximity()
{
    proximityShowedDialog_ = true;
}
