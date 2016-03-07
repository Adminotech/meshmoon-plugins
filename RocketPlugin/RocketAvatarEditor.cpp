/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketAvatarEditor.h"
#include "RocketPlugin.h"
#include "MeshmoonData.h"
#include "MeshmoonBackend.h"
#include "RocketMenu.h"
#include "RocketTaskbar.h"
#include "RocketSceneWidget.h"
#include "RocketNotifications.h"

#include "Framework.h"
#include "CoreDefines.h"
#include "LoggingFunctions.h"

#include "SceneAPI.h"
#include "Scene.h"
#include "Entity.h"

#include "FrameAPI.h"
#include "InputAPI.h"
#include "InputContext.h"

#include "AssetAPI.h"
#include "AssetCache.h"
#include "IAsset.h"
#include "IAssetTransfer.h"
#include "TundraLogicModule.h"
#include "Client.h"

#include "UiAPI.h"
#include "UiMainWindow.h"

#include "OgreRenderingModule.h"
#include "OgreWorld.h"
#include "Renderer.h"
#include "EC_Placeable.h"
#include "EC_Camera.h"
#include "EC_Mesh.h"
#include "EC_DynamicComponent.h"
#include "EC_Avatar.h"
#include "EC_MeshmoonAvatar.h"
#include "EC_AnimationController.h"

#include "AvatarModule.h"
#include "AvatarEditor.h"

#include <QGraphicsScene>
#include <QCursor>
#include <QMessageBox>
#include <QFile>

#include "MemoryLeakCheck.h"

typedef shared_ptr<EC_Mesh> TundraMeshPtr;
typedef shared_ptr<EC_Placeable> PlaceablePtr;
typedef shared_ptr<EC_Camera> CameraPtr;
typedef shared_ptr<EC_DynamicComponent> DynamicPtr;
typedef shared_ptr<EC_AnimationController> AnimationControllerPtr;

static const float3 AVATAR_START_POS = float3(-3.5f, 0, 0);
static const float3 AVATAR_HIDE_POS = float3(3.5f, 0, 0);
static const float3 AVATAR_SHOW_POS = float3::zero;

static const float3 CAMERA_FAR_POS = float3(0.5f, 0.1f, -3.0f);
static const float3 CAMERA_NEAR_POS = float3(0.2f, 0.7f, -1.5f);

static void LogNotImplemented(QString function, QString message = "")
{
    QString out = "RocketAvatarEditor::" + function + " not implemented";
    if (!message.isEmpty())
        out += ": " + message;
    LogError(out);
}

RocketAvatarEditor::RocketAvatarEditor(RocketPlugin *plugin) :
    RocketSceneWidget(plugin->GetFramework()),
    plugin_(plugin),
    identifier_("AdminoAvatarEditor"),
    LC("[AvatarEditor]: "),
    controlsWidget_(0),
    imagesInitialized_(false)
{
    // Toggle action
    actEditAvatar_ = new QAction(QIcon(":/images/icon-avatar.png"), tr("Edit Avatar"), this);
    connect(actEditAvatar_, SIGNAL(triggered()), SLOT(Open()));
    plugin_->Menu()->AddAction(actEditAvatar_, tr("Session"));
    actEditAvatar_->setVisible(false);

    // Raise Z above the avatar/parts widgets
    ui_.setupUi(widget_);
    setZValue(zValue()+1);

    // Ui signals
    connect(ui_.checkBoxAutoRotate, SIGNAL(clicked(bool)), SLOT(OnAutoRotateChecked(bool)));
    connect(ui_.sliderRotation, SIGNAL(sliderMoved(int)), SLOT(OnRotateSliderChanged(int)));
    connect(ui_.buttonSaveAndClose, SIGNAL(clicked()), SLOT(Close()));
    connect(ui_.buttonCloseWithoutSave, SIGNAL(clicked()), SLOT(OnCloseWithoutSaveClicked()));
    connect(ui_.buttonNextPage, SIGNAL(clicked()), SLOT(OnNextPage()));
    connect(ui_.buttonPreviousPage, SIGNAL(clicked()), SLOT(OnPreviousPage()));
    
    // Backend
    connect(plugin_->Backend(), SIGNAL(Response(const QUrl&, const QByteArray&, int, const QString &)),
        SLOT(OnBackendReply(const QUrl&, const QByteArray&, int, const QString &)));
    connect(plugin_->Backend(), SIGNAL(AuthReset()), SLOT(OnAuthReset()));
        
    ui_.labelIcon->setVisible(false);
    
    hide();
}

RocketAvatarEditor::~RocketAvatarEditor()
{
    data_.returnCamera.reset();
    ResetScene();
}

void RocketAvatarEditor::OnAuthenticated(const Meshmoon::User &user)
{
    user_ = user;
    ui_.labelUsername->setText(user_.name);

    SetShortcutVisible(true);
}

void RocketAvatarEditor::OnAuthReset()
{
    user_.Reset();
    imagesInitialized_ = false;

    ui_.labelUsername->clear();
    ui_.labelIcon->hide();
    
    SetShortcutVisible(false);
}

void RocketAvatarEditor::SetShortcutVisible(bool visible)
{
    if (actEditAvatar_)
        actEditAvatar_->setVisible(visible);
}

void RocketAvatarEditor::OnProfilePictureLoaded(AssetPtr asset)
{
    // Don't forget the profile image asset here again. Its done in portal widget and produces an error if called again.
    SetProfileImage(asset ? asset->DiskSource() : "");
    if (asset) plugin_->GetFramework()->Asset()->ForgetAsset(asset->Name(), false);
}

void RocketAvatarEditor::SetProfileImage(const QString &path)
{
    ui_.labelIcon->show();

    if (!path.isEmpty() && QFile::exists(path))
    {
        QPixmap loadedIcon(path);
        if (loadedIcon.width() < 50 || loadedIcon.height() < 50)
            loadedIcon = loadedIcon.scaled(QSize(50,50), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (!loadedIcon.isNull())
        {
            ui_.labelIcon->setPixmap(loadedIcon);
            return;
        }
    }

    // Fallback
    ui_.labelIcon->setPixmap(QPixmap(":/images/profile-anon.png"));
}

void RocketAvatarEditor::OnUpdate(float frametime)
{
    if (!state_.ShouldUpdate(frametime))
        return;
        
    if (data_.avatar)
    {
        TundraMeshPtr avatarMesh = data_.avatar->GetComponent<EC_Mesh>();
        PlaceablePtr avatarPlaceable = data_.avatar->GetComponent<EC_Placeable>();
        if (avatarPlaceable && avatarMesh  && avatarMesh->GetEntity())
        {
            if (ui_.checkBoxAutoRotate->isChecked() || state_.rotForce != 0)
            {
                Transform transform = avatarPlaceable->transform.Get();
                if (state_.rotForce == 0)
                    state_.rotY += (!state_.avatarPressed ? 1.0f : 0.0f);
                else
                {
                    state_.rotY += state_.rotForce;
                    if (Abs<int>(state_.rotForce) > 1)
                    {
                        if (Abs<int>(state_.rotForce) > 15)
                            state_.rotForce /= 1.25f;
                        else
                        {
                            if (state_.rotForce < 0)
                                state_.rotForce++;
                            else if (state_.rotForce > 0)
                                state_.rotForce--;
                        }
                    }
                    else
                        state_.rotForce = 0;
                }
                if (state_.rotY < 0.0f)
                    state_.rotY = 360.0f;
                else if (state_.rotY >= 360.0f)
                    state_.rotY = 0.0f;

                transform.rot.y = state_.rotY;
                avatarPlaceable->transform.Set(transform, AttributeChange::LocalOnly);
                
                if (!ui_.checkBoxAutoRotate->isChecked())
                    ui_.sliderRotation->setValue(static_cast<int>(state_.rotY));
            }
        }
        AnimationControllerPtr avatarAnim = data_.avatar->GetComponent<EC_AnimationController>();
        if (avatarAnim)
        {
            // Fill the animation combo box and select last active animation if found
            if (ui_.comboBoxAnimations->count() == 0)
            {
                QStringList availableAnims = avatarAnim->GetAvailableAnimations();
                if (!availableAnims.isEmpty())
                {
                    ui_.comboBoxAnimations->addItems(availableAnims);
                    if (!state_.lastActiveAnimation.isEmpty() && availableAnims.contains(state_.lastActiveAnimation))
                        ui_.comboBoxAnimations->setCurrentIndex(availableAnims.indexOf(state_.lastActiveAnimation));
                    else if (availableAnims.contains("Stand")) // Fallback, this should usually be there
                        ui_.comboBoxAnimations->setCurrentIndex(availableAnims.indexOf("Stand"));
                    else // Last fallback enable first animation
                        ui_.comboBoxAnimations->setCurrentIndex(0);
                }
            }
            
            QString selectedAnimation = ui_.comboBoxAnimations->currentText();
            if (selectedAnimation.isEmpty()) 
                selectedAnimation = "Stand";

            QStringList playingAnims = avatarAnim->GetActiveAnimations();
            if (!playingAnims.contains(selectedAnimation))
            {
                QStringList availableAnims = avatarAnim->GetAvailableAnimations();
                if (availableAnims.contains(selectedAnimation))
                {
                    // Others are playing, start with fade
                    if (!playingAnims.isEmpty())
                        avatarAnim->EnableExclusiveAnimation(selectedAnimation, true, 0.5f, 0.5f);
                    else
                    {
                        avatarAnim->EnableExclusiveAnimation(selectedAnimation, true);
                        if (state_.tAnimation >= 0.0f)
                        {
                            avatarAnim->SetAnimationRelativeTimePosition(selectedAnimation, state_.tAnimation);
                            state_.tAnimation = -1.0f;
                        }
                    }
                    state_.lastActiveAnimation = selectedAnimation;
                }
            }
        }
    }
    
    if (!state_.transitions.isEmpty() && data_.scene.get())
    {
        bool allFinished = true;
        for (int i=0; i<state_.transitions.size(); ++i) 
        {
            Rocket::AvatarEditorEntityTransition &transition = state_.transitions[i];
            if (!transition.entity.get())
                continue;
            
            transition.Update(frametime);
            PlaceablePtr transitionPlaceable = transition.entity->GetComponent<EC_Placeable>();
            if (transitionPlaceable)
            {
                Transform transform = transitionPlaceable->transform.Get();
                transform.pos = transition.startPos.Lerp(transition.endPos, transition.t);
                transform.scale = transition.startScale.Lerp(transition.endScale, transition.t);
                transitionPlaceable->transform.Set(transform, AttributeChange::LocalOnly);
            }
            else
                transition.SetFinished();
            
            if (transition.IsFinished())
            {
                entity_id_t entId = transition.entity->Id();
                transition.Reset();
                if (transition.remove)
                    data_.scene->RemoveEntity(entId, AttributeChange::LocalOnly);
            }
            else
                allFinished = false;
        }
        if (allFinished)
            state_.transitions.clear();
    }
}

void RocketAvatarEditor::Open(QString backgroundSceneRef)
{
    LogNotImplemented("Open");
}

void RocketAvatarEditor::Close(bool saveChanges, bool restoreMainView)
{
    LogNotImplemented("Close");
}

bool RocketAvatarEditor::IsVisible()
{
    return this->isVisible();
}

void RocketAvatarEditor::LoadBackgroundScene(const QString &sceneRef)
{
    if (!data_.IsValid())
        return;
    if (sceneRef.isEmpty())
        return;
        
    AssetTransferPtr transfer = plugin_->GetFramework()->Asset()->RequestAsset(sceneRef, "Binary");
    connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnBackgroundSceneLoaded(AssetPtr)), Qt::UniqueConnection);
}

void RocketAvatarEditor::CreateScene()
{
    LogNotImplemented("CreateScene");
}

void RocketAvatarEditor::CreateAvatar(const QString &assetRef, bool animate, bool rotateLeft)
{
    LogNotImplemented("CreateAvatar");
}

Rocket::AvatarEditorEntityTransition *RocketAvatarEditor::Transition(EntityPtr target)
{
    for(int i=0; i<state_.transitions.size(); ++i)
    {
        Rocket::AvatarEditorEntityTransition &transition = state_.transitions[i];
        if (transition.entity.get() == target.get())
            return &transition;
    }
    return 0;
}

QString RocketAvatarEditor::SelectedAvatarRef()
{
    if (data_.avatar)
    {
        shared_ptr<EC_Avatar> avatar = data_.avatar->Component<EC_Avatar>();
        if (avatar && !avatar->appearanceRef.Get().ref.trimmed().isEmpty())
            return avatar->appearanceRef.Get().ref.trimmed();
        shared_ptr<EC_MeshmoonAvatar> openAvatar = data_.avatar->Component<EC_MeshmoonAvatar>();
        if (openAvatar && !openAvatar->appearanceRef.Get().ref.trimmed().isEmpty())
            return openAvatar->appearanceRef.Get().ref.trimmed();
    }
    return "";
}

QString RocketAvatarEditor::SelectedAvatarType()
{
    if (data_.avatar)
    {
        if (data_.avatar->Component<EC_Avatar>())
            return "realxtend";
        else if (data_.avatar->Component<EC_MeshmoonAvatar>())
            return "openavatar";
    }
    return "";
}


void RocketAvatarEditor::ResetScene()
{
    /// @todo Unload all avatar editor assets that were loaded while we were open! Includes the scene and loaded avatars/parts.
    
    // Clear ui state.
    hide();
    ui_.comboBoxAnimations->clear();
    
    if (data_.inputContext.get() && data_.inputContext->MouseCursorOverride())
        data_.inputContext->ClearMouseCursorOverride();
    
    // Remove our widgets from graphics scene
    if (this->scene())
        plugin_->GetFramework()->Ui()->GraphicsScene()->removeItem(this);
    if (controlsWidget_ && controlsWidget_->scene())
        plugin_->GetFramework()->Ui()->GraphicsScene()->removeItem(controlsWidget_);
    SAFE_DELETE(controlsWidget_);

    RemoveAvatarWidgets();
    
    // Return to previous camera.    
    data_.camera.reset();
    if (data_.returnCamera)
    {
        CameraPtr cameraCamera = data_.returnCamera->GetComponent<EC_Camera>();
        if (cameraCamera)
            cameraCamera->SetActive();
    }

    // Get scene name for removal
    QString sceneName = "";
    if (data_.scene)
        sceneName = data_.scene->Name();

    // Free our shared ptrs to scene/ents.        
    data_.Reset();
    state_.Reset();
    
    // Delete our scene
    if (!sceneName.isEmpty() && plugin_->GetFramework()->Scene()->HasScene(sceneName))
        plugin_->GetFramework()->Scene()->RemoveScene(sceneName);
    sceneName = identifier_;
    if (plugin_->GetFramework()->Scene()->HasScene(sceneName))
        plugin_->GetFramework()->Scene()->RemoveScene(sceneName);
        
    // Remove background scene storage if any.
    plugin_->GetFramework()->Asset()->RemoveAssetStorage(identifier_);
}

void RocketAvatarEditor::RemoveAvatarWidgets()
{    
    foreach(RocketAvatarWidget *avatarWidget, avatarWidgets_)
    {
        if (!avatarWidget)
            continue;
            
        // Children/variations
        foreach(RocketAvatarWidget *child, avatarWidget->Children())
        {
            if (!child)
                continue;
            plugin_->GetFramework()->Ui()->GraphicsScene()->removeItem(child);
            SAFE_DELETE(child);
        }
        
        // Parent
        plugin_->GetFramework()->Ui()->GraphicsScene()->removeItem(avatarWidget);
        SAFE_DELETE(avatarWidget);
    }
    avatarWidgets_.clear();
}

void RocketAvatarEditor::OnActiveCameraChanged(Entity *ent)
{
    if (!data_.camera || !ent)
        return;
    if (ent->Name() == data_.camera->Name())
        return;
    
    // When avatar editor is active, force our camera to be active.
    CameraPtr cameraCamera = data_.camera->GetComponent<EC_Camera>();
    if (cameraCamera)
        cameraCamera->SetActive();
}

void RocketAvatarEditor::OnBackendReply(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString & /*error*/)
{
    LogNotImplemented("OnBackendReply");
}

void RocketAvatarEditor::OnAvatarSelected(RocketAvatarWidget *avatarWidget, bool updateInterface)
{
    if (!plugin_ || !avatarWidget)
        return;

    if (avatarWidget->IsParent())
    {
        if (state_.currentAvatar != avatarWidget)
        {
            QRectF sceneRect = plugin_->GetFramework()->Ui()->GraphicsScene()->sceneRect();
            
            // Hide current variations
            if (state_.currentAvatar.data())
            {
                state_.currentAvatar->Deselect();
                foreach (RocketAvatarWidget *child, state_.currentAvatar->Children())
                {
                    if (!child) 
                        continue;
                    child->Deselect();
                    child->Hide();
                }
            }
            
            // Determine rotation direction.
            int indexPrevious = avatarWidgets_.indexOf(state_.currentAvatar);
            int indexCurrent = avatarWidgets_.indexOf(avatarWidget);
            if (state_.currentAvatar == avatarWidgets_.first() && avatarWidget == avatarWidgets_.last())
                indexCurrent = -1;
            else if (state_.currentAvatar == avatarWidgets_.last() && avatarWidget == avatarWidgets_.first())
                indexPrevious = -1;

            state_.currentAvatar = avatarWidget;
            state_.currentAvatar->Select();
            
            AvatarWidgetList children = avatarWidget->Children();
            state_.currentAvatarVariation = (children.isEmpty() ? 0 : children.first());

            if (!state_.currentAvatarVariation)
                CreateAvatar(avatarWidget->AppearanceAssetRef(), true, indexPrevious < indexCurrent);
            else
            {
                state_.currentAvatarVariation->Select();   
                CreateAvatar(state_.currentAvatarVariation->AppearanceAssetRef(), true, indexPrevious < indexCurrent);
            }

            if (updateInterface)
                UpdateInterface(sceneRect);
        }
    }
    else
    {
        if (state_.currentAvatarVariation != avatarWidget)
        {            
            if (state_.currentAvatarVariation.data())
                state_.currentAvatarVariation->Deselect();

            state_.currentAvatarVariation = avatarWidget;
            state_.currentAvatarVariation->Select();

            CreateAvatar(state_.currentAvatarVariation->AppearanceAssetRef(), false);
        }
    }
}

void RocketAvatarEditor::OnAutoRotateChecked(bool checked)
{
    ui_.sliderRotation->setEnabled(!checked);
    if (!checked)
        ui_.sliderRotation->setValue(static_cast<int>(state_.rotY));
}

void RocketAvatarEditor::OnRotateSliderChanged(int value)
{
    if (data_.avatar && !ui_.checkBoxAutoRotate->isChecked())
    {
        state_.rotY = static_cast<float>(value);
        PlaceablePtr avatarPlaceable = data_.avatar->GetComponent<EC_Placeable>();
        if (avatarPlaceable)
        {
            Transform transform = avatarPlaceable->transform.Get();
            transform.rot.y = state_.rotY;
            avatarPlaceable->transform.Set(transform, AttributeChange::LocalOnly);
        }
    }
}

void RocketAvatarEditor::OnWindowResized(const QRectF &rect)
{
    UpdateInterface(rect);
}

void RocketAvatarEditor::OnBackgroundSceneLoaded(AssetPtr asset)
{
    LogNotImplemented("OnBackgroundSceneLoaded");
}

void RocketAvatarEditor::OnCloseWithoutSaveClicked()
{
    Close(false);
}

void RocketAvatarEditor::OnPreviousPage()
{
    state_.page--;
    if (state_.page < 0)
        state_.page = 0;
    UpdateInterface();
}

void RocketAvatarEditor::OnNextPage()
{
    state_.page++;
    UpdateInterface();
}

void RocketAvatarEditor::OnPreviousAvatar()
{
    if (avatarWidgets_.isEmpty())
        return;
    if (!state_.currentAvatar)
        OnAvatarSelected(avatarWidgets_.last());
    else
    {
        int index = avatarWidgets_.indexOf(state_.currentAvatar.data());
        if (index != -1)
        {
            index--;
            if (index < 0)
                index = avatarWidgets_.size()-1;
            OnAvatarSelected(avatarWidgets_.value(index, 0));
        }
    }
}

void RocketAvatarEditor::OnNextAvatar()
{
    if (avatarWidgets_.isEmpty())
        return;
    if (!state_.currentAvatar)
        OnAvatarSelected(avatarWidgets_.first());
    else
    {
        int index = avatarWidgets_.indexOf(state_.currentAvatar.data());
        if (index != -1)
        {
            index++;
            if (index >= avatarWidgets_.size())
                index = 0;
            OnAvatarSelected(avatarWidgets_.value(index, 0));
        }
    }
}

void RocketAvatarEditor::OnKeyEvent(KeyEvent *kEvent)
{
    if (!data_.IsValid() || !kEvent || kEvent->IsRepeat())
        return;
        
    if (kEvent->keyCode == Qt::Key_Right)
    {
        OnNextAvatar();
        kEvent->Suppress();
    }
    else if (kEvent->keyCode == Qt::Key_Left)
    {
        OnPreviousAvatar();
        kEvent->Suppress();
    }
    else if (kEvent->keyCode == Qt::Key_Plus)
    {
        Zoom(120, false);
        kEvent->Suppress();
    }
    else if (kEvent->keyCode == Qt::Key_Minus)
    {
        Zoom(-120, false);
        kEvent->Suppress();
    }
    else if (kEvent->HasCtrlModifier() && kEvent->keyCode == Qt::Key_L)
    {
        OpenLegacyEditor();
        kEvent->Suppress();
    }
}

void RocketAvatarEditor::OnMouseEvent(MouseEvent *mEvent)
{
    if (!data_.IsValid() || !mEvent || !data_.inputContext.get())
        return;

    if (mEvent->Type() == MouseEvent::MousePressed)
    {
        OgreRenderer::OgreRenderingModule *renderModule = plugin_->GetFramework()->GetModule<OgreRenderer::OgreRenderingModule>();
        OgreRenderer::Renderer *renderer = renderModule != 0 ? renderModule->GetRenderer().get() : 0;
        if (renderer)
        {
            RaycastResult *result = renderer->Raycast(mEvent->x, mEvent->y);
            if (result && result->entity == data_.avatar.get())
                state_.avatarPressed = true;
        }
    }
    else if (mEvent->Type() == MouseEvent::MouseMove)
    {
        if (state_.avatarPressed)
        {
            if (mEvent->relativeX > 0 && mEvent->relativeX > state_.rotForce)
                state_.rotForce = mEvent->relativeX;
            else if (mEvent->relativeX < 0 && mEvent->relativeX < state_.rotForce)
                state_.rotForce = mEvent->relativeX;
            //if (Abs<int>(mEvent->relativeY) > 1)
            //    Zoom(-mEvent->relativeY, true);
        }
        else
        {
            OgreRenderer::OgreRenderingModule *renderModule = plugin_->GetFramework()->GetModule<OgreRenderer::OgreRenderingModule>();
            OgreRenderer::Renderer *renderer = renderModule != 0 ? renderModule->GetRenderer().get() : 0;
            if (renderer)
            {
                RaycastResult *result = renderer->Raycast(mEvent->x, mEvent->y);
                if (result && result->entity == data_.avatar.get() && !data_.inputContext->MouseCursorOverride())
                    data_.inputContext->SetMouseCursorOverride(QCursor(Qt::SizeAllCursor));
                else if (result && result->entity != data_.avatar.get() && data_.inputContext->MouseCursorOverride())
                    data_.inputContext->ClearMouseCursorOverride();
            }
        }
    }
    else if (mEvent->Type() == MouseEvent::MouseReleased)
    {
        if (state_.avatarPressed)
        {
            state_.avatarPressed = false;
            if (data_.inputContext->MouseCursorOverride())
                data_.inputContext->ClearMouseCursorOverride();
        }
    }
    else if (mEvent->Type() == MouseEvent::MouseScroll)
    {
        Zoom(mEvent->relativeZ, false);
        mEvent->Suppress();
    }
}

void RocketAvatarEditor::Zoom(int relativeMovement, bool mouseMoveTriggered)
{
    if (!mouseMoveTriggered)
        relativeMovement /= 60;

    if (relativeMovement > 4)
        relativeMovement = 4;
    else if (relativeMovement < -4)
        relativeMovement = -4;
   
    state_.zoom += (relativeMovement * 0.2f);
    if (state_.zoom < 0.0f)
        state_.zoom = 0.0f;
    if (state_.zoom > 1.0f)
        state_.zoom = 1.0f;

    PlaceablePtr placeable = data_.camera.get() ? data_.camera->GetComponent<EC_Placeable>() : PlaceablePtr();
    float3 currentPos = placeable.get() ? placeable->transform.Get().pos : float3::zero;
    float3 targetPos = CAMERA_FAR_POS.Lerp(CAMERA_NEAR_POS, state_.zoom);

    if (currentPos.Equals(targetPos))
        return;
    for(int i=0; i<state_.transitions.size(); ++i)
    {
        Rocket::AvatarEditorEntityTransition &transition = state_.transitions[i];
        if (transition.entity.get() == data_.camera.get())
        {
            transition.startPos = currentPos;
            transition.endPos = targetPos;
            transition.tMultiplier *= 1.2f;
            transition.t = 0.0f;
            return;
        }
    }

    Rocket::AvatarEditorEntityTransition transition(data_.camera, false, currentPos, targetPos, float3::one);
    transition.tMultiplier = 2.0f;
    state_.transitions << transition;
}

void RocketAvatarEditor::UpdateInterface(QRectF sceneRect)
{
    if (sceneRect.isNull())
        sceneRect = plugin_->GetFramework()->Ui()->GraphicsScene()->sceneRect();

    QSizeF contentFrameSize(ui_.contentFrame->size());
    QSizeF controlFrameSize(ui_.controlsFrame->size());
    QRectF spec(0, contentFrameSize.height() + controlFrameSize.height(), size().width(), sceneRect.height() - contentFrameSize.height() - controlFrameSize.height());
    QRectF specNoControls(contentFrameSize.width(), 0, sceneRect.width() - contentFrameSize.width(), sceneRect.height());
    
    qreal itemHeight = 62;
    qreal buttonsHeight = 45;
    int items = (spec.height() - buttonsHeight) / itemHeight;
    
    // Main panel
    if (!isVisible())
        Animate(QSizeF(size().width(), sceneRect.height()), QPointF(0,0), -1.0, sceneRect, RocketAnimations::AnimateRight);
    else
        setGeometry(QRectF(0, 0, size().width(), sceneRect.height()));
        
    // Floating controls
    if (controlsWidget_)
    {
        if ((specNoControls.width() - 50) > controlsWidget_->size().width())
        {
            QPointF floatingPos(specNoControls.center().x() - (controlsWidget_->size().width()/2), sceneRect.height() - controlsWidget_->size().height());
            if (!controlsWidget_->isVisible())
                controlsWidget_->Animate(controlsWidget_->size(), floatingPos, -1.0, sceneRect, RocketAnimations::AnimateUp);
            else
                controlsWidget_->setGeometry(QRectF(floatingPos, controlsWidget_->size()));
        }
        else
            controlsWidget_->Hide();
    }
    
    // Paging
    if (state_.page < 0)
        state_.page = 0;
    int startIndex = state_.page * items;
    if (startIndex >= avatarWidgets_.size())
    {
        state_.page = (avatarWidgets_.size() / items) - 1;
        startIndex = avatarWidgets_.size() - items;
        if (state_.page < 0)
            state_.page = 0;
        if (startIndex < 0)
            startIndex = 0;
    }

    ui_.buttonPreviousPage->hide();
    ui_.buttonNextPage->hide();
        
    // Avatar list
    int visibleIndex = 0;
    for(int i=0; i<avatarWidgets_.size(); ++i)
    {
        RocketAvatarWidget *widget = avatarWidgets_[i];
        if (i < startIndex)
        {
            ui_.buttonPreviousPage->show();
            widget->Hide();
            continue;
        }
        if ((spec.height() - itemHeight < buttonsHeight))
        {
            ui_.buttonNextPage->show();
            widget->Hide();
            continue;
        }   
        visibleIndex++;
        if (visibleIndex > items)
        {
            ui_.buttonNextPage->show();
            widget->Hide();
            continue;
        }

        QPointF specPos = spec.topLeft();
        QSizeF specSize(spec.width(), itemHeight);
        widget->Animate(specSize, specPos, -1.0, sceneRect, (!widget->isVisible() ? RocketAnimations::AnimateRight : RocketAnimations::NoAnimation));
        
        spec.setTop(spec.y() + itemHeight);
    }
    
    qreal fillHeight = spec.height();
    if (fillHeight < buttonsHeight)
        fillHeight = buttonsHeight;
    ui_.fillFrame->setMinimumHeight(fillHeight);
    ui_.fillFrame->setMaximumHeight(fillHeight);
    
    // Current variations. Don't process if currently selected is a variation!
    if (state_.currentAvatar.data() && state_.currentAvatar->IsParent())
    {
        qreal controlsHeight = contentFrameSize.height() + controlFrameSize.height();
        qreal headerHeight = 25;
        qreal headerItemHeight = itemHeight + headerHeight;
        QRectF specVariations(spec.width() + 15, controlsHeight, sceneRect.width() - spec.width() - 15, sceneRect.height() - controlsHeight);

        AvatarWidgetList children = state_.currentAvatar->Children();
        bool first = true;
        for(int i=0; i<children.size(); ++i)
        {
            RocketAvatarWidget *child = children[i];
            if (!child) 
                continue;
                
            if (first)
            {
                child->ui.frameHeader->show();
                child->ui.labelHeader->setText("Outfits");
            }
            else
                child->ui.frameHeader->hide();

            QSizeF specSize = child->size();
            QPointF specPos = specVariations.topLeft();
            
            if (first)
            {
                specPos.setY(specPos.y() - child->ui.frameHeader->height());
                specSize.setHeight(headerItemHeight);
            }
            else
                specSize.setHeight(itemHeight);
            
            child->Animate(specSize, specPos, -1.0, sceneRect, RocketAnimations::AnimateDown);

            if (specVariations.height() >= (itemHeight*2))
                specVariations.setTop(specVariations.y() + itemHeight);
            else
            {
                specVariations.setTop(controlsHeight);
                specVariations.setX(specVariations.x() + specSize.width() - 1);
            }
            
            first = false;
        }
    }
}

void RocketAvatarEditor::OpenLegacyEditor()
{
    if (!data_.scene.get())
        return;
        
    AvatarModule *avatarModule = plugin_->GetFramework()->GetModule<AvatarModule>();
    if (avatarModule)
    {
        if (!avatarModule->GetAvatarEditor())
        {
            avatarModule->ToggleAvatarEditorWindow();
            if (avatarModule->GetAvatarEditor())
                avatarModule->GetAvatarEditor()->SetEntityToEdit(data_.scene->CreateEntity(data_.scene->NextFreeId(), QStringList() << "EC_Name" << "EC_Placeable" << "EC_Avatar", AttributeChange::LocalOnly, false, false));
        }
    }
}

// RocketAvatarWidget

RocketAvatarWidget::RocketAvatarWidget(Framework *framework, const Meshmoon::Avatar &inData, RocketAvatarWidget *parent) :
    RocketSceneWidget(framework),
    data(inData),
    parent_(parent)
{
    ui.setupUi(widget_);

    ui.frameHeader->hide();
    ui.labelVariations->hide();
    ui.labelIconVariations->hide();

    // Name
    if (data.name.isEmpty())
    {
        ui.labelTitle->setText("Unnamed Avatar");    
    }
    else
    {
        QString avatarUpper = data.name.toUpper();
        ui.labelTitle->setText(avatarUpper.left(1) + data.name.right(data.name.length()-1).replace("_", " "));
    }

    ui.labelAuthor->setOpenExternalLinks(false);

    if (IsParent())
    {   
        // Type    
        if (data.type == "realxtend")
            ui.labelSubtitle->setText("realXtend Avatar");
        else if (data.type == "openavatar")
            ui.labelSubtitle->setText("OpenAvatar");

        // Author
        QString linkStyle = "<html><style>a { color: rgb(115, 115, 115); text-decoration: underline; } a:visited { color: rgb(115, 115, 115); text-decoration: underline; }</style><body>";
        QString linkStyleEnd = "</body></html>";
        if (!data.author.isEmpty() && data.authorUrl.isEmpty())
            ui.labelAuthor->setText(linkStyle + QString("by %1").arg(data.author) + linkStyleEnd);
        else if (!data.author.isEmpty() && !data.authorUrl.isEmpty())
            ui.labelAuthor->setText(linkStyle + QString("by <a href=\"%1\">%2</a>").arg(data.authorUrl).arg(data.author) + linkStyleEnd);

        // Gender
        if (!data.gender.isEmpty())
            ui.labelGender->setText(data.gender);
    }

    // Image
    if (!data.imageUrl.isEmpty())
    {
        AssetTransferPtr transfer  = framework_->Asset()->RequestAsset(data.imageUrl, "Binary");
        connect(transfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnImageLoaded(AssetPtr)));
        connect(transfer.get(), SIGNAL(Failed(IAssetTransfer*, QString)), SLOT(SetImage()));
    }
    else
        SetImage();

    // Resize variants
    if (IsChild())
    {
        ui.frameContent->setStyleSheet("QFrame#frameContent { background-color: rgba(255, 255, 255, 255); border: 0px; border-bottom: 1px solid rgb(100, 100, 100); border-right: 1px solid rgb(100, 100, 100); border-left: 1px solid rgb(100, 100, 100); border-radius: 0px; }");
        setMaximumWidth(size().width()/2.0);

        connect(this, SIGNAL(VisibilityChanged(bool)), SLOT(OnVisibilityChanged(bool)));
    }
    
    hide();
}

RocketAvatarWidget::~RocketAvatarWidget()
{
}

void RocketAvatarWidget::OnVisibilityChanged(bool visible)
{
    if (!visible)
        ui.frameHeader->hide();
}

void RocketAvatarWidget::OnImageLoaded(AssetPtr asset)
{
    SetImage(asset ? asset->DiskSource() : "");
    if (asset && framework_->Asset()->GetAsset(asset->Name()).get())
        framework_->Asset()->ForgetAsset(asset, false);
}

void RocketAvatarWidget::SetImage(const QString &path)
{
    if (!path.isEmpty() && QFile::exists(path))
    {
        image = QPixmap(path);
        if (image.width() < 50 || image.height() < 50)
            image = image.scaled(QSize(50,50), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        if (!image.isNull())
        {
            ui.labelIcon->setPixmap(image);
            return;
        }
    }

    // Fallback
    image = QPixmap(":/images/profile-anon.png");
    ui.labelIcon->setPixmap(image);
}

QString RocketAvatarWidget::AppearanceAssetRef()
{
    if (!data.assetRefArchive.isEmpty())    
        return data.assetRefArchive;
    return data.assetRef;
}

bool RocketAvatarWidget::IsParent()
{
    return (parent_ == 0);
}

bool RocketAvatarWidget::IsChild()
{
    return (parent_ != 0);
}

void RocketAvatarWidget::Select()
{
    QString style = ui.frameContent->styleSheet();
    if (style.startsWith("QFrame#frameContent { background-color: rgba(255, 255, 255, 255);"))
    {
        style = style.replace("QFrame#frameContent { background-color: rgba(255, 255, 255, 255);", "QFrame#frameContent { background-color: rgba(230, 230, 230, 255);");
        ui.frameContent->setStyleSheet(style);
    }
}

void RocketAvatarWidget::Deselect()
{
    QString style = ui.frameContent->styleSheet();
    if (style.startsWith("QFrame#frameContent { background-color: rgba(230, 230, 230, 255);"))
    {
        style = style.replace("QFrame#frameContent { background-color: rgba(230, 230, 230, 255);", "QFrame#frameContent { background-color: rgba(255, 255, 255, 255);");
        ui.frameContent->setStyleSheet(style);
    }
}

QList<RocketAvatarWidget*> RocketAvatarWidget::Children()
{
    return children_;
}

void RocketAvatarWidget::AddChild(RocketAvatarWidget *child)
{
    if (!children_.contains(child))
    {
        children_ << child;
        if (!ui.labelVariations->isVisible())
            ui.labelVariations->show();
        if (!ui.labelIconVariations->isVisible())
            ui.labelIconVariations->show();
        ui.labelVariations->setText(QString::number(children_.size()));
    }
}

RocketAvatarWidget *RocketAvatarWidget::Parent()
{
    return parent_;
}

void RocketAvatarWidget::SetMouseOverride(bool activate)
{
    if (activate && !QApplication::overrideCursor())
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
    if (!activate && QApplication::overrideCursor())
    {
        while (QApplication::overrideCursor())
            QApplication::restoreOverrideCursor();
    }
}

void RocketAvatarWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverEnterEvent(event);
    SetMouseOverride(false);
}

void RocketAvatarWidget::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverMoveEvent(event);
    if (IsParent() && widget_ && !data.authorUrl.isEmpty())
    {
        QWidget *hitWidget = widget_->childAt(mapFromScene(event->scenePos()).toPoint());
        if (hitWidget == ui.labelAuthor)
        {
            SetMouseOverride(true);
            return;
        }
    }
    SetMouseOverride(false);
}

void RocketAvatarWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsProxyWidget::hoverLeaveEvent(event);
    SetMouseOverride(false);
}

void RocketAvatarWidget::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsProxyWidget::mousePressEvent(event);

    // Handle mouse press on author. Open authors web page if any.
    if (IsParent() && widget_ && !data.authorUrl.isEmpty())
    {
        QWidget *hitWidget = widget_->childAt(mapFromScene(event->scenePos()).toPoint());
        if (hitWidget == ui.labelAuthor)
        {
            QDesktopServices::openUrl(QUrl(data.authorUrl));
            return;
        }
    }
    emit Pressed(this);
}

/// @cond PRIVATE

namespace Rocket
{
    AvatarEditorData::AvatarEditorData()
    {
    }

    void AvatarEditorData::Reset()
    {
        if (scene.get())
        {
            foreach(EntityPtr ent, backgroundEntities)
            {
                if (!ent)
                    continue;
                entity_id_t id = ent->Id();
                ent.reset();
                scene->RemoveEntity(id, AttributeChange::LocalOnly);
            }    
        }
        backgroundEntities.clear();
         
        inputContext.reset();
        sceneWidgets.clear();
        
        avatar.reset();
        camera.reset();
        returnCamera.reset();
        scene.reset();
    }

    bool AvatarEditorData::IsValid()
    {
        if (!avatar.get() || !camera.get() || !scene.get())
            return false;
        return true;
    }

    AvatarEditorEntityTransition::AvatarEditorEntityTransition(const EntityPtr &entity_, bool remove_, const float3 &startPos_, const float3 &endPos_, const float3 &startScale_) :
        t(0.0f),
        tMultiplier(1.0f),
        entity(entity_),
        remove(remove_),
        startPos(startPos_),
        endPos(endPos_),
        startScale(startScale_)
    {
        endScale = remove ? float3::zero : float3::one;
    }

    void AvatarEditorEntityTransition::Reset()
    {
        entity.reset();
    }

    void AvatarEditorEntityTransition::Update(float frametime)
    {
        t += (frametime * tMultiplier);
        if (t > 1.0f)
            t = 1.0f;
    }
    
    float3 AvatarEditorEntityTransition::CurrentPosFromPlaceable() const
    {
        PlaceablePtr placeable = entity.get() ? entity->GetComponent<EC_Placeable>() : PlaceablePtr();
        return placeable.get() ? placeable->transform.Get().pos : float3::zero;
    }
    
    float3 AvatarEditorEntityTransition::CurrentPosFromInterpolation() const
    {
        return startPos.Lerp(endPos, t);
    }

    void AvatarEditorEntityTransition::SetFinished()
    {
        t = 10.0f;
    }

    bool AvatarEditorEntityTransition::IsFinished() const
    {
        return (t >= 1.0f);
    }

    AvatarEditorState::AvatarEditorState() : 
        rotY(0.0f)
    {
        Reset();
    }

    void AvatarEditorState::Reset()
    {
        // Don't reset rotY.
        tAnimation = -1.0f;
        t = 0.0f;
        zoom = 0.0f;
        
        currentAvatar = 0;
        currentAvatarVariation = 0;
        rotForce = 0;
        page = 0;
        
        avatarPressed = false;
        lastActiveAnimation = "";
        transitions.clear();
    }

    bool AvatarEditorState::ShouldUpdate(float frametime)
    {
        t += frametime;
        if (t >= 0.016f)
        {
            t = 0.0f;
            return true;
        }
        return false;
    }
}

/// @endcond
