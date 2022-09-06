#include "shared/hlslCppShared.hlsli"
#include "common.inl"
#include "tonemaping.hlsli"
#include "samplers.hlsli"
#include "noise.inl"
#include "math.hlsli"

Texture2D<float> gDepth;
RWTexture2D<float4> gColor;

StructuredBuffer<SLight> gLights;
Texture2D<float> gShadowMap;

float LinearizeDepth(float depth) {
   // depth = A + B / z
   float A = gCamera.projection[2][2];
   float B = gCamera.projection[2][3];
   // todo: why minus?
   return -B / (depth - A);
}

// todo: copy past'a
float SunShadowAttenuation(float3 posW, float2 jitter = 0) {
   if (0) {
      // jitter = (rand3dTo2d(posW) - 0.5) * 0.001;
   }

   float3 shadowUVZ = mul(float4(posW, 1), gScene.toShadowSpace).xyz;
   if (shadowUVZ.z >= 1) {
      return 1;
   }

   float2 shadowUV = shadowUVZ.xy + jitter;
   float z = shadowUVZ.z;

   float bias = 0.01;

   if (0) {
      return gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias);
   } else {
      float sum = 0;

      sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, float2(0, 0));

      int offset = 1;

      sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(-offset, 0));
      sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(offset, 0));
      sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(0, -offset));
      sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias, int2(0, offset));

      return sum / (5);
   }
}

float LightAttenuation(SLight light, float3 posW) {
  float attenuation = 1;

  if (light.type == SLIGHT_TYPE_DIRECT) {
    attenuation = SunShadowAttenuation(posW);
  } else if (light.type == SLIGHT_TYPE_POINT) {
    float distance = length(light.position - posW);
    // attenuation = 1 / (distance * distance);
    attenuation = smoothstep(1, 0, distance / light.radius);
  }

  return attenuation;
}

float3 LightRadiance(SLight light, float3 posW) {
  float attenuation = LightAttenuation(light, posW);
  float3 radiance = light.color * attenuation;
  return radiance;
}

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
   // gColor[dispatchThreadID.xy] = float4(frac(LinearizeDepth(depth)).xxx, 1);
}
