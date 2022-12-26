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

   struct Plane {
      vec3 normal;
      float d;

      float Distance(vec3 p) const {
         return glm::dot(normal, p) + d;
      }
   };

   struct Frustum {
      enum Side {
         RIGHT,
         LEFT,
         BOTTOM,
         TOP,
         BACK,
         FRONT
      };

      Plane planes[6];

      Frustum(const mat4& m);

      bool PointTest(vec3 p);
      bool SphereTest(const Sphere& s);
   };

}
