#pragma once

#include "Types.h"
#include "core/Core.h"

namespace pbe {
   namespace Random {

      CORE_API bool Bool(float trueChance = 0.5);
      CORE_API float Uniform(float min = 0, float max = 1);
      CORE_API vec3 Uniform(vec3 min = vec3_Zero, vec3 max = vec3_One);

   }
}
