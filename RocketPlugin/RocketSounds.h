/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "RocketFwd.h"
#include "FrameworkFwd.h"
#include "AudioFwd.h"
#include "AssetFwd.h"

#include <QObject>
#include <QStringList>
#include <QPair>
#include <QHash>

/// Provides audio playback capabilities.
/** Light wrapper around AudioAPI for better usability and built in sounds bank.
    @ingroup MeshmoonRocket */
class RocketSounds : public QObject
{
Q_OBJECT

public:
/// @cond PRIVATE
    RocketSounds(RocketPlugin *plugin);
    ~RocketSounds();
/// @endcond

    enum Sound
    {
        SoundNone,
        SoundErrorBeep,
        SoundClick1,
        SoundClick2,
        SoundMinimize
    };
    
public slots:
    /// Play sound with name.
    void PlaySound(const QString soundName);

    /// Play sound with enum.
    void PlaySound(Sound sound);

    /// Play error beep.
    void PlayErrorBeep();

    /// Loads a sound and plays it once its loaded.
    /** This function can be called with any valid sound asset ref. It is not limited to what PlaySound function can play.
        Optimized so that we only check once from the network if the sound resource has changed, after that it will just play it. 
        @note When Rocket disconnects from a world all assets, include sounds, will be unloaded and we have to re-check out cache expiration. */
    void LoadAndPlaySound(const QString &soundRef);
    
    /// Get list of available sound names.
    QStringList AvailableSoundsNames() const;

private slots:
    void UnloadSounds();
    void OnSoundLoadedForPlayback(AssetPtr asset);
    void OnSettingsApplied(const ClientSettings *settings);

private:
    RocketPlugin *plugin_;

    // Is Rocket audio disabled.
    bool muted_;
    
    // Sound name to enum.
    QHash<QString, Sound> sounds_;
    
    // Sound enum to asset ref.
    QHash<Sound, QString> soundRefs_;
    
    // Refs that have been loaded from source once.
    QList<QString> refsLoadedFromSource_;
};
Q_DECLARE_METATYPE(RocketSounds*)
