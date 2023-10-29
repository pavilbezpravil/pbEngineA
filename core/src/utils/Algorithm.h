#pragma once

#include <vector>

namespace pbe {

   template<typename T>
   void VectorErase(std::vector<T>& vec, const T& value) {
      auto it = std::ranges::find(vec, value);
      if (it != vec.end()) {
         vec.erase(it);
      }
   }

   template<typename T>
   void VectorEraseSwap(std::vector<T>& vec, const T& value) {
      auto it = std::ranges::find(vec, value);
      if (it != vec.end()) {
         std::swap(*it, vec.back());
         vec.pop_back();
      }
   }

}
