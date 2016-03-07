
#include "StableHeaders.h"

#include "MeshmoonSceneValidator.h"
#include "Framework.h"
#include "Scene.h"

QString MeshmoonSceneValidator::RocketEnvironmentEntityName = "RocketEnvironment";

void MeshmoonSceneValidator::Validate(Scene *scene)
{
    if (!scene)
    {
        LogError("[MeshmoonSceneValidator]: Passed in scene is null!");
        return;
    }

    /** Rocket environment entity. We only want one of these to go the clients.
        Prioritize as follows:
          1) Replicated non-temporary entity (from the main scene)
          2) Replicated temporary entity (from a layer)
        Anything but the first found entity will be removed from scene. */
    EntityList rocketEnvEnts = scene->FindEntitiesByName(RocketEnvironmentEntityName);
    if (rocketEnvEnts.size() > 1)
    {
        LogInfo(QString("[MeshmoonSceneValidator]: Found %1 %2 Entities:").arg(rocketEnvEnts.size()).arg(RocketEnvironmentEntityName));
        for(EntityList::const_iterator it = rocketEnvEnts.begin(); it != rocketEnvEnts.end(); ++it)
        {
            const EntityPtr ent = (*it);
            LogInfo("    " + QString::number(ent->Id()) + QString(" temporary=%1 replicated=%2 sky=%3 water=%4 fog=%5 envlight=%6 shadowsetup=%7 components=%8")
                .arg(ent->IsTemporary() ? "true" : "false").arg(ent->IsReplicated() ? "true" : "false")
                .arg(ent->Component("EC_Sky") || ent->Component("EC_SkyX") || ent->Component("EC_MeshmoonSky") ? "true" : "false")
                .arg(ent->Component("EC_WaterPlane") || ent->Component("EC_Hydrax") || ent->Component("EC_MeshmoonWater") ? "true" : "false")
                .arg(ent->Component("EC_Fog") ? "true" : "false").arg(ent->Component("EC_EnvironmentLight") ? "true" : "false")
                .arg(ent->Component("EC_SceneShadowSetup") ? "true" : "false").arg(ent->NumComponents()));
        }

        // 1) Replicated non-temporary with the most components 
        Entity *keep = 0;
        for(EntityList::const_iterator it = rocketEnvEnts.begin(); it != rocketEnvEnts.end(); ++it)
        {
            const EntityPtr ent = (*it);
            if (!ent->IsTemporary() && ent->IsReplicated())
            {
                // Only pick this one if it has more components than the previous one.
                if (keep && ent->NumComponents() < keep->NumComponents())
                    continue;
                keep = ent.get();
            }
        }
        // 2) Replicated temporary with the most components 
        if (!keep)
        {
            for(EntityList::const_iterator it = rocketEnvEnts.begin(); it != rocketEnvEnts.end(); ++it)
            {
                const EntityPtr ent = (*it);
                if (ent->IsReplicated())
                {
                    // Only pick this one if it has more components than the previous one.
                    if (keep && ent->NumComponents() < keep->NumComponents())
                        continue;
                    keep = ent.get();
                }
            }
        }
        // Report
        if (keep)
        {
            LogInfo("Preserving " + QString::number(keep->Id()) + QString(" temporary=%1 replicated=%2 sky=%3 water=%4 fog=%5 envlight=%6 shadowsetup=%7 components=%8")
                .arg(keep->IsTemporary() ? "true" : "false").arg(keep->IsReplicated() ? "true" : "false")
                .arg(keep->Component("EC_Sky") || keep->Component("EC_SkyX") || keep->Component("EC_MeshmoonSky") ? "true" : "false")
                .arg(keep->Component("EC_WaterPlane") || keep->Component("EC_Hydrax") || keep->Component("EC_MeshmoonWater") ? "true" : "false")
                .arg(keep->Component("EC_Fog") ? "true" : "false").arg(keep->Component("EC_EnvironmentLight") ? "true" : "false")
                .arg(keep->Component("EC_SceneShadowSetup") ? "true" : "false").arg(keep->NumComponents()));

            Entity::ComponentMap comps = keep->Components();
            for(Entity::ComponentMap::const_iterator it = comps.begin(); it != comps.end(); ++it)
            {
                ComponentPtr comp = it->second;
                if (comp.get())
                    LogInfo(QString("    typeId=%1 typeName=%2 name=%3").arg(comp->TypeId()).arg(comp->TypeName()).arg(comp->Name()));
            }

            bool warned = false;
            for(EntityList::iterator it = rocketEnvEnts.begin(); it != rocketEnvEnts.end(); ++it)
            {
                EntityPtr ent = (*it);
                if (ent->Id() != keep->Id())
                {
                    if (!warned)
                    {
                        LogWarning("[MeshmoonSceneValidator]: Deleting extraneous " + RocketEnvironmentEntityName + " Entities.");
                        LogWarning("Having multiple environment entities may cause problems on the clients.");
                        warned = true;
                    }
                    LogWarning(QString::number(ent->Id()) +  (ent->IsTemporary() ? " This Entity is temporary, so it probably originated from a loaded Meshmoon Layer." : ""));

                    entity_id_t id = ent->Id();
                    ent.reset();
                    scene->RemoveEntity(id);
                }
            }
        }
    }
}
