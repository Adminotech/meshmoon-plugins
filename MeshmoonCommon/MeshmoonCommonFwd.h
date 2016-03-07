/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   
    @brief   */

#pragma once

#include <QList>

class MeshmoonCommonPlugin;

class MeshmoonSpaceLoader;

class MeshmoonLayers;
class MeshmoonLayerProcessor;

namespace Meshmoon
{
    class Layer;
    class Space;
    
    typedef QList<Meshmoon::Layer*> LayerList;
    typedef QList<Meshmoon::Space*> SpaceList;
}
