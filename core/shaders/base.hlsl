#include "shared/common.hlsli"
#include "common.inl"
#include "pbr.hlsli"
#include "noise.inl"

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

cbuffer gCameraCB {
  CameraCB gCamera;
}

StructuredBuffer<Instance> gInstances;
StructuredBuffer<Decal> gDecals;
StructuredBuffer<Light> gLights;

SamplerState gSamplerPoint : register(s0);
SamplerState gSamplerLinear : register(s1);

Texture2D<float> gDepth;
Texture2D<float> gSsao;

// todo: copy paste from ssao.cs
float3 GetWorldPositionFromDepth(float2 uv, float depth ) {
	float4 ndc = float4(TexToNDC(uv), depth, 1);
	float4 wp = mul(ndc, gCamera.invViewProjection);
	return (wp / wp.w).xyz;
}

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float4x4 transform = gInstances[gCamera.instanceStart + input.instanceID].transform;

  float3 posW = mul(float4(input.posL, 1), transform).xyz;
  float4 posH = mul(float4(posW, 1), gCamera.viewProjection);

  output.posW = posW;
  output.posH = posH;
  output.instanceID = input.instanceID;

  return output;
}

struct PsOut {
  float4 color : SV_Target0;
};

PsOut ps_main(VsOut input) : SV_TARGET {
  float2 screenUV = input.posH.xy / gCamera.rtSize;

  float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));
  float3 posW = input.posW;

  #ifdef DECAL
    float sceneDepth = gDepth.SampleLevel(gSamplerPoint, screenUV, 0);
    float3 scenePosW = GetWorldPositionFromDepth(screenUV, sceneDepth);

    float4x4 decalViewProjection = gDecals[gCamera.instanceStart + input.instanceID].viewProjection;
    float3 posDecalSpace = mul(float4(scenePosW, 1), decalViewProjection).xyz;
    if (any(posDecalSpace > float3(1, 1, 1) || posDecalSpace < float3(-1, -1, 0))) {
      discard;
    }

    float3 albedo = 1;
    // albedo = frac(scenePosW);
    albedo = float3(NDCToTex(posDecalSpace.xy), 0);
    float roughness = 0.2;
    float metallic = 0;
  #else
    Material material = gInstances[gCamera.instanceStart + input.instanceID].material;

    float3 albedo = material.albedo;
    float roughness = material.roughness;
    float metallic = material.metallic;
  #endif

  float3 N = normalize(normalW);
  float3 V = normalize(gCamera.position - posW);

  float3 F0 = lerp(0.04, albedo, metallic);
	           
  // reflectance equation
  float3 Lo = 0;
  int useDirectionLight = 1;
  for(int i = 0; i < gCamera.nLights + useDirectionLight; ++i) {
      bool isDirectLight = i == gCamera.nLights;

      Light light = gLights[i];
      if (isDirectLight) {
        light = gCamera.directLight;
      }

      // calculate per-light radiance
      float3 L = normalize(light.position - posW);
      if (isDirectLight) {
        L = -light.direction;
      }
      float3 H = normalize(V + L);
      float distance = length(light.position - posW);
      float attenuation = 1.0 / (distance * distance);
      if (isDirectLight) {
        attenuation = 1;
      }
      // float attenuation = 1.0 / pow(distance, 1.5);
      float3 radiance = light.color * attenuation;

      // cook-torrance brdf
      float NDF = DistributionGGX(N, H, roughness);
      float G = GeometrySmith(N, V, L, roughness);
      float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

      float3 kS = F;
      float3 kD = 1 - kS;
      kD *= 1.0 - metallic;

      float3 numerator = NDF * G * F;
      float denominator = 4 * max(dot(N, V), 0) * max(dot(N, L), 0) + 0.0001;
      float3 specular = numerator / denominator;

      // add to outgoing radiance Lo
      float NdotL = max(dot(N, L), 0);
      Lo += (kD * albedo / PI + specular) * radiance * NdotL;
  }

  float ssaoMask = gSsao.SampleLevel(gSamplerLinear, screenUV, 0).x;

  // float ao = ssaoMask; // todo:
  float ao = 1; // todo:
  float3 ambient = 0.02 * albedo * ao;
  float3 color = ambient + Lo;

  color *= ssaoMask; // todo: applied on transparent too

  // color = noise(posW * 0.3);

  if (0) {
    const int maxSteps = 20;

    float stepLength = length(posW - gCamera.position) / maxSteps;

    float accTransmittance = 1;
    float3 accScaterring = 0;

    for(int i = 0; i < maxSteps; ++i) {
      float t = i / float(maxSteps - 1);
      float3 fogPosW = lerp(gCamera.position, posW, t);

      float fogDensity = noise(fogPosW * 0.5) * 0.3 * saturate(-fogPosW.y / 5);

      float3 scattering = 0;
      for(int i = 0; i < gCamera.nLights + useDirectionLight; ++i) {
          bool isDirectLight = i == gCamera.nLights;

          Light light = gLights[i];
          if (isDirectLight) {
            light = gCamera.directLight;
          }

          float distance = length(light.position - fogPosW);
          float attenuation = 1.0 / (distance * distance);
          if (isDirectLight) {
            attenuation = 1;
          }
          float3 radiance = light.color * attenuation;

          scattering += float3(1, 1, 1) * 0.9 / PI * radiance;
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

  color = color / (color + 1);
  color = pow(color, 1.0 / 2.2); // todo: use srgb

  // color = ssaoMask;

  PsOut output = (PsOut)0;
  output.color.rgb = color;
  // output.color.rgb = normalW * 0.5 + 0.5;
  output.color.a = 0.75;
  // output.color.a = 1;
  
  // output.color.rg = screenUV;
  // output.color.b = 0;

  #ifdef ZPASS
    output.color.rgb = normalW;
    output.color.a = 1; // todo:
  #endif
  return output;
}
