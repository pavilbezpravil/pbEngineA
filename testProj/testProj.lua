project "testProj"
   sharedCppLib()

   files { "**.h", "**.cpp" }

   pchheader "pch.h"
   pchsource "src/pch.cpp"

   includedirs(libsinfo.core.includedirs)
   links { "core" }
