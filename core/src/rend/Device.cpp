#include "pch.h"
#include "Device.h"

#include "app/Window.h"
#include "Common.h"
#include "Texture2D.h"
#include "core/Common.h"
#include "core/Log.h"

namespace pbe {

   Device* sDevice = nullptr;

   Device::Device() {
      sDevice = this;

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
      INFO("Create device with debug layer");
#endif

      ID3D11Device* device;
      ID3D11DeviceContext* context{};

      D3D_FEATURE_LEVEL featureLevel;
      const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_10_0, };
      // const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_10_0, };
      if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2,
         D3D11_SDK_VERSION, &sd, &g_pSwapChain, &device, &featureLevel,
         &context) != S_OK) {
         WARN("Failed create device");
         return;
      }

      device->QueryInterface(IID_PPV_ARGS(&g_pd3dDevice));
      context->QueryInterface(IID_PPV_ARGS(&g_pd3dDeviceContext));

      device->Release();
      context->Release();

      SetDbgName(g_pd3dDeviceContext, "immidiate contex");

      if (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) {
         INFO("Querry debug interface");
         g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&g_d3dDebug));
      }

      INFO("Device init success");

      SetupBackbuffer();

      created = true;
   }

   Device::~Device() {
      backBuffer = nullptr;

      SAFE_RELEASE(g_pSwapChain);
      SAFE_RELEASE(g_pd3dDeviceContext);

      if (g_d3dDebug) {
         // g_d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
         // g_d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
      }
      SAFE_RELEASE(g_d3dDebug);
      SAFE_RELEASE(g_pd3dDevice);
   }

   void Device::Resize(int2 size) {
      INFO("Device resize {}", size);

      backBuffer = nullptr;

      g_pSwapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, 0);

      SetupBackbuffer();
   }

   void Device::SetupBackbuffer() {
      ID3D11Texture2D* pBackBuffer;
      g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
      backBuffer = Texture2D::Create(pBackBuffer);
      backBuffer->SetDbgName("back buffer");
   }

}
