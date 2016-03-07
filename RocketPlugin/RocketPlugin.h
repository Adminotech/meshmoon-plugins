/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPlugin.h
    @brief  The Meshmoon Rocket client plugin. */

#pragma once

#include "RocketFwd.h"
#include "common/MeshmoonCommon.h"
#include "MeshmoonData.h"

#include "SceneFwd.h"
#include "InputFwd.h"
#include "AttributeChangeType.h"
#include "IModule.h"

#include <kNetFwd.h>
#include <kNet/Types.h>

#include <QTimer>

class QFrame;
class QPushButton;

namespace TundraLogic { class Client; }

/// Main entity point scripting API for client side %Meshmoon functionality.
/** RocketPlugin is exposed to scripting as 'rocket'.
    @ingroup MeshmoonRocket */
class RocketPlugin : public IModule
{
    Q_OBJECT
    Q_ENUMS(RocketView)

    Q_PROPERTY(RocketTaskbar* taskbar READ Taskbar)                     /**< @copydoc Taskbar */
    Q_PROPERTY(RocketMenu* menu READ Menu)                              /**< @copydoc Menu */
    
    Q_PROPERTY(RocketSounds* sounds READ Sounds)                        /**< @copydoc Sounds */
    Q_PROPERTY(RocketNotifications *notifications READ Notifications)   /**< @copydoc Notifications */
    Q_PROPERTY(RocketAssetMonitor *assetMonitor READ AssetMonitor)      /**< @copydoc AssetMonitor */
    Q_PROPERTY(RocketFileSystem* fileSystem READ FileSystem)            /**< @copydoc FileSystem */
    Q_PROPERTY(RocketReporter* reporter READ Reporter)                  /**< @copydoc Reporter */
    Q_PROPERTY(RocketOculusManager* oculus READ OculusManager)          /**< @copydoc RocketOculusManager */

    Q_PROPERTY(RocketAvatarEditor* avatarEditor READ AvatarEditor)      /**< @copydoc AvatarEditor */
    Q_PROPERTY(RocketBuildEditor* buildEditor READ BuildEditor)         /**< @copydoc BuildEditor */
    
    Q_PROPERTY(RocketLayersWidget* layersWidget READ LayersWidget)      /**< @copydoc LayersWidget */
    // @note Using Meshmoon::SceneLayerList typedef from MeshmoonCommon borks moc.
    Q_PROPERTY(QList<Meshmoon::SceneLayer> layers READ Layers)          /**< @copydoc Layers */
    
    Q_PROPERTY(MeshmoonUser* user READ User)                            /**< @copydoc User */
    Q_PROPERTY(MeshmoonWorld* world READ World)                         /**< @copydoc World */
    
    Q_PROPERTY(MeshmoonStorage* storage READ Storage)                   /**< @copydoc Storage */
    Q_PROPERTY(MeshmoonAssetLibrary* assetLibrary READ AssetLibrary)    /**< @copydoc AssetLibrary */
    
    Q_PROPERTY(RocketView currentView READ CurrentView)                 /**< @copydoc CurrentView */

public:
    /// @cond PRIVATE
    RocketPlugin();
    virtual ~RocketPlugin();

    /// Get TundraLogic Client object.
    TundraLogic::Client *GetTundraClient() const;

    /// Meshmoon backend.
    MeshmoonBackend *Backend() const;

    /// Rocket networking.
    RocketNetworking *Networking() const;

    /// Rocket lobby.
    RocketLobby *Lobby() const;

    /// Rocket offline lobby.
    RocketOfflineLobby *OfflineLobby() const;

    /// Rocket settings.
    RocketSettings *Settings() const;

    /// Ogre resource group for Meshmoon assets.
    static const std::string MESHMOON_RESOURCE_GROUP;

    /// @endcond

    /// Rocket user interface view mode enumeration.
    enum RocketView
    {
        MainView,
        WebBrowserView,
        AvatarEditorView,
        SettingsView,
        PresisView
    };

    /// Returns the current Rocket user interface view mode.
    RocketView CurrentView() const { return currentView_; }

public slots:
    /// Shows the portal and taskbar widgets depending on the input parameters.
    /** @note Does not hide the inworld taskbar, see Taskbar() for it. */
    void Show(bool animate = false, uint msecDelay = 500);

    /// Hides all Rocket login/loading user interfaces.
    /** @note Does not hide the inworld taskbar, see Taskbar() for it. */
    void Hide();

    /// Shows anything in the center of the plugin widget (eg. server listings).
    void ShowCenterContent();

    /// Hides anything in the center of the plugin widget (eg. server listings).
    void HideCenterContent();

    /// Sets progress message to the loading widget if visible. 
    /** Keep the message short, there is limited space to render it */
    void SetProgressMessage(const QString &message);

    /// Sets if avatar auto detection is enabled.
    /** This can be called from scripts if they do not wish to hide the loading ui
        after the avatar is created and its mesh loaded. 
        @note if you don't have avatars in your scene, there is a auto hide mechanism 
        that hits in after 10 seconds. You can call Hide() if you want to hide ui earlier. */
    void SetAvatarDetection(bool enabled);

    /// Return if client is currently connected to a server.
    bool IsConnectedToServer() const;

    /// Returns the taskbar widget.
    RocketTaskbar *Taskbar() const;

    /// Returns the Rocket menu.
    RocketMenu *Menu() const;

    /// Returns the Meshmoon user.
    /** Contains user information and signaling for authentication/permissions. */
    MeshmoonUser *User() const;
    
    /// Returns the Meshmoon world.
    /** Contains information and signaling about the world. */
    MeshmoonWorld *World() const;

    /// Returns the sounds object that can be used to play sounds.
    RocketSounds *Sounds() const;

    /// Returns the avatar editor that can be used to select/edit users current avatar.
    RocketAvatarEditor *AvatarEditor() const;

    /// Returns the storage for the scene client is connected to.
    /** Access to the Meshmoon scene storage. Widgets that provides various 
        administrative operations on the storage and content imports.
        @note Can be opened by Admin level authenticated users. */
    MeshmoonStorage *Storage() const;

    /// Returns the Meshmoon Asset Library.
    /** Provides Meshmoon asset library utilities. You can query assets 
        and add your own library sources for the end user. */
    MeshmoonAssetLibrary *AssetLibrary() const;

    /// Rocket notifications.
    /** Provides modal dialog and notification utility functions. 
        If you need to inform the user about something, use the 
        functionality found in RocketNotifications. */
    RocketNotifications *Notifications() const;

    /// Rocket asset monitor.
    /** Provides asset tracking and monitoring. */
    RocketAssetMonitor *AssetMonitor() const;

    /// Rocket build editor.
    RocketBuildEditor *BuildEditor() const;

    /// Returns the layers widget.
    RocketLayersWidget *LayersWidget() const;

    /// Returns list of available scene layers.
    /** Use SetLayerVisibility to alter the layer's client side visibility. */
    const Meshmoon::SceneLayerList &Layers() const { return layers_; }

    /// Rocket file system utilities.
    RocketFileSystem *FileSystem() const;

    /// Rocket Reporter.
    RocketReporter *Reporter() const;

    // Rocket Oculus manager
    RocketOculusManager *OculusManager() const;

    /// Sets visibility of a layer.
    /** @todo Move these to a layer manager and embed LayersWidget into it. */
    void SetLayerVisibility(const Meshmoon::SceneLayer &layer, bool visible);
    void SetLayerVisibility(u32 layerId, bool visible); /**< @overload */

    /// Opens the given URL with the default operating system web browser.
    /** @todo Make RocketBrowserUtilities and move there.
        @note For security reasons the user is prompted by a notification
        if he wants to continue opening the web page.
        @param URL to open.
        @return Returns if the @c url could be validated as a proper URL.
        False can also be returned if the notifications widgets are not available. */
    bool OpenUrlWithWebBrowser(const QString &url);

signals:
    /// Visibility changed.
    void VisibilityChanged(bool visible);

    /// Connected or disconnected from server.
    void ConnectionStateChanged(bool connectedToServer);

    /// Change denied for entity with id with reason.
    void ChangePermissionDenied(entity_id_t id, Meshmoon::DenyReason reason);

    /// Ui graphics scene was resized.
    /** @param QRectF Current graphics scene rect.
        @note This has a small delay compared to the actual resize event
        so that qt has time to update everything. */
    void SceneResized(const QRectF &rect);

private slots:
    // Registers Meshmoon resource location(s) and rendering initialization.
    void InitializeRendering();

    // Loads persistent Meshmoon media to Ogre.
    void LoadPersistentMedia();
    void ReloadPersistentMedia();

    /// @todo Remove this once clean co-op with Renderer has been implemented.
    void RenderingCreateInstancingShaders();

    // Meshmoon backend API response.
    void OnBackendResponse(const QUrl &url, const QByteArray &data, int httpStatusCode, const QString &error);

    // Handles when basic permission level client connects to a server.
    void OnBasicUserConnected();

    // Handles when basic permission level client disconnects from a server.
    void OnBasicUserDisconnect();

    // Basic user pressed a key. Used to disabled ec, scene etc. windows via key shortcuts.
    void OnBasicUserKeyPress(KeyEvent *e);

    // User authenticated to Meshmoon backend.
    void OnAuthenticated(const Meshmoon::User &user);

    // Resets current user authentication.
    void OnAuthReset();

    // Sets server filtering.
    void OnFilterRequest(QStringList sceneIds);

    // Sets login extra query items.
    void OnExtraLoginQuery(Meshmoon::EncodedQueryItems queryItems);

    // Starts Rocket UI loader widget.
    void OnStartLoader(const QString &message);

    // Sets progress message to loader widget if it is visible.
    void SetProgressMessageInternal(const QString &message, bool stopLoadAnimation = false);

    // Connects to currently pending server information.
    void OnConnect();

    // Cancels current connection attempt. @bug This does not work, kNet does not respect our cancel/disconnect request!
    void OnCancelLogin();

    // Handles when client is about to connect to the server.
    void OnAboutToConnect();

    // Handles when client connects to a server.
    void OnConnected();

    // Handles when client disconnects from a server.
    void OnDisconnected();

    // Handles client connection errors.
    void OnConnectionFailed(const QString &reason);

    // Processes a teleport request.
    void OnTeleportRequest(const QString &sceneId, const QString &pos, const QString &rot);

    // Exposes Meshmoon specific QObjects etc. to a created engine.
    void OnScriptEngineCreated(QScriptEngine *engine);

    // Delayed resize handler that emits SceneResized to our widgets to handle resizing.
    // This logic is needed as if not delayed the scene can report incorrect scene rect.
    void OnDelayedResize();

    // Called when returning from a scene that had been double-clicked for preview
    void OnReturnToLobby();

    // Teleportation failed handler. Notifies the current teleport EC of the failure.
    void TeleportationFailed(const QString &message);

    // Hooks avatar detection on/off. This makes the load screen automatically hide when clients own avatar mesh is loaded.
    // @todo Extract this to its own class.
    void HookAvatarDetection(bool enabled, bool unhookComponents = false);

    // Scene created handler.
    void OnSceneCreated(const QString &name);

    // Component was created to a monitored scene.
    void OnSceneComponentCreated(Entity *entity, IComponent* component, AttributeChange::Type change);

    // Component was created to a monitored entity. @see HookAvatarDetection. @todo Extract this to its own class.
    void OnEntityComponentCreated(IComponent* component, AttributeChange::Type change);

    // Entity was created. @see HookAvatarDetection. @todo Extract this to its own class.
    void OnEntityCreated(Entity *entity, AttributeChange::Type change);
    
    // Our avatar mesh was created. @see HookAvatarDetection. @todo Extract this to its own class.
    void OnAvatarMeshCreated();

    // Handles Meshmoon custom kNet messages.
    void HandleKristalliMessage(kNet::MessageConnection* source, kNet::packet_id_t packetId,
        kNet::message_id_t messageId, const char* data, size_t numBytes);

    // Show settings.
    void SetSettingsVisible(bool visible = true);

    // Show avatar editor.
    void SetAvatarEditorVisible(bool visible = true);

    // Show profile editor.
    void SetProfileEditorVisible(bool visible = true);

    // Show presis.
    void SetPresisVisible(bool visible = true);

    // Show main Rocket lobby.
    void RestoreMainView();

    // Sets current extra query items to url.
    void SetExtraQueryItems(QUrl &url);

    // Sets item to current extra query items.
    void SetOrCreateExtraQueryItem(const Meshmoon::EncodedQueryItem &item);

    // Check if extra query item exists.
    bool HasExtraQueryItem(const QString &key) const;

    // OpenUrlWithWebBrowser functions authorized handler.
    void OnOpenUrlWithWebBrowserAuthorized();

private:
    void Initialize(); ///< IModule override.
    void Uninitialize(); ///< IModule override.

    // Adds Rocket taskbar to the UI.
    void AddTaskbar();

    // Removes Rocket taskbar from the UI.
    void RemoveTaskbar();

    // Logging channel.
    const QString LC;

    // Timers.
    QTimer mediaLoadDelay_;
    QTimer fallBackHide_;
    QTimer resizeDelayTimer_;

    // "Return to Lobby" button in case of Rocket was started with a --file parameter.
    QFrame *scenePreviewWidget_;

    // Rocket functionality object instances.
    RocketLobby *lobby_;
    RocketOfflineLobby *offlineLobby_;
    RocketMenu *menu_;
    RocketNetworking *networking_;
    RocketSettings *settings_;
    RocketSounds *sounds_;
    RocketTaskbar *taskbar_;
    RocketUpdater *updater_;
    RocketPresis *presis_;
    RocketReporter *reporter_;
    RocketNotifications *notifications_;
    RocketBuildEditor *buildEditor_;
    RocketAvatarEditor *avatarEditor_;
    RocketAssetMonitor *assetMonitor_;
    RocketLayersWidget *layersWidget_;
    RocketCaveManager *caveManager_;
    RocketOcclusionManager *occlusionManager_;
    RocketOculusManager* oculusManager_;
    RocketFileSystem *fileSystem_;

    // Meshmoon functionality object instances.
    MeshmoonBackend *backend_;
    MeshmoonStorage *storage_;
    MeshmoonAssetLibrary *library_;
    MeshmoonUser *user_;
    MeshmoonWorld *world_;

    // Currently authenticated user permissions for current scene.
    Meshmoon::UserPermissions permissions_;

    // Current layer state
    Meshmoon::SceneLayerList layers_;

    // Avatar auto detection enabled? @see HookAvatarDetection.
    bool avatarDetection_;

    // Should we log out permission denied information to UI and console?
    bool noPermissionNotified_;

    // Active server filtering.
    QStringList filterIds_;

    // Pending login url.
    QUrl loginUrl_;

    // Extra login query parameters that will get applied to loginUrl_. These go to client login properties.
    Meshmoon::EncodedQueryItems loginQueryItems_;
    RocketView currentView_;
};
Q_DECLARE_METATYPE(RocketPlugin*)
