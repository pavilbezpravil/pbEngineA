#pragma once

#include "ECSScene.h"
#include "core/Core.h"
#include "core/Ref.h"
#include "core/Type.h"
#include "math/Types.h"


namespace pbe {

   class System;
   class PhysicsScene;
   struct Deserializer;
   struct Serializer;

   class DbgRend;
   class UUID;

   class Entity;

   class CORE_API Scene : public ECSScene {
   public:
      Scene(bool withRoot = true);
      ~Scene();

      Entity Create(std::string_view name = {});

      void DestroyDelayed(EntityID entityID);
      void DestroyImmediate(EntityID entityID);

      Entity GetEntity(UUID uuid);

      Entity GetRootEntity();
      void SetRootEntity(const Entity& entity);

      Entity Duplicate(const Entity& entity);

      // Delayed
      bool EntityEnabled(EntityID entityID) const;
      void EntityEnable(EntityID entityID, bool enable, bool withChilds = true);

      struct ComponentEventHandlers {
         using OnEventF = std::function<void(Entity&)>;
         using OnEventMultipleF = std::function<void(Scene&)>;

         OnEventF onEnable;
         OnEventF onDisable;

         OnEventMultipleF onEnableMultiple;
         OnEventMultipleF onDisableMultiple;

         std::vector<OnEventF> onUpdates;
      };

      std::unordered_map<TypeID, ComponentEventHandlers> componentEventMap;

      template<typename Comp>
      void OnComponentConstruct(entt::registry& registry, EntityID entityID) {
         if (EntityEnabled(entityID)) {
            auto it = componentEventMap.find(GetTypeID<Comp>());
            // todo: if this function was called, handler must be registered
            if (it != componentEventMap.end()) {
               Entity entity{ entityID, this };
               it->second.onEnable(entity);
            }
         }
      }

      template<typename Comp>
      void OnComponentDestroy(entt::registry& registry, EntityID entityID) {
         if (EntityEnabled(entityID)) {
            auto it = componentEventMap.find(GetTypeID<Comp>());
            if (it != componentEventMap.end()) {
               Entity entity{ entityID, this };
               it->second.onDisable(entity);
            }
         }
      }

      template<typename Comp>
      void OnComponentUpdate(entt::registry& registry, EntityID entityID) {
         if (EntityEnabled(entityID)) {
            auto it = componentEventMap.find(GetTypeID<Comp>());
            if (it != componentEventMap.end()) {
               Entity entity{ entityID, this };

               for (auto& onUpdate : it->second.onUpdates) {
                  onUpdate(entity);
               }
            }
         }
      }

      template<typename Comp, typename OnEnableT, typename OnDisableT>
      void RegisterOnComponentEnableDisable(OnEnableT&& onEnableF, OnDisableT&& onDisableF) {
         ComponentEventHandlers handlers;

         handlers.onEnable = onEnableF;
         handlers.onDisable = onDisableF;

         handlers.onEnableMultiple = [onEnableF](Scene& scene) {
            for (auto e : scene.ViewAll<Comp, DelayedEnableMarker>()) {
               Entity entity{ e, &scene };
               onEnableF(entity);
            }
         };

         handlers.onDisableMultiple = [onDisableF](Scene& scene) {
            for (auto e : scene.ViewAll<Comp, DelayedDisableMarker>()) {
               Entity entity{ e, &scene };
               onDisableF(entity);
            }
         };

         auto typeID = GetTypeID<Comp>();
         ASSERT(!componentEventMap.contains(typeID));
         componentEventMap[typeID] = std::move(handlers);

         registry.on_construct<Comp>().connect<&Scene::OnComponentConstruct<Comp>>(this);
         registry.on_destroy<Comp>().connect<&Scene::OnComponentDestroy<Comp>>(this);
      }

      template<typename Comp, typename OnUpdateT>
      void RegisterOnComponentUpdate(OnUpdateT&& onUpdateF) {
         auto typeID = GetTypeID<Comp>();
         ASSERT(componentEventMap.contains(typeID));
         ComponentEventHandlers& handlers = componentEventMap[typeID];

         if (handlers.onUpdates.empty()) {
            registry.on_update<Comp>().connect<&Scene::OnComponentUpdate<Comp>>(this);
         }

         handlers.onUpdates.push_back(onUpdateF);
      }

      // process all pending changes with entities (remove\add entity, remove\add component, etc)
      void OnSync();

      // call this every frame
      void OnTick();

      void OnStart();
      void OnUpdate(float dt);
      void OnStop();

      void AddSystem(Own<System>&& system);

      Entity FindByName(std::string_view name);

      uint EntitiesCount() const;

      // todo:
      PhysicsScene* GetPhysics() { return (PhysicsScene*)systems[0].get(); }

      Own<Scene> Copy() const;

      Own<DbgRend> dbgRend; // todo:

      static Scene* GetCurrentDeserializedScene();

   private:
      EntityID rootEntityId = NullEntityID;

      std::unordered_map<uint64, entt::entity> uuidToEntities;

      // todo: move to scene component?
      std::vector<Own<System>> systems;

      // todo: to private
      Entity CreateWithUUID(UUID uuid, const Entity& parent, std::string_view name = {});

      void ProcessDelayedEnable();

      void EntityDisableImmediate(Entity& entity);

      struct DuplicateContext {
         entt::entity enttEntity{ entt::null };
         bool enabled = false;
      };

      void DuplicateHierEntitiesWithMap(Entity& dst, const Entity& src, bool copyUUID, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap);
      void Duplicate(Entity& dst, const Entity& src, bool copyUUID, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap);
      void DuplicateEntityEnable(Entity& root, std::unordered_map<UUID, DuplicateContext>& hierEntitiesMap);

      friend Entity;
      friend CORE_API Own<Scene> SceneDeserialize(std::string_view path);
      friend CORE_API void EntityDeserialize(const Deserializer& deser, Scene& scene);
   };

   CORE_API void SceneSerialize(std::string_view path, Scene& scene);
   CORE_API Own<Scene> SceneDeserialize(std::string_view path);

   // todo: move to Entity.h
   CORE_API void EntitySerialize(Serializer& ser, const Entity& entity);
   CORE_API void EntityDeserialize(const Deserializer& deser, Scene& scene);

}
