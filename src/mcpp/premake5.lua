project "mcpp"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   files { "**.h", "**.cpp" }

   -- includedirs { "../imgui" }
   -- links { "imgui" }
