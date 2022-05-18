project "dll_example"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   defines { "DLL_EXAMPLE" }

   files { "**.h", "**.cpp" }
