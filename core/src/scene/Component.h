#pragma once

#include "Entity.h"
#include "core/Type.h"
#include "core/UUID.h"
#include "math/Types.h"

namespace pbe {

   struct CORE_API ComponentRegisterGuard {
      template<typename Func>
      ComponentRegisterGuard(Func f, TypeID typeID) : typeID(typeID) {
         f();
      }
      ~ComponentRegisterGuard();
      TypeID typeID;
   };

#define INTERNAL_ADD_COMPONENT(Component) \
      ci = {}; \
      ci.typeID = GetTypeID<Component>(); \
      ci.tryGet = [](Entity& e) { return (void*)e.TryGet<Component>(); }; \
      ci.getOrAdd = [](Entity& e) { return (void*)&e.GetOrAdd<Component>(); }; \
      ci.duplicate = [](void* dst, const void* src) { *(Component*)dst = *(Component*)src; }; \
      typer.RegisterComponent(std::move(ci))

#define TYPER_REGISTER_COMPONENT(Component) \
   static void TyperComponentRegister_##Component() { \
      auto& typer = Typer::Get(); \
      ComponentInfo ci{}; \
      INTERNAL_ADD_COMPONENT(Component); \
   } \
   static ComponentRegisterGuard ComponentInfo_##Component = {TyperComponentRegister_##Component, GetTypeID<Component>()};

// #define DECL_COMPONENT(Component) \
//    static const char* GetName() { \
//       return STRINGIFY(Component); \
//    }
#define DECL_COMPONENT(Component)

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

   struct CORE_API SceneTransformComponent {
      Entity entity;

      vec3 position{};
      quat rotation = quat_Identity;
      vec3 scale{ 1.f };

      vec3 Right() const;
      vec3 Up() const;
      vec3 Forward() const;

      mat4 GetMatrix() const;
      void SetMatrix(const mat4& transform);

      DECL_COMPONENT(SceneTransformComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(SceneTransformComponent);

   struct SimpleMaterialComponent {
      vec3 albedo = vec3_One;
      float roughness = 0.1f;
      float metallic = 0;
      bool opaque = true;

      DECL_COMPONENT(SimpleMaterialComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(SimpleMaterialComponent);

   struct LightComponent {
      vec3 color{1};
      float intensity = 1;
      float radius = 5;

      DECL_COMPONENT(LightComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(LightComponent);

   struct DirectLightComponent {
      vec3 color{ 1 };
      float intensity = 1;

      DECL_COMPONENT(DirectLightComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(DirectLightComponent);

   struct DecalComponent {
      vec4 albedo = vec4_One;
      float metallic = 0.f;
      float roughness = 0.1f;
      DECL_COMPONENT(DecalComponent);
   };
   COMPONENT_EXPLICIT_TEMPLATES_DECL(DecalComponent);

   struct SkyComponent {
      Entity directLight;
      vec3 color = vec3_One;
   };

   struct WaterComponent {
      vec3 fogColor = vec3(21, 95, 179) / 256.f;
      float fogUnderwaterLength = 5.f;
      float softZ = 0.1f;
   };

   struct TerrainComponent {
      vec3 color = vec3(21, 95, 179) / 256.f;
   };

   void RegisterBasicComponents(class Typer& typer);

}
