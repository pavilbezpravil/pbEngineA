#pragma once

#include "Entity.h"

namespace pbe {

   struct EditorSelection;

   Entity CORE_API CreateEmpty(Scene& scene, string_view namePrefix = "Empty", Entity parent = {}, const vec3& pos = {});

   struct CubeDesc {
      Entity parent = {};
      string_view namePrefix = "Cube";
      vec3 pos = vec3_Zero;
      quat rotation = quat_Identity;
      vec3 scale = vec3_One;
      bool dynamic = true;
      vec3 color = vec3_One * 0.7f;
   };

   Entity CORE_API CreateCube(Scene& scene, const CubeDesc& desc = {});

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
