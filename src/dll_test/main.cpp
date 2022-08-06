#include <windows.h>
#include <iostream>
#include "coreDll.h"

using scriptFunc = int(__stdcall *)();

int main() {
   coreFunc();
   core::namespaceFunc();

   Engine e;
   e.helloStatic();

   HINSTANCE hGetProcIDDLL = LoadLibrary(L"scriptDll.dll");

   if (!hGetProcIDDLL) {
      std::cout << "could not load the dynamic library" << std::endl;
      return EXIT_FAILURE;
   }

   // resolve function address here
   scriptFunc funci = (scriptFunc)GetProcAddress(hGetProcIDDLL, "scriptFunc");
   if (!funci) {
      std::cout << "could not locate the function" << std::endl;
      return EXIT_FAILURE;
   }

   funci();

   FreeLibrary(hGetProcIDDLL);

   return 0;
}
