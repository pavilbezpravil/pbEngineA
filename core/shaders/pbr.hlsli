#ifndef PBR_HEADER
#define PBR_HEADER

#include "math.hlsli"

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1 - F0) * pow(clamp(1 - cosTheta, 0, 1), 5);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1) + 1);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1);
    float k = (r*r) / 8;

    float num   = NdotV;
    float denom = NdotV * (1 - k) + k;
	
    return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0);
    float NdotL = max(dot(N, L), 0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

#endif // PBR_HEADER