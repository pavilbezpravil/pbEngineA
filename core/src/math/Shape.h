#pragma once

#include "Types.h"

namespace pbe {

   struct Sphere {
      vec3 center;
      float radius;
   };

   struct AABB {
      vec3 min;
      vec3 max;
   };

}
