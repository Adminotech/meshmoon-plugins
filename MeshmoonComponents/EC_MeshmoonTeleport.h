/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonComponentsApi.h"
#include "IComponent.h"
#include "SceneFwd.h"
#include "InputFwd.h"
#include "EntityReference.h"

#include "Math/float3.h"

#include "ui_EC_MeshmoonTeleport.h"

#include <QWidget>
#include <QPointer>
#include <QBrush>
#include <QString>
#include <QStringList>

/// Makes entity a teleport.
/** This component can be used to teleport the active camera or, if parented, its parent entity
    to a new position in the current scene or in another Meshmoon world. This teleport can be
    triggered from a script via TeleportNow, TeleportNowWithConfirmation. with mouse click*/
class MESHMOON_COMPONENTS_API EC_MeshmoonTeleport : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonTeleport", 503) // Note this is the closed source EC Meshmoon range ID.

public:
    /// Trigger entity for left mouse clicks and proximity. Leave empty for parent entity. Default: empty ref (parent entity).
    Q_PROPERTY(EntityReference triggerEntity READ gettriggerEntity WRITE settriggerEntity);
    DEFINE_QPROPERTY_ATTRIBUTE(EntityReference, triggerEntity);

    /// Define Meshmoon world ID or txml url if you want to teleport to another Meshmoon world. Default: empty string.
    /** @note Leave empty if the target scene is the current parent scene. */
    Q_PROPERTY(QString destinationScene READ getdestinationScene WRITE setdestinationScene);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, destinationScene);

    /// Teleport destination position. Default: float3::zero.
    /** @note If you are teleporting to another Meshmoon world and the value is not float3::zero,
        this position will be applied after login to the avatar entity. */
    Q_PROPERTY(float3 destinationPos READ getdestinationPos WRITE setdestinationPos);
    DEFINE_QPROPERTY_ATTRIBUTE(float3, destinationPos);
    
    /// Teleport destination rotation. Default: float3::zero.
    /** @note If you are teleporting to another Meshmoon world and the value is not float3::zero,
        this rotation will be applied after login to the avatar entity. */
    Q_PROPERTY(float3 destinationRot READ getdestinationRot WRITE setdestinationRot);
    DEFINE_QPROPERTY_ATTRIBUTE(float3, destinationRot);
    
    /// Teleport destination name that is shown in the user interface. Default: empty string.
    Q_PROPERTY(QString destinationName READ getdestinationName WRITE setdestinationName);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, destinationName);

    /// Confirm teleportation from user with a user interface dialog. Default: true.
    Q_PROPERTY(bool confirmTeleport READ getconfirmTeleport WRITE setconfirmTeleport);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, confirmTeleport);
    
    /// Teleport when triggerEntity is mouse left clicked. Default: false.
    Q_PROPERTY(bool teleportOnLeftClick READ getteleportOnLeftClick WRITE setteleportOnLeftClick);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, teleportOnLeftClick);
    
    /// Teleport when active camera is inside the teleportProximity from triggerEntity. Default: false.
    Q_PROPERTY(bool teleportOnProximity READ getteleportOnProximity WRITE setteleportOnProximity);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, teleportOnProximity);
    
    /// Teleport proximity distance for teleportOnProximity. Default: 5.
    /** @note 0 disables proximity triggering. */
    Q_PROPERTY(uint teleportProximity READ getteleportProximity WRITE setteleportProximity);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, teleportProximity);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonTeleport(Scene* scene);
    /// @endcond
    virtual ~EC_MeshmoonTeleport();
    
    /// Shows the given error message and prepares the dialog so it can be close by the user.
    void TeleportationFailed(const QString &message);
   
public slots:   
    /// Teleports right away to the destination.
    /** @note This function will not respect the current confirmTeleport, triggerEntity and teleportProximity values. */
    void TeleportNow();

    /// Teleports right away to the destination with a user interface dialog confirmation.
    /** @note This function will not respect the current confirmTeleport, triggerEntity and teleportProximity values. */
    void TeleportNowWithConfirmation();
    
    /// Returns if this component is showing dialog
    bool IsShowingDialog() const { return !dialog_.isNull(); }
    
    /// This will disable/paralyze the component locally.
    /** Used internally when user selects 'No, don't ask me again' to user interface dialog. */
    void DisableLocally();
    
    /// Opposite of DisableLocally.
    void EnableLocally();
    
    /// Is locally disabled.
    bool IsDisabledLocally() const { return locallyDisabled_; }
    
    /// Returns if this teleport is triggered based on proximity.
    bool IsTriggeredWithProximity() const { return (teleportOnProximity.Get() && teleportProximity.Get() > 0); }

    /// Returns if the target scene of this teleport is the parent scene.
    bool IsParentSceneTeleport() const { return destinationScene.Get().isEmpty(); }
    
    /** Make the component ignore proximity triggering inspection if the active
        camera is inside the range now. The confirmation dialog will be shown
        once the camera moves out of the proximity radius and goes back in again.
        @note This is useful if you create the teleport component so that the
        currently active camera is inside of it. This will not trigger the teleport
        immediately after component creation. */
    void IgnoreCurrentProximity();

private slots:
    void ServerPostInit();
    void ServerTeleportNow(QString targetEntId, QString posStr, QString rotStr, QStringList ignore);
    
    void OnUpdate(float frametime);
    void OnMouseLeftPressed(MouseEvent *mouseEvent);
    void OnKeyPressed(KeyEvent *keyEvent);
    void OnSceneRectChanged(const QRectF &rect);
    
    void ShowConfirmDialog();
    void CloseConfirmDialog();
    void SetDisableLocally(bool disabled);

    // Returns the trigger entity for the teleport action.
    Entity *TriggerEntity() const;
    
    // Returns the target entity that will be teleported.
    // This will be the active camera entity or whatever it
    // is parented to.
    Entity *TargetEntity() const;

private:
    void AttributesChanged();
    
    QPointer<QWidget> dialog_;
    Ui::EC_MeshmoonTeleport ui_;
    
    float proximityDelta_;
    bool proximityIsOn_;
    bool proximityShowedDialog_;
    bool locallyDisabled_;
};
COMPONENT_TYPEDEFS(MeshmoonTeleport);
