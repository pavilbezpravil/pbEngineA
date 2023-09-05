#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"
#include "random.hlsli"
#include "intersection.hlsli"

#include "NRDEncoding.hlsli"
#include "NRD.hlsli"

#define USE_BVH

cbuffer gRTConstantsCB : register(b0) {
  SRTConstants gRTConstants;
}

StructuredBuffer<SRTObject> gRtObjects : register(t0);
StructuredBuffer<BVHNode> gBVHNodes : register(t1);
// StructuredBuffer<SMaterial> gMaterials;

float3 GetCameraRayDirection(float2 uv) {
    float3 origin = gCamera.position;
    // float3 posW = mul(float4(uv, 1, 1), gCamera.invViewProjection).xyz;
    float3 posW = GetWorldPositionFromDepth(uv, 1, gCamera.invViewProjection);
    return normalize(posW - origin);
}

Ray CreateCameraRay(float2 uv) {
    float3 origin = gCamera.position;
    float3 direction = GetCameraRayDirection(uv);
    return CreateRay(origin, direction);
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
    uint stackPtr = 1;

    uint iNode = 0;

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

float3 RayColor(Ray ray, int nRayDepth, inout float firstHitDistance, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < nRayDepth; depth++) {
        RayHit hit = Trace(ray);

        if (depth == 0) {
            firstHitDistance += hit.tMax;
        }

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


uint GetRandomSeed(uint2 id) {
    return id.x * 214234 + id.y * 521334 + asuint(gRTConstants.random01);
}

float2 GetUV(uint2 id, uint2 texSize, float2 pixelOffset = 0) {
    return (float2(id) + 0.5 + pixelOffset) / float2(texSize);
}

#if 0
RWTexture2D<float> gDepthOut : register(u1);
RWTexture2D<float4> gNormalOut : register(u2);

// todo:
#define EDITOR
#ifdef EDITOR
  #include "editor.hlsli"
#endif

[numthreads(8, 8, 1)]
void GBufferCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float2 uv = GetUV(id, gRTConstants.rtSize);
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
#endif

Texture2D<float> gDepth;
Texture2D<float> gViewZ;
Texture2D<float4> gNormal;

RWTexture2D<float4> gColorOut : register(u0);

[numthreads(8, 8, 1)]
void rtCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    uint seed = GetRandomSeed(id);

    int nRays = gRTConstants.nRays;

    float3 color = 0;

    for (int i = 0; i < nRays; i++) {
        float2 offset = RandomFloat2(seed) - 0.5; // todo: halton sequence
        offset = 0;
        float2 uv = GetUV(id, gRTConstants.rtSize, offset);

        Ray ray = CreateCameraRay(uv);
        float firstHitDistance;
        color += RayColor(ray, gRTConstants.rayDepth, firstHitDistance, seed);
    }

    color /= nRays;

    gColorOut[id.xy] = float4(color, 1);
}

bool ClipByDepth(float depth) {
    return depth > 0.9999;
}

// from NRD source
// orthoMode = { 0 - perspective, -1 - right handed ortho, 1 - left handed ortho }
float3 ReconstructViewPosition( float2 uv, float2 cameraFrustumSize, float viewZ = 1.0, float orthoMode = 0.0 ) {
    float3 p;
    p.xy = uv * cameraFrustumSize * float2(2, -2) + cameraFrustumSize * float2(-1, 1);
    p.xy *= viewZ * ( 1.0 - abs( orthoMode ) ) + orthoMode;
    p.z = viewZ;

    return p;
}

// float3 ReconstructViewPosition( float2 uv, float4 cameraFrustum, float viewZ = 1.0, float orthoMode = 0.0 ) {
//     float3 p;
//     p.xy = uv * cameraFrustum.zw + cameraFrustum.xy;
//     p.xy *= viewZ * ( 1.0 - abs( orthoMode ) ) + orthoMode;
//     p.z = viewZ;

//     return p;
// }


[numthreads(8, 8, 1)]
void RTDiffuseSpecularCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float2 uv = GetUV(id, gRTConstants.rtSize);

    float depth = gDepth[id];
    if (ClipByDepth(depth)) {
        gColorOut[id] = float4(0, 0, 0, 1);
        return;
    }

    float viewZ = gViewZ[id]; // todo: clip far

    float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
    float3 posW = mul(float4(viewPos, 1), GetCamera().invView).xyz;

    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(gNormal[id]);
    float3 normal = normalRoughness.xyz;
    float roughness = normalRoughness.w;

    uint seed = GetRandomSeed(id);

    // todo:
    int nRays = gRTConstants.nRays;

    float3 radiance = 0;

    float3 V = normalize(gCamera.position - posW);

    float firstHitDistance = 0;

    for (int i = 0; i < nRays; i++) {
        Ray ray;
        ray.origin = posW;

        #if defined(DIFFUSE)
            float3 L = -gScene.directLight.direction;
            float NDotL = dot(normal, L);

            if (NDotL > 0) {
                const float spread = 0.02; // todo:
                Ray shadowRay = CreateRay(ray.origin, normalize(L + RandomInUnitSphere(seed) * spread));
                RayHit shadowHit = Trace(shadowRay);
                if (shadowHit.tMax == INF) {
                    float3 directLightShade = NDotL * gScene.directLight.color;
                    radiance += directLightShade;
                }
            }

            ray.direction = normalize(RandomInHemisphere(normal, seed));
        #elif defined(SPECULAR)
            ray.direction = reflect(-V, normal);
            ray.direction = normalize(ray.direction + RandomInUnitSphere(seed) * roughness * roughness);
        #else
            // #error "Define DIFFUSE or SPECULAR"
        #endif

        radiance += RayColor(ray, gRTConstants.rayDepth, firstHitDistance, seed);
    }

    firstHitDistance /= nRays;
    radiance /= nRays;

    // color = normal * 0.5 + 0.5;
    // color = reflect(V, normal) * 0.5 + 0.5;
    // radiance = frac(posW);

    float normHitDist = REBLUR_FrontEnd_GetNormHitDist(firstHitDistance, viewZ, gRTConstants.nrdHitDistParams, roughness);
    gColorOut[id] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(radiance, normHitDist);
}

Texture2D<float4> gBaseColor;
Texture2D<float4> gDiffuse;
Texture2D<float4> gSpecular;

[numthreads(8, 8, 1)]
void RTCombineCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float depth = gDepth[id];
    float2 uv = GetUV(id, gRTConstants.rtSize);
    float3 posW = GetWorldPositionFromDepth(uv, depth, gCamera.invViewProjection);

    float3 V = normalize(gCamera.position - posW);
    if (ClipByDepth(depth)) {
        float3 skyColor = GetSkyColor(-V);
        gColorOut[id] = float4(skyColor, 1);
        return;
    }

    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(gNormal[id]);
    float3 normal = normalRoughness.xyz;

    float4 baseColorMetallic = gBaseColor[id];
    float3 baseColor = baseColorMetallic.xyz;
    float  metallic = baseColorMetallic.w;

    float3 diffuse = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(gDiffuse[id]).xyz;
    float3 specular = REBLUR_BackEnd_UnpackRadianceAndNormHitDist(gSpecular[id]).xyz;

    float3 F0 = lerp(0.04, baseColor, metallic);
    float3 F = fresnelSchlick(max(dot(normal, V), 0.0), F0);

    // todo:
    float3 albedo = baseColor * (1 - metallic);
    float3 color = (1 - F) * albedo * diffuse / PI + F * specular;

    gColorOut[id] = float4(color, 1);
}
