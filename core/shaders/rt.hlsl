#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"

static uint UINT_MAX = uint(-1);

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

#define INF 1000000

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

// todo: test struct fucntion call
// struct asdf {
//     uint seed;
//     float Rng() {
//         return RandomFloat(seed);
//     }
// };

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

#ifdef USE_BVH

// RayHit BVHTraverse(Ray ray) {
RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();

    if (gRTConstants.bvhNodes == 0) {
        return bestHit;
    }

    float tMax = INF;

    const uint BVH_STACK_SIZE = 32;
    uint stack[BVH_STACK_SIZE];
    int stackPtr = 0;

    stack[0] = 0;

    while (stackPtr >= 0) {
        int iNode = stack[stackPtr];

        uint objIdx = gBVHNodes[iNode].objIdx;
        bool isLeaf = objIdx != UINT_MAX;

        // todo: slow
        if (isLeaf) {
            SRTObject obj = gRtObjects[objIdx];
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

            stackPtr--;
            continue;
        }

        uint iNodeLeft = iNode * 2 + 1;
        uint iNodeRight = iNode * 2 + 2;

        BVHNode nodeLeft = gBVHNodes[iNodeLeft];
        BVHNode nodeRight = gBVHNodes[iNodeRight];

        bool intersectL = IntersectAABB_Fast(ray, bestHit.tMax, nodeLeft.aabbMin, nodeLeft.aabbMax);
        bool intersectR = IntersectAABB_Fast(ray, bestHit.tMax, nodeRight.aabbMin, nodeRight.aabbMax);

        // todo: simplify
        if (!intersectL && !intersectR) {
            stackPtr--;
        } else {
            if (intersectL && intersectR) {
                stack[stackPtr] = iNodeLeft;

                // todo: assert?
                if (stackPtr + 1 < BVH_STACK_SIZE) {
                    stackPtr++;
                    stack[stackPtr] = iNodeRight;
                }
            } else {
                if (intersectL) {
                    stack[stackPtr] = iNodeLeft;
                } else {
                    stack[stackPtr] = iNodeRight;
                }
            }
        }
    }

    return bestHit;
}

#else

RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();
    // if (IntersectGroundPlane(ray, bestHit)) {
    //     bestHit.materialID = 0;
    // }

    for (int iObject = 0; iObject < gRTConstants.nObjects; iObject++) {
        SRTObject obj = gRtObjects[iObject];
        int geomType = obj.geomType;

        // geomType = 0;
        if (geomType == 1) {
            // if (IntersectAABB(ray, bestHit, obj.position, obj.halfSize)) {
            if (IntersectOBB(ray, bestHit, obj.position, obj.rotation, obj.halfSize)) {
                bestHit.materialID = iObject;
            }
        } else {
            if (IntersectSphere(ray, bestHit, float4(obj.position, obj.halfSize.x))) {
                bestHit.materialID = iObject;
            }
        }
    }

    return bestHit;
}

#endif

// write luminance from rgb
float Luminance(float3 color) {
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

#ifdef IMPORTANCE_SAMPLING
// todo:
#else
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
#endif

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

bool IsNaN(float x) {
  return !(x < 0.0f || x > 0.0f || x == 0.0f);
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


// todo: register s?
Texture2D<float> gDepthPrev : register(s1);
Texture2D<float> gDepth : register(s2);

Texture2D<float4> gNormalPrev : register(s3);
Texture2D<float4> gNormal : register(s4);

Texture2D<float4> gHistory : register(s5); // prev
Texture2D<uint> gReprojectCount : register(s6); // prev

Texture2D<float4> gColor : register(s7);

RWTexture2D<float4> gHistoryOut : register(u4);
RWTexture2D<uint> gReprojectCountOut : register(u5);

// #define ENABLE_REPROJECTION

float3 NormalFromTex(float3 normal) {
    // todo: dont know why, but it produce artifacts
    // return normal.xyz * 2 - 1;
    return SafeNormalize(normal.xyz * 2 - 1);
}

// todo:
// 1. neight sample
[numthreads(8, 8, 1)]
void HistoryAccCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    #ifdef ENABLE_REPROJECTION
        float depth = gDepth[id];

        float2 uv = (float2(id) + 0.5) / float2(gCamera.rtSize);
        float3 posW = GetWorldPositionFromDepth(uv, depth, gCamera.invViewProjection);

        float4 prevSample = mul(float4(posW, 1), gCamera.prevViewProjection);
        prevSample /= prevSample.w;

        float2 prevUV = NDCToTex(prevSample.xy);
        uint2 prevSampleIdx = prevUV * gCamera.rtSize;

        bool prevInScreen = all(prevUV > 0 && prevUV < 1); // todo: get info from neigh that are in screen
        // bool prevInScreen = all(prevUV > 0.02 && prevUV < 0.98); // todo: get info from neigh that are in screen

        float historyWeight = gRTConstants.historyWeight;

        float3 color = gColorOut[id].xyz;

        float3 normal = NormalFromTex(gNormal[id].xyz);
        // validDepth = depthRaw != 1
        //            || gDepth[id + int2( 1, 0)] != 1
        //            || gDepth[id + int2(-1, 0)] != 1
        //            || gDepth[id + int2( 0, 1)] != 1
        //            || gDepth[id + int2( 0,-1)] != 1
        //            ;

        float3 dbgColor = 0;

        bool successReproject = true;

        if (prevInScreen) {
            float3 normalPrev = NormalFromTex(gNormalPrev[prevSampleIdx].xyz);

            float normalCoeff = pow(max(dot(normal, normalPrev), 0), 3); // todo:
            bool normalFail = normalCoeff <= 0;
            historyWeight *= normalCoeff;

            float prevDepth = gDepthPrev[prevSampleIdx];
            float3 prevPosW = GetWorldPositionFromDepth(prevUV, prevDepth, gCamera.prevInvViewProjection);

            // gRTConstants.historyMaxDistance
            float depthCoeff = saturate(2 - distance(posW, prevPosW) / 0.1);
            // depthCoeff = 1;
            bool depthFail = depthCoeff <= 0;
            historyWeight *= depthCoeff;

            // dbgColor.r += 1 - depthCoeff;

            if (normalFail || depthFail) {
                historyWeight = 0;
                successReproject = false;

                if (gRTConstants.debugFlags & DBG_FLAG_SHOW_NEW_PIXEL) {
                    dbgColor += float3(1, 0, 0);
                }
            }
        } else {
            successReproject = false;
            historyWeight = 0;
        }

        if (successReproject) {
            int nRays = gRTConstants.nRays;

            uint oldCount = gReprojectCount[prevSampleIdx];
            uint nextCount = oldCount + nRays;
            historyWeight *= float(oldCount) / float(nextCount);

            gReprojectCountOut[id] = min(nextCount, 255);

            // float3 historyColor = gHistory.SampleLevel(gSamplerPoint, prevUV, 0);
            // float3 historyColor = gHistory.SampleLevel(gSamplerLinear, prevUV, 0);
            float3 historyColor = gHistory[prevSampleIdx].xyz;
            color = lerp(color, historyColor, historyWeight);
        } else {
            gReprojectCountOut[id] = 0;
        }

        float4 color4 = float4(color, 1);

        gHistoryOut[id] = color4;
        // dbgColor.r += saturate(float(255 - gReprojectCountOut[id]) / 255);
        color4.xyz += dbgColor;
        // color4.xyz = normal;
        gColorOut[id] = color4;
    #else
        float w = gRTConstants.historyWeight;
        float3 history = gHistoryOut[id.xy].xyz * w + gColorOut[id.xy].xyz * (1 - w);
        float4 color = float4(history, 1);

        gColorOut[id.xy] = color;
        gHistoryOut[id.xy] = color;
    #endif
}

void DenoiseSample() {
    
}

#define METHOD 1

[numthreads(8, 8, 1)]
void DenoiseCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float3 color = gColor[id].xyz;

#if METHOD == 0
    gHistoryOut[id] = float4(color, 1);
    gColorOut[id] = float4(color, 1);
#elif METHOD == 1

    uint seed = id.x * 214234 + id.y * 521334 + asuint(gRTConstants.random01); // todo: first attempt, dont thing about it

    // todo: mb tex size different from rt size?
    float2 rtSizeInv = 1 / float2(gCamera.rtSize);

    float2 uv = (float2(id) + 0.5) * rtSizeInv;

    float depth = gDepth[id];
    
    if (depth > 0.999) {
        gHistoryOut[id] = float4(color, 1);
        gColorOut[id] = float4(color, 1);
        return;
    }
    
    // float2 motion = uMotion.Sample(MinMagMipNearestWrapEdge, input.texCoord).rg;
    float2 motion = 0;
    float3 normal = NormalFromTex(gNormal[id].xyz);
    
    float3 posW = GetWorldPositionFromDepth(uv, depth, gCamera.invViewProjection);

    float3 colNew = color;

    float3 org = colNew;
    float3 mi = colNew;
    float3 ma = colNew;
    
    float tot = 1.0;
    
    const float goldenAngle = 2.39996323;
    const float size = 6.0;
    const float radInc = 1.0;
    
    float radius = 0.5;
    
    // todo: blue noise
    for (float ang = RandomFloat(seed) * goldenAngle; radius < size; ang += goldenAngle) {
        radius += radInc;
        float2 sampleUV = uv + float2(cos(ang), sin(ang)) * rtSizeInv * radius;
        
        float weight = saturate(1.2 - radius / size);
        
        // Check depth
        float sampleDepth = gDepth.SampleLevel(gSamplerPointClamp, sampleUV, 0);
        float3 samplePosW = GetWorldPositionFromDepth(sampleUV, sampleDepth, gCamera.invViewProjection);

        float3 toSample = normalize(samplePosW - posW);
        float toSampleDotN = dot(toSample, normal);
        weight *= step(abs(toSampleDotN), 0.2);
        
        // Check normal
        float3 sampleNormal = NormalFromTex(gNormal.SampleLevel(gSamplerPointClamp, sampleUV, 0).xyz);
        weight *= step(0.9, dot(normal, sampleNormal));
        
        float3 sampleColor = gColor.SampleLevel(gSamplerPointClamp, sampleUV, 0).xyz;
        float3 mc = lerp(org, sampleColor, weight);
        
        mi = min(mi, mc);
        ma = max(ma, mc);
        
        colNew += weight * sampleColor;
        tot += weight;
    }
    
    colNew /= tot;
    
    float3 colOld = gHistory.SampleLevel(gSamplerPointClamp, uv - motion, 0).xyz;
    colOld = clamp(colOld, mi, ma);

    colNew = lerp(colNew, colOld, gRTConstants.historyWeight);

    gHistoryOut[id] = float4(colNew, 1);
    gColorOut[id] = float4(colNew, 1);

#endif
}
