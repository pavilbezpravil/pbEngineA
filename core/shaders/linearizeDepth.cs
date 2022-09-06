#include "shared/hlslCppShared.hlsli"
#include "common.hlsli"
#include "samplers.hlsli"

Texture2D<float> gDepthRaw;
RWTexture2D<float> gDepth;

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   float depthRaw = gDepthRaw[dispatchThreadID.xy];
   // gDepth[dispatchThreadID.xy] = LinearizeDepth(depthRaw, gCamera.projection) / 100;
   gDepth[dispatchThreadID.xy] = LinearizeDepth(depthRaw, gCamera.zNear, gCamera.zFar) / 20;
}
