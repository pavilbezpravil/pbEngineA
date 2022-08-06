#ifdef CORE_API_EXPORT
   #define CORE_API  __declspec(dllexport)
#else
   #define CORE_API  __declspec(dllimport)
#endif

extern "C" CORE_API int coreFunc();
