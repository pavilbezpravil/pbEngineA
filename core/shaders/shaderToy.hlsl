#include "commonResources.hlsli"
#include "common.hlsli"

RWTexture2D<float4> gOut;

[numthreads(8, 8, 1)]
void main( uint2 id : SV_DispatchThreadID ) {
   uint idx = id.x * 1231224457 + id.y * 2315213213;

   float v = float(idx) / 0xFFFFFFFF;

   float3 outColor = v;
   gOut[id] = float4(outColor, 1);
}
