#include "common.hlsli"
#include "math.hlsli"


struct RayHit {
    float3 position; // todo: can be restore from tMax
    float tMin;
    float tMax;
    float3 normal;
    uint objID;
};

RayHit CreateRayHit(float tMax = INF) {
    RayHit hit;
    hit.position = 0;
    hit.tMin = 0.00001; // todo:
    hit.tMax = tMax;
    hit.normal = 0;
    hit.objID = UINT_MAX;
    return hit;
}

// todo: stop trace if hit from inside

bool IntersectGroundPlane(Ray ray, inout RayHit bestHit) {
    float t = -ray.origin.y / ray.direction.y;
    if (t > bestHit.tMin && t < bestHit.tMax) {
        bestHit.tMax = t;
        bestHit.position = ray.origin + t * ray.direction;
        bestHit.normal = float3(0.0f, 1.0f, 0.0f);
        return true;
    }
    return false;
}

bool IntersectSphere(Ray ray, inout RayHit bestHit, float4 sphere) {
    float3 d = ray.origin - sphere.xyz;
    float p1 = -dot(ray.direction, d);
    float p2sqr = p1 * p1 - dot(d, d) + sphere.w * sphere.w;
    if (p2sqr < 0) {
        return false;
    }

    float p2 = sqrt(p2sqr);
    float t = p1 - p2 > 0 ? p1 - p2 : p1 + p2;
    if (t > bestHit.tMin && t < bestHit.tMax) {
        bestHit.tMax = t;
        bestHit.position = ray.origin + t * ray.direction;
        bestHit.normal = normalize(bestHit.position - sphere.xyz);
        return true;
    }
    return false;
}

float IntersectAABB_Fast(float3 rayOrigin, float3 rayInvDirection, float3 aabbMin, float3 aabbMax) {
    float3 tMin = (aabbMin - rayOrigin) * rayInvDirection;
    float3 tMax = (aabbMax - rayOrigin) * rayInvDirection;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    if (tFar > tNear && tFar > 0) {
        return tNear;
    }
    return INF;
}

bool IntersectAABB(Ray ray, inout RayHit bestHit, float3 center, float3 halfSize) {
    // todo: optimize - min\max
    float3 boxMin = center - halfSize;
    float3 boxMax = center + halfSize;

    float3 tMin = (boxMin - ray.origin) / ray.direction;
    float3 tMax = (boxMax - ray.origin) / ray.direction;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    if (tFar > tNear && tNear > bestHit.tMin && tNear < bestHit.tMax) {
        bestHit.tMax = tNear;
        bestHit.position = ray.origin + tNear * ray.direction; // todo: move from hear

        // tood: find better way
        float3 localPoint = bestHit.position - center;
        float3 normal = localPoint / halfSize;
        float3 s = sign(normal);
        normal *= s;

        float3 normalXY = normal.x > normal.y ? float3(s.x, 0, 0) : float3(0, s.y, 0);
        bestHit.normal = normal.z > max(normal.x, normal.y) ? float3(0, 0, s.z) : normalXY;

        return true;
    }
    return false;
}

bool IntersectOBB(Ray ray, inout RayHit bestHit, float3 center, float4 rotation, float3 halfSize) {
    float4 rotationInv = QuatConjugate(rotation);
    Ray rayL = CreateRay(QuatMulVec3(rotationInv, ray.origin - center), QuatMulVec3(rotationInv, ray.direction));

    if (IntersectAABB(rayL, bestHit, 0, halfSize)) {
        bestHit.position = QuatMulVec3(rotation, bestHit.position) + center;
        bestHit.normal = QuatMulVec3(rotation, bestHit.normal);
        return true;
    }

    return false;
}
