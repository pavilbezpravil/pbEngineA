#pragma once

#include <xhash>
#include <limits>

#include "Assert.h"
#include "Core.h"

namespace pbe {

   class UUID {
   public:
      UUID();
      constexpr UUID(uint64 uuid) : uuid(uuid) {}

      bool operator==(const UUID& rhs) const { return uuid == rhs.uuid; }
      bool operator!=(const UUID& rhs) const { return !(*this == rhs); }

      operator const uint64() const { return uuid; }

      bool Valid() const;

   private:
      uint64 uuid;
   };


   SASSERT(std::is_move_constructible_v<UUID>);
   SASSERT(std::is_move_assignable_v<UUID>);
   SASSERT(std::is_move_assignable_v<int>);

   constexpr UUID UUID_INVALID = std::numeric_limits<uint64>::max();

}

namespace std {

   template <>
   struct hash<pbe::UUID> {
      std::size_t operator()(const pbe::UUID& uuid) const {
         return hash<pbe::uint64>()(uuid);
      }
   };

}
