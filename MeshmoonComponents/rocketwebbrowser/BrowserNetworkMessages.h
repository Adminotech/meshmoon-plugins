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

namespace BrowserProtocol
{
    enum MessageType
    {
        Invalid = 0,
        // Browser
        Url,
        FocusIn,
        FocusOut,
        Close,
        LoadStart,
        LoadEnd,
        // Rendering
        Resize,
        Invalidate,
        DirtyRects,
        // Input
        MouseMoveEvent,
        MouseButtonEvent,
        KeyboardEvent  
    };
    
    struct TypedMessage
    {
        MessageType type;
        
        TypedMessage(MessageType type_) :
            type(type_)
        {
        }
        
        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(type);
            return data;
        }
    };
    
    struct TypedStringMessage
    {
        MessageType type;
        QByteArray str;

        TypedStringMessage(MessageType type_, const QString &str_) :
            type(type_),
            str(str_.toUtf8())
        {
        }

        TypedStringMessage(QDataStream &s)
        {
            // Type has already been parsed as the header!
            s >> str;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(type);
            s << str;
            return data;
        }
    };
    
    struct DirtyRectsMessage
    {
        QList<QRect> rects;

        DirtyRectsMessage() {}
        DirtyRectsMessage(QDataStream &s)
        {
            int count = 0;
            s >> count;
            for(int i=0; i<count; ++i)
            {
                int x, y, w, h;
                s >> x;
                s >> y;
                s >> w;
                s >> h;
                rects << QRect(x, y, w, h);
            }
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(DirtyRects)
              << rects.size();
            foreach(const QRect &rect, rects)
            {
                s << rect.x()
                  << rect.y()
                  << rect.width()
                  << rect.height();
            }
            return data;
        }
    };
    
    struct UrlMessage
    {
        QByteArray url;
        
        UrlMessage(const QString &url_) :
            url(url_.toUtf8())
        {
        }

        UrlMessage(QDataStream &s)
        {
            s >> url;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Url)
              << url;
            return data;
        }
    };
    
    struct ResizeMessage
    {
        int width;
        int height;
        
        ResizeMessage(int width_, int height_) :
            width(width_),
            height(height_)
        {
        }

        ResizeMessage(QDataStream &s)
        {
            s >> width;
            s >> height;
        }
        
        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(Resize)
              << width
              << height;
            return data;
        }
    };
   
    struct MouseMoveMessage
    {
        int x;
        int y;

        MouseMoveMessage(int x_, int y_) :
            x(x_),
            y(y_)
        {
        }

        MouseMoveMessage(QDataStream &s)
        {
            s >> x;
            s >> y;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(MouseMoveEvent)
              << x
              << y;
            return data;
        }
    };
     
    struct MouseButtonMessage
    {
        int x;
        int y;
        qint16 pressCount;
        bool left;
        bool right;
        bool mid;
        
        MouseButtonMessage(int x_, int y_, qint16 pressCount_, bool left_ = false, bool right_ = false, bool mid_ = false) :
            x(x_),
            y(y_),
            pressCount(pressCount_),
            left(left_),
            right(right_),
            mid(mid_)
        {
        }
        
        MouseButtonMessage(QDataStream &s)
        {
            s >> x;
            s >> y;
            s >> pressCount;
            s >> left;
            s >> right;
            s >> mid;
        }
        
        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(MouseButtonEvent)
              << x
              << y
              << pressCount
              << left
              << right
              << mid;
            return data;
        }
    };
    
#ifdef ROCKET_CEF3_ENABLED
    struct KeyboardMessage
    {
        int key;
        int nativeKey;
        int character;
        int characterNoModifiers;
        Qt::KeyboardModifiers modifiers;
        quint8 type;
        
        KeyboardMessage(int key_, int nativeKey_, int character_, int characterNoModifiers_, Qt::KeyboardModifiers modifiers_, quint8 type_) :
            key(key_),
            nativeKey(nativeKey_),
            character(character_),
            characterNoModifiers(characterNoModifiers_),
            modifiers(modifiers_),
            type(type_)
        {
        }

        KeyboardMessage(QDataStream &s)
        {
            s >> key;
            s >> nativeKey;
            s >> character;
            s >> characterNoModifiers;
            quint32 modifiersNum = 0;
            s >> modifiersNum;
            modifiers = static_cast<Qt::KeyboardModifiers>(modifiersNum);
            s >> type;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(KeyboardEvent)
              << key
              << nativeKey
              << character
              << characterNoModifiers
              << static_cast<quint32>(modifiers)
              << type;
            return data;
        }
    };
#else
    struct KeyboardMessage
    {
#ifdef Q_WS_WIN
        int key;
        int modifier;
        quint8 type;
        
        KeyboardMessage(int key_, int modifier_, quint8 type_) :
            key(key_),
            modifier(modifier_),
            type(type_)
        {
        }
        
        KeyboardMessage(QDataStream &s)
        {
            s >> key;
            s >> modifier;
            s >> type;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(KeyboardEvent)
              << key
              << modifier
              << type;
            return data;
        }
#elif (__APPLE__)
        int key;
        int character;
        int characterNoModifiers;
        int modifier;
        quint8 type;
        
        KeyboardMessage(int key_, int character_, int characterNoModifiers_, int modifier_, quint8 type_) :
            key(key_),
            character(character_),
            characterNoModifiers(characterNoModifiers_),
            modifier(modifier_),
            type(type_)
        {
        }

        KeyboardMessage(QDataStream &s)
        {
            s >> key;
            s >> character;
            s >> characterNoModifiers;
            s >> modifier;
            s >> type;
        }

        QByteArray Serialize() const
        {
            QByteArray data;
            QDataStream s(&data, QIODevice::WriteOnly);
            s << static_cast<quint8>(KeyboardEvent)
              << key
              << character
              << characterNoModifiers
              << modifier
              << type;
            return data;
        }
#endif
    };
#endif
}
