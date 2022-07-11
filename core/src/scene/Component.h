#pragma once

#include <entt/entt.hpp>

#include "core/UUID.h"
#include "math/Types.h"
#include "core/Type.h"

namespace pbe {

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

   // todo:
#define COMPONENT_EXPLICIT_TEMPLATES_DECL(Component)
#define COMPONENT_EXPLICIT_TEMPLATE_DEF(Component)
// #define COMPONENT_EXPLICIT_TEMPLATES_DECL(Component) \
//    extern template struct entt::type_hash<Component>; \
//    extern template TypeID GetTypeID<Component>(); \
//    extern template decltype(auto) entt::registry::get<Component>(const entt::entity); \
//    extern template auto entt::registry::try_get<Component>(const entt::entity);
//
// #define COMPONENT_EXPLICIT_TEMPLATE_DEF(Component) \
//    template struct entt::type_hash<Component>; \
//    template TypeID GetTypeID<Component>(); \
//    template decltype(auto) entt::registry::get<Component>(const entt::entity); \
//    template auto entt::registry::try_get<Component>(const entt::entity);

   struct UUIDComponent {
      UUID uuid;

      DECL_COMPONENT(UUIDComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(UUIDComponent);

   struct TagComponent {
      std::string tag;

      DECL_COMPONENT(TagComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(TagComponent);

   struct SceneTransformComponent {
      vec3 position;
      quat rotation;

      DECL_COMPONENT(SceneTransformComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(SceneTransformComponent);

   struct TestCustomUIComponent {
      int integer;
      float floating;
      vec3 float3;

      DECL_COMPONENT(TestCustomUIComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(TestCustomUIComponent);

}
