#pragma once
#include "Types.h"


namespace pbe {

   struct Transform {
      vec3 position;
      quat rotation;
      vec3 scale;

      Transform operator*(const Transform& other) const {
         Transform result;
         result.position = TransformPosition(other.position);
         result.rotation = TransformRotation(other.rotation);
         result.scale = TransformScale(other.scale);
         return result;
      }

      Transform TransformInv(const Transform& other) const {
         Transform result;
         result.position = TransformInvPosition(other.position);
         result.rotation = TransformInvRotation(other.rotation);
         result.scale = TransformInvScale(other.scale);
         return result;
      }

      vec3 TransformPosition(const vec3& point) const {
         return position + rotation * (scale * point);
      }

      quat TransformRotation(const quat& rot) const {
         return rotation * rot;
      }

      vec3 TransformScale(const vec3& s) const {
         return scale * s;
      }

      vec3 TransformInvPosition(const vec3& point) const {
         return glm::inverse(rotation) * (point - position) / scale;
      }

      quat TransformInvRotation(const quat& rot) const {
         return glm::inverse(rotation) * rot;
      }

      vec3 TransformInvScale(const vec3& s) const {
         return s / scale;
      }

      vec3 TransformDirection(const vec3& dir) const {
         return rotation * dir;
      }

      vec3 Right() const {
         return TransformDirection(vec3_Right);
      }

      vec3 Up() const {
         return TransformDirection(vec3_Up);
      }

      vec3 Forward() const {
         return TransformDirection(vec3_Forward);
      }
   };

}
