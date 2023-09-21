#pragma once

#include "Types.h"
#include "core/Core.h"

namespace pbe {

   struct Ray {
      vec3 origin;
      vec3 direction;
   };

   struct Sphere {
      vec3 center;
      float radius = 0.5f;
   };

   struct AABB {
      vec3 min;
      vec3 max;

      static AABB Empty();
      static AABB MinMax(const vec3& min, const vec3& max);
      static AABB Extends(const vec3& center, const vec3& extends);
      static AABB FromAABBs(const AABB* aabbs, uint size);

      void AddPoint(const vec3& p);
      void AddAABB(const AABB& aabb);

      void Translate(const vec3& v);
      void Expand(float expand); // increase size by expand in all directions

      vec3 Size() const { return max - min; }
      vec3 Extents() const { return Size() * 0.5f; }

      bool Contains(const vec3& p) const;
      bool Intersects(const AABB& aabb) const;

      float Volume() const;
   };

   struct CORE_API Plane {
      vec3 normal;
      float d;

      static Plane FromPointNormal(const vec3& p, const vec3& n);
      static Plane FromPoints(const vec3& p0, const vec3& p1, const vec3& p2);

      float RayIntersectionT(const Ray& ray) const {
         return -Distance(ray.origin) / glm::dot(normal, ray.direction);
      }

      vec3 RayIntersectionAt(const Ray& ray) const {
         float t = RayIntersectionT(ray);
         return ray.origin + ray.direction * t;
      }

      vec3 Project(const vec3& p) const {
         return p - normal * Distance(p);
      }

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
