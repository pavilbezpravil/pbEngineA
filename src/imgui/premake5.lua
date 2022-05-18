project "imgui"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   defines { "IMGUI_API_EXPORT" }
   files { "**.h", "**.cpp" }
