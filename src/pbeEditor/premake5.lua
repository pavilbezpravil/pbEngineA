project "pbeEditor"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   debugdir "../../bin/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }

   includedirs { "../imgui", "../core" }
   links { "imgui", "core", "d3d11.lib" }
   -- remove d3d11
