libsinfo.glm = {}
libsinfo.glm.includepath = os.getcwd().."/glm"
libsinfo.glm.natvis = os.getcwd().."/glm/util/*.natvis"

libsinfo.spdlog = {}
libsinfo.spdlog.includepath = os.getcwd().."/spdlog/include"

libsinfo.entt = {}
libsinfo.entt.includepath = os.getcwd().."/entt/src"
libsinfo.entt.natvis = os.getcwd().."/entt/natvis/*.natvis"

libsinfo.physx = {}
libsinfo.physx.includepath = os.getcwd().."/physx/include"
libsinfo.physx.libDir = os.getcwd().."/physx/bin/%{cfg.buildcfg}" -- todo:

libsinfo.nrd = {}
libsinfo.nrd.includepath = os.getcwd().."/NRD/include"
libsinfo.nrd.integration_includepath = os.getcwd().."/NRD/integration"
libsinfo.nrd.libDir = os.getcwd().."/NRD/bin/%{cfg.buildcfg}"

libsinfo.nri = {}
libsinfo.nri.includepath = os.getcwd().."/NRI/include"
libsinfo.nri.libDir = os.getcwd().."/NRI/bin/%{cfg.buildcfg}"
