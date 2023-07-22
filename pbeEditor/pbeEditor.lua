project "pbeEditor"
   consoleCppApp()

   files { "**.h", "**.cpp" }

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs(libsinfo.core.includedirs)
   includedirs("src")

   links { "core", "imgui" }
