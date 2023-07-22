workspace "pbEngine"
   configurations { "Debug", "Release" }
   flags { "MultiProcessorCompile" }
   platforms { "x64" }
   systemversion "latest"

   characterset "ASCII"

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "RELEASE", "NDEBUG" }
      optimize "On"

   filter {}

   startproject "pbeEditor"

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

function sharedCppLib()
   kind "SharedLib"
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

include "deps/deps.lua"

group "deps"
   include "deps/imgui/imgui.lua"
   include "deps/yaml-cpp/yaml.lua"
   include "deps/optick/optick.lua"
group ""

include "core/core.lua"
include "pbeEditor/pbeEditor.lua"
include "testProj/testProj.lua"
