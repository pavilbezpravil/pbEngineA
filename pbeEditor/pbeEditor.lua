project "pbeEditor"
   consoleCppApp()

   files { "**.h", "**.cpp" }

   includedirs { libsinfo.core.includepath, libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath,
                 libsinfo.optick.includepath,
                 libsinfo.entt.includepath }
   links { "core" }
