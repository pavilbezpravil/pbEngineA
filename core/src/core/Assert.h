#pragma once

#include "Log.h"

#define SASSERT(cond) static_assert(cond, "")

#ifdef DEBUG
   #define ENABLE_ASSERTS
#endif

#ifdef ENABLE_ASSERTS
   #define ASSERT_NO_MESSAGE(condition) { if(!(condition)) { ERROR("Assertion Failed"); __debugbreak(); } }
   #define ASSERT_MESSAGE(condition, ...) { if(!(condition)) { ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }

   #define ASSERT(condition) { if(!(condition)) { ERROR("Assertion Failed"); __debugbreak(); } }

   #define UNIMPLEMENTED() ASSERT(FALSE)
#else
   #define ASSERT(...)

   #define UNIMPLEMENTED()
#endif
