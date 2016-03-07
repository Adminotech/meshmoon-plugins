/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonComponentsApi.h"
#include "rocketmediaplayer/MediaPlayerNetworkMessages.h"
#include "ui_MediaPlayer.h"

#include "IComponent.h"
#include "SceneFwd.h"
#include "InputFwd.h"
#include "EntityReference.h"

#include <QTimer>
#include <QImage>
#include <QSharedMemory>
#include <QSize>
#include <QAbstractSocket>
#include <QProcess>

class EC_Mesh;
class QWidget;
class QTcpSocket;
class QGraphicsView;
class QGraphicsScene;

/// Shows a media player on an entity.
class MESHMOON_COMPONENTS_API EC_MediaBrowser : public IComponent
{
    Q_OBJECT
    Q_ENUMS(MediaPlayerProtocol::PlaybackState)
    COMPONENT_NAME("MediaBrowser", 502) // Note this is the closed source EC Meshmoon range ID.

public:
    /// Is rendering to the target enabled. Default: true.
    Q_PROPERTY(bool enabled READ getenabled WRITE setenabled);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, enabled);

    /// Use audio only mode without any rendering. Default: false.
    Q_PROPERTY(bool audioOnly READ getaudioOnly WRITE setaudioOnly);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, audioOnly);

    /// Play media when ready. Default: false.
    /** @note We wont actually download anything before starting playback.
        if playOnLoad is true, a play command is activated on VLC and it will
        take care of potential streaming. */
    Q_PROPERTY(bool playOnLoad READ getplayOnLoad WRITE setplayOnLoad);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, playOnLoad);

    /// Medias audio can be heard inside this radius. Default: 0.0 (disabled).
    /** 0.0 means no radius and can be heard from anywhere in the scene. */
    Q_PROPERTY(float spatialRadius READ getspatialRadius WRITE setspatialRadius);
    DEFINE_QPROPERTY_ATTRIBUTE(float, spatialRadius);

    /// Source media. Default: Empty string.
    /** {http|ftp|mms}://ip:port/file
        udp:[[<source address>]@[<bind address>][:<bind port>]] */
    Q_PROPERTY(QString sourceRef READ getsourceRef WRITE setsourceRef);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, sourceRef);

    /// Rendering target entity. Default: By value empty, meaning the parent entity is used.
    Q_PROPERTY(EntityReference renderTarget READ getrenderTarget WRITE setrenderTarget);
    DEFINE_QPROPERTY_ATTRIBUTE(EntityReference, renderTarget);

    /// Rendering target submesh index. Default: 0.
    Q_PROPERTY(uint renderSubmeshIndex READ getrenderSubmeshIndex WRITE setrenderSubmeshIndex);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, renderSubmeshIndex);

    /// Boolean for illuminating the player. This means the materials emissive will be manipulated to show the browser always 
    /// with full brightness. If illuminating is true there are no shadows affecting the light, otherwise shadows will be shown.
    Q_PROPERTY(bool illuminating READ getilluminating WRITE setilluminating);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, illuminating);

    /// 3D mouse input. Default: true.
    Q_PROPERTY(bool mouseInput READ getmouseInput WRITE setmouseInput);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, mouseInput);

    /// Looping the media playback. Default: false.
    Q_PROPERTY(bool looping READ getlooping WRITE setlooping);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, looping);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MediaBrowser(Scene* scene);
    /// @endcond
    virtual ~EC_MediaBrowser();

    friend class MeshmoonComponents;

public slots:
    // Play if not playing.
    bool Play();

    // Pause if not paused.
    bool Pause();

    /// Toggle play and pause. Play if paused, pause if playing.
    bool TogglePlay();

    /// Stop playback.
    bool Stop();

    /// Seek video.
    bool Seek(qint64 msec);

    /// Set player volume.
    bool SetVolume(int volume);

    /// Return if media is seekable.
    /** @note Will only return real value when
        playback has started at least once on the media. */
    bool IsSeekable() const;

    /// Return if media is pausable.
    /** @note Will only return real value when 
        playback has started at least once on the media. */
    bool IsPausable() const;

    /// Current playback state.
    MediaPlayerProtocol::PlaybackState PlaybackState() const;

    /// Current playback state MediaPlayerProtocol::PlaybackState as int for scripts.
    /// @see PlaybackState() for enum to int documentation.
    int PlaybackStateInt() const;

    /// Returns the current render target entity.
    EntityPtr RenderTarget();

    /// Set illumination to the material.
    void SetMaterialIllumination(bool illuminating);

    /// Sets VLC process logging.
    /** This needs to be called before playback is started,
        as it needs to be set when the VLC process is started.
        @note This will log only to the OS shell, not to Tundras graphical UI console.
        @param If logging should be enabled. */
    void SetVlcLoggingEnabled(bool enabled);

    /// Get string representation of a playback state.
    /** @return "NothingSpecial", "Opening", "Buffering", "Playing", "Paused", "Stopped", "Ended", "Error" or empty string for unknown state. */
    QString PlaybackStateToString(int playbackState);

signals:
    /// Player created.
    void PlayerCreated(IComponent *component);

    /// Player closed.
    void PlayerClosed(IComponent *component);

    /// Playback state changed.
    /// @todo State as int until MediaPlayerProtocol::PlaybackState is exposed to script.
    void StateChanged(IComponent *component, int /** @todo MediaPlayerProtocol::PlaybackState*/ playbackState);

private slots:
    void LoadSource(const QString &source);
    void SeekWithSlider();
    
    bool SendLooping();

    void OnParentEntitySet();

    void OnComponentAdded(IComponent *component, AttributeChange::Type change);
    void OnComponentRemoved(IComponent *component, AttributeChange::Type change);

    void OnMeshAssetLoaded();
    void OnMeshMaterialChanged(uint index, const QString &materialName);

    void OnWindowResized(int newWidth, int newHeight);

    void OnMouseEventReceived(MouseEvent *mouseEvent);
    void SendMouseEvent(QEvent::Type type, const QPointF &point, MouseEvent *e);

    void OnEntityEnterView(Entity *ent);
    void OnEntityLeaveView(Entity *ent);

    void OnUpdate(float);
    void UpdateVolume();
    void OnIpcError(QAbstractSocket::SocketError socketError);

private slots:
    void ApplyMaterial(bool force = false);

    // IPC
    void UpdateWidgetState();
    void BlitToTexture();

    void OnIpcProcessStarted();
    void OnIpcProcessClosed(int exitCode, QProcess::ExitStatus);
    void OnIpcProcessError(QProcess::ProcessError error);
    void OnIpcConnected();
    void OnIpcData();

private:
    void ResizeTexture(int width, int height);

    /// Returns if this entity is in view of currently active 
    /// camera and if browser rendering should be updated.
    bool IsInView() const;

    /// Returns if this entity is in rendering distance.
    bool IsInDistance() const;

    /// Returns the distance to the active camera.
    int DistanceToCamera(Entity *renderTarget = 0);

    /// QObject override.
    bool eventFilter(QObject *obj, QEvent *e);

    /// MeshmoonComponents sets permission to run this component.
    /// This limits the processes we have running at any given time.
    void SetRunPermission(bool permitted);

    quint16 IpcPort();
    quint16 FreeIpcPort();

    QProcess *ipcProcess;
    QTcpSocket *ipcSocket;
    QSharedMemory ipcMemory;

    QSize size;

    /// IComponent override.
    void AttributesChanged();

    void Init();
    void Reset(bool immediateShutdown);

    void InitInput();
    void CheckInput();
    void ResetInput();

    void InitPlayer();
    void ResetPlayer(bool immediateShutdown);

    void InitUi();
    void ResetUi();

    void InitRendering();
    void ResetRendering();
    void RenderWidget(bool & locked);

    void RemoveMaterial();
    void RestoreMaterials(EC_Mesh *mesh);

    float ActualTextureScale(int width = 0, int height = 0);
    QSize ActualTextureSize(int width = 0, int height = 0);

    void FocusPlayer();
    void UnfocusPlayer();

    std::string textureName_;
    std::string materialName_;

    EntityWeakPtr currentTarget_;
    uint currentSubmesh_;
    int currentDistance_;

    bool inView_;
    bool inDistance_;
    bool renderedOnFrame_;
    bool permissionRun_;
    bool vlcLogging_;

    float updateDelta_;
    int volume_;

    QTimer resizeTimer_;
    InputContextPtr inputContext_;

    QImage imgPaused_;

    MediaPlayerProtocol::PlayerState state_;

    QGraphicsScene *graphicsScene_;
    QGraphicsView *graphicsView_;
    QWidget *widget_;
    Ui::MediaPlayer ui_;

    static const QString LC;
    QString componentMaterial_;
    QString totalTime_;
};
COMPONENT_TYPEDEFS(MediaBrowser);
