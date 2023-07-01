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
   { \
      ComponentInfo ci{}; \
      ci.typeID = GetTypeID<Component>(); \
      ci.tryGet = [](Entity& e) { return (void*)e.TryGet<Component>(); }; \
      ci.tryGetConst = [](const Entity& e) { return (const void*)e.TryGet<Component>(); }; \
      ci.getOrAdd = [](Entity& e) { return (void*)&e.GetOrAdd<Component>(); }; \
      ci.duplicate = [](void* dst, const void* src) { *(Component*)dst = *(Component*)src; }; \
      typer.RegisterComponent(std::move(ci)); \
   }   

#define TYPER_REGISTER_COMPONENT(Component) \
   static void TyperComponentRegister_##Component() { \
      auto& typer = Typer::Get(); \
      ComponentInfo ci{}; \
      INTERNAL_ADD_COMPONENT(Component); \
   } \
   static ComponentRegisterGuard ComponentInfo_##Component = {TyperComponentRegister_##Component, GetTypeID<Component>()};

   struct UUIDComponent {
      UUID uuid;
   };

   struct TagComponent {
      std::string tag;
   };

   // todo: move to math
   CORE_API std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform);

   struct CORE_API SceneTransformComponent {
      Entity entity;

      // local space
      vec3 position{};
      quat rotation = quat_Identity;
      vec3 scale{ 1.f };

      // todo: return const ref
      // World space
      vec3 Position() const;
      quat Rotation() const;
      vec3 Scale() const;

      void SetPosition(const vec3& pos);
      void SetRotation(const quat& rot);
      void SetScale(const vec3& s);

      vec3 Right() const;
      vec3 Up() const;
      vec3 Forward() const;

      mat4 GetMatrix() const;
      void SetMatrix(const mat4& transform);

      Entity parent;
      std::vector<Entity> children;
      bool HasParent() const { return (bool)parent; }
      bool HasChilds() const { return !children.empty(); }

      void AddChild(Entity child);
      void RemoveChild(int idx);
      void RemoveAllChild(Entity theirNewParent = {});
      bool SetParent(Entity newParent = {});
   };

   struct SimpleMaterialComponent {
      vec3 baseColor = vec3_One;
      float roughness = 0.1f;
      float metallic = 0;
      bool opaque = true;
   };

   struct LightComponent {
      vec3 color{1};
      float intensity = 1;
      float radius = 5;
   };

   struct DirectLightComponent {
      vec3 color{ 1 };
      float intensity = 1;
   };

   struct DecalComponent {
      vec4 baseColor = vec4_One;
      float metallic = 0.f;
      float roughness = 0.1f;
   };

   struct SkyComponent {
      Entity directLight; // todo: remove
      vec3 color = vec3_One;
      float intensity = 1;
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
