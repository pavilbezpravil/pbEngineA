#pragma once

#include "Types.h"
#include "core/Core.h"

namespace pbe {

   struct CORE_API Random {

      // todo: clear up random API
      static float FloatSeeded(uint seed);

      static bool Bool(float trueChance = 0.5);
      static float Float(float min = 0, float max = 1);
      static vec2 Float2(vec2 min = vec2_Zero, vec2 max = vec2_One);
      static vec3 Float3(vec3 min = vec3_Zero, vec3 max = vec3_One);

      static vec3 Color(uint seed);
      static vec3 Color();

      static vec3 UniformInSphere();
      static vec2 UniformInCircle();

   };

}
