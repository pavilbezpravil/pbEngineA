#include "pch.h"
#include "Common.h"

#include <glm/gtx/matrix_decompose.hpp>


namespace pbe {

   int VectorUtils::LargestAxisIdx(const vec3& v) {
      int index = 0;
      float maxValue = v[0];

      for (int i = 1; i < 3; ++i) {
         if (v[i] > maxValue) {
            index = i;
            maxValue = v[i];
         }
      }

      return index;
   }

   std::tuple<glm::vec3, glm::quat, glm::vec3> GetTransformDecomposition(const glm::mat4& transform) {
      glm::vec3 scale, translation, skew;
      glm::vec4 perspective;
      glm::quat orientation;
      glm::decompose(transform, scale, orientation, translation, skew, perspective);

      return { translation, orientation, scale };
   }

}
