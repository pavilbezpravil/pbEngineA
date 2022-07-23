#include "pch.h"
#include "Random.h"
#include "core/Core.h"
#include "Types.h"

namespace pbe {
   namespace Random {

      static std::random_device sRandomDevice;
      static std::mt19937_64 sEng(sRandomDevice());

      bool Bool(float trueChance) {
         return Uniform(0.f, 1.f) < trueChance;
      }

      float Uniform(float min, float max) {
         const std::uniform_real_distribution distribution{ min, max };
         return distribution(sEng);
      }

      vec3 Uniform(vec3 min, vec3 max) {
         return {
            Uniform(min.x, max.x),
            Uniform(min.y, max.y),
            Uniform(min.z, max.z) };
      }

   }
}
