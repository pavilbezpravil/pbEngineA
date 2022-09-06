#include "shared/hlslCppShared.hlsli"
#include "common.hlsli"
#include "pbr.hlsli"
#include "tonemaping.hlsli"
#include "noise.inl"
#include "samplers.hlsli"

struct VsIn {
  float3 posL : POSITION;
  float3 normalL : NORMAL;
  uint instanceID : SV_InstanceID;
};

struct VsOut {
  float3 posW : POS_W;
  float3 normalW : NORMAL_W;
  float4 posH : SV_POSITION;
  uint instanceID : SV_InstanceID;
};

cbuffer gDrawCallCB {
  SDrawCallCB gDrawCall;
}

StructuredBuffer<Instance> gInstances;
StructuredBuffer<SDecal> gDecals;
StructuredBuffer<SLight> gLights;

Texture2D<float> gShadowMap;
Texture2D<float> gSsao;

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float4x4 transform = gInstances[gDrawCall.instanceStart + input.instanceID].transform;

  float3 posW = mul(float4(input.posL, 1), transform).xyz;
  float4 posH = mul(float4(posW, 1), gCamera.viewProjection);

  output.posW = posW;
  output.posH = posH;
  output.instanceID = input.instanceID;

  return output;
}

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
    jitter = (rand3dTo2d(posW) - 0.5) * 0.001;
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

float3 Shade(Surface surface, float3 V) {
  float3 Lo = 0;

  Lo += LightShadeLo(gScene.directLight, surface, V);

  for(int i = 0; i < gScene.nLights; ++i) {
      Lo += LightShadeLo(gLights[i], surface, V);
  }

  return Lo;
}

struct PsOut {
  float4 color : SV_Target0;
};

PsOut ps_main(VsOut input) : SV_TARGET {
  float2 screenUV = input.posH.xy / gCamera.rtSize;

  float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
  float3 posW = input.posW;

  float alpha = 0.75;

  Material material = gInstances[gDrawCall.instanceStart + input.instanceID].material;
  
  Surface surface;
  surface.albedo = material.albedo;
  surface.metallic = material.metallic;
  surface.roughness = material.roughness;

  surface.posW = posW;
  surface.normalW = normalW;

  for (int iDecal = 0; iDecal < gScene.nDecals; ++iDecal) {
    SDecal decal = gDecals[iDecal];

    float3 posDecalSpace = mul(float4(posW, 1), decal.viewProjection).xyz;
    if (any(posDecalSpace > float3(1, 1, 1) || posDecalSpace < float3(-1, -1, 0))) {
      continue;
    }

    float alpha = decal.albedo.a;

    // float3 decalForward = float3(0, -1, 0);
    // alpha *= lerp(-3, 1, dot(decalForward, -surface.normalW));
    // if (alpha < 0) {
    //   continue;
    // }

    float2 decalUV = NDCToTex(posDecalSpace.xy);

    surface.albedo = lerp(surface.albedo, decal.albedo.rgb, alpha);
    surface.metallic = lerp(surface.metallic, decal.metallic, alpha);
    surface.roughness = lerp(surface.roughness, decal.roughness, alpha);
  }

  surface.F0 = lerp(0.04, surface.albedo, surface.metallic);

  float3 V = normalize(gCamera.position - posW);
  float3 Lo = Shade(surface, V);

  float ssaoMask = gSsao.SampleLevel(gSamplerLinear, screenUV, 0).x;

  // float ao = ssaoMask; // todo:
  float ao = 1; // todo:
  float3 ambient = 0.02 * surface.albedo * ao;
  float3 color = ambient + Lo;

  color *= ssaoMask; // todo: applied on transparent too

  // color = noise(posW * 0.3);

  if (0) {
    const int maxSteps = 20;

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
      scattering += fogColor / PI * radiance;

      for(int i = 0; i < gScene.nLights; ++i) {
          float3 radiance = LightRadiance(gLights[i], fogPosW);
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

  // color = ACESFilm(color);
  // color = color / (color + 1);
  // color = GammaCorrection(color); // todo: use srgb

  PsOut output = (PsOut)0;
  output.color.rgb = color;

  // float2 jitter = rand3dTo2d(posW);
  // output.color.rgb = SunShadowAttenuation(posW, jitter * 0.005);
  output.color.a = alpha;

  #ifdef ZPASS
    output.color.rgb = normalW;
    output.color.a = 1; // todo:
  #endif
  return output;
}
