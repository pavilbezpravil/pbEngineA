#pragma once

#include <entt/entt.hpp>

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
};
