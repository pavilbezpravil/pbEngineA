#pragma once

#include "Entity.h"
#include "core/UUID.h"
#include "SceneTransform.h"
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

   struct MaterialComponent {
      vec3 baseColor = vec3_One * 0.5f;
      float roughness = 0.3f;
      float metallic = 0;
      float emissivePower = 0;

      bool opaque = true;
   };

   enum class GeomType {
      Sphere,
      Box, // todo: cube
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

   struct RTImportanceVolumeComponent {
      float radius = 1;
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

   // todo:
   struct OutlineComponent {
      vec3 color = vec3(209, 188, 50) / 256.f;
   };

   // todo:
   struct CORE_API TimedDieComponent {
      float time = 0.f;

      void SetRandomDieTime(float min, float max);
   };

   void RegisterBasicComponents(class Typer& typer);

}
