project "imgui"
   -- kind "SharedLib"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{prj.name}/%{cfg.buildcfg}"
   objdir "../../bin-int/%{prj.name}/%{cfg.buildcfg}"

   -- defines { 'IMGUI_API=extern "C" __declspec(dllexport)' }
   files { "**.h", "**.cpp" }
