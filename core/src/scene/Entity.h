#pragma once

#include <entt/entt.hpp>

#include "Scene.h"
#include "core/Assert.h"
#include "core/UUID.h"


class Entity {
public:
   Entity() = default;
   Entity(entt::entity id, Scene* scene);

   template<typename T, typename...Cs>
   void Add(Cs&&... cs) {
      ASSERT(!Has<T>());
      scene->registry.emplace<T>(id, std::forward<Cs>(cs)...);
   }

   template<typename T>
   bool Has() const {
      T* c = scene->registry.try_get<T>(id);
      return c != nullptr;
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
   T* GetPtr() {
      return scene->registry.try_get<T>(id);
   }

   template<typename T>
   const T* GetPtr() const {
      return scene->registry.try_get<T>(id);
   }

   bool Valid() const {
      return id != entt::null;
   }

private:
   entt::entity id{ entt::null };
   Scene* scene{};
};
