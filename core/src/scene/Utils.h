#pragma once

#include "Entity.h"
#include "math/Transform.h"

namespace pbe {

   struct EditorSelection;

   Entity CORE_API CreateEmpty(Scene& scene, string_view namePrefix = "Empty", Entity parent = {}, const vec3& pos = {}, Space space = Space::World);

   // todo: add space for transform
   struct CubeDesc {
      Entity parent = {};
      string_view namePrefix = "Cube";

      Space space = Space::World;
      vec3 pos = vec3_Zero;
      quat rotation = quat_Identity;
      vec3 scale = vec3_One;

      vec3 color = vec3_One * 0.7f;

      enum Type {
         Geom = BIT(1),
         Material = BIT(2),
         RigidBodyShape = BIT(3),
         RigidBodyStatic = BIT(4),
         RigidBodyDynamic = BIT(5),
      };

      constexpr static Type GeomMaterial = Type(Geom | Material);
      constexpr static Type PhysShape = Type(GeomMaterial | RigidBodyShape);
      constexpr static Type PhysStatic = Type(PhysShape | RigidBodyStatic);
      constexpr static Type PhysDynamic = Type(PhysShape | RigidBodyDynamic);

      Type type = PhysDynamic;
   };

   DEFINE_ENUM_FLAG_OPERATORS(CubeDesc::Type)

   Entity CORE_API CreateCube(Scene& scene, const CubeDesc& desc = {});

   Entity CORE_API CreateLight(Scene& scene, string_view namePrefix = "Light", const vec3& pos = {});
   Entity CORE_API CreateDirectLight(Scene& scene, string_view namePrefix = "Direct Light", const vec3& pos = {});
   Entity CORE_API CreateSky(Scene& scene, string_view namePrefix = "Sky", const vec3& pos = {});

   Entity CORE_API CreateTrigger(Scene& scene, const vec3& pos = {});

   Entity CORE_API SceneAddEntityMenu(Scene& scene, const vec3& spawnPosHint, EditorSelection* selection = nullptr);

   // todo: 
   template<typename T>
   struct Component {
      T* operator->() { return entity.TryGet<T>(); }
      const T* operator->() const { return entity.TryGet<T>(); }

      Entity entity;
   };

}
