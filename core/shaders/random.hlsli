#include "shared/rt.hlsli"
#include "math.hlsli"

uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float RandomFloat(inout uint seed) {
    seed = pcg_hash(seed);
    return (float)seed / (float)UINT_MAX;
}

float2 RandomFloat2(inout uint seed) {
    return float2(RandomFloat(seed), RandomFloat(seed));
}

float3 RandomFloat3(inout uint seed) {
    return float3(RandomFloat(seed), RandomFloat(seed), RandomFloat(seed));
}

// todo: mb find better way?
float3 RandomInUnitSphere(inout uint seed) {
    for (int i = 0; i < 100; i++) {
        float3 p = RandomFloat3(seed) * 2 - 1;
        if (length(p) < 1) {
            return p;
        }
    }
    return float3(0, 1, 0);
}

float3 RandomInHemisphere(float3 normal, inout uint seed) {
    float3 p = RandomInUnitSphere(seed);
    return p * sign(dot(p, normal));
}
