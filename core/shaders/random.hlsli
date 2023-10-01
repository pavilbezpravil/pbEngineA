#include "shared/rt.hlsli"
#include "math.hlsli"

uint pcg_hash(uint input) {
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

static uint gRandomSeed;

void RandomInitSeed(uint seed) {
    gRandomSeed = pcg_hash(seed);
}

void RandomInitSeedFromUint2(uint2 id, uint frameDepend) {
    RandomInitSeed(id.x * 214234 + id.y * 521334 + frameDepend);
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

float RandomFloat() {
    return RandomFloat(gRandomSeed);
}

float RandomFloat2() {
    return RandomFloat2(gRandomSeed);
}

float RandomFloat3() {
    return RandomFloat3(gRandomSeed);
}

// float RandomFloatNormalDistribution(inout uint seed) {
//     float u1 = RandomFloat(seed);
//     float u2 = RandomFloat(seed);
//     return sqrt(-2 * log(u1)) * cos(2 * PI * u2);
// }

// float RandomFloatNormalDistribution() {
//     return RandomFloatNormalDistribution(gRandomSeed);
// }

float3 RandomPointInUnitSphereCube() {
    for (int i = 0; i < 100; i++) {
        float3 p = RandomFloat3(gRandomSeed) * 2 - 1;
        if (length(p) < 1) {
            return p;
        }
    }
    return float3(0, 1, 0);
}

// todo: test
// float3 RandomPointInUnitSphere() {
//     float u = RandomFloat(gRandomSeed);
//     float3 x = float3(RandomFloatNormalDistribution(gRandomSeed), RandomFloatNormalDistribution(gRandomSeed), RandomFloatNormalDistribution(gRandomSeed));
//     x /= length(x);

//     return x * pow(u, 1.0 / 3.0);
// }
float3 RandomPointInUnitSphere() {
    // return RandomPointInUnitSphereCube();
    float u = RandomFloat();
    float v = RandomFloat();

    float theta = u * PI_2;
    float phi = acos(2.0 * v - 1.0);

    float r = pow(RandomFloat(), 1.0 / 3.0);

    float sinTheta;
    float cosTheta;
    sincos(theta, sinTheta, cosTheta);

    float sinPhi;
    float cosPhi;
    sincos(phi, sinPhi, cosPhi);

    float x = sinPhi * cosTheta;
    float y = sinPhi * sinTheta;
    float z = cosPhi;
    return float3(x, y, z) * r;
}

float3 RandomPointOnUnitSphere() {
    float theta = PI_2 * RandomFloat(gRandomSeed);
    float phi = acos(1.0 - 2.0 * RandomFloat(gRandomSeed));
    
    float x = sin(phi) * cos(theta);
    float y = sin(phi) * sin(theta);
    float z = cos(phi);
    
    return float3(x, y, z);
}

float3 RandomPointOnUnitHemisphere(float3 normal) {
    float3 p = RandomPointOnUnitSphere();
    return p * sign(dot(p, normal));
}
