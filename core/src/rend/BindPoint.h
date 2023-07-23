#pragma once
#include "math/Types.h"

namespace pbe {

   struct BindPoint {
      uint slot = (uint)-1;
      // int space = 0;

      operator bool() const { return slot != (uint)-1; }
   };

}
