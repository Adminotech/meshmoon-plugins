/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "MeshmoonCommonPluginApi.h"

#include "FrameworkFwd.h"
#include "TundraProtocolModuleFwd.h"
#include "common/MeshmoonCommon.h"

#include <QObject>
#include <QString>
#include <QNetworkReply>

class MeshmoonLayerProcessor;

/// Provides per user layer management for scripting.
/** MeshmoonLayers is exposed to scripting as 'meshmoonserver.layers'.
    @ingroup MeshmoonServer */
class MESHMOON_COMMON_API MeshmoonLayers : public QObject
{
Q_OBJECT

public:
    /// @cond PRIVATE

    explicit MeshmoonLayers(Framework *framework);
    ~MeshmoonLayers();

    /** The following public, but not documented, functionality
        is used internally by MeshmoonServerPlugin. These functions
        are not and should not be exposed to scripting. */    

    /// Clear layer state.
    void RemoveAll();
    bool Remove(u32 id);

    /// Add new layer.
    /** @param Layer to add.
        @return True if added, false if already existed and the layer was ignored. */
    bool Add(const Meshmoon::SceneLayer &layer);
    bool Add(const Meshmoon::SceneLayerList &layers);

    /// Check if layer exists.
    /** @param Layer to check.
        @return True if added, false if already existed and the layer was ignored. */    
    bool Exists(const Meshmoon::SceneLayer &layer) const;

    /// Check if this reply is for a known layer scene data.
    bool CheckLayerDownload(QNetworkReply *reply);

    /// Load layers to the currently active scene.
    void Load();

    /// Unloads all layers from the currently active scene.
    /** @note Does not remove the layer from the state, use RemoveAll for that if desirable. */
    bool UnloadAll();

    /// Unloads layer by id.
    /** @note Does not remove the layer from the state, use Remove for that if desirable. */
    bool Unload(u32 id);

    /// @endcond

public slots:
    /// Returns all layers.
    const Meshmoon::SceneLayerList &Layers() const;

    /// Returns layer by id.
    /** @note Returns a copy, modifying the returned object wont invoke changes. */
    Meshmoon::SceneLayer Layer(u32 id) const;
    
    /// Returns layer by name.
    /** @note Returns a copy, modifying the returned object wont invoke changes. */
    Meshmoon::SceneLayer Layer(const QString &name) const;
    
    /// Returns if all layers source material has been downloaded.
    bool AllDownloaded() const;

    /// Returns if all layers source material has been downloaded.
    bool AllLoaded() const;
    
    /// Shows or hides layer of @c id from connection of @c connectionId.
    /** @param Target client connection id.
        @param Target layer id.
        @param Target visibility.
        @return False if connection or layer does not exist. */
    bool UpdateClientLayerState(u32 connectionId, u32 layerId, bool visible);

    /// Shows or hides layer of @c id from @c connection.
    /** @param Target client connection.
        @param Target layer id.
        @param Target visibility.
        @return False if connection or layer does not exist. */
    bool UpdateClientLayerState(UserConnection *connection, u32 layerId, bool visible);

signals:
    /// Emitted when a layer is loaded to the server.
    /** @note You cannot assume layers are loaded when a script starts execution.
        @see AllDownloaded and AllLoaded. */
    void LayerLoaded(const Meshmoon::SceneLayer &layer);

    /// Emitted when a layers visibility changes for a particular client.
    void LayerVisibilityChanged(UserConnection *connection, const Meshmoon::SceneLayer &layer, bool visible);
    
    /// Emitted when layer is removed.
    void LayerRemoved(const Meshmoon::SceneLayer &layer);
    
    /// Emitted when all layers have been loaded to the server.
    /** @note You cannot assume layers are loaded when a script starts execution.
        @see AllDownloaded and AllLoaded. */
    void LayersLoaded();

private:
    QString LC;

    Framework *framework_;
    MeshmoonLayerProcessor *processor_;

    Meshmoon::SceneLayerList layers_;

    /// Unloads layer by notifying all clients about it being removed.
    bool Unload(const Meshmoon::SceneLayer &layer);
};
Q_DECLARE_METATYPE(MeshmoonLayers*)
