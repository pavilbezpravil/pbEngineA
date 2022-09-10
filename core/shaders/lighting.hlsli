#include "shared/hlslCppShared.hlsli"
#include "pbr.hlsli"

float3 LightGetL(SLight light, float3 posW) { // L
  if (light.type == SLIGHT_TYPE_DIRECT) {
    return -light.direction;
  } else if (light.type == SLIGHT_TYPE_POINT) {
    float3 L = normalize(light.position - posW);
    return L;
  }
  return float3(0, 0, 0);
}

float SunShadowAttenuation(float3 posW, float2 jitter = 0) {
  if (0) {
    // jitter = (rand3dTo2d(posW) - 0.5) * 0.001;
  }

  float3 shadowUVZ = mul(float4(posW, 1), gScene.toShadowSpace).xyz;

  float2 shadowUV = shadowUVZ.xy + jitter;
  float z = shadowUVZ.z;

  float bias = 0.003;

  if (0) {
    return gShadowMap.SampleCmpLevelZero(gSamplerShadow, shadowUV, z - bias);
  } else {
    float sum = 0;

    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, float2(0, 0)));

    // int offset = 2;
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, -offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, offset)));
    // // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, -offset)));

    // // offset = 3;
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(-offset, 0)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(offset, 0)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(0, -offset)));
    // sum += ComponentSum(gShadowMap.GatherCmpRed(gSamplerShadow, shadowUV, z - bias, int2(0, offset)));
    // return sum / (4 * 5);

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

struct Surface {
  float3 posW;
  float3 normalW;
  float roughness;
  float metallic;
  float3 albedo;
  float3 F0;
};

float3 LightRadiance(SLight light, float3 posW) {
  float attenuation = LightAttenuation(light, posW);
  float3 radiance = light.color * attenuation;
  return radiance;
}

float3 LightShadeLo(SLight light, Surface surface, float3 V) {
  float attenuation = LightAttenuation(light, surface.posW);
  if (attenuation <= 0.0001) {
    return 0;
  }
  float3 radiance = light.color * attenuation;

  float3 L = LightGetL(light, surface.posW);

  float3 H = normalize(V + L);
  float3 N = surface.normalW;

  // cook-torrance brdf
  float NDF = DistributionGGX(N, H, surface.roughness);
  float G = GeometrySmith(N, V, L, surface.roughness);
  float3 F = fresnelSchlick(max(dot(H, V), 0.0), surface.F0);

  float3 kS = F;
  float3 kD = 1 - kS;
  kD *= 1.0 - surface.metallic;

  float3 numerator = NDF * G * F;
  float denominator = 4 * max(dot(N, V), 0) * max(dot(N, L), 0) + 0.0001;
  float3 specular = numerator / denominator;

  // add to outgoing radiance Lo
  float NdotL = max(dot(N, L), 0);
  return (kD * surface.albedo / PI + specular) * radiance * NdotL;  
}
