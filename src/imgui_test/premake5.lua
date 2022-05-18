project "imgui_test"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{prj.name}/%{cfg.buildcfg}"
   objdir "../../bin-int/%{prj.name}/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }

   -- defines { 'IMGUI_API=extern "C" __declspec(dllimport)' }
   includedirs { "../imgui" }
   links { "imgui", "d3d11.lib" }
   -- links { "imgui", "d3d12.lib", "dxgi.lib", "dxguid.lib" }
