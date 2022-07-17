#include "shared/common.hlsli"

SamplerState gSamplerPoint;

Texture2D<float> gDepth;
Texture2D<float3> gNormal;
// RWTexture2D<float> gSsao;
RWTexture2D<float3> gSsao;

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

float3 GetWorldPositionFromDepth(float2 uv, float depth ) {
	float4 ndc = float4(uv * 2 - 1, depth, 1);
	ndc.y *= -1;
	float4 wp = mul(ndc, gCamera.invViewProjection);
	return (wp / wp.w).xyz;
}

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   float depthRaw = gDepth[dispatchThreadID.xy];
   float depth = LinearizeDepth(depthRaw);

   float2 uv = float2(dispatchThreadID.xy) / float2(gCamera.rtSize);
   float3 posW = GetWorldPositionFromDepth(uv, depthRaw);
   gSsao[dispatchThreadID.xy] = frac(posW);

   // float3 dirs[1] = {
   //    float3(0, 1, 0)
   // };

   // 
   // // gSsao[dispatchThreadID.xy] = frac(lineardepth);

   // const int sampleCount = 1;

   // float occusion = 0;
   // for (int iSample = 0; iSample < sampleCount; iSample++) {
   //    float3 samplePosW = posW + dirs[iSample];

   //    float4 sample = mul(float4(samplePosW, 1), gCamera.viewProjection);
   //    sample /= sample.w;
   //    float samplePosDepth = sample.z;

   //    float sampleDepth = gDepth.SampleLevel(gSamplerPoint, sample.xy, 0);
   //    float4 q = float4(uv, sampleDepth, 1);
   //    q *= q.z;
   //    float sceneDepth = q.w;

   //    // float sceneDepth = LinearizeDepth(sampleDepth);

   //    // float3 sceneSamplePosW = mul(q, gCamera.invViewProjection).xyz;

   //    occusion += sceneDepth < samplePosDepth;
   // }

   // gSsao[dispatchThreadID.xy] = (1 - occusion) / sampleCount;
}
