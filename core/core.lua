project "core"
   staticCppLib()

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"

   pchheader "pch.h"
   pchsource "pch.cpp"

   includedirs { libsinfo.core.includepath,
                 libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.yaml.includepath,
                 libsinfo.optick.includepath,
                 libsinfo.entt.includepath }

   links { "imgui", "d3d11.lib", "yaml", "optick", "dxguid.lib" }

   files { "**.h", "**.cpp" }
