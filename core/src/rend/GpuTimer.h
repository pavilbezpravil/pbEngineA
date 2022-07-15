#pragma once
#include "Common.h"
#include "Device.h"
#include "core/Core.h"


namespace pbe {

   class GpuTimer {
   public:
      GpuTimer();

      void Start();
      void Stop();

      bool Ready();

      float GetTimeMs();

   private:
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData{};
      uint64 start = 0;
      uint64 stop = 0;

      bool busy = false;
      float time = 0;

      ComPtr<ID3D11Query> disjointQuery;
      ComPtr<ID3D11Query> startQuery;
      ComPtr<ID3D11Query> stopQuery;
   };

}
