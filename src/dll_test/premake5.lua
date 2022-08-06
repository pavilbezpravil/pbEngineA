project "coreDll"
   sharedCppLib()
   defines { "CORE_API_EXPORT" }
   files { "coreDll.cpp", "coreDll.h" }

project "scriptDll"
   sharedCppLib()

   defines { "SCRIPT_API_EXPORT" }
   files { "scriptDll.cpp", "scriptDll.h", "coreDll.h" }

   links { "coreDll" }

project "dllEngine"
   consoleCppApp()

   files { "main.cpp" }

   links { "coreDll" }
