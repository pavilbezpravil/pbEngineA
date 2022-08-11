#pragma once

#include <entt/entt.hpp>

#include "core/Core.h"
#include "core/Ref.h"
#include "math/Types.h"


namespace pbe {
   class DbgRend;
   class UUID;

   class Entity;

   class CORE_API Scene {
   public:
      Scene();
      ~Scene();

      Entity Create(std::string_view name = {});
      Entity CreateWithUUID(UUID uuid,  std::string_view name = {});

      void Duplicate(Entity dst, Entity src);
      Entity Duplicate(Entity entity);

      void DestroyImmediate(Entity entity);

      template<typename Component, typename... Other>
      auto GetEntitiesWith() const {
         return registry.view<Component, Other...>();
      }

      template<typename Component, typename... Other>
      auto GetEntitiesWith() {
         return registry.view<Component, Other...>();
      }

      void OnStart();
      void OnUpdate(float dt);
      void OnStop();

      Entity FindByName(std::string_view name);

      uint EntitiesCount() const;

      Own<Scene> Copy(); // todo: const

      Own<DbgRend> dbgRend; // todo:

   private:
      entt::registry registry;

      friend Entity;
   };

   CORE_API void SceneSerialize(std::string_view path, Scene& scene);
   CORE_API Own<Scene> SceneDeserialize(std::string_view path);

}
