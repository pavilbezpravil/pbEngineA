#pragma once

#include <entt/entt.hpp>
#include "core/Assert.h"
#include "math/Types.h"

namespace pbe {

   using EntityID = entt::entity;
   constexpr EntityID NullEntityID = entt::null;

   struct DelayedDisableMarker {};
   struct DelayedEnableMarker {};
   struct DisableMarker {};

   class CORE_API ECSScene {
   public:
      template<typename T, typename...Cs>
      decltype(auto) AddComponent(EntityID id, Cs&&... cs) {
         ASSERT(!HasComponent<T>(id));
         return registry.emplace<T>(id, std::forward<Cs>(cs)...);
      }

      template<typename T, typename...Cs>
      decltype(auto) AddOrReplaceComponent(EntityID id, Cs&&... cs) {
         return registry.emplace_or_replace<T>(id, std::forward<Cs>(cs)...);
      }

      template<typename T>
      void RemoveComponent(EntityID id) {
         ASSERT(HasComponent<T>(id));
         registry.erase<T>(id);
      }

      template<typename... Type>
      bool HasComponent(EntityID id) const {
         return registry.all_of<Type...>(id);
      }

      template<typename... Type>
      bool HasAnyComponent(EntityID id) const {
         return registry.any_of<Type...>(id);
      }

      template<typename... Type>
      decltype(auto) GetComponent(EntityID id) {
         ASSERT(HasComponent<Type...>(id));
         return registry.get<Type...>(id);
      }

      template<typename... Type>
      decltype(auto) GetComponent(EntityID id) const {
         ASSERT(HasComponent<Type...>(id));
         return registry.get<Type...>(id);
      }

      template<typename T>
      T* TryGetComponent(EntityID id) {
         return registry.try_get<T>(id);
      }

      template<typename T>
      const T* TryGetComponent(EntityID id) const {
         return registry.try_get<T>(id);
      }

      template<typename T, typename...Cs>
      T& GetOrAddComponent(EntityID id, Cs&&... cs) {
         if (auto c = TryGetComponent<T>(id)) {
            return *c;
         }
         return AddComponent<T>(id, std::forward<Cs>(cs)...);
      }

      template<typename T>
      void PatchComponent(EntityID id) {
         ASSERT(HasComponent<T>(id));
         registry.patch<T>(id);
      }

      template<typename Component>
      void ClearComponent() {
         registry.clear<Component>();
      }

      // todo: it strange and may be not needed
      template<typename... Exclude>
      void RemoveAllComponents(EntityID id) {
         for (auto [storageNameID, storage] : registry.storage()) {
            if (((storage.type() != entt::type_id<Exclude>()) && ...)) {
               storage.remove(id);
            }
         }
      }

      // todo: mb DelayedDisableMarker add on 'exludes'?
      template<typename Type, typename... Other, typename... Exclude>
      const auto View(entt::exclude_t<DisableMarker, Exclude...> excludes = entt::exclude_t<DisableMarker>{}) const {
         return registry.view<Type, Other...>(excludes);
      }

      template<typename Type, typename... Other, typename... Exclude>
      auto View(entt::exclude_t<DisableMarker, Exclude...> excludes = entt::exclude_t<DisableMarker>{}) {
         return registry.view<Type, Other...>(excludes);
      }

      template<typename Type, typename... Other, typename... Exclude>
      auto ViewAll(entt::exclude_t<Exclude...> excludes = entt::exclude_t{}) {
         return registry.view<Type, Other...>(excludes);
      }

      template<typename Type, typename... Other>
      uint CountEntitiesWithComponents() const {
         uint count = 0;
         View<Type, Other...>().each([&](auto& c) {
            ++count;
            });
         return count;
      }

   protected:
      entt::registry registry;
   };

}
