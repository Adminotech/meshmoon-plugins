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
#include "TundraProtocolModuleFwd.h"
#include "EntityReference.h"

#include <QString>
#include <QPoint>
#include <QList>
#include <QByteArray>
#include <QSharedMemory>
#include <QSize>
#include <QAbstractSocket>
#include <QProcess>

#include "EC_WebBrowser_Protocol.h"

class EC_Mesh;
class QWidget;
class QTcpSocket;

/// Shows a web browser on an entity.
class MESHMOON_COMPONENTS_API EC_WebBrowser : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("WebBrowser", 501) // Note this is the closed source EC Meshmoon range ID.

public:
    /// Is rendering to the target enabled. Default: true.
    Q_PROPERTY(bool enabled READ getenabled WRITE setenabled);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, enabled);
   
    /// URL. Default: Empty string.
    Q_PROPERTY(QString url READ geturl WRITE seturl);
    DEFINE_QPROPERTY_ATTRIBUTE(QString, url);

    /// Size. Default: 1024x1024. 
    /** @note The texture size will be adjusted during runtime depending on the distance to the active EC_Camera. */
    Q_PROPERTY(QPoint size READ getsize WRITE setsize);
    DEFINE_QPROPERTY_ATTRIBUTE(QPoint, size);
    
    /// Rendering target entity. Default: By value empty, meaning the parent entity is used.
    Q_PROPERTY(EntityReference renderTarget READ getrenderTarget WRITE setrenderTarget);
    DEFINE_QPROPERTY_ATTRIBUTE(EntityReference, renderTarget);

    /// Rendering target submesh index. Default: 0.
    Q_PROPERTY(uint renderSubmeshIndex READ getrenderSubmeshIndex WRITE setrenderSubmeshIndex);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, renderSubmeshIndex);

    /// Boolean for illuminating the browser. This means the materials emissive will be manipulated to show the browser always 
    /// with full brightness. If illuminating is true there are no shadows affecting the light, otherwise shadows will be shown.
    Q_PROPERTY(bool illuminating READ getilluminating WRITE setilluminating);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, illuminating);

    /// 3D mouse input.
    Q_PROPERTY(bool mouseInput READ getmouseInput WRITE setmouseInput);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, mouseInput);

    /// 3D keyboard input. Default: true. 
    /** @note Mouse input must be enabled to enable focus for keyboard input */
    Q_PROPERTY(bool keyboardInput READ getkeyboardInput WRITE setkeyboardInput);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, keyboardInput);

    /// Synchronized client connection ID. Default: -1 (no controller).
    /** Setting this to a valid (connected) client connection ID will synchronize
        that clients mouse actions to every other client. This at the same time disabled
        any input from non-controlling clients to their own views. Set back to -1 when releasing control. */
    Q_PROPERTY(int synchronizedClient READ getsynchronizedClient WRITE setsynchronizedClient);
    DEFINE_QPROPERTY_ATTRIBUTE(int, synchronizedClient);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_WebBrowser(Scene* scene);
    virtual ~EC_WebBrowser();
    
    friend class MeshmoonComponents;
    /// @endcond

public slots:
    /// Returns the current url in the loaded browser.
    /** @note This may be different from the url attribute.
        While browsing the attribute is not updated. */
    QString CurrentUrl() const;
    
    /// Returns the current render target entity.
    EntityPtr RenderTarget();
    
    /// Set illumination to the material.
    void SetMaterialIllumination(bool illuminating);
    
signals:
    /// Browser created.
    void BrowserCreated(IComponent *component);
    
    /// Browser closed.
    void BrowserClosed(IComponent *component);
    
    /// Load of a web page started.
    void LoadStarted(IComponent *component, const QString &url);
    
    /// Load of a web page completed.
    void LoadCompleted(IComponent *component, const QString &url);
        
private slots:
    void LoadUrl(const QString &url);
    void ResizeBrowser(int width, int height);
    
    void OnParentEntitySet();
    
    void OnComponentAdded(IComponent *component, AttributeChange::Type change);
    void OnComponentRemoved(IComponent *component, AttributeChange::Type change);
    
    void OnMeshAssetLoaded();
    void OnMeshMaterialChanged(uint index, const QString &materialName);
    
    void OnWindowResized(int newWidth, int newHeight);
    
    void OnMouseEventReceived(MouseEvent *mouseEvent);
    void OnKeyEventReceived(KeyEvent *keyEvent);
    
    void OnEntityEnterView(Entity *ent);
    void OnEntityLeaveView(Entity *ent);
    
    void OnUpdate(float);
    
    void OnServerMessageReceived(UserConnection *connection, kNet::packet_id_t, kNet::message_id_t id, const char* data, size_t numBytes);
    void OnClientMessageReceived(kNet::packet_id_t, kNet::message_id_t id, const char *data, size_t numBytes);

protected:
    void ResizeTexture(int width, int height, bool updateAfter);

    void FocusBrowser();
    void UnfocusBrowser();

    /// Returns if this entity is in view of currently active 
    /// camera and if browser rendering should be updated.
    bool IsInView();
    
    /// Returns if this entity is in rendering distance.
    bool IsInDistance();

    /// Returns the distance to the active camera.
    int DistanceToCamera(Entity *renderTarget = 0);
    
    /// Invalidate rendering.
    void Invalidate(int width = 0, int height = 0);
    
    /// MeshmoonComponents sets permission to run this component.
    /// This limits the processes we have running at any given time.
    void SetRunPermission(bool permitted);
    
    quint16 IpcPort();
    quint16 FreeIpcPort();
    QString IpcId(quint16 port);

    QProcess *ipcProcess;
    QTcpSocket *ipcSocket;
    QSharedMemory ipcMemory;
    
private slots:
    void ApplyMaterial(bool force = false);
    void BlitToTexture(const QList<QRect> &dirtyRects, const QList<QByteArray> &buffers);

    // IPC
    void OnIpcProcessStarted();
    void OnIpcProcessClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void OnIpcProcessError(QProcess::ProcessError error);
    void OnIpcConnected();
    void OnIpcError(QAbstractSocket::SocketError socketError);
    void OnIpcData();
    void OnIpcPaint(const QList<QRect> &dirtyRects);

private:   
    /// IComponent override.
    void AttributesChanged();

    void Init();
    void Reset(bool immediateShutdown);
    
    void InitInput();
    void CheckInput();
    void ResetInput();

    void InitBrowser();
    void ResetBrowser(bool immediateShutdown);

    void InitRendering();
    void ResetRendering();
    
    void RemoveMaterial();
    void RestoreMaterials(EC_Mesh *mesh);
    
    TundraLogic::TundraLogicModule *TundraLogic() const;
    TundraLogic::Client *TundraClient() const;
    TundraLogic::Server *TundraServer() const;
    
    float ActualTextureScale(int width = 0, int height = 0);
    QSize ActualTextureSize(int width = 0, int height = 0);
    
    std::string textureName_;
    std::string materialName_;
    
    EntityWeakPtr currentTarget_;
    int currentSubmesh_;
    int currentDistance_;

    int connectionRetries_;

    bool inView_;
    bool inDistance_;
    bool controlling_;
    bool permissionRun_;
    
    float updateDelta_;
    float syncDelta_;
    
    ECWebBrowserNetwork::MsgMouseMove syncMoveMsg_;

    struct InputState
    {
        bool focus;
        bool clickFocus;
        InputContextPtr inputContext;

        InputState();
        void Reset();
    };
    InputState inputState_;

    QString LC;
    QString currentUrl_;
};
COMPONENT_TYPEDEFS(WebBrowser);
