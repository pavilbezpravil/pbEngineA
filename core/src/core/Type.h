#pragma once

namespace pbe {

   using TypeID = uint64;

   template<typename T>
   TypeID GetTypeID() {
      return typeid(T).hash_code(); // todo:
   }

}
