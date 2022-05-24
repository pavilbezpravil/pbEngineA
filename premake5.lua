workspace "pbEngine"
   configurations { "Debug", "Release" }
   flags { "MultiProcessorCompile" }
   platforms { "x64" }
   systemversion "latest"

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "RELEASE" }
      optimize "On"

   filter {}

-- targetdir "../../bin/%{prj.name}/%{cfg.buildcfg}"
-- objdir "../../bin-int/%{prj.name}/%{cfg.buildcfg}"

include "src/tests/premake5.lua"
include "src/dll_example/premake5.lua"
include "src/imgui/premake5.lua"
include "src/imgui_test/premake5.lua"
include "src/core/premake5.lua"
include "src/pbeEditor/premake5.lua"
