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

include "src/tests/premake5.lua"
