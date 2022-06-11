project "yaml"
    staticCppLib()

    libsinfo.yaml = {}
    libsinfo.yaml.includepath = os.getcwd().."/include"

    includedirs { libsinfo.yaml.includepath }

    defines { "YAML_CPP_STATIC_DEFINE" }
    files { "include/**.h", "src/**.cpp" }
