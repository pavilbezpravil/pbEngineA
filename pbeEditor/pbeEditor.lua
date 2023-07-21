project "pbeEditor"
   consoleCppApp()

   libsinfo.editor = {}
   libsinfo.editor.includepath = os.getcwd().."/src"

   files { "**.h", "**.cpp" }

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs { libsinfo.core.includepath,
                 libsinfo.editor.includepath,
                 libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.yaml.includepath,
                 libsinfo.optick.includepath,
                 libsinfo.entt.includepath,
                 libsinfo.shaders.includepath,
                 libsinfo.physx.includepath,
               }
    -- remove yaml
   links { "core", "imgui", "yaml" }
