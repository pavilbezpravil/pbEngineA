#include "shared/hlslCppShared.hlsli"
#include "common.inl"
#include "tonemaping.hlsli"

Texture2D<float3> gColorHDR;
RWTexture2D<float4> gColorLDR;
// RWTexture2D<float4> gColorLDR;

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: check border
   float3 colorHDR = gColorHDR[dispatchThreadID.xy];

   // color = color / (color + 1);
   float3 colorLDR = ACESFilm(colorHDR);
   colorLDR = GammaCorrection(colorLDR);
   // gColorLDR[dispatchThreadID.xy] = colorLDR;
   gColorLDR[dispatchThreadID.xy] = float4(colorLDR, 1);
   // gColorLDR[dispatchThreadID.xy] = colorHDR;
   // gColorLDR[dispatchThreadID.xy] = float3(1, 1, 0);
}
