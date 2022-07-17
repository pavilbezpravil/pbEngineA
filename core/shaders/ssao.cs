#include "shared/common.hlsli"

SamplerState gSamplerPoint;

Texture2D<float> gDepth;
Texture2D<float3> gNormal;
RWTexture2D<float> gSsao;

cbuffer gCameraCB {
  CameraCB gCamera;
}

// float LinearizeDepth(float depth, float near, float far) {
//    return (2.0f * near) / (far + near - depth * (far - near));
// }
float LinearizeDepth(float depth) {
   // depth = A + B / z
   float A = gCamera.projection[2][2];
   float B = gCamera.projection[2][3];
   // todo: why minus?
   return -B / (depth - A);
}

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   float depth = gDepth[dispatchThreadID.xy];
   float lineardepth = LinearizeDepth(depth);
   gSsao[dispatchThreadID.xy] = frac(lineardepth);

   // gSsao[dispatchThreadID.xy] = 0.5;
}
