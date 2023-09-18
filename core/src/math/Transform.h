#pragma once
#include "Types.h"
#include "core/Core.h"


namespace pbe {

   struct CORE_API Transform {
      vec3 position;
      quat rotation;
      vec3 scale;

      Transform operator*(const Transform& other) const;
      Transform TransformInv(const Transform& other) const;

      vec3 TransformPosition(const vec3& point) const;
      quat TransformRotation(const quat& rot) const;
      vec3 TransformScale(const vec3& s) const;

      vec3 TransformInvPosition(const vec3& point) const;
      quat TransformInvRotation(const quat& rot) const;
      vec3 TransformInvScale(const vec3& s) const;

      vec3 TransformDirection(const vec3& dir) const;

      vec3 Right() const;
      vec3 Up() const;
      vec3 Forward() const;

      mat4 GetMatrix() const;
   };

}
