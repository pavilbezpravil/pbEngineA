#pragma once

#include <entt/entt.hpp>

namespace pbe {

   class Entity;

   class Scene {
   public:

      Entity Create(std::string_view name = {});

      template<typename T>
      auto GetEntitiesWith() {
         return registry.view<T>();
      }

   private:
      entt::registry registry;

      friend Entity;

      friend void SceneSerialize(std::string_view path, Scene& scene);
   };

}
