#include "shared/common.hlsli"
#include "common.inl"
#include "tonemaping.hlsli"
#include "samplers.hlsli"

Texture2D<float> gDepth;
RWTexture2D<float3> gColor;

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: check border
   float3 color = gColor[dispatchThreadID.xy];
   float depth = gDepth[dispatchThreadID.xy];

   // gColor[dispatchThreadID.xy] = color * 0.5;
   gColor[dispatchThreadID.xy] = color * 1;
}
