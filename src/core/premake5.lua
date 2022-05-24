project "core"
   kind "SharedLib"
   language "C++"
   cppdialect "C++20"

   targetdir "../../bin/%{cfg.buildcfg}"
   objdir "../../bin-int/%{cfg.buildcfg}"

   defines { "PBE_CORE_API_DLL" }

   files { "**.h", "**.cpp" }

   includedirs { "../imgui" }
   links { "imgui" }
