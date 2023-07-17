#pragma once

#include <entt/entt.hpp>

#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"


namespace pbe {
   class PhysicsScene;
   struct Deserializer;
   struct Serializer;

   class DbgRend;
   class UUID;

   class Entity;

   class CORE_API Scene {
   public:
      Scene();
      ~Scene();

      Entity Create(std::string_view name = {});
      Entity CreateWithUUID(UUID uuid,  std::string_view name = {});

      Entity GetEntity(UUID uuid);
      Entity GetRootEntity();

      void Duplicate(Entity& dst, const Entity& src);
      Entity Duplicate(const Entity& entity);

      void DestroyImmediate(Entity entity);

      template<typename Component, typename... Other>
      auto GetEntitiesWith() const {
         return registry.view<Component, Other...>();
      }

      template<typename Component, typename... Other>
      auto GetEntitiesWith() {
         return registry.view<Component, Other...>();
      }

      // todo: return multiple
      template<typename Component>
      Component* GetAnyWithComponent() {
         auto view = registry.view<Component>();
         if (view.empty()) {
            return nullptr;
         } else {
            return registry.try_get<Component>(view.front());
         }
      }

      void OnStart();
      void OnUpdate(float dt);
      void OnStop();

      Entity FindByName(std::string_view name);

      uint EntitiesCount() const;

      PhysicsScene* GetPhysics() { return pPhysics.get(); }

      Own<Scene> Copy(); // todo: const

      Own<DbgRend> dbgRend; // todo:

      static Scene* GetCurrentDeserializedScene();

   private:
      entt::registry registry;
      entt::entity rootEntityId;

      std::unordered_map<uint64, entt::entity> uuidToEntities;


      // todo: move to scene component?
      Own<PhysicsScene> pPhysics;

      void OnConstructRigidBody(entt::registry& registry, entt::entity entity);
      void OnDestroyRigidBody(entt::registry& registry, entt::entity entity);

      friend Entity;
   };

   CORE_API void SceneSerialize(std::string_view path, Scene& scene);
   CORE_API Own<Scene> SceneDeserialize(std::string_view path);

   // todo: move to Entity.h
   CORE_API void EntitySerialize(Serializer& ser, const Entity& entity);
   CORE_API void EntityDeserialize(const Deserializer& deser, Scene& scene);

}
