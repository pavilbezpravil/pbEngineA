project "core"
   staticCppLib()

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"

   includedirs { libsinfo.core.includepath,
                 libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.yaml.includepath,
                 libsinfo.optick.includepath }
   links { "imgui", "d3d11.lib", "yaml", "optick" }

   files { "**.h", "**.cpp" }
