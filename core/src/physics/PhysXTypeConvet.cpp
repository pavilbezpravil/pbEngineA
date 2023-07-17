#include "pch.h"
#include "PhysXTypeConvet.h"

namespace pbe {

   vec2 PxVec2ToPBE(const PxVec2& v) {
      return {v.x, v.y};
   }

   vec3 PxVec3ToPBE(const PxVec3& v) {
      return { v.x, v.y, v.z };
   }

   vec4 PxVec4ToPBE(const PxVec4& v) {
      return { v.x, v.y, v.z, v.w };
   }

   quat PxQuatToPBE(const PxQuat& q) {
      return { q.w, q.x, q.y, q.z };
   }

   PxVec2 Vec2ToPx(const vec2& v) {
      return { v.x, v.y };
   }

   PxVec3 Vec3ToPx(const vec3& v) {
      return { v.x, v.y, v.z };
   }

   PxVec4 Vec4ToPx(const vec4& v) {
      return { v.x, v.y, v.z, v.w };
   }

   PxQuat QuatToPx(const quat& q) {
      return { q.x, q.y, q.z, q.w };
   }

}
