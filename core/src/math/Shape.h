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

      static AABB Empty();
      static AABB CenterHalfSize(vec3 center, vec3 halfSize);

      void AddPoint(vec3 p);
      void AddAABB(const AABB& aabb);

   };

}
