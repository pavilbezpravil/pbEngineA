project "tests"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{prj.name}/%{cfg.buildcfg}"
   objdir "../../bin-int/%{prj.name}/%{cfg.buildcfg}"

   debugdir "../../bin/%{prj.name}/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }