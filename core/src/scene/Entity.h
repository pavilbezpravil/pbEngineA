#pragma once

#include <entt/entt.hpp>

#include "Scene.h"
#include "core/Assert.h"

namespace pbe {

   class Entity {
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
      T& GetOrCreate() {
         if (auto c = TryGet<T>()) {
            return *c;
         }
         return Add<T>();
      }

      bool Valid() const {
         return id != entt::null;
      }

      operator bool() const { return Valid(); }

      entt::entity GetID() const { return id; }

   private:
      entt::entity id{ entt::null };
      Scene* scene{};
   };

}
