#pragma once

#include "core/Core.h"

// #include <entt/entt.hpp>
// #include "Scene.h"
// #include "core/Assert.h"

namespace pbe {

   class Scene;

   class CORE_API System {
   public:
      virtual ~System() = default;

      virtual void OnUpdate(float dt) {}

      // virtual void OnConstruct(entt::registry& registry, entt::entity entity) {}
      // virtual void OnDeconstruct(entt::registry& registry, entt::entity entity) {}

      // template<typename T>
      // T& GetSceneData() {
      //    ASSERT(m_SceneData.find(typeid(T).hash_code()) != m_SceneData.end());
      //    return *(T*)m_SceneData[typeid(T).hash_code()];
      // }
      //
      // template<typename T>
      // void SetSceneData(T& data) {
      //    m_SceneData[typeid(T).hash_code()] = &data;
      // }

      void SetScene(Scene* scene) { pScene = scene; }

   protected:
      Scene* pScene = nullptr;
   };

}
