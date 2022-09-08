#include "shared/hlslCppShared.hlsli"
#include "common.hlsli"
#include "samplers.hlsli"
#include "tonemaping.hlsli"

Texture2D<float3> gColorHDR;
RWTexture2D<float4> gColorLDR;
// RWTexture2D<float4> gColorLDR;

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: check border
   float3 colorHDR = gColorHDR[dispatchThreadID.xy];

   // float3 colorLDR = ACESFilm(colorHDR);
   float3 colorLDR = 1 - exp(-colorHDR * gScene.exposition);
   // float3 colorLDR = colorHDR / (colorHDR + 1);
   colorLDR = GammaCorrection(colorLDR);

   gColorLDR[dispatchThreadID.xy] = float4(colorLDR, 1);
}
