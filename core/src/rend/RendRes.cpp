#include "pch.h"
#include "RendRes.h"

#include "Device.h"
#include "core/Common.h"

namespace pbe {
   namespace rendres {

      ID3D11RasterizerState1* rasterizerState;
      ID3D11SamplerState* samplerState;
      ID3D11DepthStencilState* depthStencilState;

      void Init() {
         auto device = sDevice->g_pd3dDevice;

         D3D11_RASTERIZER_DESC1 rasterizerDesc = {};
         rasterizerDesc.FillMode = D3D11_FILL_SOLID;
         rasterizerDesc.CullMode = D3D11_CULL_BACK;

         device->CreateRasterizerState1(&rasterizerDesc, &rasterizerState);

         ///////////////////////////////////////////////////////////////////////////////////////////////

         D3D11_SAMPLER_DESC samplerDesc = {};
         samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
         samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
         samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
         samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
         samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

         device->CreateSamplerState(&samplerDesc, &samplerState);

         ///////////////////////////////////////////////////////////////////////////////////////////////

         D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
         depthStencilDesc.DepthEnable = true;
         depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
         depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

         device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
      }

      void Term() {
         SAFE_RELEASE(rasterizerState);
         SAFE_RELEASE(samplerState);
         SAFE_RELEASE(depthStencilState);
      }

   }
}
