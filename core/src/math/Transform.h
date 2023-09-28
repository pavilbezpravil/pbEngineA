#pragma once
#include "Types.h"
#include "core/Core.h"


namespace pbe {

   enum class Space {
      Local,
      World
   };

   struct CORE_API Transform {
      vec3 position;
      quat rotation;
      vec3 scale;

      Transform operator*(const Transform& other) const;
      Transform TransformInv(const Transform& other) const;

      vec3 TransformPoint(const vec3& point) const;
      quat TransformRotation(const quat& rot) const;
      vec3 TransformScale(const vec3& s) const;

      vec3 TransformInvPoint(const vec3& point) const;
      quat TransformInvRotation(const quat& rot) const;
      vec3 TransformInvScale(const vec3& s) const;

      vec3 Rotate(const vec3& dir) const;

      vec3 Right() const;
      vec3 Up() const;
      vec3 Forward() const;

      mat4 GetMatrix() const;
   };

}
