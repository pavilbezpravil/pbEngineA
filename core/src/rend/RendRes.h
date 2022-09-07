#pragma once

#include "Device.h"


namespace pbe {

   struct VertexPos {
      vec3 position;

      static inline std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosNormal {
      vec3 position;
      vec3 normal;

      static inline std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   struct VertexPosColor {
      vec3 position;
      vec4 color;

      static inline std::vector<D3D11_INPUT_ELEMENT_DESC> inputElementDesc;
   };

   namespace rendres {

      void Init();
      void Term();

      extern ID3D11RasterizerState1* rasterizerState;
      extern ID3D11RasterizerState1* rasterizerStateWireframe;
      extern CORE_API ID3D11SamplerState* samplerStateWrapPoint;
      extern ID3D11SamplerState* samplerStateWrapLinear;
      extern ID3D11SamplerState* samplerStateShadow;
      extern ID3D11DepthStencilState* depthStencilStateDepthReadWrite;
      extern ID3D11DepthStencilState* depthStencilStateDepthReadNoWrite;
      extern ID3D11DepthStencilState* depthStencilStateEqual;
      extern ID3D11BlendState* blendStateDefaultRGB;
      extern ID3D11BlendState* blendStateDefaultRGBA;
      extern ID3D11BlendState* blendStateTransparency;

      ID3D11InputLayout* GetInputLayout(ID3DBlob* vsBlob, std::vector<D3D11_INPUT_ELEMENT_DESC>& inputElementDesc);

   }
}
