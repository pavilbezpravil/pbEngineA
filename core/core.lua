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

      -- todo: not in release
      libsinfo.winPixEventRuntime.includepath
   }
   libsinfo.core.natvis = os.getcwd().."/natvis/*.natvis"

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs {
      libsinfo.core.includedirs,
      "%{libsinfo.blast.includepath}/shared/NvFoundation",
      "%{libsinfo.blast.includepath}/lowlevel",
      "%{libsinfo.blast.includepath}/toolkit",
      -- todo: compile option. It must be easyly disabled
      libsinfo.nrd.includepath,
   }

   libdirs {
      libsinfo.physx.libDir,
      libsinfo.blast.libDir,
      libsinfo.nrd.libDir,
      libsinfo.winPixEventRuntime.libDir
   }

   links {
       "imgui", "d3d11", "yaml", "optick", "dxguid",
       "PhysX_64",
       "PhysXCommon_64",
       "PhysXExtensions_static_64",
       "PhysXFoundation_64",
       "PhysXPvdSDK_static_64",
       "NvBlastTk",
       "NRD",
       "WinPixEventRuntime",
   }

   postbuildcommands {
      '{COPY} "%{libsinfo.physx.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.blast.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.nrd.libDir}/*.dll" "%{cfg.targetdir}"',
      '{COPY} "%{libsinfo.winPixEventRuntime.libDir}/*.dll" "%{cfg.targetdir}"',
   }

   defines { "CORE_API_EXPORT" }
   files { "src/**.h", "src/**.cpp", "shaders/**", libsinfo.core.natvis, libsinfo.glm.natvis, libsinfo.entt.natvis }

   filter "files:shaders/**"
      buildaction "None"
