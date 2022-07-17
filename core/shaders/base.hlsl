#include "shared/common.hlsli"
#include "common.inl"
#include "pbr.hlsli"

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
StructuredBuffer<Light> gLights;

SamplerState gSamplerLinear;
Texture2D<float> gSsao;

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
  float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));

  float3 posW = input.posW;

  Material material = gInstances[gCamera.instanceStart + input.instanceID].material;

  float3 albedo = material.albedo;
  float roughness = material.roughness;
  float metallic = material.metallic;

  float3 N = normalize(normalW);
  float3 V = normalize(gCamera.position - posW);

  float3 F0 = lerp(0.04, albedo, metallic);
	           
  // reflectance equation
  float3 Lo = 0;
  for(int i = 0; i < gCamera.nLights + 1; ++i) {
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

  float2 screenUV = input.posH.xy / gCamera.rtSize;
  float ssaoMask = gSsao.SampleLevel(gSamplerLinear, screenUV, 0).x;

  // float ao = ssaoMask; // todo:
  float ao = 1; // todo:
  float3 ambient = 0.02 * albedo * ao;
  float3 color = ambient + Lo;

  color *= ssaoMask; // todo: applied on transparent too

  // float3 fogColor = 0.1;
  // float fogCoeff = 1 - exp(-length(posW - gCamera.position) * 0.001);
  // color = lerp(color, fogColor, fogCoeff);

  color = color / (color + 1);
  color = pow(color, 1.0 / 2.2); // todo: use srgb

  // color = ssaoMask;

  PsOut output = (PsOut)0;
  output.color.rgb = color;
  // output.color.rgb = normalW * 0.5 + 0.5;
  output.color.a = 0.75;

  // output.color.rg = screenUV;
  // output.color.b = 0;

  #ifdef ZPASS
    output.color.rgb = normalW;
    output.color.a = 1; // todo:
  #endif
  return output;
}
