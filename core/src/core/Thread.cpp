#include "pch.h"
#include "Thread.h"

#include <thread>

namespace pbe {

   void ThreadSleepMs(float ms) {
      int ims = std::max(int(ms), 1);
      std::this_thread::sleep_for(std::chrono::milliseconds(ims));
   }

}
