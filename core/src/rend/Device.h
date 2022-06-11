#pragma once

#include <d3d11.h>
#include <dxgi.h>

#include "core/Ref.h"
#include "math/Types.h"


class Texture2D;

class Device {
public:
   Device();

   ~Device();

   void Resize(int2 size);

   ID3D11Device* g_pd3dDevice = NULL;
   ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
   IDXGISwapChain* g_pSwapChain = NULL;


   Ref<Texture2D> backBuffer;
   bool created = false;

private:
   void SetupBackbuffer();
};

extern Device* sDevice;

