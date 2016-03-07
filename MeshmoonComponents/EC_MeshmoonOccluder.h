/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file
    @brief   */

#pragma once

#ifdef ROCKET_UMBRA_ENABLED

#include "MeshmoonComponentsApi.h"
#include "IComponent.h"

#include "Entity.h"
#include "EC_Mesh.h"

#include "AssetReference.h"
#include "EntityReference.h"

class MESHMOON_COMPONENTS_API EC_MeshmoonOccluder : public IComponent
{
    Q_OBJECT
    COMPONENT_NAME("MeshmoonOccluder", 504) // Note this is the closed source EC Meshmoon range ID.

public:
    Q_PROPERTY(int tomeId READ gettomeId WRITE settomeId);
    DEFINE_QPROPERTY_ATTRIBUTE(int, tomeId);

    Q_PROPERTY(AssetReference meshRef READ getmeshRef WRITE setmeshRef);
    DEFINE_QPROPERTY_ATTRIBUTE(AssetReference, meshRef);

    Q_PROPERTY(unsigned int id READ getid WRITE setid);
    DEFINE_QPROPERTY_ATTRIBUTE(uint, id);

    Q_PROPERTY(bool occluder READ getoccluder WRITE setoccluder);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, occluder);

    Q_PROPERTY(bool target READ gettarget WRITE settarget);
    DEFINE_QPROPERTY_ATTRIBUTE(bool, target);

    /// @cond PRIVATE
    /// Do not directly allocate new components using operator new, but use the factory-based SceneAPI::CreateComponent functions instead.
    explicit EC_MeshmoonOccluder(Scene *scene);
    ~EC_MeshmoonOccluder();
    /// @endcond

    bool MeshReady();

public slots:
    void Show();

    void Hide();

    EC_Mesh* Mesh();

private:
    /// IComponent override
    void AttributesChanged();

    QString LC;

    bool lastState_;

    EC_Mesh *targetMesh_;
};
COMPONENT_TYPEDEFS(MeshmoonOccluder);
#endif
