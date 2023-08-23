#pragma once
#include "scene/Entity.h"


namespace pbe {

   struct RayCastResult {
      Entity physActor;
      vec3 position;
      vec3 normal;

      operator bool() const { return physActor; }
   };

}
