#pragma once

#include <d3d11_3.h>
#include <dxgi.h>

#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {

   class Texture2D;

   class Device {
   public:
      Device();
      ~Device();

      void Resize(int2 size);

      ID3D11Device3* g_pd3dDevice{};
      ID3D11DeviceContext3* g_pd3dDeviceContext{};
      IDXGISwapChain* g_pSwapChain{};
      ID3D11Debug* g_d3dDebug{};

      Ref<Texture2D> backBuffer;
      bool created = false;

   private:
      void SetupBackbuffer();
   };

   extern Device* sDevice;

}
