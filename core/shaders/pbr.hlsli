#ifndef PBR_HEADER
#define PBR_HEADER

#include "math.hlsli"
#include "STL.hlsli"

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1 - F0) * pow(saturate(1 - cosTheta), 5);
}

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a      = roughness * roughness;
    float a2     = a * a;
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

float3 ComputeNDotL(float3 N, float3 L) {
    return max(dot(N, L), 0);
}

void BRDFSpecular(float3 L, float3 V, float3 normal, float3 F0, float roughness, out float3 kD, out float3 kS) {
    float3 H = normalize(V + L);
    float3 N = normal;

    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    kD = 1 - F;

    float3 numerator = NDF * G * F;
    float denominator = 4 * max(dot(N, V), 0) * max(dot(N, L), 0) + 0.0001;
    float3 specular = numerator / denominator;
    kS = specular;
}

#endif // PBR_HEADER