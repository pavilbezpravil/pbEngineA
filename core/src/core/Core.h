#pragma once

#include <cstdint>
#include <string>
#include <vector>

// warning C4251: 'SomeClass::member': class 'OtherClass' needs to have dll-interface
// to be used by clients of class 'SomeClass'
#pragma warning( disable : 4251 )

#define EXTERN_C extern "C"

#ifdef CORE_API_EXPORT
   #define CORE_API  __declspec(dllexport)
#else
   #define CORE_API  __declspec(dllimport)
#endif

namespace pbe {

   using int8 = int8_t;
   using uint8 = uint8_t;
   using int16 = int16_t;
   using uint16 = uint16_t;
   using int64 = int64_t;
   using uint64 = uint64_t;
   using byte = unsigned char;

   using std::string;
   using std::string_view;

   using std::vector;

#define STRINGIFY(x) #x
#define CONCAT(a, b) a ## b

#define BIT(x) 1 << x

#define CALL_N_TIMES(f, times) \
   { \
      static int executed = times; \
      if (executed > 0) { \
            executed--; \
            f(); \
      } \
   }

#define CALL_ONCE(f) \
   CALL_N_TIMES(f, 1)

}
