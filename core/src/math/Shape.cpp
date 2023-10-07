#include "pch.h"
#include "Shape.h"

#include "core/Assert.h"

namespace pbe {

   AABB AABB::Empty() {
      return {vec3{FLT_MAX}, vec3{-FLT_MAX}};
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

   AABB AABB::Union(const AABB& a, const AABB& b) {
      return AABB::MinMax(
         glm::min(a.min, b.min),
         glm::max(a.max, b.max)
      );
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

   void AABB::Expand(float expand) {
      min -= expand;
      max += expand;
   }

   vec3 AABB::Center() const {
      ASSERT(!IsEmpty());
      return (min + max) * 0.5f;
   }

   vec3 AABB::Size() const { return IsEmpty() ? vec3_Zero : max - min; }

   vec3 AABB::Extents() const { return Size() * 0.5f; }

   bool AABB::Contains(const vec3& p) const {
      return
         p.x >= min.x && p.x <= max.x &&
         p.y >= min.y && p.y <= max.y &&
         p.z >= min.z && p.z <= max.z;
   }

   bool AABB::Intersects(const AABB& aabb) const {
      return
         min.x <= aabb.max.x && max.x >= aabb.min.x &&
         min.y <= aabb.max.y && max.y >= aabb.min.y &&
         min.z <= aabb.max.z && max.z >= aabb.min.z;
   }

   float AABB::Volume() const {
      vec3 s = Size();
      return s.x * s.y * s.z;
   }

   float AABB::Area() const {
      vec3 s = Size();
      return 2.f * (s.x * s.y + s.x * s.z + s.y * s.z);
   }

   bool AABB::IsEmpty() const { return min.x > max.x; }

   Plane Plane::FromPointNormal(const vec3& p, const vec3& n) {
      Plane plane;
      plane.normal = n;
      plane.d = -glm::dot(n, p);
      return plane;
   }

   Plane Plane::FromPoints(const vec3& p0, const vec3& p1, const vec3& p2) {
      auto normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
      return FromPointNormal(p0, normal);
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
