#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pbe {

   using int8 = int8_t;
   using uint8 = uint8_t;
   using int64 = int64_t;
   using uint64 = uint64_t;
   using byte = unsigned char;

   using std::string;
   using std::string_view;

   using std::vector;

#define STRINGIFY(x) #x
#define CONCATENATION(a, b) a ## b

}
