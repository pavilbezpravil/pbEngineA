#ifdef SCRIPT_API_EXPORT
   #define SCRIPT_API  __declspec(dllexport)
#else
   #define SCRIPT_API  __declspec(dllimport)
#endif

extern "C" SCRIPT_API int scriptFunc();
