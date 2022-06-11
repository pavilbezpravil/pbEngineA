project "core"
   staticCppLib()

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"

   includedirs { libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.core.includepath }
   links { "imgui", "d3d11.lib"  }

   files { "**.h", "**.cpp" }
