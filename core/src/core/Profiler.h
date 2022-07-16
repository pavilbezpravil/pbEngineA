#pragma once

#include <chrono>
#include <deque>

#include "Assert.h"
#include "Common.h"
#include "optick.h"
#include "rend/GpuTimer.h"


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

      struct AverageTime {
         void Add(float time, int maxValues) {
            average += time;
            times.push_back(time);
            while (times.size() > maxValues) {
               average -= times.front();
               times.pop_front();
            }
         }

         float GetCur() const {
            return times.empty() ? 0 : times.back();
         }

         float GetAverage() const {
            return times.empty() ? 0 : average / times.size();
         }

         void Clear() {
            times.clear();
         }

      private:
         std::deque<float> times;
         float average = 0;
      };

      struct CpuEvent {
         std::string name;
         CpuTimer timer;
         float elapsedMsCur = 0;

         AverageTime averageTime;

         bool usedInFrame = false;

         void Start() {
            ASSERT(usedInFrame == false);
            usedInFrame = true;
            timer.Start();
         }

         void Stop() {
            elapsedMsCur = timer.ElapsedMs();
         }
      };

      struct GpuEvent {
         std::string name;
         GpuTimer timer[2];
         int timerIdx = 0;

         AverageTime averageTime;

         bool usedInFrame = false;

         void Start() {
            ASSERT(usedInFrame == false);
            usedInFrame = true;
            timer[timerIdx].Start();
         }

         void Stop() {
            timer[timerIdx].Stop();
         }
      };

      int historyLength = 30;

      void NextFrame() {
         for (auto& [name, event] : cpuEvents) {
            if (event.usedInFrame) {
               event.usedInFrame = false;

               event.averageTime.Add(event.elapsedMsCur, historyLength);
               event.elapsedMsCur = -1;
            } else {
               event.averageTime.Clear();
            }
         }

         for (auto& [name, event] : gpuEvents) {
            if (event.usedInFrame) {
               event.usedInFrame = false;

               event.timerIdx = 1 - event.timerIdx;

               if (event.timer[event.timerIdx].GetData()) {
                  event.averageTime.Add(event.timer[event.timerIdx].GetTimeMs(), historyLength);
               }
            } else {
               event.averageTime.Clear();
            }
         }
      }

      CpuEvent& CreateCpuEvent(std::string_view name) {
         if (cpuEvents.find(name) == cpuEvents.end()) {
            cpuEvents[name] = CpuEvent{name.data()};
         }

         CpuEvent& cpuEvent = cpuEvents[name];
         return cpuEvent;
      }

      GpuEvent& CreateGpuEvent(std::string_view name) {
         if (gpuEvents.find(name) == gpuEvents.end()) {
            gpuEvents[name] = GpuEvent{ name.data() };
         }

         GpuEvent& gpuEvent = gpuEvents[name];
         return gpuEvent;
      }

      std::unordered_map<std::string_view, CpuEvent> cpuEvents;
      std::unordered_map<std::string_view, GpuEvent> gpuEvents;
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

   struct GpuEventGuard {
      GpuEventGuard(Profiler::GpuEvent& gpuEvent)
         : gpuEvent(gpuEvent) {
         gpuEvent.Start();
      }

      ~GpuEventGuard() {
         gpuEvent.Stop();
      }

      Profiler::GpuEvent& gpuEvent;
   };

#define PROFILE_CPU(Name) CpuEventGuard cpuEvent{ Profiler::Get().CreateCpuEvent(Name) }
#define PROFILE_GPU(Name) GpuEventGuard gpuEvent{ Profiler::Get().CreateGpuEvent(Name) }

}
