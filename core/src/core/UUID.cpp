#include "UUID.h"

#include <random>

namespace pbe {

   static std::random_device s_RandomDevice;
   static std::mt19937_64 sEng(s_RandomDevice());
   static std::uniform_int_distribution<uint64_t> sDistribution;

   UUID::UUID() : uuid(sDistribution(sEng)) {
   }

   bool UUID::Valid() const {
      return uuid != (uint64)UUID_INVALID;
   }

}
