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

            // if (0) {
            if (RandomFloat(seed) > Luminance(F)) { // todo: is it ok?
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

RWTexture2D<float> gDepthOut;
RWTexture2D<float4> gNormalOut;

[numthreads(8, 8, 1)]
void GBufferCS (uint2 id : SV_DispatchThreadID) { // todo: may i use uint2?
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float2 uv = (float2(id.xy) + 0.5) / float2(gRTConstants.rtSize);
    Ray ray = CreateCameraRay(uv);

    RayHit hit = Trace(ray);
    if (hit.distance < INF) {
        // SRTObject obj = gRtObjects[hit.materialID];

        gNormalOut[id] = float4(hit.normal * 0.5f + 0.5f, 1);

        float4 posH = mul(float4(hit.position, 1), gCamera.viewProjection);
        // posH.z / posH.w;
        // gDepthOut[int2(posH.xy * gRTConstants.rtSize)] = posH.z;
        // gDepthOut[id] = posH.z;
        gDepthOut[id] = posH.z / posH.w;
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
        float2 uv = float2(id.xy + offset) / float2(gRTConstants.rtSize);

        Ray ray = CreateCameraRay(uv);
        color += RayColor(ray, seed);
    }

    color /= nRays;

    gColor[id.xy] = float4(color, 1);
}


Texture2D<float> gDepth;
Texture2D<float4> gNormal;
Texture2D<float4> gHistory;
Texture2D<uint> gReprojectCount;

RWTexture2D<float4> gHistoryOut;
RWTexture2D<uint> gReprojectCountOut;

// #define ENABLE_REPROJECTION

[numthreads(8, 8, 1)]
void HistoryAccCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    #ifdef ENABLE_REPROJECTION
        float3 normalW = gNormal[id].xyz;

        float depthRaw = gDepth[id];
        // float depth = LinearizeDepth(depthRaw);

        float2 uv = (float2(id) + 0.5) / float2(gCamera.rtSize);
        float3 posW = GetWorldPositionFromDepth(uv, depthRaw, gCamera.invViewProjection);

        float historyWeight = gRTConstants.historyWeight;

        float4 prevSample = mul(float4(posW, 1), gCamera.prevViewProjection);
        prevSample /= prevSample.w;

        float2 prevUV = NDCToTex(prevSample.xy);
        uint2 prevSampleIdx = prevUV * gCamera.rtSize;

        float3 color = gColor[id].xyz;

        bool validDepth = depthRaw != 1;
        // validDepth = depthRaw != 1
        //            || gDepth[id + int2( 1, 0)] != 1
        //            || gDepth[id + int2(-1, 0)] != 1
        //            || gDepth[id + int2( 0, 1)] != 1
        //            || gDepth[id + int2( 0,-1)] != 1
        //            ;

        if (all(prevSample.xy > -1 && prevSample.xy < 1) && validDepth) {
            int nRays = gRTConstants.nRays;

            uint oldCount = gReprojectCount[prevSampleIdx];
            uint nextCount = oldCount + nRays;
            // historyWeight = float(oldCount) / float(nextCount);

            // todo: race
            gReprojectCountOut[id] = min(nextCount, 255);
        } else {
            gReprojectCountOut[id] = 0;
            historyWeight = 0;
        }

        if (historyWeight > 0) {
            // todo: cant sample uav with sampler
            // float3 historyColor = gHistory.SampleLevel(gSamplerPoint, prevUV, 0);
            // float3 historyColor = gHistory.SampleLevel(gSamplerLinear, prevUV, 0);
            float3 historyColor = gHistory[prevSampleIdx].xyz;
            // historyColor = 0;
            // float3 historyColor = gHistory[id].xyz;
            color = lerp(color, historyColor, historyWeight);
        }

        float4 color4 = float4(color, 1);

        gHistoryOut[id] = color4;
        // color4.r += saturate(float(255 - gReprojectCountOut[id]) / 255);
        gColor[id] = color4;        
    #else
        float w = gRTConstants.historyWeight;
        float3 history = gHistoryOut[id.xy].xyz * w + gColor[id.xy].xyz * (1 - w);
        float4 color = float4(history, 1);

        gColor[id.xy] = color;
        gHistoryOut[id.xy] = color;
    #endif
}
