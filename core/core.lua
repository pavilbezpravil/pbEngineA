project "core"
   sharedCppLib()

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"
   libsinfo.core.natvis = os.getcwd().."/natvis/*.natvis"

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

   defines { "CORE_API_EXPORT" }
   files { "src/**.h", "src/**.cpp", libsinfo.core.natvis, libsinfo.glm.natvis, libsinfo.entt.natvis }
