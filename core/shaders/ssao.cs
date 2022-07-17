#include "shared/common.hlsli"

SamplerState gSamplerPoint;

Texture2D<float> gDepth;
RWTexture2D<float> gSsao;

cbuffer gCameraCB {
  CameraCB gCamera;
}

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   gSsao[dispatchThreadID.xy] = gDepth[dispatchThreadID.xy];
   gSsao[dispatchThreadID.xy] = 0.5;
}
