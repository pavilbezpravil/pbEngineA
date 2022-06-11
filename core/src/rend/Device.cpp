#include "Device.h"
#include "app/Window.h"

#include <d3dcommon.h>
#include <winerror.h>

#include "Texture2D.h"


Device* sDevice = nullptr;

Device::Device() {
   // Setup swap chain
   DXGI_SWAP_CHAIN_DESC sd;
   ZeroMemory(&sd, sizeof(sd));
   sd.BufferCount = 2;
   sd.BufferDesc.Width = 0;
   sd.BufferDesc.Height = 0;
   sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   sd.BufferDesc.RefreshRate.Numerator = 60;
   sd.BufferDesc.RefreshRate.Denominator = 1;
   sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
   sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   sd.OutputWindow = sWindow->hwnd;
   sd.SampleDesc.Count = 1;
   sd.SampleDesc.Quality = 0;
   sd.Windowed = TRUE;
   sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

   UINT createDeviceFlags = 0;
#if defined(DEBUG)
   createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

   D3D_FEATURE_LEVEL featureLevel;
   const D3D_FEATURE_LEVEL featureLevelArray[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0,};
   if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2,
                                     D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel,
                                     &g_pd3dDeviceContext) != S_OK) {
      return;
   }

   ID3D11Texture2D* pBackBuffer;
   g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
   backBuffer = Ref<Texture2D>::Create(pBackBuffer);

   created = true;
}

Device::~Device() {
   backBuffer = nullptr;

   if (g_pSwapChain) {
      g_pSwapChain->Release();
      g_pSwapChain = NULL;
   }
   if (g_pd3dDeviceContext) {
      g_pd3dDeviceContext->Release();
      g_pd3dDeviceContext = NULL;
   }
   if (g_pd3dDevice) {
      g_pd3dDevice->Release();
      g_pd3dDevice = NULL;
   }
}

void Device::Resize(int2 size) {

   backBuffer = nullptr;

   g_pSwapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, 0);

   ID3D11Texture2D* pBackBuffer;
   g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
   backBuffer = Ref<Texture2D>::Create(pBackBuffer);
}
