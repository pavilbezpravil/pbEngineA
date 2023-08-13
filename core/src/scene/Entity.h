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

   template<typename T>
   concept Entity_HasOwner = requires(T a) {
      { a.owner };
   };

   struct SceneTransformComponent;

   class CORE_API Entity {
   public:
      Entity() = default;
      Entity(entt::entity id, Scene* scene);

      template<typename T>
      void AddMarker() {
         ASSERT(!Has<T>());
         scene->registry.emplace<T>(id);
      }

      template<typename T, typename...Cs>
      T& Add(Cs&&... cs) {
         ASSERT(!Has<T>());
         auto& component = scene->registry.emplace<T>(id, std::forward<Cs>(cs)...);
         // not best way to set owner, but it works fine)
         if constexpr (Entity_HasOwner<T>) {
            component.owner = *this;
         }
         return component;
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

      template<typename T>
      bool Has() const {
         return scene->registry.all_of<T>(id);
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

      template<typename T, typename...Cs>
      T& GetOrAdd(Cs&&... cs) {
         if (auto c = TryGet<T>()) {
            return *c;
         }
         return Add<T>(std::forward<Cs>(cs)...);
      }

      void DestroyDelayed(bool withChilds = true);
      void DestroyImmediate(bool withChilds = true);

      bool Enabled() const;
      void Enable();
      void Disable();
      void EnableToggle();

      bool Valid() const {
         return id != entt::null;
      }

      operator bool() const { return Valid(); } // todo: include Enabled?
      bool operator==(const Entity&) const = default;

      entt::entity GetID() const { return id; }
      Scene* GetScene() const { return scene; }

      SceneTransformComponent& GetTransform();
      const SceneTransformComponent& GetTransform() const;

      const char* GetName() const;
      UUID GetUUID() const;

   private:
      entt::entity id{ entt::null };
      Scene* scene{};
   };

}
