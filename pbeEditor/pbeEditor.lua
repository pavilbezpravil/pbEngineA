project "pbeEditor"
   consoleCppApp()

   files { "**.h", "**.cpp" }

   includedirs { libsinfo.core.includepath, libsinfo.imgui.includepath,
                 libsinfo.glm.includepath,
                 libsinfo.spdlog.includepath }
   links { "imgui", "core", "d3d11.lib" }
   -- remove d3d11
