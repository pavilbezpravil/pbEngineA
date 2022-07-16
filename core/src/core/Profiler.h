#pragma once

#include <chrono>

#include "Common.h"
#include "optick.h"


namespace pbe {

   struct CpuTimer {
      void Start() {
         tStart = std::chrono::high_resolution_clock::now();
      }

      float ElapsedMs(bool restart = false) {
         std::chrono::time_point tEnd = std::chrono::high_resolution_clock::now();
         float elapsed = std::chrono::duration<float, std::milli>(tEnd - tStart).count();

         if (restart) {
            tStart = tEnd;
         }

         return elapsed;
      }

   private:
      std::chrono::time_point<std::chrono::high_resolution_clock> tStart = std::chrono::high_resolution_clock::now();
   };

   class Profiler {
      NON_COPYABLE(Profiler);
   public:
      Profiler() = default;
      static Profiler& Get();

      struct CpuEvent {
         std::string name;
         CpuTimer timer;
         float elapsedMs = 0;

         void Start() {
            timer.Start();
         }

         void Stop() {
            elapsedMs = timer.ElapsedMs();
         }
      };

      CpuEvent& CreateCpuEvent(std::string_view name) {
         if (cpuEvents.find(name) == cpuEvents.end()) {
            cpuEvents[name] = CpuEvent{name.data()};
         }

         CpuEvent& cpuEvent = cpuEvents[name];
         return cpuEvent;
      }

      std::unordered_map<std::string_view, CpuEvent> cpuEvents;
   };

   struct CpuEventGuard {
      CpuEventGuard(Profiler::CpuEvent& cpu_event)
         : cpuEvent(cpu_event) {
         cpuEvent.Start();
      }

      ~CpuEventGuard() {
         cpuEvent.Stop();
      }

      Profiler::CpuEvent& cpuEvent;
   };

#define PROFILE_CPU(Name) CpuEventGuard cpuEvent{Profiler::Get().CreateCpuEvent(Name)}

}
