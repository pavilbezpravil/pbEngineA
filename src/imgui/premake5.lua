project "imgui"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }
