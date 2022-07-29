#include "shared/common.hlsli"
#include "common.inl"

SamplerState gSamplerPoint;

Texture2D<float> gDepth;
Texture2D<float3> gNormal;
RWTexture2D<float> gSsao;

cbuffer gCameraCB {
  SCameraCB gCamera;
}

StructuredBuffer<float3> gRandomDirs;

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
	float4 ndc = float4(TexToNDC(uv), depth, 1);
	float4 wp = mul(ndc, gCamera.invViewProjection);
	return (wp / wp.w).xyz;
}

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   float3 normalW = gNormal[dispatchThreadID.xy];

   float depthRaw = gDepth[dispatchThreadID.xy];
   float depth = LinearizeDepth(depthRaw);

   float2 uv = float2(dispatchThreadID.xy) / float2(gCamera.rtSize);
   float3 posW = GetWorldPositionFromDepth(uv, depthRaw);

   const int sampleCount = 32;

   float occusion = 0;
   for (int iSample = 0; iSample < sampleCount; iSample++) {
      float3 dir = gRandomDirs[iSample];
      // dirs[iSample]

      float flip = sign(dot(normalW, dir));
      float3 offset = flip * dir * 0.3;
      float3 samplePosW = posW + offset;

      float4 sample = mul(float4(samplePosW, 1), gCamera.viewProjection);
      sample /= sample.w;
      float samplePosDepth = sample.z;

      float sampleDepth = gDepth.SampleLevel(gSamplerPoint, NDCToTex(sample.xy), 0);
      float4 q = float4(uv, sampleDepth, 1);
      q *= q.z;
      float sceneDepth = q.w;

      occusion += sceneDepth < samplePosDepth - 0.0001;
   }

   occusion = occusion / sampleCount;
   gSsao[dispatchThreadID.xy] = 1 - occusion;
}
