#include "pch.h"
#include "Entity.h"

#include "Component.h"

namespace pbe {

   Entity::Entity(entt::entity id, Scene* scene) : id(id), scene(scene) { }

   void Entity::DestroyDelayed() {
      scene->DestroyDelayed(GetEntityID());
   }

   bool Entity::Enabled() const {
      return scene->EntityEnabled(GetEntityID());
   }

   void Entity::Enable(bool enable) {
      scene->EntityEnable(GetEntityID(), enable);
   }

   void Entity::EnableToggle() {
      Enable(!Enabled());
   }

   SceneTransformComponent& Entity::GetTransform() {
      return Get<SceneTransformComponent>();
   }

   const SceneTransformComponent& Entity::GetTransform() const {
      return Get<SceneTransformComponent>();
   }

   const char* Entity::GetName() const {
      return Get<TagComponent>().tag.c_str();
   }

   UUID Entity::GetUUID() const {
      return Get<UUIDComponent>().uuid;
   }

}
