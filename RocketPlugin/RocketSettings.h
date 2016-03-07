/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "RocketSceneWidget.h"
#include "FrameworkFwd.h"

#include "ui_RocketSettingsWidget.h"

#include <QObject>
#include <QTextStream>

// ClientSettings

struct ClientSettings
{
    enum GraphicsMode
    {
        GraphicsNone = 0,
        GraphicsCustom,
        GraphicsUltra,
        GraphicsHigh,
        GraphicsMedium,
        GraphicsLow
    };

    bool enableSounds;
    bool showHostingNews;
    bool showHostingStats;
    GraphicsMode graphicsMode;
    
    bool detectRenderingPerformance;
    bool detectNetworkPerformance;

    int numProcessesWebBrowser;
    int numProcessesMediaPlayer;

    QString proxyHost;
    int proxyPort;

    ClientSettings() :
        enableSounds(true),
        showHostingNews(true),
        showHostingStats(true),
        graphicsMode(GraphicsLow),
        detectRenderingPerformance(true),
        detectNetworkPerformance(true),
        numProcessesWebBrowser(0),
        numProcessesMediaPlayer(0),
        proxyPort(0)
    {
    }
};
Q_DECLARE_METATYPE(ClientSettings)
Q_DECLARE_METATYPE(ClientSettings::GraphicsMode)

// RocketSettings

class RocketSettings : public QObject
{
    Q_OBJECT

public:
    explicit RocketSettings(RocketPlugin *plugin);
    ~RocketSettings();

public slots:
    /// Open the settings widget.
    /** @note Cannot be opened if we have an active connection to a server. */
    void Open();

    /// Close the settings widget.
    /** @param Should changes be saved 
        @param Should the center content of our lobby be restored. */
    void Close(bool saveChanges = true, bool restoreMainView = true);

    /// Is widget currently visible.
    bool IsVisible();

    /// Toggle widget visibility.
    void ToggleSettingsWidget();

    /// Emits current settings via SettingsApplied signal.
    void EmitSettings();

    /// Returns current settings.
    ClientSettings CurrentSettings() const;

    /// Returns current graphics mode.
    ClientSettings::GraphicsMode CurrentGraphicsMode() const;

    /// Returns the view distance, used for camera far clip adjustment.
    int ViewDistance() const;
    
    /// Dumps settings a formatted text to given stream.
    void DumpSettings(QTextStream &stream);

private slots:
    /// Save settings to config.
    void SaveSettings(bool emitSettingChanged = true);

    /// Load settings from config.
    void LoadSettings();

    /// Downgrades the graphics setting with one level.
    /** @note Does nothing if current mode if custom or low.
        @note Don't invoke this if the client widget is visible */
    void DowngradeGraphicsMode();

    void DelayedShow();
    void OnSceneResized(const QRectF &rect);
    
    void SelectUltra();
    void SelectHigh();
    void SelectMedium();
    void SelectLow();
    
    /// maxLevel must be dividable by 2. includeQuality gets the extra [Quality] qualifier into account.
    QString GetHighestSupportedFSAA(int maxLevel, bool includeQuality);
    
    void OnCustomGfxSettingsToggled();
    void OnShadowQualitySelected(int index);
    
    void OnCaveEnabledToggled(bool enabled);
    void OnOculusEnabledToggled(bool enabled);
    void OnOculusScreenToggled(bool checked);
    void OnProxyTypeChanged(int currentIndex);
    
    bool ApplyHttpProxy();
    //void OnHeadTrackingEnabledToggled(bool enabled);

signals:
    void SettingsApplied(const ClientSettings *settings);

private:
    void InitUi();
    void DetectRenderingOptions();

    RocketPlugin *plugin_;
    Framework *framework_;
    RocketSceneWidget *proxy_;
    Ui::RocketSettingsWidget ui_;
    
    // Various options that are read from 
    // the rendering system during runtime.
    QStringList fsaaSupportedModes_;
    bool vsyncSupported_;
    QString vsyncSupportedYes_;
    QString vsyncSupportedNo_;

    // Oculus Rift monitor buttons
    QList<QPushButton*> oculusMonitors_;
    
    const QString LC;
    bool notificationChecked_;
};
