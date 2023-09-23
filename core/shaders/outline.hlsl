#include "commonResources.hlsli"
#include "common.hlsli"

Texture2D<float3> gOutline;
RWTexture2D<float4> gOutlineBlurOut;

[numthreads(8, 8, 1)]
void OutlineBlurCS( uint2 pixelPos : SV_DispatchThreadID ) { 
   // todo: check border

   float3 x0 = gOutline[pixelPos];

   float3 x1 = gOutline[pixelPos + int2( 1, 1)];
   float3 x2 = gOutline[pixelPos + int2( 1,-1)];
   float3 x3 = gOutline[pixelPos + int2(-1, 1)];
   float3 x4 = gOutline[pixelPos + int2(-1,-1)];

   float3 x5 = gOutline[pixelPos + int2( 1, 0)];
   float3 x6 = gOutline[pixelPos + int2(-1, 0)];
   float3 x7 = gOutline[pixelPos + int2( 0, 1)];
   float3 x8 = gOutline[pixelPos + int2( 0,-1)];

   float3 o = x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8;

   gOutlineBlurOut[pixelPos] = float4(saturate(o), 1);
}

Texture2D<float3> gOutlineBlur;
RWTexture2D<float4> gSceneOut;

[numthreads(8, 8, 1)]
void OutlineApplyCS( uint2 pixelPos : SV_DispatchThreadID ) { 
   // todo: check border

   float3 color = gSceneOut[pixelPos].xyz;
   float3 outline = gOutlineBlur[pixelPos];

   bool isBorder = any(gOutlineBlur[pixelPos] > 0) && all(gOutline[pixelPos] == 0);
   
   gSceneOut[pixelPos] = float4(lerp(color, gOutlineBlur[pixelPos], isBorder), 1);
}
