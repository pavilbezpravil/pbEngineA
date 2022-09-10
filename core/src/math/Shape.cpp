#include "pch.h"
#include "Shape.h"

namespace pbe {

   AABB AABB::Empty() {
      return {vec3{FLT_MAX}, vec3{FLT_MIN}};
   }

   AABB AABB::CenterHalfSize(vec3 center, vec3 halfSize) {
      AABB aabb;
      aabb.min = center - halfSize;
      aabb.max = center + halfSize;
      return aabb;
   }

   void AABB::AddPoint(vec3 p) {
      min = glm::min(min, p);
      max = glm::max(max, p);
   }

   void AABB::AddAABB(const AABB& aabb) {
      min = glm::min(min, aabb.min);
      max = glm::max(max, aabb.max);
   }

}
