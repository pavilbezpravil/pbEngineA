#pragma once

#include "Types.h"
#include "core/Core.h"


namespace pbe {

   struct VectorUtils {
      static int LargestAxisIdx(const vec3& v);
   };

   template<typename T>
   constexpr T Clamp(T value, T minValue, T maxValue) {
      return glm::clamp(value, minValue, maxValue);
   }

   template<typename T>
   constexpr T Saturate(T value) {
      return glm::clamp(value, T{0}, T{1});
   }

   template<typename T, typename U>
   constexpr T Lerp(T x, T y, U t) {
      return glm::mix(x, y, t);
   }

   CORE_API std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform);


}
