#include "pch.h"
#include "Shape.h"

#include "core/Assert.h"

namespace pbe {

   AABB AABB::Empty() {
      return {vec3{FLT_MAX}, vec3{FLT_MIN}};
   }

   AABB AABB::MinMax(const vec3& min, const vec3& max) {
      return { min, max };
   }

   AABB AABB::Extends(const vec3& center, const vec3& extends) {
      AABB aabb;
      aabb.min = center - extends;
      aabb.max = center + extends;
      return aabb;
   }

   AABB AABB::FromAABBs(const AABB* aabbs, uint size) {
      AABB aabb = Empty();
      for (uint i = 0; i < size; ++i) {
         aabb.AddAABB(aabbs[i]);
      }
      return aabb;
   }

   void AABB::AddPoint(const vec3& p) {
      min = glm::min(min, p);
      max = glm::max(max, p);
   }

   void AABB::AddAABB(const AABB& aabb) {
      min = glm::min(min, aabb.min);
      max = glm::max(max, aabb.max);
   }

   void AABB::Translate(const vec3& v) {
      min += v;
      max += v;
   }

   Frustum::Frustum(const mat4& m) {
      planes[RIGHT] = {
         {
            m[0][3] - m[0][0],
            m[1][3] - m[1][0],
            m[2][3] - m[2][0],
         },
         m[3][3] - m[3][0],
      };

      planes[LEFT] = {
         {
            m[0][3] + m[0][0],
            m[1][3] + m[1][0],
            m[2][3] + m[2][0],
         },
         m[3][3] + m[3][0],
      };

      planes[BOTTOM] = {
         {
            m[0][3] + m[0][1],
            m[1][3] + m[1][1],
            m[2][3] + m[2][1],
         },
         m[3][3] + m[3][1],
      };

      planes[TOP] = {
         {
            m[0][3] - m[0][1],
            m[1][3] - m[1][1],
            m[2][3] - m[2][1],
         },
         m[3][3] - m[3][1],
      };

      planes[BACK] = {
         {
            m[0][2],
            m[1][2],
            m[2][2],
         },
         m[3][2],
      };

      planes[FRONT] = {
         {
            m[0][3] - m[0][2],
            m[1][3] - m[1][2],
            m[2][3] - m[2][2],
         },
         m[3][3] - m[3][2],
      };

      for (int i = 0; i < 6; ++i) {
         float mag = glm::length(planes[i].normal);
         planes[i].normal /= mag;
         planes[i].d /= mag;
      }
   }

   bool Frustum::PointTest(vec3 p) {
      for (int i = 0; i < 6; ++i) {
         if (planes[i].Distance(p) <= 0.f) {
            return false;
         }
      }

      return true;
   }

   bool Frustum::SphereTest(const Sphere& s) {
      for (int i = 0; i < 6; ++i) {
         if (planes[i].Distance(s.center) <= -s.radius) {
            return false;
         }
      }

      return true;
   }
}
