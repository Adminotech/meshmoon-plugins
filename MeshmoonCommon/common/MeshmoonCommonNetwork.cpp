/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#include "StableHeaders.h"

#include "MeshmoonCommon.h"

namespace Meshmoon
{
    namespace Network
    {
        // MsgAssetReload
        
        MsgAssetReload::MsgAssetReload()
        {
            InitToDefault();
        }

        MsgAssetReload::MsgAssetReload(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void MsgAssetReload::InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }
        
        // MsgUserPermissions

        MsgUserPermissions::MsgUserPermissions()
        {
            InitToDefault();
        }

        MsgUserPermissions::MsgUserPermissions(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void MsgUserPermissions::InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }
        
        // MsgLayerUpdate
        
        MsgLayerUpdate::MsgLayerUpdate()
        {
            InitToDefault();
        }

        MsgLayerUpdate::MsgLayerUpdate(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void MsgLayerUpdate::InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }
        
        // MsgPermissionDenied

        MsgPermissionDenied::MsgPermissionDenied()
        {
            InitToDefault();
        }

        MsgPermissionDenied::MsgPermissionDenied(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void MsgPermissionDenied::InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }

        // MsgStorageInformation
        
        MsgStorageInformation::MsgStorageInformation()
        {
            InitToDefault();
        }

        MsgStorageInformation::MsgStorageInformation(const char *data, size_t numBytes)
        {
            InitToDefault();
            kNet::DataDeserializer dd(data, numBytes);
            DeserializeFrom(dd);
        }

        void MsgStorageInformation::InitToDefault()
        {
            reliable = defaultReliable;
            inOrder = defaultInOrder;
            priority = defaultPriority;
        }
    }
}
