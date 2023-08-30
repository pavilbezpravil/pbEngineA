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
      static AABB MinMax(const vec3& min, const vec3& max);
      static AABB CenterHalfSize(const vec3& center, const vec3& halfSize);
      static AABB FromAABBs(const AABB* aabbs, uint size);

      void AddPoint(const vec3& p);
      void AddAABB(const AABB& aabb);

      vec3 Size() const { return max - min; }
      vec3 Extents() const { return Size() * 0.5f; }

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
