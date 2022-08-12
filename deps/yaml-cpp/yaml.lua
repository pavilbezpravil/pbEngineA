project "yaml"
    sharedCppLib()

    libsinfo.yaml = {}
    libsinfo.yaml.includepath = os.getcwd().."/include"

    includedirs { libsinfo.yaml.includepath }

    disablewarnings { "4251", "4275" }
    defines { "yaml_cpp_EXPORTS" }
    files { "include/**.h", "src/**.cpp" }
