#pragma once

#include "ECSScene.h"
#include "core/Core.h"
#include "core/Ref.h"
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
      Entity Create(const Entity& parent, std::string_view name = {});

      // todo: to private
      Entity CreateWithUUID(UUID uuid, const Entity& parent, std::string_view name = {});

      Entity GetEntity(UUID uuid);

      Entity GetRootEntity();
      void SetRootEntity(const Entity& entity);

      Entity Duplicate(const Entity& entity);

      // Delayed
      bool EntityEnabled(const Entity& entity) const;
      void EntityEnable(Entity& entity, bool withChilds = true);
      void EntityDisable(Entity& entity, bool withChilds = true);

      void ProcessDelayedEnable();

      void DestroyDelayed(Entity entity, bool withChilds = true);
      void DestroyImmediate(Entity entity, bool withChilds = true);

      void DestroyDelayedEntities();

      template<typename Component>
      Entity GetAnyWithComponent() const {
         auto entity = View<Component>().front();
         if (entity == entt::null) {
            return Entity{};
         } else {
            // todo: const_cast((
            return Entity{ entity, const_cast<Scene*>(this) };
         }
      }

      template<typename Component>
      Entity GetAnyWithComponent() {
         auto entity = View<Component>().front();
         if (entity == entt::null) {
            return Entity{};
         } else {
            return Entity{ entity, this };
         }
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
   };

   CORE_API void SceneSerialize(std::string_view path, Scene& scene);
   CORE_API Own<Scene> SceneDeserialize(std::string_view path);

   // todo: move to Entity.h
   CORE_API void EntitySerialize(Serializer& ser, const Entity& entity);
   CORE_API void EntityDeserialize(const Deserializer& deser, Scene& scene);

}
