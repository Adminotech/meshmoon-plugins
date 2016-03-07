/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"
#include "RocketSounds.h"
#include "RocketSettings.h"
#include "RocketPlugin.h"
#include "Framework.h"

#include "AssetAPI.h"
#include "IAssetTransfer.h"
#include "LoggingFunctions.h"

#include "AudioAPI.h"
#include "SoundChannel.h"
#include "AudioAsset.h"

#include "MemoryLeakCheck.h"

RocketSounds::RocketSounds(RocketPlugin *plugin) :
    plugin_(plugin),
    muted_(false)
{
    // Init sound map
    sounds_["error-beep"] = SoundErrorBeep;
    sounds_["click1"] = SoundClick1;
    sounds_["click2"] = SoundClick2;
    sounds_["minimize"] = SoundMinimize;
    
    soundRefs_[SoundErrorBeep] = "http://meshmoon.data.s3.amazonaws.com/sounds/error-beep.ogg";
    soundRefs_[SoundClick1] = "http://meshmoon.data.s3.amazonaws.com/sounds/click1.ogg";
    soundRefs_[SoundClick2] = "http://meshmoon.data.s3.amazonaws.com/sounds/click2.ogg";
    soundRefs_[SoundMinimize] = "http://meshmoon.data.s3.amazonaws.com/sounds/minimize.ogg";
    
    connect(plugin_->Settings(), SIGNAL(SettingsApplied(const ClientSettings*)), this, SLOT(OnSettingsApplied(const ClientSettings*)));
}

RocketSounds::~RocketSounds()
{
    UnloadSounds();
}

void RocketSounds::PlaySound(const QString soundName)
{
    PlaySound(sounds_.value(soundName.toLower(), SoundNone));
}

void RocketSounds::PlaySound(Sound sound)
{
    if (sound != SoundNone)
        LoadAndPlaySound(soundRefs_.value(sound, ""));
}

void RocketSounds::PlayErrorBeep()
{
    LoadAndPlaySound(soundRefs_.value(SoundErrorBeep, ""));
}

void RocketSounds::LoadAndPlaySound(const QString &soundRef)
{
    if (muted_ || soundRef.isEmpty())
        return;

    AssetPtr asset = plugin_->GetFramework()->Asset()->GetAsset(soundRef);
    if (asset.get() && asset->Type() != "Audio")
    {
        LogWarning("[RocketSounds]: Cannot load and play " + soundRef + ", type is not Audio.");
        return;   
    }

    // This sound has been loaded once from the source. Just play it.
    if (asset.get() && refsLoadedFromSource_.contains(soundRef))
    {
        plugin_->GetFramework()->Audio()->PlaySound(asset);
        return;
    }

    // Load the sound from the source.
    if (plugin_->GetFramework()->Asset()->GetResourceTypeFromAssetRef(soundRef) != "Audio")
    {
        LogWarning("[RocketSounds]: Cannot load and play " + soundRef + ", type is not Audio.");
        return;
    }

    AssetTransferPtr soundTransfer = plugin_->GetFramework()->Asset()->RequestAsset(soundRef);
    connect(soundTransfer.get(), SIGNAL(Succeeded(AssetPtr)), SLOT(OnSoundLoadedForPlayback(AssetPtr)));

    if (!refsLoadedFromSource_.contains(soundRef))
        refsLoadedFromSource_ << soundRef;
}

QStringList RocketSounds::AvailableSoundsNames() const
{
    return sounds_.keys();
}

void RocketSounds::OnSettingsApplied(const ClientSettings *settings)
{
    muted_ = !settings->enableSounds;
}

void RocketSounds::UnloadSounds()
{
    foreach(const QString &soundRef, soundRefs_.values())
        plugin_->GetFramework()->Asset()->ForgetAsset(soundRef, false);
}

void RocketSounds::OnSoundLoadedForPlayback(AssetPtr asset)
{
    if (muted_)
        return;

    if (!dynamic_cast<AudioAsset*>(asset.get()))
    {
        LogWarning("[RocketSounds]: Cannot play sound after loading, type not Audio!");
        return;
    }
    
    plugin_->GetFramework()->Audio()->PlaySound(asset);
}
