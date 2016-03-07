/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include "CoreTypes.h"

#include "kNet/DataDeserializer.h"
#include "kNet/DataSerializer.h"

namespace ECWebBrowserNetwork
{
    static const bool defaultReliable = true;
    static const bool defaultInOrder = true;
    static const u32 defaultPriority = 100;

    struct MsgMouseMove
    {      
        MsgMouseMove()
        {
            InitToDefault();
        }

        MsgMouseMove(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
            send = false;
        }

        enum { messageID = 500 };
        static inline const char * const Name() { return "EC_WebBrowserMsgMouseMove"; }

        bool reliable;
        bool inOrder;
        u32 priority;

        bool send;

        u32 entId;
        u32 compId;
        u16 x;
        u16 y;

        inline size_t Size() const
        {
            return 12;
        }

        inline void SerializeTo(kNet::DataSerializer &dst) const
        {
            dst.Add<u32>(entId);
            dst.Add<u32>(compId);
            dst.Add<u16>(x);
            dst.Add<u16>(y);
        }

        inline void DeserializeFrom(kNet::DataDeserializer &src)
        {
            entId = src.Read<u32>();
            compId = src.Read<u32>();
            x = src.Read<u16>();
            y = src.Read<u16>();
        }
    };

    struct MsgMouseButton
    {      
        MsgMouseButton()
        {
            InitToDefault();
        }

        MsgMouseButton(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }

        enum { messageID = 501 };
        static inline const char * const Name() { return "EC_WebBrowserMsgMouseButton"; }

        bool reliable;
        bool inOrder;
        u32 priority;

        u32 entId;
        u32 compId;
        u16 x;
        u16 y;
        u8 button;
        s16 pressCount;

        inline size_t Size() const
        {
            return 15;
        }

        inline void SerializeTo(kNet::DataSerializer &dst) const
        {
            dst.Add<u32>(entId);
            dst.Add<u32>(compId);
            dst.Add<u16>(x);
            dst.Add<u16>(y);
            dst.Add<u8>(button);
            dst.Add<s16>(pressCount);
        }

        inline void DeserializeFrom(kNet::DataDeserializer &src)
        {
            entId = src.Read<u32>();
            compId = src.Read<u32>();
            x = src.Read<u16>();
            y = src.Read<u16>();
            button = src.Read<u8>();
            pressCount = src.Read<s16>();
        }
    };

    struct MsgBrowserAction
    {      
        MsgBrowserAction()
        {
            InitToDefault();
        }

        MsgBrowserAction(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }

        enum { messageID = 502 };
        static inline const char * const Name() { return "EC_WebBrowserMsgBrowserAction"; }

        bool reliable;
        bool inOrder;
        u32 priority;

        u32 entId;
        u32 compId;
        u8 type;
        std::vector<u8> payload;

        inline size_t Size() const
        {
            return 9 + 2 + payload.size();
        }

        inline void SerializeTo(kNet::DataSerializer &dst) const
        {
            dst.Add<u32>(entId);
            dst.Add<u32>(compId);
            dst.Add<u8>(type);
            dst.Add<u16>((u16)payload.size());
            if (payload.size() > 0)
                dst.AddArray<u8>(&payload[0], (u32)payload.size());
        }

        inline void DeserializeFrom(kNet::DataDeserializer &src)
        {
            entId = src.Read<u32>();
            compId = src.Read<u32>();
            type = src.Read<u8>();
            payload.resize(src.Read<u16>());
            if (payload.size() > 0)
                src.ReadArray<u8>(&payload[0], payload.size());
        }
    };
}
