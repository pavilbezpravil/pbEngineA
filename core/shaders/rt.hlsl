#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"
#include "random.hlsli"

#define USE_BVH

RWTexture2D<float4> gColorOut : register(u0);

cbuffer gRTConstantsCB : register(b0) {
  SRTConstants gRTConstants;
}

StructuredBuffer<SRTObject> gRtObjects : register(t0);
StructuredBuffer<BVHNode> gBVHNodes : register(t1);
// StructuredBuffer<SMaterial> gMaterials;

struct Sphere {
    float3 origin;
    float radius;
};

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

Ray CreateCameraRay(float2 uv) {
    float3 origin = gCamera.position;
    // float3 posW = mul(float4(uv, 1, 1), gCamera.invViewProjection).xyz;
    float3 posW = GetWorldPositionFromDepth(uv, 1, gCamera.invViewProjection);
    float3 direction = normalize(posW - origin);
    return CreateRay(origin, direction);
}

struct RayHit {
    float3 position;
    float tMin;
    float tMax;
    float3 normal;
    int materialID;
};

RayHit CreateRayHit() {
    RayHit hit;
    hit.position = 0;
    hit.tMin = 0.0001; // todo:
    hit.tMax = INF;
    hit.normal = 0;
    hit.materialID = 0;
    return hit;
}

// todo: stop trace if hit from inside

bool IntersectGroundPlane(Ray ray, inout RayHit bestHit) {
    // Calculate distance along the ray where the ground plane is intersected
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
    // Calculate distance along the ray where the sphere is intersected
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

bool IntersectAABB_Fast(Ray ray, float bestHitTMax, float3 aabbMin, float3 aabbMax) {
    // todo: ray inv direction
    float3 tMin = (aabbMin - ray.origin) / ray.direction;
    float3 tMax = (aabbMax - ray.origin) / ray.direction;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    if (tFar > tNear && tNear < bestHitTMax && tFar > 0) {
        return true;
    }
    return false;
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

void IntersectObj(Ray ray, inout RayHit bestHit, SRTObject obj, uint objIdx) {
    int geomType = obj.geomType;

    // geomType = 0;
    if (geomType == 1) {
        // if (IntersectAABB(ray, bestHit, obj.position, obj.halfSize)) {
        if (IntersectOBB(ray, bestHit, obj.position, obj.rotation, obj.halfSize)) {
            bestHit.materialID = objIdx;
        }
    } else {
        if (IntersectSphere(ray, bestHit, float4(obj.position, obj.halfSize.x))) {
            bestHit.materialID = objIdx;
        }
    }
}

#ifdef USE_BVH

// #define DELAYED_INTERSECT

RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();

    if (gRTConstants.bvhNodes == 0) {
        return bestHit;
    }

    float tMax = INF;

    #ifdef DELAYED_INTERSECT
        // todo: group shared?
        const uint BVH_LEAF_INTERSECTS = 18;
        uint leafs[BVH_LEAF_INTERSECTS];
        uint nLeafsIntersects = 0;
    #endif

    const uint BVH_STACK_SIZE = 12;
    uint stack[BVH_STACK_SIZE];
    stack[0] = UINT_MAX;
    int stackPtr = 1;

    int iNode = 0;

    while (iNode != UINT_MAX) {
        uint objIdx = gBVHNodes[iNode].objIdx;
        bool isLeaf = objIdx != UINT_MAX;

        // todo: slow
        if (isLeaf) {
            #ifdef DELAYED_INTERSECT
                // todo: message it?
                if (nLeafsIntersects < BVH_LEAF_INTERSECTS - 1) {
                    leafs[nLeafsIntersects++] = objIdx;
                }
            #else
                SRTObject obj = gRtObjects[objIdx];
                IntersectObj(ray, bestHit, obj, objIdx);
            #endif

            iNode = stack[--stackPtr];
            continue;
        }

        uint iNodeLeft = iNode * 2 + 1;
        uint iNodeRight = iNode * 2 + 2;

        BVHNode nodeLeft = gBVHNodes[iNodeLeft];
        BVHNode nodeRight = gBVHNodes[iNodeRight];

        bool intersectL = IntersectAABB_Fast(ray, bestHit.tMax, nodeLeft.aabbMin, nodeLeft.aabbMax);
        bool intersectR = IntersectAABB_Fast(ray, bestHit.tMax, nodeRight.aabbMin, nodeRight.aabbMax);

        if (!intersectL && !intersectR) {
            iNode = stack[--stackPtr];
        } else {
            iNode = intersectL ? iNodeLeft : iNodeRight;

            if (intersectL && intersectR) {
                // todo: message it?
                if (stackPtr < BVH_STACK_SIZE - 1) {
                    stack[stackPtr++] = iNodeRight;
                }
            }
        }
    }

    #ifdef DELAYED_INTERSECT
        for (int i = 0; i < nLeafsIntersects; ++i) {
            uint objIdx = leafs[i];

            SRTObject obj = gRtObjects[objIdx];
            IntersectObj(ray, bestHit, obj, objIdx);
        }
    #endif

    return bestHit;
}

#else

RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();

    for (int iObject = 0; iObject < gRTConstants.nObjects; iObject++) {
        SRTObject obj = gRtObjects[iObject];
        IntersectObj(ray, bestHit, obj, objIdx);
    }

    return bestHit;
}

#endif

float3 RayColor(Ray ray, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < gRTConstants.rayDepth; depth++) {
        RayHit hit = Trace(ray);
        if (hit.tMax < INF) {
            // float3 albedo = hit.normal * 0.5f + 0.5f;
            // return hit.normal * 0.5f + 0.5f;
            SRTObject obj = gRtObjects[hit.materialID];

            ray.origin = hit.position + hit.normal * 0.0001;

            float3 V = -ray.direction;
            float3 N = hit.normal;
            float3 F0 = lerp(0.04, obj.baseColor, obj.metallic);
            float3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);

            color += obj.emissiveColor * energy; // todo: device by PI_2?

            if (1) {
            // if (RandomFloat(seed) > Luminance(F)) { // todo: is it ok?
                // todo: metallic?
                // float3 albedo = obj.baseColor;
                float3 albedo = obj.baseColor * (1 - obj.metallic);

                // Shadow test ray
                float3 L = -gScene.directLight.direction;
                float NDotL = dot(N, L);

                if (NDotL > 0) {
                    const float spread = 0.02; // todo:
                    Ray shadowRay = CreateRay(ray.origin, normalize(L + RandomInUnitSphere(seed) * spread));
                    RayHit shadowHit = Trace(shadowRay);
                    if (shadowHit.tMax == INF) {
                        float3 directLightShade = NDotL * albedo * gScene.directLight.color / PI_2;
                        color += directLightShade * energy;
                    }
                }

                ray.direction = normalize(RandomInHemisphere(hit.normal, seed));

                float NDotRayDir = max(dot(N, ray.direction), 0);
                energy *= albedo * NDotRayDir;
            } else {
                // todo:
                // float3 specular = obj.baseColor;
                float3 specular = F;

                float roughness = obj.roughness * obj.roughness; // todo:
                // ray.direction = reflect(ray.direction, hit.normal + RandomInUnitSphere(seed) * roughness);
                ray.direction = reflect(ray.direction, hit.normal);
                ray.direction = normalize(ray.direction + RandomInUnitSphere(seed) * roughness);

                energy *= specular;
            }
        } else {
            float3 skyColor = GetSkyColor(ray.direction);

            // remove sky bounce
            // if (depth != 0) { break; }

            color += skyColor * energy;
            break;
        }
    }

    return color;
}

RWTexture2D<float> gDepthOut : register(u1);
RWTexture2D<float4> gNormalOut : register(u2);

// todo:
#define EDITOR
#ifdef EDITOR
  #include "editor.hlsli"
#endif

[numthreads(8, 8, 1)]
void GBufferCS (uint2 id : SV_DispatchThreadID) { // todo: may i use uint2?
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    // todo: jitter
    float2 uv = (float2(id.xy) + 0.5) / float2(gRTConstants.rtSize);
    Ray ray = CreateCameraRay(uv);

    RayHit hit = Trace(ray);
    if (hit.tMax < INF) {
        SRTObject obj = gRtObjects[hit.materialID];

        gNormalOut[id] = float4(hit.normal * 0.5f + 0.5f, 1);

        float4 posH = mul(float4(hit.position, 1), gCamera.viewProjection);
        gDepthOut[id] = posH.z / posH.w;

        #if defined(EDITOR)
            SetEntityUnderCursor(id, obj.id);
        #endif
    } else {
        gNormalOut[id] = 0;
        gDepthOut[id] = 1;
    }
}

[numthreads(8, 8, 1)]
void rtCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    uint seed = id.x * 214234 + id.y * 521334 + asuint(gRTConstants.random01); // todo: first attempt, dont thing about it

    int nRays = gRTConstants.nRays;

    float3 color = 0;

    for (int i = 0; i < nRays; i++) {
        float2 offset = RandomFloat2(seed); // todo: halton sequence
        // float2 uv = float2(id.xy + offset) / float2(gRTConstants.rtSize);
        float2 uv = (float2(id.xy) + 0.5) / float2(gRTConstants.rtSize);

        Ray ray = CreateCameraRay(uv);
        color += RayColor(ray, seed);
    }

    color /= nRays;

    gColorOut[id.xy] = float4(color, 1);
}
