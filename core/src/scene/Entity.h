#pragma once

#include <entt/entt.hpp>

#include "Scene.h"
#include "core/Assert.h"

namespace pbe {

   template<typename... T>
   void remove_all_with_filter(entt::registry& registry, entt::entity entt) {
      for (auto [id, storage] : registry.storage()) {
         if (((storage.type() != entt::type_id<T>()) && ...)) {
            storage.remove(entt);
         }
      }
   }

   class CORE_API Entity {
   public:
      Entity() = default;
      Entity(entt::entity id, Scene* scene);

      template<typename T, typename...Cs>
      T& Add(Cs&&... cs) {
         ASSERT(!Has<T>());
         return scene->registry.emplace<T>(id, std::forward<Cs>(cs)...);
      }

      template<typename T, typename...Cs>
      void Remove() {
         ASSERT(!Has<T>());
         scene->registry.erase<T>(id);
      }

      template<typename... Exclude>
      void RemoveAll() {
         remove_all_with_filter<Exclude...>(scene->registry, id);
      }

      template<typename T>
      bool Has() const {
         return TryGet<T>() != nullptr;
      }

      template<typename T>
      T& Get() {
         ASSERT(Has<T>());
         return scene->registry.get<T>(id);
      }

      template<typename T>
      const T& Get() const {
         ASSERT(Has<T>());
         return scene->registry.get<T>(id);
      }

      template<typename T>
      T* TryGet() {
         return scene->registry.try_get<T>(id);
      }

      template<typename T>
      const T* TryGet() const {
         return scene->registry.try_get<T>(id);
      }

      template<typename T>
      T& GetOrAdd() {
         if (auto c = TryGet<T>()) {
            return *c;
         }
         return Add<T>();
      }

      void DestroyImmediate();

      bool Valid() const {
         return id != entt::null;
      }

      operator bool() const { return Valid(); }
      bool operator==(const Entity&) const = default;

      entt::entity GetID() const { return id; }
      Scene* GetScene() const { return scene; }

      const char* GetName() const;
      UUID GetUUID() const;

   private:
      entt::entity id{ entt::null };
      Scene* scene{};
   };

}
