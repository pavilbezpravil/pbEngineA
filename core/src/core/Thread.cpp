#include "Thread.h"

#include <thread>

void ThreadSleepMs(float ms) {
   int ims = std::max(int(ms), 1);
   std::this_thread::sleep_for(std::chrono::microseconds(ims));
}
