#include "commonResources.hlsli"
#include "common.hlsli"
#include "tonemaping.hlsli"
#include "noise.hlsli"
#include "math.hlsli"
#include "sky.hlsli"
#include "lighting.hlsli"

Texture2D<float> gDepth;
RWTexture2D<float4> gColor;

// float LinearizeDepth(float depth) {
//    // depth = A + B / z
//    float A = gCamera.projection[2][2];
//    float B = gCamera.projection[2][3];
//    // todo: why minus?
//    return -B / (depth - A);
// }

[numthreads(8, 8, 1)]
void main( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   // todo: check border
   float3 color = gColor[dispatchThreadID.xy].xyz;
   float depthRaw = gDepth[dispatchThreadID.xy];

   float2 uv = float2(dispatchThreadID.xy) / float2(gCamera.rtSize);
   float3 posW = GetWorldPositionFromDepth(uv, depthRaw, gCamera.invViewProjection);

   // todo:
   float3 V = posW - gCamera.position;
   if (length(V) > 900) {
      V = normalize(V);
      color = GetSkyColor(V);
   }

   float2 screenUV = uv;

   if (1) {
      const int maxSteps = gScene.fogNSteps;

      float stepLength = length(posW - gCamera.position) / maxSteps;

      float randomOffset = rand2(screenUV * 100 + rand1dTo2d(gScene.iFrame % 64));
      float3 startPosW = lerp(gCamera.position, posW, randomOffset / maxSteps);

      // float3 randomOffset = rand2dTo3d(screenUV * 100 + rand1dTo2d(gScene.iFrame % 64));
      // float3 startPosW = gCamera.position + randomOffset * stepLength;

      float accTransmittance = 1;
      float3 accScaterring = 0;

      float3 fogColor = float3(1, 1, 1) * 0.7;

      for(int i = 0; i < maxSteps; ++i) {
         float t = i / float(maxSteps - 1);
         float3 fogPosW = lerp(startPosW, posW, t);

         float fogDensity = saturate(noise(fogPosW * 0.3) - 0.2);
         fogDensity *= saturate(-fogPosW.y / 3);
         fogDensity *= 0.5;

         // fogDensity = 0.2;

         float3 scattering = 0;

         float3 radiance = LightRadiance(gScene.directLight, fogPosW);
         // float3 radiance = gScene.directLight.color; // todo
         scattering += fogColor / PI * radiance;

         for(int i = 0; i < gScene.nLights; ++i) {
            float3 radiance = LightRadiance(gLights[i], fogPosW);
            // float3 radiance = gLights[i].color; // todo
            scattering += fogColor / PI * radiance;
         }

         float transmittance = exp(-stepLength * fogDensity);
         scattering *= accTransmittance * (1 - transmittance);

         accScaterring += scattering;
         accTransmittance *= transmittance;

         if (accTransmittance < 0.01) {
            accTransmittance = 0;
            break;
         }
      }

      color *= accTransmittance;
      color += accScaterring;
   }

   gColor[dispatchThreadID.xy] = float4(color, 1);
}
