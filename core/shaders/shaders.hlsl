#include "shared/common.hlsli"
#include "pbr.hlsli"

struct VsIn {
  float3 posL : POSITION;
  float3 normalL : NORMAL;
};

struct VsOut {
  float3 posW : POS_W;
  float3 normalW : NORMAL_W;
  float4 posH : SV_POSITION;
};

cbuffer gCamera {
  CameraCB camera;
}

StructuredBuffer<Instance> gInstances;

VsOut vs_main(VsIn input) {
  VsOut output = (VsOut)0;

  float4x4 transform = gInstances[camera.instanceStart].transform;
  // transform = camera.transform;

  float3 posW = mul(float4(input.posL, 1), transform).xyz;
  float4 posH = mul(float4(posW, 1), camera.viewProjection);

  output.posW = posW;
  output.posH = posH;

  return output;
}


float4 ps_main(VsOut input) : SV_TARGET {
  float3 normalW = normalize(cross(ddx(input.posW), ddy(input.posW)));

  float3 posW = input.posW;

  Material material = (Material)0;
  material.albedo = camera.color;

  float roughness = camera.roughness;
  float metallic = camera.metallic;

  float3 albedo = material.albedo;

  float3 N = normalize(normalW);
  float3 V = normalize(camera.position - posW);

  float3 F0 = lerp(0.04, albedo, metallic);
	           
  // reflectance equation
  float3 Lo = 0;
  for(int i = 0; i < 1; ++i) {
      Light light = (Light)0;
      light.position = float3(0, 0, 0);
      light.color = float3(1, 1, 1) * 10;

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

  float ao = 0.1; // todo:
  float3 ambient = 0.03 * albedo * ao;
  float3 color = ambient + Lo;

  // color = color / (color + 1);
  color = pow(color, 1.0 / 2.2); // todo: use srgb

  return float4(color, 1);
  // return float4(normalW * 0.5 + 0.5, 1);
}
