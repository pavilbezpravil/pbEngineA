#pragma once
#include <d3d11_3.h>

namespace pbe {
   namespace rendres {

      void Init();
      void Term();

      extern ID3D11RasterizerState1* rasterizerState;
      extern ID3D11SamplerState* samplerState;
      extern ID3D11DepthStencilState* depthStencilState;

   }
}
