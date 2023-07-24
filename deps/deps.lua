libsinfo.glm = {}
libsinfo.glm.includepath = os.getcwd().."/glm"
libsinfo.glm.natvis = os.getcwd().."/glm/util/*.natvis"

libsinfo.spdlog = {}
libsinfo.spdlog.includepath = os.getcwd().."/spdlog/include"

libsinfo.entt = {}
libsinfo.entt.includepath = os.getcwd().."/entt/src"
libsinfo.entt.natvis = os.getcwd().."/entt/natvis/*.natvis"

libsinfo.d3dx12 = {}
libsinfo.d3dx12.includepath = os.getcwd().."/d3dx12"

libsinfo.physx = {}
libsinfo.physx.includepath = os.getcwd().."/physx/include"
libsinfo.physx.libDir = os.getcwd().."/physx/bin/%{cfg.buildcfg}" -- todo:
