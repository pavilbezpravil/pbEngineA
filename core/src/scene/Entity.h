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

   // todo: remove
   template<typename T>
   concept Entity_HasOwner = requires(T a) {
      { a.owner };
   };

   struct SceneTransformComponent;

   using EntityID = entt::entity;
   constexpr EntityID NullEntityID = entt::null;

   class CORE_API Entity {
   public:
      Entity() = default;
      Entity(EntityID id, Scene* scene);

      template<typename T, typename...Cs>
      decltype(auto) Add(Cs&&... cs) {
         ASSERT(!Has<T>());

         if constexpr (std::is_empty_v<T>) {
            return scene->registry.emplace<T>(id, std::forward<Cs>(cs)...);
         } else {
            // todo: remove branch
            auto& component = scene->registry.emplace<T>(id, std::forward<Cs>(cs)...);
            // not best way to set owner, but it works fine)
            if constexpr (Entity_HasOwner<T>) {
               component.owner = *this;
            }
            return component;
         }
      }

      template<typename T, typename...Cs>
      decltype(auto) AddOrReplace(Cs&&... cs) {
         return scene->registry.emplace_or_replace<T>(id, std::forward<Cs>(cs)...);
      }

      template<typename T, typename...Cs>
      void Remove() {
         ASSERT(Has<T>());
         scene->registry.erase<T>(id);
      }

      template<typename... Exclude>
      void RemoveAll() {
         remove_all_with_filter<Exclude...>(scene->registry, id);
      }

      template<typename... Type>
      bool Has() const {
         return scene->registry.all_of<Type...>(id);
      }

      template<typename... Type>
      bool HasAny() const {
         return scene->registry.any_of<Type...>(id);
      }

      template<typename... Type>
      decltype(auto) Get() {
         ASSERT(Has<Type...>());
         return scene->registry.get<Type...>(id);
      }

      template<typename... Type>
      decltype(auto) Get() const {
         ASSERT(Has<Type...>());
         return scene->registry.get<Type...>(id);
      }

      template<typename T>
      T* TryGet() {
         return scene->registry.try_get<T>(id);
      }

      template<typename T>
      const T* TryGet() const {
         return scene->registry.try_get<T>(id);
      }

      template<typename T, typename...Cs>
      T& GetOrAdd(Cs&&... cs) {
         if (auto c = TryGet<T>()) {
            return *c;
         }
         return Add<T>(std::forward<Cs>(cs)...);
      }

      template<typename T>
      void MarkComponentUpdated() {
         ASSERT(Has<T>());
         scene->registry.patch<T>(id);
      }

      void DestroyDelayed(bool withChilds = true);
      void DestroyImmediate(bool withChilds = true);

      bool Enabled() const;
      void Enable();
      void Disable();
      void EnableToggle();

      bool Valid() const {
         return id != NullEntityID && scene->registry.valid(id);
      }

      operator bool() const { return Valid(); } // todo: include Enabled?
      bool operator==(const Entity&) const = default;

      EntityID GetID() const { return id; }
      Scene* GetScene() const { return scene; }

      SceneTransformComponent& GetTransform();
      const SceneTransformComponent& GetTransform() const;

      const char* GetName() const;
      UUID GetUUID() const;

   private:
      EntityID id = NullEntityID;
      Scene* scene = nullptr;
   };

   constexpr Entity NullEntity = {};

}
