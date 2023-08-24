#pragma once

namespace pbe {

   // using TypeID = uint64;
   using TypeID = entt::id_type;
   constexpr TypeID InvalidTypeID = 0;

   template<typename T>
   TypeID GetTypeID() {
      return entt::type_hash<T>::value(); // todo:
      // return typeid(T).hash_code(); // todo:
   }

   template<typename UnregFunc>
   struct RegisterGuardT {
      template<typename RegFunc>
      RegisterGuardT(TypeID typeID, RegFunc f) : typeID(typeID) {
         f();
      }
      ~RegisterGuardT() {
         UnregFunc()(typeID);
      }
      TypeID typeID;
   };

}
