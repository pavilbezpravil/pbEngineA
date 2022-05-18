project "imgui_test"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   debugdir "../../bin/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }

   includedirs { "../imgui" }
   links { "imgui", "d3d11.lib" }
   -- links { "imgui", "d3d12.lib", "dxgi.lib", "dxguid.lib" }
