#include "pch.h"
#include "Random.h"
#include "Types.h"

namespace pbe {

   // todo: use faster random generator
   // todo: not thread safe
   static std::random_device sRandomDevice;
   static std::mt19937_64 sEng(sRandomDevice());

   uint pcg_hash(uint input) {
      uint state = input * 747796405u + 2891336453u;
      uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
      return (word >> 22u) ^ word;
   }

   float Random::FloatSeeded(uint seed) {
      seed = pcg_hash(seed);
      return (float)seed / (float)uint(-1);
   }

   bool Random::Bool(float trueChance) {
      return Float(0.f, 1.f) < trueChance;
   }

   float Random::Float(float min, float max) {
      std::uniform_real_distribution distribution{ min, max };
      return distribution(sEng);
   }

   vec2 Random::Float2(vec2 min, vec2 max) {
      return {
         Float(min.x, max.x),
         Float(min.y, max.y) };
   }

   vec3 Random::Float3(vec3 min, vec3 max) {
      return {
         Float(min.x, max.x),
         Float(min.y, max.y),
         Float(min.z, max.z) };
   }

   Color Random::Color(uint seed) {
      return {
        FloatSeeded(seed),
        FloatSeeded(seed + 111),
        FloatSeeded(seed + 222) };
   }

   Color Random::Color() {
      return Float3();
   }

   vec3 Random::UniformInSphere() {
      vec3 test;
      do {
         test = Float3(vec3{ -1 }, vec3{ 1 });
      } while (glm::length(test) > 1);
      return test;
   }

   vec2 Random::UniformInCircle() {
      vec2 test;
      do {
         test = Float2(vec2{ -1 }, vec2{ 1 });
      } while (glm::length(test) > 1);
      return test;
   }

}
