#include "pch.h"
#include "Entity.h"

#include "Component.h"

namespace pbe {

   Entity::Entity(entt::entity id, Scene* scene) :id(id), scene(scene) {
   }

   const char* Entity::GetName() const {
      return Get<TagComponent>().tag.c_str();
   }
}
