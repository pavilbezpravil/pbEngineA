#pragma once

#include "Log.h"

#define SASSERT(cond) static_assert(cond, "")

#ifdef DEBUG
   #define ENABLE_ASSERTS
   #define ENABLE_MT_ASSERTS
#endif

#ifdef ENABLE_ASSERTS
   #define DEBUG_BREAK() __debugbreak()

   #define ASSERT_MESSAGE(condition, ...) { if(!(condition)) { ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
   #define ASSERT(condition) { if(!(condition)) { ERROR("Assertion Failed"); __debugbreak(); } }

   #define UNIMPLEMENTED() ASSERT(FALSE)
#else
   #define DEBUG_BREAK

   #define ASSERT_MESSAGE(condition, ...)
   #define ASSERT(...)

   #define UNIMPLEMENTED()
#endif


#ifdef ENABLE_MT_ASSERTS
   struct AssertMTUnique {
      AssertMTUnique() {
         bool prev = entered.exchange(true);
         ASSERT_MESSAGE(!prev, "Multiple thread touch scope!");
      }

      ~AssertMTUnique() {
         entered = false;
      }

      std::atomic_bool entered = false;
   };
   #define ASSERT_MT_UNIQUE() static AssertMTUnique __AssertMTUnique;
#else
   #define ASSERT_MT_UNIQUE()
#endif
