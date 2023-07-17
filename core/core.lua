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
                 libsinfo.shaders.includepath,
                 libsinfo.physx.includepath,
                }

   libdirs {
      "%{libsinfo.physx.libDir}",
   }

   links {
       "imgui", "d3d11.lib", "yaml", "optick", "dxguid.lib",
       "PhysX_64",
       "PhysXCommon_64",
       "PhysXExtensions_static_64",
       "PhysXFoundation_64",
       "PhysXPvdSDK_static_64",
   }

   postbuildcommands {
      '{COPY} "%{libsinfo.physx.libDir}/*.dll" "%{cfg.targetdir}"',
   }

   defines { "CORE_API_EXPORT" }
   files { "src/**.h", "src/**.cpp", libsinfo.core.natvis, libsinfo.glm.natvis, libsinfo.entt.natvis }
