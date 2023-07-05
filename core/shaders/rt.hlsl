#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"

RWTexture2D<float4> gColor;

cbuffer gRTConstantsCB : register(b0) {
  SRTConstants gRTConstants;
}

StructuredBuffer<SRTObject> gRtObjects;
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
    return (float)seed / (float)uint(-1);
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
    return 0;
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
    float distance;
    float3 normal;
    int materialID;
};

RayHit CreateRayHit() {
    RayHit hit;
    hit.position = 0;
    hit.distance = INF;
    hit.normal = 0;
    hit.materialID = 0;
    return hit;
}

bool IntersectGroundPlane(Ray ray, inout RayHit bestHit) {
    // Calculate distance along the ray where the ground plane is intersected
    float t = -ray.origin.y / ray.direction.y;
    if (t > 0 && t < bestHit.distance) {
        bestHit.distance = t;
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
    if (t > 0 && t < bestHit.distance) {
        bestHit.distance = t;
        bestHit.position = ray.origin + t * ray.direction;
        bestHit.normal = normalize(bestHit.position - sphere.xyz);
        return true;
    }
    return false;
}

bool IntersectAABB(Ray ray, inout RayHit bestHit, float3 center, float3 halfSize) {
    float3 boxMin = center - halfSize;
    float3 boxMax = center + halfSize;

    float3 tMin = (boxMin - ray.origin) / ray.direction;
    float3 tMax = (boxMax - ray.origin) / ray.direction;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    if (tFar > tNear && tNear > 0 && tNear < bestHit.distance) {
        bestHit.distance = tNear;
        bestHit.position = ray.origin + tNear * ray.direction; // todo: move from hear

        float3 localPoint = bestHit.position - center;
        float3 normal = abs(localPoint / halfSize) > 0.99999;
        normal *= localPoint;
        bestHit.normal = normalize(normal);
        return true;
    }
    return false;
};

RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();
    // if (IntersectGroundPlane(ray, bestHit)) {
    //     bestHit.materialID = 0;
    // }

    for (int iObject = 0; iObject < gRTConstants.nObjects; iObject++) {
        SRTObject rtObject = gRtObjects[iObject];
        int geomType = rtObject.geomType;

        // geomType = 0;
        if (geomType == 1) {
            if (IntersectAABB(ray, bestHit, rtObject.position, rtObject.halfSize)) {
                bestHit.materialID = iObject;
            }
        } else {
            if (IntersectSphere(ray, bestHit, float4(rtObject.position, rtObject.halfSize.x))) {
                bestHit.materialID = iObject;
            }
        }
    }

    return bestHit;
}

// write luminance from rgb
float Luminance(float3 color) {
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

#ifdef IMPORTANCE_SAMPLING
float3 RayColor(Ray ray, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < gRTConstants.rayDepth; depth++) {
        RayHit hit = Trace(ray);
        if (hit.distance < INF) {
            SRTObject obj = gRtObjects[hit.materialID];

            ray.origin = hit.position + hit.normal * 0.0001;

            color += obj.emissiveColor * energy; // todo: device by PI_2?

            float3 albedo = obj.baseColor * (1 - obj.metallic);

            #if 0
                // Shadow test ray
                float3 L = -gScene.directLight.direction;
                const float spread = 0.1; // todo:
                Ray shadowRay = CreateRay(ray.origin, normalize(L + RandomInUnitSphere(seed) * spread));
                RayHit shadowHit = Trace(shadowRay);
                if (shadowHit.distance == INF) {
                    float3 directLightShade = dot(hit.normal, L) * albedo * gScene.directLight.color / PI_2;
                    color += directLightShade * energy;
                }
            #endif

            ray.direction = normalize(RandomInHemisphere(hit.normal, seed));

            energy *= albedo;
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
#else
float3 RayColor(Ray ray, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < gRTConstants.rayDepth; depth++) {
        RayHit hit = Trace(ray);
        if (hit.distance < INF) {
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
                const float spread = 0.1; // todo:
                Ray shadowRay = CreateRay(ray.origin, normalize(L + RandomInUnitSphere(seed) * spread));
                RayHit shadowHit = Trace(shadowRay);
                if (shadowHit.distance == INF) {
                    float3 directLightShade = dot(hit.normal, L) * albedo * gScene.directLight.color / PI_2;
                    color += directLightShade * energy;
                }

                ray.direction = normalize(RandomInHemisphere(hit.normal, seed));

                energy *= albedo;
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

RWTexture2D<float> gDepthOut;
RWTexture2D<float4> gNormalOut;
RWTexture2D<uint> gObjIDOut;

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
    if (hit.distance < INF) {
        SRTObject obj = gRtObjects[hit.materialID];

        gObjIDOut[id] = obj.id;
        gNormalOut[id] = float4(hit.normal * 0.5f + 0.5f, 1);

        float4 posH = mul(float4(hit.position, 1), gCamera.viewProjection);
        gDepthOut[id] = posH.z / posH.w;

        #if defined(EDITOR)
            SetEntityUnderCursor(id, obj.id);
        #endif
    } else {
        gObjIDOut[id] = -1;
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
        float2 uv = float2(id.xy + 0.5) / float2(gRTConstants.rtSize);

        Ray ray = CreateCameraRay(uv);
        color += RayColor(ray, seed);
    }

    color /= nRays;

    gColor[id.xy] = float4(color, 1);
}


Texture2D<float> gDepthPrev;
Texture2D<float> gDepth;

Texture2D<float4> gNormalPrev;
Texture2D<float4> gNormal;

Texture2D<float4> gHistory; // prev
Texture2D<uint> gReprojectCount; // prev

Texture2D<uint> gObjIDPrev;
Texture2D<uint> gObjID;

RWTexture2D<float4> gHistoryOut;
RWTexture2D<uint> gReprojectCountOut;

// #define ENABLE_REPROJECTION

float3 SafeNormalize(float3 v) {
    float len = length(v);
    if (len > 0) {
        return v / len;
    } else {
        return 0;
    }
}

float3 NormalFromTex(float4 normal) {
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

        float3 color = gColor[id].xyz;

        uint objID = gObjID[id];
        float3 normal = NormalFromTex(gNormal[id]);
        // validDepth = depthRaw != 1
        //            || gDepth[id + int2( 1, 0)] != 1
        //            || gDepth[id + int2(-1, 0)] != 1
        //            || gDepth[id + int2( 0, 1)] != 1
        //            || gDepth[id + int2( 0,-1)] != 1
        //            ;

        float3 dbgColor = 0;

        bool successReproject = true;

        if (objID != -1 && prevInScreen) {
            uint objIDPrev = gObjIDPrev[prevSampleIdx];
            float3 normalPrev = NormalFromTex(gNormalPrev[prevSampleIdx]);

            float normalCoeff = pow(max(dot(normal, normalPrev), 0), 3); // todo:
            bool normalFail = normalCoeff <= 0;
            historyWeight *= normalCoeff;

            float prevDepth = gDepthPrev[prevSampleIdx];
            float3 prevPosW = GetWorldPositionFromDepth(prevUV, prevDepth, gCamera.prevInvViewProjection);

            // gRTConstants.historyMaxDistance
            float depthCoeff = saturate(2 - distance(posW, prevPosW) / 0.1);
            depthCoeff = 1;
            bool depthFail = depthCoeff <= 0;
            historyWeight *= depthCoeff;

            // dbgColor.r += 1 - depthCoeff;

            if (objIDPrev != objID || normalFail || depthFail) {
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
        gColor[id] = color4;
    #else
        float w = gRTConstants.historyWeight;
        float3 history = gHistoryOut[id.xy].xyz * w + gColor[id.xy].xyz * (1 - w);
        float4 color = float4(history, 1);

        gColor[id.xy] = color;
        gHistoryOut[id.xy] = color;
    #endif
}
