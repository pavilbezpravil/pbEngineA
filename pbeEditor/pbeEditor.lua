project "pbeEditor"
   consoleCppApp()

   files { "**.h", "**.cpp" }

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs { libsinfo.core.includepath,
                 libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.yaml.includepath,
                 libsinfo.optick.includepath,
                 libsinfo.entt.includepath,
                 libsinfo.shaders.includepath }
   links { "core" }
