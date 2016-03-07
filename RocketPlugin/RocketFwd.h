/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   RocketPluginFwd.h
    @brief  Forward declarations for commonly used Meshmoon Rocket classes. */

#pragma once

#include <QList>
#include <QDebug>
#include <QPointer>
#include <QHash>
#include <QString>
#include <QMetaType>

/** @defgroup MeshmoonRocket Meshmoon Rocket
    Meshmoon Rocket classes and functionality.
*/

/// @cond PRIVATE

// Rocket/Meshmoon classes
class RocketPlugin;
class RocketNetworking;
class RocketAssetMonitor;
class RocketTaskbar;
class RocketAuthObject;
class RocketUpdater;
class RocketMenu;
class RocketAvatarEditor;
class RocketUserAuthenticator;
class RocketSounds;
class RocketSettings;
class RocketPresis;
class RocketCaveManager;
class RocketOculusManager;
class RocketBuildEditor;
class RocketBlockPlacer;
class RocketComponentConfigurationDialog;
class RocketScenePackager;
class RocketZipWorker;
class RocketOcclusionManager;
class RocketReporter;
class RocketNotifications;
class RocketFileSystem;
class RocketFileDialogSignaler;

class MeshmoonBackend;
class MeshmoonUser;
class MeshmoonWorld;

// Structs
struct ClientSettings;
struct MeshmoonStorageItem;

// Asset library
class MeshmoonAssetLibrary;
class MeshmoonAsset;
class MeshmoonAssetSelectionDialog;

// Storage
class MeshmoonStorage;
class MeshmoonStorageAuthenticationMonitor;
class MeshmoonStorageOperationMonitor;
class MeshmoonStorageItemWidget;
class RocketStorageSelectionDialog;
class RocketStorageInfoDialog;
class RocketStorageFileDialog;
class RocketStorageAuthDialog;
class RocketStorageSceneImporter;

// Asset editors
class IRocketAssetEditor;
class IRocketSyntaxHighlighter;
class RocketTextEditor;
class RocketMaterialEditor;
class RocketParticleEditor;
class RocketTextureEditor;
class RocketOgreSceneRenderer;

// Widgets
class RocketLobby;
class RocketOfflineLobby;
class RocketSceneWidget;
class RocketServerWidget;
class RocketPromotionWidget;
class RocketAvatarWidget;
class RocketLayersWidget;
class RocketStorageWidget;
class RocketServerFilter;
class RocketLobbyActionButton;
class RocketLoaderWidget;
class RocketBuildWidget;
class RocketBuildContextWidget;
class RocketNotificationWidget;
class RocketAnimationEngine;

// Legacy Adminotech
/// @todo Rename and cleanup.
class AdminotechSlideManagerWidget;
class AdminotechSlideControlsWidget;

// Rendering
class IRocketGPUProgramConstant;
class RocketGPUPRogramGenerator;

// Qt
class QNetworkAccessManager;
class QNetworkReply;
class QGraphicsWebView;
class QParallelAnimationGroup;
class QPropertyAnimation;
class QGraphicsProxyWidget;
class QScriptEngine;
class QAction;
class QMessageBox;

// Framework
class Framework;

/// @endcond

// Typedefs
typedef QList<MeshmoonUser*> MeshmoonUserList;
typedef QList<MeshmoonAsset*> MeshmoonAssetList;
typedef QList<MeshmoonStorageItem> MeshmoonStorageItemList;
typedef QList<MeshmoonStorageItemWidget*> MeshmoonStorageItemWidgetList;
typedef QList<RocketServerWidget*> ServerWidgetList;
typedef QList<RocketPromotionWidget*> PromotedWidgetList;
typedef QPair<QString, QString> MeshmoonLibrarySource; ///< Meshmoon library source. Maps name to library source url.
Q_DECLARE_METATYPE(MeshmoonLibrarySource)
typedef QList<MeshmoonLibrarySource > MeshmoonLibrarySourceList; ///< Meshmoon library source list.
