#pragma once

#include "Types.h"

namespace pbe {
   namespace Random {

      bool Bool(float trueChance = 0.5);
      float Uniform(float min = 0, float max = 1);
      vec3 Uniform(vec3 min = vec3_Zero, vec3 max = vec3_One);

   }
}
