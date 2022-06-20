#pragma once

#include <entt/entt.hpp>

#include "core/UUID.h"
#include "math/Types.h"
#include "core/Type.h"

class Entity;

class ComponentList {
public:

   using ComponentID = TypeID;
   using ComponentFunc = std::function<void(Entity&)>;

   static ComponentList& Get();

   void RegisterComponent(ComponentID componentID, ComponentFunc&& func);

   std::unordered_map<ComponentID, ComponentFunc> components2;
};

#define DECL_COMPONENT(Component) \
   static const char* GetName() { \
      return STRINGIFY(Component); \
   }

struct UUIDComponent {
   UUID uuid;

   DECL_COMPONENT(UUIDComponent);
};

struct TagComponent {
   std::string tag;

   DECL_COMPONENT(TagComponent);
};

struct SceneTransformComponent {
   vec3 position;
   quat rotation;

   DECL_COMPONENT(SceneTransformComponent);
};

struct TestCustomUIComponent {
   int integer;
   float floating;
   vec3 float3;

   DECL_COMPONENT(TestCustomUIComponent);
};
