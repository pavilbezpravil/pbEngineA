#ifndef MATH_HEADER
#define MATH_HEADER

static const float PI = 3.14159265359;
static const float PI_2 = PI * 2.f;

static const float FLT_MAX = asfloat(0x7F7FFFFF);
static const float EPSILON = 0.000001;

static const uint UINT_MAX = uint(-1);

static const float INF = 1000000; // todo: FLT_MAX?

float Eerp(float a, float b, float t) {
    return pow(a, 1 - t) * pow(b, t);
}

float4 QuatConjugate(float4 q) {
    return float4(q.x, -q.yzw);
}

// todo: not sure
float4 QuatDot(float4 q) {
    return dot(q, q);
}

float4 QuatInv(float4 q) {
    return QuatConjugate(q) / QuatDot(q);
}

float3 QuatMulVec3(float4 qInv, float3 v) {
	float3 QuatVector = qInv.yzw;
    float3 uv = cross(QuatVector, v);
	float3 uuv = cross(QuatVector, uv);

    return v + ((uv * qInv.x) + uuv) * 2;
}

struct Ray {
    float3 origin;
    float3 direction;
};

Ray CreateRay(float3 origin, float3 direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;
    return ray;
}

#endif
