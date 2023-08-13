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

   struct DisableMarker {};

   class CORE_API Scene {
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

      // todo: to private
      void Duplicate(Entity& dst, const Entity& src, bool copyUUID);
      Entity Duplicate(const Entity& entity);

      bool EntityEnabled(const Entity& entity) const;
      void EntityEnable(Entity& entity);
      void EntityDisable(Entity& entity);

      void DestroyDelayed(Entity entity, bool withChilds = true);
      void DestroyImmediate(Entity entity, bool withChilds = true);

      void DestroyDelayedEntities();

      template<typename Type, typename... Other, typename... Exclude>
      const auto View(entt::exclude_t<DisableMarker, Exclude...> excludes = entt::exclude_t<DisableMarker>{}) const {
         return registry.view<Type, Other...>(excludes);
      }

      template<typename Type, typename... Other, typename... Exclude>
      auto View(entt::exclude_t<DisableMarker, Exclude...> excludes = entt::exclude_t<DisableMarker>{}) {
         return registry.view<Type, Other...>(excludes);
      }

      template<typename Type, typename... Other>
      int CountEntitiesWithComponents() const {
         int count = 0;
         View<Type, Other...>().each([&](auto& c) {
            ++count;
         });
         return count;
      }

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

      void OnStart();
      void OnUpdate(float dt);
      void OnStop();

      Entity FindByName(std::string_view name);

      uint EntitiesCount() const;

      PhysicsScene* GetPhysics() { return pPhysics.get(); }

      Own<Scene> Copy() const;

      Own<DbgRend> dbgRend; // todo:

      static Scene* GetCurrentDeserializedScene();

   private:
      entt::registry registry;
      entt::entity rootEntityId { entt::null };

      std::unordered_map<uint64, entt::entity> uuidToEntities;

      // todo: move to scene component?
      Own<PhysicsScene> pPhysics;

      friend Entity;
   };

   CORE_API void SceneSerialize(std::string_view path, Scene& scene);
   CORE_API Own<Scene> SceneDeserialize(std::string_view path);

   // todo: move to Entity.h
   CORE_API void EntitySerialize(Serializer& ser, const Entity& entity);
   CORE_API void EntityDeserialize(const Deserializer& deser, Scene& scene);

}
