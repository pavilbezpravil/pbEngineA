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
         std::uniform_real_distribution distribution{ min, max };
         return distribution(sEng);
      }

      vec3 Uniform(vec3 min, vec3 max) {
         return {
            Uniform(min.x, max.x),
            Uniform(min.y, max.y),
            Uniform(min.z, max.z) };
      }

      vec2 Uniform(vec2 min, vec2 max) {
         return {
            Uniform(min.x, max.x),
            Uniform(min.y, max.y)};
      }

      vec3 UniformInSphere() {
         vec3 test;
         do {
            test = Uniform(vec3{ -1 }, vec3{ 1 });
         } while (glm::length(test) > 1);
         return test;
      }

      vec2 UniformInCircle() {
         vec2 test;
         do {
            test = Uniform(vec2{ -1 }, vec2{ 1 });
         } while (glm::length(test) > 1);
         return test;
      }

   }
}
