#include "pch.h"
#include "Profiler.h"

namespace pbe {

   Profiler& Profiler::Get() {
      static Profiler sProfiler;
      return sProfiler;
   }

}
