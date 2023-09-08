#include "commonResources.hlsli"
#include "common.hlsli"
#include "tonemaping.hlsli"

#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

Texture2D<float4> gIn;
RWTexture2D<float4> gOut;

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: check border

   float4 colorIn = gIn[dispatchThreadID.xy];

   #if defined(NORMAL)
      colorIn = NRD_FrontEnd_UnpackNormalAndRoughness(colorIn);
      colorIn.xyz = colorIn.xyz * 0.5 + 0.5;
   #elif defined(ROUGHNESS)
      colorIn = NRD_FrontEnd_UnpackNormalAndRoughness(colorIn);
      colorIn.xyz = colorIn.www;
   #elif defined(METALLIC)
      colorIn.xyz = colorIn.www;
   #elif defined(DIFFUSE_SPECULAR)
      colorIn = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(colorIn);
   #elif defined(MOTION)
      float motionShowScale = 1;
      colorIn.xyz = colorIn.xyz * motionShowScale * 0.5 + 0.5;
   #endif

   float3 colorOut = colorIn.xyz;
   #if defined(DIFFUSE_SPECULAR)
      colorOut = ACESFilm(colorIn.xyz * gScene.exposition);
   #endif

   colorOut = GammaCorrection(colorOut);
   gOut[dispatchThreadID.xy] = float4(colorOut, 1);
}
