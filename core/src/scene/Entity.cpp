#include "pch.h"
#include "Entity.h"

#include "Component.h"

namespace pbe {

   Entity::Entity(entt::entity id, Scene* scene) :id(id), scene(scene) {
   }

   void Entity::DestroyDelayed(bool withChilds) {
      scene->DestroyDelayed(*this, withChilds);
   }

   void Entity::DestroyImmediate(bool withChilds) {
      scene->DestroyImmediate(*this, withChilds);
      (*this) = {};
   }

   bool Entity::Enabled() const {
      return scene->EntityEnabled(*this);
   }

   void Entity::Enable() {
      scene->EntityEnable(*this);
   }

   void Entity::Disable() {
      scene->EntityDisable(*this);
   }

   void Entity::EnableToggle() {
      if (Enabled()) {
         Disable();
      } else {
         Enable();
      }
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
