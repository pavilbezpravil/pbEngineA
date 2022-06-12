project "optick"
   staticCppLib()

   defines { "OPTICK_ENABLE_GPU=0" }

   libsinfo.optick = {}
   libsinfo.optick.includepath = os.getcwd().."/src"

   includedirs { libsinfo.optick.includepath }

   files { "src/**.h", "src/**.cpp" }
