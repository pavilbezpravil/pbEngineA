#include "pch.h"
#include "Device.h"

#include "app/Window.h"
#include "Common.h"
#include "Texture2D.h"
#include "core/Common.h"
#include "core/CVar.h"
#include "core/Log.h"

namespace pbe {

   CVarValue<bool> cfgVSyncEnable{ "render/vsync", true };

   Device* sDevice = nullptr;

   std::string ToString(std::wstring_view wstr) {
      std::string result;

      int sz = WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), 0, 0, 0, 0);
      result = std::string(sz, 0);
      WideCharToMultiByte(CP_ACP, 0, &wstr[0], (int)wstr.size(), &result[0], sz, 0, 0);

      return result;
   }

   Microsoft::WRL::ComPtr<IDXGIAdapter1> CreateDeviceWithMostPowerfulAdapter() {
      Microsoft::WRL::ComPtr<IDXGIFactory1> dxgiFactory;
      HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)dxgiFactory.GetAddressOf());

      if (FAILED(hr)) {
         return {};
      }

      Microsoft::WRL::ComPtr<IDXGIAdapter1> mostPowerfulAdapter{};
      size_t maxDedicatedVideoMemory = 0;

      INFO("Adapter list:");
      for (UINT i = 0; ; ++i) {
         Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
         if (dxgiFactory->EnumAdapters1(i, adapter.GetAddressOf()) == DXGI_ERROR_NOT_FOUND) {
            break; // No more adapters to enumerate
         }

         DXGI_ADAPTER_DESC1 desc;
         adapter->GetDesc1(&desc);

         auto deviceName = desc.Description;
         auto name = ToString(std::wstring_view{ deviceName, wcslen(deviceName) });
         INFO("{}. {} {} Mb", i, name, desc.DedicatedVideoMemory / 1024 / 1024);

         if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
            maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
            mostPowerfulAdapter = adapter;
         }
      }

      return mostPowerfulAdapter;
   }

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
      sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

      UINT createDeviceFlags = 0;
      // createDeviceFlags = D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
#if defined(DEBUG)
      createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
      INFO("Create device with debug layer");
#endif

      auto adapter = CreateDeviceWithMostPowerfulAdapter();

      ID3D11Device* device;
      ID3D11DeviceContext* context{};

      D3D_FEATURE_LEVEL featureLevel;
      const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_10_0, };
      // const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_10_0, };
      if (D3D11CreateDeviceAndSwapChain(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, createDeviceFlags, featureLevelArray, 2,
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
      backBuffer[0] = nullptr;
      backBuffer[1] = nullptr;

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

      backBuffer[0] = nullptr;
      backBuffer[1] = nullptr;

      g_pSwapChain->ResizeBuffers(0, size.x, size.y, DXGI_FORMAT_UNKNOWN, 0);

      SetupBackbuffer();
   }

   Texture2D& Device::GetBackBuffer() {
      return *backBuffer[backBufferIdx];
   }

   void Device::Present() {
      int syncInterval = cfgVSyncEnable ? 1 : 0;
      sDevice->g_pSwapChain->Present(syncInterval, 0);
      // ++backBufferIdx; // todo:
   }

   void Device::SetupBackbuffer() {
      for (int i = 0; i < 1; ++i) {
         ID3D11Texture2D* pBackBuffer;
         g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
         backBuffer[i] = Texture2D::Create(pBackBuffer);
         backBuffer[i]->SetDbgName(std::string("back buffer ") + std::to_string(i));
      }
   }

}
