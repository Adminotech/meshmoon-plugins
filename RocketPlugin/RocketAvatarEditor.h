/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "SceneFwd.h"
#include "AssetFwd.h"
#include "InputFwd.h"
#include "RocketSceneWidget.h"

#include <QObject>
#include <QAction>
#include <QString>
#include <QList>
#include <QPointer>
#include <QGraphicsItem>
#include <QPixmap>
#include <QTimer>

#include "Math/float3.h"

#include "ui_RocketAvatarWidget.h"
#include "ui_RocketAvatarEditorWidget.h"
#include "ui_RocketAvatarEditorControlsWidget.h"

/// @cond PRIVATE

class RocketAvatarWidget : public RocketSceneWidget
{
    Q_OBJECT

public:
    RocketAvatarWidget(Framework *framework, const Meshmoon::Avatar &inData, RocketAvatarWidget *parent = 0);
    ~RocketAvatarWidget();

    Meshmoon::Avatar data;
    Ui::RocketAvatarWidget ui;
    QPixmap image;

public slots:
    QString AppearanceAssetRef();

    bool IsParent();
    bool IsChild();

    void Select();
    void Deselect();

    QList<RocketAvatarWidget*> Children();
    void AddChild(RocketAvatarWidget *child);

    RocketAvatarWidget *Parent();

signals:
    void Pressed(RocketAvatarWidget *avatarWidget);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);

private slots:
    void SetMouseOverride(bool activate);
    void OnImageLoaded(AssetPtr asset);
    void SetImage(const QString &path = QString());
    void OnVisibilityChanged(bool visible);

private:
    RocketAvatarWidget *parent_;
    QList<RocketAvatarWidget*> children_;
};

namespace Rocket
{
    struct AvatarEditorData
    {
        EntityPtr avatar;
        EntityPtr camera;
        EntityPtr returnCamera;
        ScenePtr scene;
        InputContextPtr inputContext;
        
        QList<EntityPtr> backgroundEntities;
        QList<QPointer<QGraphicsObject> > sceneWidgets;
        
        AvatarEditorData();

        void Reset();    
        bool IsValid();
    };

    struct AvatarEditorEntityTransition
    {
        EntityPtr entity;
        float3 startPos;
        float3 endPos;
        float3 startScale;
        float3 endScale;
        
        float t;
        float tMultiplier;
        bool remove;
        
        AvatarEditorEntityTransition(const EntityPtr &entity_, bool remove_, const float3 &startPos_, const float3 &endPos_, const float3 &startScale_ = float3::zero);
        
        void Reset();
        void Update(float frametime);
        float3 CurrentPosFromPlaceable() const;
        float3 CurrentPosFromInterpolation() const;
        
        void SetFinished();
        bool IsFinished() const;
    };

    struct AvatarEditorState
    {
        float tAnimation;
        float t;
        float rotY;
        float zoom;
        int rotForce;
        int page;
        bool avatarPressed;
        
        QString lastActiveAnimation;
        QPointer<RocketAvatarWidget> currentAvatar;
        QPointer<RocketAvatarWidget> currentAvatarVariation;
        QList<AvatarEditorEntityTransition> transitions;
        
        AvatarEditorState();
        
        void Reset();
        bool ShouldUpdate(float frametime);
    };
}

typedef QList<RocketAvatarWidget*> AvatarWidgetList;

/// @endcond

/// Avatar editor UI
/** @ingroup MeshmoonRocket */
class RocketAvatarEditor : public RocketSceneWidget
{
    Q_OBJECT

public:
    /// @cond PRIVATE
    RocketAvatarEditor(RocketPlugin *plugin);
    ~RocketAvatarEditor();

    friend class RocketPlugin;
    /// @endcond
    
public slots:
    /// Opens avatar editor with optional background scene txml ref.
    void Open(QString backgroundSceneRef = QString());

    /// Closes the editor with optional saving of changes.
    void Close(bool saveChanges = true, bool restoreMainView = true);

    /// Is editor visible
    bool IsVisible();

    /// Loads new background scene to active avatar editor. If not open this does nothing.
    void LoadBackgroundScene(const QString &sceneRef);

    /// Returns currently selected avatar reference.
    QString SelectedAvatarRef();

    /// Return currently selected avatar type.
    /** @note The returned string is either "realxtend" or "openavatar" */
    QString SelectedAvatarType();

    // Shows/hides the session menu shortcut
    void SetShortcutVisible(bool visible);

signals:
    /// Return the users changed appearance asset reference and avatar type.
    /** This signal will fire when users saves the currently selected avatar to his profile.
        @note The returned avatar type string is either "realxtend" or "openavatar" */
    void AvatarAppearanceChanged(const QString &appearanceRef, const QString &avatarType);

private slots:
    void OnAuthenticated(const Meshmoon::User &user);
    void OnAuthReset();

    void OnUpdate(float frametime);
    void OnActiveCameraChanged(Entity *ent);
    void OnBackendReply(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString &error);
    void OnAvatarSelected(RocketAvatarWidget *avatarWidget, bool updateInterface = true);

    void OnAutoRotateChecked(bool checked);
    void OnRotateSliderChanged(int value);
    void OnWindowResized(const QRectF &rect);

    void OnBackgroundSceneLoaded(AssetPtr asset);
    void OnProfilePictureLoaded(AssetPtr asset);

    void OnPreviousPage();
    void OnNextPage();
    void OnPreviousAvatar();
    void OnNextAvatar();
    void Zoom(int factor, bool mouseMoveTriggered);

    void OnCloseWithoutSaveClicked();

    void OnKeyEvent(KeyEvent *kEvent);
    void OnMouseEvent(MouseEvent *mEvent);

    void UpdateInterface(QRectF sceneRect = QRectF());
    void SetProfileImage(const QString &path = QString());

    void OpenLegacyEditor();

private:
    void CreateScene();
    void CreateAvatar(const QString &assetRef, bool animate = true, bool rotateLeft = true);
    void ResetScene();
    void RemoveAvatarWidgets();

    Rocket::AvatarEditorEntityTransition *Transition(EntityPtr target);

    RocketPlugin *plugin_;
    Meshmoon::User user_;
    Rocket::AvatarEditorData data_;
    Rocket::AvatarEditorState state_;

    bool imagesInitialized_;

    QAction *actEditAvatar_;
    QString identifier_;
    QString LC;

    RocketSceneWidget *controlsWidget_;
    AvatarWidgetList avatarWidgets_;

    Ui::RocketAvatarEditorWidget ui_;
    Ui::RocketAvatarEditorControlsWidget uiControls_;
};
Q_DECLARE_METATYPE(RocketAvatarEditor*)
