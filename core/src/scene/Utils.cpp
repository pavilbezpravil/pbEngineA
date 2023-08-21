#include "pch.h"
#include "Utils.h"

#include "Component.h"
#include "physics/PhysComponents.h"

namespace pbe {

   Entity CreateEmpty(Scene& scene, string_view namePrefix, Entity parent, const vec3& pos) {
      // todo: find appropriate name
      auto entity = scene.Create(parent, namePrefix);
      entity.Get<SceneTransformComponent>().position = pos;
      return entity;
   }

   Entity CreateCube(Scene& scene, const CubeDesc& desc) {
      auto entity = CreateEmpty(scene, desc.namePrefix, desc.parent, desc.pos);

      auto& trans = entity.Get<SceneTransformComponent>();
      trans.scale = desc.scale;
      trans.rotation = desc.rotation;

      entity.Add<GeometryComponent>();
      entity.Add<MaterialComponent>().baseColor = desc.color;

      // todo:
      RigidBodyComponent _rb{};
      _rb.dynamic = desc.dynamic;
      entity.Add<RigidBodyComponent>(_rb);

      return entity;
   }

   Entity CreateDirectLight(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Get<SceneTransformComponent>().rotation = quat{ vec3{PIHalf * 0.5, PIHalf * 0.5, 0 } };
      entity.Add<DirectLightComponent>();
      return entity;
   }

   Entity CreateSky(Scene& scene, string_view namePrefix, const vec3& pos) {
      auto entity = CreateEmpty(scene, namePrefix, {}, pos);
      entity.Add<SkyComponent>();
      return entity;
   }

   Entity CreateTrigger(Scene& scene, const vec3& pos) {
      auto entity = CreateEmpty(scene, "Trigger", {}, pos);
      entity.Add<GeometryComponent>();
      entity.Add<TriggerComponent>();
      return entity;
   }

}
