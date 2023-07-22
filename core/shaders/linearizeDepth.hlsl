#include "commonResources.hlsli"
#include "common.hlsli"

Texture2D<float> gDepth;
RWTexture2D<float> gDepthOut;

float LinDepth(float depthRaw) {
   return LinearizeDepth(depthRaw, gCamera.zNear, gCamera.zFar);
}

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   float depthRaw = gDepth[dispatchThreadID.xy];
   // gDepth[dispatchThreadID.xy] = LinearizeDepth(depthRaw, gCamera.projection) / 100;
   gDepthOut[dispatchThreadID.xy] = LinDepth(depthRaw) / 40;
}

[numthreads(8, 8, 1)]
void downsampleDepth( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: min sampler
   float depth1 = gDepth[dispatchThreadID.xy * 2 + int2(0, 0)];
   float depth2 = gDepth[dispatchThreadID.xy * 2 + int2(1, 0)];
   float depth3 = gDepth[dispatchThreadID.xy * 2 + int2(0, 1)];
   float depth4 = gDepth[dispatchThreadID.xy * 2 + int2(1, 1)];

   float maxDepth = max4(depth1, depth2, depth3, depth4);

   gDepthOut[dispatchThreadID.xy] = maxDepth;
}
