/**
    @author Admino Technologies Ltd.

    Copyright 2013 Admino Technologies Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include <QString>
#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QRect>
#include <QMetaType>

namespace MediaPlayerProtocol
{
    enum MessageType
    {
        Invalid = 0,
        // Player
        Source,
        Play,
        Pause,
        PlayPauseToggle,
        Stop,
        Seek,
        Volume,
        // State
        StateUpdate,
        PlayerError,
        // Rendering
        Resize,
        Dirty,
        // Boolean switchers
        Looping
    };
    
    // Maps to libvlc_state_t
    enum PlaybackState
    {
        NothingSpecial = 0,
        Opening,
        Buffering,
        Playing,
        Paused,
        Stopped,
        Ended,
        Error
    };

    inline QString PlaybackStateToString(PlaybackState playbackState)
    {
        switch(playbackState)
        {
        case NothingSpecial: return "NothingSpecial";
        case Opening: return "Opening";
        case Buffering: return "Buffering";
        case Playing: return "Playing";
        case Paused: return "Paused";
        case Stopped: return "Stopped";
        case Ended: return "Ended";
        case Error: return "Error";
        default: return "";
        }
    }

    struct PlayerState
    {
        PlaybackState state;
        qint64 mediaTimeMsec;
        qint64 mediaLengthMsec;
        bool seekable;
        bool pausable;
        
        PlayerState() :
            state(Stopped),
            mediaTimeMsec(0),
            mediaLengthMsec(0),
            seekable(false),
            pausable(false)
        {
        }
    };
    
    struct TypedMessage
    {
        MessageType type;
        
        TypedMessage(MessageType type_) : type(type_) {}
         
        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(type);
            return data;
        }
    };
    
    struct TypedBooleanMessage
    {
        MessageType type;
        bool value;

        TypedBooleanMessage(MessageType type_, bool value_) :
            type(type_),
            value(value_)
        {
        }

        TypedBooleanMessage(MessageType type_, QDataStream &s)
        {
            type = type_;
            s >> value;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(type)
              << value;
            return data;
        }
    };

    struct SeekMessage
    {
        qint64 msec;

        SeekMessage(qint64 msec_) : msec(msec_) {}

        SeekMessage(QDataStream &s)
        {
            s >> msec;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Seek)
              << msec;
            return data;
        }
    };
    
    struct VolumeMessage
    {
        quint8 volume; // Accepted volume range is 0-100

        VolumeMessage(int volume_) : volume(static_cast<quint8>(volume_)) {}

        VolumeMessage(QDataStream &s)
        {
            s >> volume;
        }
        
        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Volume)
              << volume;
            return data;
        }
        
        int VolumeToInt() const
        {
            return static_cast<int>(volume);
        }
    };
    
    struct SourceMessage
    {
        QByteArray source;

        SourceMessage(const QString &source_) : source(source_.toUtf8()) {}

        SourceMessage(QDataStream &s)
        {
            s >> source;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Source)
              << source;
            return data;
        }
    };
    
    struct PlayerErrorMessage
    {
        QByteArray error;

        PlayerErrorMessage(const QString &error_) : error(error_.toUtf8()) {}

        PlayerErrorMessage(QDataStream &s)
        {
            s >> error;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(PlayerError)
              << error;
            return data;
        }
    };
    
    struct ResizeMessage
    {
        int width;
        int height;
        QString ipcMemoryId;

        ResizeMessage(int width_, int height_, const QString &ipcMemoryId_) :
            width(width_),
            height(height_),
            ipcMemoryId(ipcMemoryId_)
        {
        }

        ResizeMessage(QDataStream &s)
        {
            s >> width;
            s >> height;
            s >> ipcMemoryId;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Resize)
              << width
              << height
              << ipcMemoryId;
            return data;
        }
    };

    struct StateUpdateMessage
    {
        PlayerState state;

        StateUpdateMessage(const PlayerState &state_) : state(state_) {}

        StateUpdateMessage(QDataStream &s)
        {
            quint8 stateTemp = 0;
            s >> stateTemp;
            state.state = (PlaybackState)stateTemp;
            s >> state.mediaTimeMsec;
            s >> state.mediaLengthMsec;
            s >> state.seekable;
            s >> state.pausable;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(StateUpdate)
              << static_cast<quint8>(state.state)
              << state.mediaTimeMsec
              << state.mediaLengthMsec
              << state.seekable
              << state.pausable;
            return data;
        }
    };
}
Q_DECLARE_METATYPE(MediaPlayerProtocol::PlaybackState)
