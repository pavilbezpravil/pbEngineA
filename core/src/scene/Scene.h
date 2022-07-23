#pragma once

#include <entt/entt.hpp>

#include "core/Ref.h"

namespace pbe {
   class UUID;

   class Entity;

   class Scene {
   public:

      Entity Create(std::string_view name = {});
      Entity CreateWithUUID(UUID uuid,  std::string_view name = {});

      void DestroyImmediate(Entity entity);

      template<typename Component, typename... Other>
      auto GetEntitiesWith() {
         return registry.view<Component, Other...>();
      }

      Entity FindByName(std::string_view name);

      int EntitiesCount() const {
         return (int)registry.size();
      }

   private:
      entt::registry registry;

      friend Entity;

      friend void SceneSerialize(std::string_view path, Scene& scene);
   };

   Own<Scene> SceneDeserialize(std::string_view path);

}
