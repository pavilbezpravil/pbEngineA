#pragma once

#include "Entity.h"
#include "core/UUID.h"
#include "math/Types.h"

namespace pbe {

   struct UUIDComponent {
      UUID uuid;
   };

   struct TagComponent {
      std::string tag;
   };

   struct CameraComponent {
      bool main = false;
   };

   // todo: move to math
   CORE_API std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform);

   struct CORE_API SceneTransformComponent {
      SceneTransformComponent() = default;
      SceneTransformComponent(Entity entity, Entity parent = {});

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

      void AddChild(Entity child, int iChild = -1, bool keepLocalTransform = false);
      void RemoveChild(int idx);
      void RemoveAllChild(Entity theirNewParent = {});

      bool SetParent(Entity newParent = {}, int iChild = -1, bool keepLocalTransform = false);
      bool SetParentInternal(Entity newParent = {}, int iChild = -1, bool keepLocalTransform = false);
      int GetChildIdx() const;

      void Serialize(Serializer& ser) const;
      bool Deserialize(const Deserializer& deser);
      bool UI();
   };

   struct MaterialComponent {
      vec3 baseColor = vec3_One;
      float roughness = 0.1f;
      float metallic = 0;

      vec3 emissiveColor = vec3_One;
      float emissivePower = 0;

      bool opaque = true;
   };

   enum class GeomType {
      Sphere,
      Box,
      Cylinder,
      Cone,
      Capsule,
   };

   // todo:
   struct GeomSphere {
      float radius = 1.f;
   };

   struct GeomBox {
      vec3 extends;
   };

   struct Geom {
      GeomType type;

      union {
         GeomSphere sphere;
         GeomBox box;
      };
   };

   struct GeometryComponent {
      GeomType type = GeomType::Box;
      vec3 sizeData = vec3_One; // todo: full size
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
