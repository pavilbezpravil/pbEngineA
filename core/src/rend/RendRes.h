#pragma once
#include <d3d11_3.h>

#include "Common.h"
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

   namespace rendres {

      void Init();
      void Term();

      extern ID3D11RasterizerState1* rasterizerState;
      extern ID3D11SamplerState* samplerState;
      extern ID3D11DepthStencilState* depthStencilState;

      ID3D11InputLayout* GetInputLayout(ID3DBlob* vsBlob, std::vector<D3D11_INPUT_ELEMENT_DESC>& inputElementDesc);

   }
}
