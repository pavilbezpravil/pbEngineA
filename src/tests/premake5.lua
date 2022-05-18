project "tests"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   debugdir "../../bin/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }
