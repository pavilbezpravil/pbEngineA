#include "pch.h"
#include "Random.h"
#include "core/Core.h"
#include "Types.h"

namespace pbe {
   namespace Random {

      static std::random_device sRandomDevice;
      static std::mt19937_64 sEng(sRandomDevice());
      static std::uniform_int_distribution<uint64> sDistribution;

      float UniformFloat(float min, float max) {
         const std::uniform_real_distribution distribution{ min, max };
         return distribution(sEng);
      }

      vec3 UniformVec3(vec3 min, vec3 max) {
         return {
            UniformFloat(min.x, max.x),
            UniformFloat(min.y, max.y),
            UniformFloat(min.z, max.z) };
      }

   }
}
