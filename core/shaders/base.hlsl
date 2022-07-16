#include "shared/common.hlsli"
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
  uint instanceID : INSTANCE_ID;
};

cbuffer gCamera {
  CameraCB camera;
}

StructuredBuffer<Instance> gInstances;
StructuredBuffer<Light> gLights;

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float4x4 transform = gInstances[camera.instanceStart + input.instanceID].transform;

  float3 posW = mul(float4(input.posL, 1), transform).xyz;
  float4 posH = mul(float4(posW, 1), camera.viewProjection);

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

  Material material = gInstances[camera.instanceStart + input.instanceID].material;

  float3 albedo = material.albedo;
  float roughness = material.roughness;
  float metallic = material.metallic;

  float3 N = normalize(normalW);
  float3 V = normalize(camera.position - posW);

  float3 F0 = lerp(0.04, albedo, metallic);
	           
  // reflectance equation
  float3 Lo = 0;
  for(int i = 0; i < camera.nLights; ++i) {
      Light light = gLights[i];

      // calculate per-light radiance
      float3 L = normalize(light.position - posW);
      float3 H = normalize(V + L);
      float distance = length(light.position - posW);
      float attenuation = 1.0 / (distance * distance);
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

  float ao = 1; // todo:
  float3 ambient = 0.01 * albedo * ao;
  float3 color = ambient + Lo;

  color = color / (color + 1);
  color = pow(color, 1.0 / 2.2); // todo: use srgb

  PsOut output = (PsOut)0;
  output.color.rgb = color;
  // output.color.rgb = normalW * 0.5 + 0.5;
  output.color.a = 0.75;
  return output;
}
