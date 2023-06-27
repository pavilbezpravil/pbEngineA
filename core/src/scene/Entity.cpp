#include "pch.h"
#include "Entity.h"

#include "Component.h"

namespace pbe {

   Entity::Entity(entt::entity id, Scene* scene) :id(id), scene(scene) {
   }

   void Entity::DestroyImmediate() {
      scene->DestroyImmediate(*this);
      (*this) = {};
   }

   const char* Entity::GetName() const {
      return Get<TagComponent>().tag.c_str();
   }

   UUID Entity::GetUUID() const {
      return Get<UUIDComponent>().uuid;
   }
}
