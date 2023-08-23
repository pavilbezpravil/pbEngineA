#pragma once

namespace pbe {

   struct TimedAction {
      TimedAction(float freq = 1) : freq(freq) {}

      int Update(float dt, int maxKeepingActions = -1) {
         timeAccumulator += dt;

         const float freqInv = 1.0f / freq;
         int steps = (int)(timeAccumulator / freqInv);
         timeAccumulator -= steps * freqInv;

         if (maxKeepingActions >= 0) {
            timeAccumulator = std::max(steps, maxKeepingActions) * freqInv;
         }

         return steps;
      }

      void Reset() {
         timeAccumulator = 0;
      }

      void SetFreq(float _freq) {
         freq = _freq;
      }

      float GetActTime() const {
         return 1.0f / freq;
      }

   private:
      float timeAccumulator = 0;
      float freq = 1;
   };

}
