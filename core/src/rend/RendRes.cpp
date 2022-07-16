#include "pch.h"
#include "RendRes.h"

#include "Common.h"
#include "Device.h"
#include "core/Assert.h"
#include "core/Common.h"

namespace pbe {
   namespace rendres {

      ID3D11RasterizerState1* rasterizerState;
      ID3D11SamplerState* samplerState;
      ID3D11DepthStencilState* depthStencilState;
      ID3D11DepthStencilState* depthStencilStateEqual;
      ID3D11DepthStencilState* depthStencilStateDisable;
      ID3D11BlendState* blendStateTransparency;

      void Init() {
         VertexPos::inputElementDesc = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
         };

         VertexPosNormal::inputElementDesc = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         };

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

         depthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
         depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
         device->CreateDepthStencilState(&depthStencilDesc, &depthStencilStateEqual);

         // todo: wtf?
         depthStencilDesc.DepthEnable = false;
         depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
         device->CreateDepthStencilState(&depthStencilDesc, &depthStencilStateDisable);

         D3D11_BLEND_DESC transparentDesc = { 0 };
         transparentDesc.AlphaToCoverageEnable = false;
         transparentDesc.IndependentBlendEnable = false;

         transparentDesc.RenderTarget[0].BlendEnable = true;
         transparentDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
         transparentDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
         transparentDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
         transparentDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
         transparentDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
         transparentDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
         transparentDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

         device->CreateBlendState(&transparentDesc, &blendStateTransparency);
      }

      void Term() {
         SAFE_RELEASE(rasterizerState);
         SAFE_RELEASE(samplerState);
         SAFE_RELEASE(depthStencilState);
         SAFE_RELEASE(depthStencilStateEqual);
         SAFE_RELEASE(depthStencilStateDisable);
         SAFE_RELEASE(blendStateTransparency);
      }

      struct InputLayoutEntry {
         ComPtr<ID3D11InputLayout> inputLayout;
         // std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
      };

      static std::unordered_map<ID3DBlob*, InputLayoutEntry> sLayoutMap;

      ID3D11InputLayout* GetInputLayout(ID3DBlob* vsBlob,
         std::vector<D3D11_INPUT_ELEMENT_DESC>& inputElementDesc) {
         // todo: inputElementDesc

         if (sLayoutMap.find(vsBlob) == sLayoutMap.end()) {
            ComPtr<ID3D11InputLayout> input_layout_ptr;

            HRESULT hr = sDevice->g_pd3dDevice->CreateInputLayout(
               inputElementDesc.data(),
               (uint)inputElementDesc.size(),
               vsBlob->GetBufferPointer(),
               vsBlob->GetBufferSize(),
               input_layout_ptr.GetAddressOf());
            ASSERT(SUCCEEDED(hr));
            SetDbgName(input_layout_ptr.Get(), "input layout");

            sLayoutMap[vsBlob] = InputLayoutEntry{ input_layout_ptr };
         }

         return sLayoutMap[vsBlob].inputLayout.Get();
      }

   }
}
