/**
    @author Adminotech Ltd.

    Copyright Adminotech Ltd.
    All rights reserved.

    @file   MeshmoonComponents.cpp
    @brief  MeshmoonComponents registers Meshmoon ECs and handles logic related to them. */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "MeshmoonComponents.h"

#include "EC_MeshmoonAvatar.h"
#include "EC_WebBrowser.h"
#include "EC_MediaBrowser.h"
#include "EC_MeshmoonTeleport.h"
#include "EC_Meshmoon3DText.h"
#ifdef ROCKET_UMBRA_ENABLED
#include "EC_MeshmoonOccluder.h"
#include "EC_MeshmoonCulling.h"
#endif

#include "Framework.h"
#include "Profiler.h"
#include "SceneAPI.h"
#include "ConfigAPI.h"

#include "IComponentFactory.h"
#include "TundraLogicModule.h"
#include "Client.h"

#include "Scene/Scene.h"
#include "Entity.h"

#include "AssetAPI.h"
#include "IAsset.h"

#include "OgreRenderingModule.h"
#include "OgreMeshAsset.h"
#include "OgreMaterialAsset.h"
#include "Renderer.h"
#include "EC_Placeable.h"

#include <OgreSubMesh.h>

#include <algorithm>

#include "MemoryLeakCheck.h"

MeshmoonComponents::MeshmoonComponents() :
    IModule("MeshmoonComponents"),
    processMonitorDelta_(0.0f),
    numMaxWebBrowserProcesses_(5),
    numMaxMediaPlayerProcesses_(5)
{
}

MeshmoonComponents::~MeshmoonComponents()
{
}

void MeshmoonComponents::Load()
{
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonAvatar>));
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_WebBrowser>));
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MediaBrowser>));
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonTeleport>));
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_Meshmoon3DText>));

#ifdef ROCKET_UMBRA_ENABLED
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonOccluder>));
    Fw()->Scene()->RegisterComponentFactory(MAKE_SHARED(GenericComponentFactory<EC_MeshmoonCulling>));
#endif
}

void MeshmoonComponents::Initialize()
{
    // Hook to client connected signal.
    if (!Fw()->IsHeadless() && !Fw()->HasCommandLineParameter("--server"))
    {
        TundraLogicModule *tundraModule = Fw()->Module<TundraLogicModule>();
        TundraLogic::Client *client = tundraModule ? tundraModule->GetClient().get() : 0;
        if (client)
        {
            connect(client, SIGNAL(Connected(UserConnectedResponseData*)), SLOT(OnClientConnected()));
            connect(client, SIGNAL(Disconnected()), SLOT(OnClientDisconnected()));
        }

        connect(Fw()->Scene(), SIGNAL(SceneCreated(Scene *, AttributeChange::Type)), SLOT(ConnectToScene(Scene *)));
        connect(Fw()->Scene(), SIGNAL(SceneAboutToBeRemoved(Scene *, AttributeChange::Type)), SLOT(DisconnectFromScene(Scene *)));
    }
}

template <typename BrowserType>
struct DistanceComparer : public std::binary_function<EntityWeakPtr, EntityWeakPtr, bool>
{
    const float3 cameraPos;
    QHash<entity_id_t, float> cache;

    DistanceComparer(const float3 &cameraPos_) : cameraPos(cameraPos_) {}

    inline bool operator()(const EntityWeakPtr &e1, const EntityWeakPtr &e2)
    {
        const bool e1Expired = e1.expired();
        const bool e2Expired = e2.expired();
        if (!e1Expired && e2Expired)
            return true;
        if (e1Expired && !e2Expired)
            return false;
        if (e1Expired && e2Expired)
            return false;

        const EntityPtr ent1 = e1.lock();
        const EntityPtr ent2 = e2.lock();

        float distanceToCamera1 = 0.0f;
        if (cache.contains(ent1->Id()))
            distanceToCamera1 = cache[ent1->Id()];
        else
        {
            BrowserType *m1 = ent1->Component<BrowserType>().get();
            Entity *target1 = m1 ? m1->RenderTarget().get() : 0;
            EC_Placeable *p1 = target1 ? target1->Component<EC_Placeable>().get() : 0;
            if (!p1)
                return false;

            distanceToCamera1 = p1->WorldPosition().DistanceSq(cameraPos);
            cache[ent1->Id()] = distanceToCamera1;
        }

        float distanceToCamera2 = 0.0f;
        if (cache.contains(ent2->Id()))
            distanceToCamera2 = cache[ent2->Id()];
        else
        {
            BrowserType *m2 = ent2->Component<BrowserType>().get();
            Entity *target2 = m2 ? m2->RenderTarget().get() : 0;
            EC_Placeable *p2 = target2 ? target2->Component<EC_Placeable>().get() : 0;
            if (!p2)
                return true;

            distanceToCamera2 = p2->WorldPosition().DistanceSq(cameraPos);
            cache[ent2->Id()] = distanceToCamera2;
        }

        return (distanceToCamera1 < distanceToCamera2);
    }
};

void MeshmoonComponents::Update(f64 frametime)
{
    if (Fw()->IsHeadless())
        return;

    processMonitorDelta_ += frametime;
    if (processMonitorDelta_ > 1.0)
    {
        processMonitorDelta_ = 0.0f;

        PROFILE(MeshmoonComponents_Order_Priority)

        Entity *cameraEnt = Fw()->Module<OgreRenderingModule>()->Renderer()->MainCamera();
        EC_Placeable *cameraPlaceable = cameraEnt != 0 ? cameraEnt->Component<EC_Placeable>().get() : 0;
        if (!cameraPlaceable)
            return;

        if (!webBrowsers.empty())
        {
            DistanceComparer<EC_WebBrowser> comparer(cameraPlaceable->WorldPosition());
            webBrowsers.sort(comparer);

            int idx = 0;
            for(WeakEntityList::iterator it = webBrowsers.begin(); it != webBrowsers.end();)
            {
                EntityWeakPtr entIter = (*it);
                EC_WebBrowser *browser = (!entIter.expired() ? entIter.lock()->Component<EC_WebBrowser>().get() : 0);
                if (browser)
                {
                    browser->SetRunPermission(idx < numMaxWebBrowserProcesses_);
                    ++idx;
                    ++it;
                }
                else
                    it = webBrowsers.erase(it);
            }
        }

        if (!mediaBrowsers.empty())
        {
            DistanceComparer<EC_MediaBrowser> comparer(cameraPlaceable->WorldPosition());
            mediaBrowsers.sort(comparer);

            int idx = 0;
            for(WeakEntityList::iterator it = mediaBrowsers.begin(); it != mediaBrowsers.end();)
            {
                EntityWeakPtr entIter = (*it);
                EC_MediaBrowser *browser = (!entIter.expired() ? entIter.lock()->Component<EC_MediaBrowser>().get() : 0);
                if (browser)
                {
                    browser->SetRunPermission(idx < numMaxMediaPlayerProcesses_);
                    ++idx;
                    ++it;
                }
                else
                    it = webBrowsers.erase(it);
            }
        }

        ELIFORP(MeshmoonComponents_Order_Priority)
    }
}

void MeshmoonComponents::OnClientConnected()
{
    numMaxWebBrowserProcesses_ = Fw()->Config()->Read("adminotech", "clientplugin", "numprocessesweb", 5).toInt();
    numMaxMediaPlayerProcesses_ = Fw()->Config()->Read("adminotech", "clientplugin", "numprocessesmedia", 5).toInt();
}

void MeshmoonComponents::OnClientDisconnected()
{
    webBrowsers.clear();
    mediaBrowsers.clear();
}

void MeshmoonComponents::EmitTeleportRequest(const QString &sceneId, const QString &pos, const QString &rot)
{
    emit TeleportRequest(sceneId, pos, rot);
}

void MeshmoonComponents::ConnectToScene(Scene *s)
{
    connect(s, SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentAdded(Entity*, IComponent*)));
    connect(s, SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentRemoved(Entity*, IComponent*)));
}

void MeshmoonComponents::DisconnectFromScene(Scene *s)
{
    disconnect(s, SIGNAL(ComponentAdded(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentAdded(Entity*, IComponent*)));
    disconnect(s, SIGNAL(ComponentRemoved(Entity*, IComponent*, AttributeChange::Type)),
        this, SLOT(OnComponentRemoved(Entity*, IComponent*)));

    for(WeakEntityList::iterator it = webBrowsers.begin(); it != webBrowsers.end();)
    {
        if ((*it).expired() || (*it).lock()->ParentScene() == s)
            it = webBrowsers.erase(it);
        else
            ++it;
    }

    for(WeakEntityList::iterator it = mediaBrowsers.begin(); it != mediaBrowsers.end();)
    {
        if ((*it).expired() || (*it).lock()->ParentScene() == s)
            it = mediaBrowsers.erase(it);
        else
            ++it;
    }
}

void MeshmoonComponents::OnComponentAdded(Entity *entity, IComponent *comp)
{
    if (comp->TypeId() == EC_WebBrowser::TypeIdStatic())
        webBrowsers.push_back(entity->shared_from_this());
    else if (comp->TypeId() == EC_MediaBrowser::TypeIdStatic())
        mediaBrowsers.push_back(entity->shared_from_this());
}

void MeshmoonComponents::OnComponentRemoved(Entity *entity, IComponent *comp)
{
    if (comp->TypeId() == EC_WebBrowser::ComponentTypeId)
    {
        for(WeakEntityList::iterator it = webBrowsers.begin(); it != webBrowsers.end();)
        {
            if ((*it).expired() || (*it).lock().get() == entity)
                it = webBrowsers.erase(it);
            else
                ++it;
        }
    }
    else if (comp->TypeId() == EC_MediaBrowser::TypeIdStatic())
    {
        for(WeakEntityList::iterator it = mediaBrowsers.begin(); it != mediaBrowsers.end();)
        {
            if ((*it).expired() || (*it).lock().get() == entity)
                it = mediaBrowsers.erase(it);
            else
                ++it;
        }
    }
}

extern "C"
{
    DLLEXPORT void TundraPluginMain(Framework *fw)
    {
        Framework::SetInstance(fw);
        fw->RegisterModule(new MeshmoonComponents());
    }
}
