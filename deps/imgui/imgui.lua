project "imgui"
   staticCppLib()

   files { "**.h", "**.cpp" }

   libsinfo.imgui = {}
   libsinfo.imgui.includepath = os.getcwd()
