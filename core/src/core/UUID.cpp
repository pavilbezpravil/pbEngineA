#include "pch.h"
#include "UUID.h"

#include <random>

namespace pbe {

   static std::random_device sRandomDevice;
   static std::mt19937_64 sEng(sRandomDevice());
   static std::uniform_int_distribution<uint64_t> sDistribution;

   UUID::UUID() : uuid(sDistribution(sEng)) {}

   bool UUID::Valid() const {
      return uuid != (uint64)UUID_INVALID;
   }

}
