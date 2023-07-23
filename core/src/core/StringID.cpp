#include "pch.h"
#include "StringID.h"


namespace pbe {

   // todo: slow hash function
   static std::unordered_map<std::string, int> sStringIDs;

   StringID::StringID(std::string_view str) {
      auto it = sStringIDs.find(str.data());
      if (it == sStringIDs.end()) {
         id = (int)sStringIDs.size();
         sStringIDs[str.data()] = id;
      } else {
         id = it->second;
      }
   }

}
