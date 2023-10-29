#pragma once

#include "Scene.h"
#include "core/Assert.h"

namespace pbe {

   struct SceneTransformComponent;

   class CORE_API Entity {
   public:
      Entity() = default;
      Entity(EntityID id, Scene* scene);

      template<typename T, typename...Cs>
      decltype(auto) Add(Cs&&... cs) { return scene->AddComponent<T>(id, std::forward<Cs>(cs)...); }

      template<typename T, typename...Cs>
      decltype(auto) AddOrReplace(Cs&&... cs) { return scene->AddOrReplaceComponent<T>(id, std::forward<Cs>(cs)...); }

      template<typename T, typename...Cs>
      void Remove() { scene->RemoveComponent<T>(id); }

      template<typename... Type>
      bool Has() const { return scene->HasComponent<Type...>(id); }

      template<typename... Type>
      bool HasAny() const { return scene->HasAnyComponent<Type...>(id); }

      template<typename... Type>
      decltype(auto) Get() { return scene->GetComponent<Type...>(id); }

      template<typename... Type>
      decltype(auto) Get() const { return scene->GetComponent<Type...>(id); }

      template<typename T>
      T* TryGet() { return scene->TryGetComponent<T>(id); }

      template<typename T>
      const T* TryGet() const { return scene->TryGetComponent<T>(id); }

      template<typename T, typename...Cs>
      T& GetOrAdd(Cs&&... cs) { return scene->GetOrAddComponent<T>(id, std::forward<Cs>(cs)...); }

      template<typename T>
      void MarkComponentUpdated() { scene->PatchComponent<T>(id); }

      void DestroyDelayed();

      bool Enabled() const;
      void Enable(bool enable);
      void EnableToggle();

      bool Valid() const {
         return id != NullEntityID && scene->registry.valid(id);
      }

      operator bool() const { return Valid(); } // todo: include Enabled?
      bool operator==(const Entity&) const = default;

      EntityID GetEntityID() const { return id; }
      Scene* GetScene() const { return scene; }

      SceneTransformComponent& GetTransform();
      const SceneTransformComponent& GetTransform() const;

      const char* GetName() const;
      UUID GetUUID() const;

      template<typename Component>
      static Entity GetAnyWithComponent(const Scene& scene) {
         return Entity{ scene.GetAnyWithComponent<Component>(), const_cast<Scene*>(&scene) };
      }

      template<typename Component>
      static Entity GetAnyWithComponent(Scene& scene) {
         return Entity{ scene.GetAnyWithComponent<Component>(), &scene };
      }

   private:
      EntityID id = NullEntityID;
      Scene* scene = nullptr;
   };

   constexpr Entity NullEntity = {};

}
