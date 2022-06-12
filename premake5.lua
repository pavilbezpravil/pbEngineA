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

libsinfo = {}

-- print(os.getcwd())

workspace_dir = os.getcwd()

function setBuildDirs()
   targetdir(workspace_dir.."/bin/%{cfg.buildcfg}")
   objdir(workspace_dir.."/bin-int/%{cfg.buildcfg}")
end

function staticCppLib()
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"

   setBuildDirs()
end

function consoleCppApp()
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"

   debugdir(workspace_dir.."/bin/%{cfg.buildcfg}")

   setBuildDirs()
end

-- include "src/tests/premake5.lua"
-- include "src/dll_example/premake5.lua"
-- include "src/imgui/premake5.lua"
-- include "src/imgui_test/premake5.lua"
-- include "src/core/premake5.lua"
-- include "src/pbeEditor/premake5.lua"

include "deps/deps.lua"

group "deps"
   include "deps/imgui/imgui.lua"
   include "deps/yaml-cpp/yaml.lua"
   include "deps/optick/optick.lua"
group ""

include "core/core.lua"
include "pbeEditor/pbeEditor.lua"
