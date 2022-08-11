project "yaml"
    sharedCppLib()

    libsinfo.yaml = {}
    libsinfo.yaml.includepath = os.getcwd().."/include"

    includedirs { libsinfo.yaml.includepath }

    defines { "yaml_cpp_EXPORTS" }
    files { "include/**.h", "src/**.cpp" }
