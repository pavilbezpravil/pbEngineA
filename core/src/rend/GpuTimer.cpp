#include "pch.h"
#include "GpuTimer.h"

#include "Device.h"
#include "core/Assert.h"


namespace pbe {

   GpuTimer::GpuTimer() {
      auto device = sDevice->g_pd3dDevice;

      CD3D11_QUERY_DESC queryDesc(D3D11_QUERY_TIMESTAMP);
      device->CreateQuery(&queryDesc, startQuery.GetAddressOf());
      device->CreateQuery(&queryDesc, stopQuery.GetAddressOf());

      queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
      device->CreateQuery(&queryDesc, disjointQuery.GetAddressOf());
   }

   void GpuTimer::Start() {
      if (busy) {
         return;
      }

      auto context = sDevice->g_pd3dDeviceContext;
      context->Begin(disjointQuery.Get());
      context->End(startQuery.Get());
   }

   void GpuTimer::Stop() {
      if (busy) {
         return;
      }
      busy = true;

      auto context = sDevice->g_pd3dDeviceContext;
      context->End(stopQuery.Get());
      context->End(disjointQuery.Get());
   }

   bool GpuTimer::GetData() {
      if (!busy) {
         return false;
      }

      auto context = sDevice->g_pd3dDeviceContext;

      bool dataReady = context->GetData(disjointQuery.Get(), &disjointData, sizeof(disjointData), 0) == S_OK
                    && context->GetData(startQuery.Get(), &start, sizeof(start), 0) == S_OK
                    && context->GetData(stopQuery.Get(), &stop, sizeof(stop), 0) == S_OK;
      if (!dataReady) {
         return false;
      }

      if (!disjointData.Disjoint) {
         timeMs = float(double(stop - start) / double(disjointData.Frequency) * 1000.);
      } else {
         WARN("Disjoint!");
      }

      busy = false;
      return true;
   }

   float GpuTimer::GetTimeMs() {
      GetData();
      return timeMs;
   }

}
