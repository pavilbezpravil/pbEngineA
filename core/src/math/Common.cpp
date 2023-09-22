#include "pch.h"
#include "Common.h"


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

}
