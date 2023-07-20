#include "pch.h"
#include "Entity.h"

#include "Component.h"

namespace pbe {

   Entity::Entity(entt::entity id, Scene* scene) :id(id), scene(scene) {
   }

   void Entity::DestroyImmediate(bool withChilds) {
      scene->DestroyImmediate(*this, withChilds);
      (*this) = {};
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
