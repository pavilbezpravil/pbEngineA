#pragma once

namespace pbe {

   // using TypeID = uint64;
   using TypeID = entt::id_type;

   template<typename T>
   TypeID GetTypeID() {
      return entt::type_hash<T>::value(); // todo:
      // return typeid(T).hash_code(); // todo:
   }

}
