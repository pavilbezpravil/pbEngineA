project "core"
   sharedCppLib()

   libsinfo.shaders = {}
   libsinfo.shaders.includepath = os.getcwd().."/shaders"

   libsinfo.core = {}
   libsinfo.core.includepath = os.getcwd().."/src"
   libsinfo.core.includedirs = {
      libsinfo.core.includepath,
      libsinfo.imgui.includepath,
      libsinfo.glm.includepath,
      libsinfo.spdlog.includepath,
      libsinfo.yaml.includepath,
      libsinfo.optick.includepath,
      libsinfo.entt.includepath,
      libsinfo.shaders.includepath, -- todo: remove
      libsinfo.physx.includepath,
   }
   libsinfo.core.natvis = os.getcwd().."/natvis/*.natvis"

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs( libsinfo.core.includedirs )
   includedirs {
      "path",
   }

   libdirs {
      "%{libsinfo.physx.libDir}",
   }

   links {
       "imgui", "d3d11", "yaml", "optick", "dxguid",
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
   files { "src/**.h", "src/**.cpp", "shaders/**", libsinfo.core.natvis, libsinfo.glm.natvis, libsinfo.entt.natvis }

   filter "files:shaders/**"
      buildaction "None"
