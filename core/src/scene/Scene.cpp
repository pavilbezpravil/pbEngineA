#include "Scene.h"
#include "Entity.h"

Entity Scene::Create(std::string_view name) {
   auto id = registry.create();

   auto e = Entity{id, this};
   e.Add<UUIDComponent>();
   if (!name.empty()) {
      e.Add<TagComponent>(name.data());
   }

   e.Add<SceneTransformComponent>();

   return e;
}
