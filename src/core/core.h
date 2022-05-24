#pragma once

#ifdef PBE_CORE_API_DLL
#define PBE_CORE_API  __declspec(dllexport)
#else
#define PBE_CORE_API  __declspec(dllimport)
#endif

PBE_CORE_API void CoreUI();

namespace core {
   PBE_CORE_API void CoreUI2();
}
