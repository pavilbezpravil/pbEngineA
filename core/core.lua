project "core"
   staticCppLib()

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   libsinfo.shaders = {}
   libsinfo.shaders.includepath = os.getcwd().."/shaders"

   includedirs { libsinfo.core.includepath,
                 libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.yaml.includepath,
                 libsinfo.optick.includepath,
                 libsinfo.entt.includepath,
                 libsinfo.shaders.includepath }

   links { "imgui", "d3d11.lib", "yaml", "optick", "dxguid.lib" }

   files { "**.h", "**.cpp", "natvis/glm.natvis" }
