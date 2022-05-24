#pragma once

#ifdef DLL_EXAMPLE
#define DLL_EXAMPLE_API  __declspec(dllexport)
#else
#define DLL_EXAMPLE_API  __declspec(dllimport)
#endif

class DLL_EXAMPLE_API DllExample {
public:
   DllExample();
   ~DllExample();

   void hello();
   static void helloStatic();
};

extern "C" DLL_EXAMPLE_API void dllFunc();
