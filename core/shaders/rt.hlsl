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

    // geomType = 1;
    if (geomType == 1) {
        // if (IntersectAABB(ray, bestHit, obj.position, obj.halfSize)) {
        if (IntersectOBB(ray, bestHit, obj.position, obj.rotation, obj.halfSize)) {
            bestHit.objID = objIdx;
        }
    } else {
        if (IntersectSphere(ray, bestHit, float4(obj.position, obj.halfSize.x))) {
            bestHit.objID = objIdx;
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

    float3 rayInvDirection = 1 / ray.direction;

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

        float intersectLDist = IntersectAABB_Fast(ray.origin, rayInvDirection, nodeLeft.aabbMin, nodeLeft.aabbMax);
        float intersectRDist = IntersectAABB_Fast(ray.origin, rayInvDirection, nodeRight.aabbMin, nodeRight.aabbMax);

        bool intersectL = intersectLDist < bestHit.tMax;
        bool intersectR = intersectRDist < bestHit.tMax;

        if (!intersectL && !intersectR) {
            iNode = stack[--stackPtr];
        } else {
            // when we nedd to visit both node chose left or first visit nearest node
            #if 0
                iNode = intersectL ? iNodeLeft : iNodeRight;

                if (intersectL && intersectR) {
                    // todo: message it?
                    if (stackPtr < BVH_STACK_SIZE - 1) {
                        stack[stackPtr++] = iNodeRight;
                    }
                }
            #else
                if (intersectL && intersectR) {
                    bool isFirstVisitLeft = intersectLDist < intersectRDist;
                    iNode = isFirstVisitLeft ? iNodeLeft : iNodeRight;

                    // todo: message it?
                    if (stackPtr < BVH_STACK_SIZE - 1) {
                        stack[stackPtr++] = !isFirstVisitLeft ? iNodeLeft : iNodeRight;
                    }
                } else {
                    iNode = intersectL ? iNodeLeft : iNodeRight;
                }
            #endif
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
        IntersectObj(ray, bestHit, obj, iObject);
    }

    return bestHit;
}

#endif

float3 RayColor(Ray ray, int nRayDepth, inout float hitDistance, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < nRayDepth; depth++) {
        RayHit hit = Trace(ray);

        if (depth == 0) {
            hitDistance += hit.tMax;
        }

        if (hit.tMax < INF) {
            // float3 albedo = hit.normal * 0.5f + 0.5f;
            // return hit.normal * 0.5f + 0.5f;
            SRTObject obj = gRtObjects[hit.objID];

            ray.origin = hit.position + hit.normal * 0.0001;

            float3 V = -ray.direction;
            float3 N = hit.normal;
            float3 F0 = lerp(0.04, obj.baseColor, obj.metallic);
            float3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);

            color += obj.baseColor * obj.emissivePower * energy; // todo: device by PI_2?

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
        SRTObject obj = gRtObjects[hit.objID];

        gNormalOut[id] = float4(hit.normal * 0.5f + 0.5f, 1);

        float4 posH = mul(gCamera.viewProjection, float4(hit.position, 1));
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

bool ViewZDiscard(float viewZ) {
    return viewZ > 1e4; // todo: constant
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

float3 ReconstructWorldPosition( float3 viewPos ) {
    return mul(GetCamera().invView, float4(viewPos, 1)).xyz;
}

RWTexture2D<float4> gDiffuseOut;
RWTexture2D<float4> gSpecularOut;

// #if defined(USE_PSR)
    RWTexture2D<float> gViewZOut;
    RWTexture2D<float4> gNormalOut;
    RWTexture2D<float4> gBaseColorOut;
// #endif

#if defined(USE_PSR)
    bool CheckSurfaceForPSR(float roughness, float metallic) {
        // float3 normal, float3 V
        return roughness < 0.01 && metallic > 0.95;
    }

    float RTDiffuseSpecularCS_ReadViewZ(uint2 pixelPos) {
        return gViewZOut[pixelPos];
    }

    float4 RTDiffuseSpecularCS_ReadNormal(uint2 pixelPos) {
        return gNormalOut[pixelPos];
    }
#else
    float RTDiffuseSpecularCS_ReadViewZ(uint2 pixelPos) {
        return gViewZ[pixelPos];
    }

    float4 RTDiffuseSpecularCS_ReadNormal(uint2 pixelPos) {
        return gNormal[pixelPos];
    }
#endif

[numthreads(8, 8, 1)]
void RTDiffuseSpecularCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float viewZ = RTDiffuseSpecularCS_ReadViewZ(id);
    if (ViewZDiscard(viewZ)) {
        return;
    }

    float2 uv = GetUV(id, gRTConstants.rtSize);
    float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
    float3 posW = ReconstructWorldPosition(viewPos);

    float3 V = normalize(gCamera.position - posW);

    float4 normalRoughnessNRD = RTDiffuseSpecularCS_ReadNormal(id);
    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(normalRoughnessNRD);
    float3 normal = normalRoughness.xyz;
    float roughness = normalRoughness.w;

    uint seed = GetRandomSeed(id);

    Ray ray;

    #if defined(USE_PSR)
        float4 baseColorMetallic = gBaseColorOut[id];
        float3 baseColor = baseColorMetallic.xyz;
        float  metallic = baseColorMetallic.w;

        float3 psrEnergy = 1;

        // todo: better condition
        if (CheckSurfaceForPSR(roughness, metallic)) {
            ray.origin = posW;
            ray.direction = reflect(-V, normal);

            RayHit hit = Trace(ray);
            if (hit.tMax < INF) {
                float3 F0 = lerp(0.04, baseColor, metallic);
                float3 F = fresnelSchlick(max(dot(normal, V), 0.0), F0);
                psrEnergy = F;

                SRTObject obj = gRtObjects[hit.objID];

                float3 psrPosW = ray.origin + -V * hit.tMax;
                gBaseColorOut[id] = float4(obj.baseColor, obj.metallic);
                gViewZOut[id] = mul(gCamera.view, float4(psrPosW, 1)).z;

                float3 psrNormal = reflect(hit.normal, normal);
                gNormalOut[id] = NRD_FrontEnd_PackNormalAndRoughness(psrNormal, obj.roughness);

                float3 emissive = (obj.baseColor * obj.emissivePower) * psrEnergy;
                float4 prevColor = gColorOut[id];
                gColorOut[id] = float4(prevColor.xyz + emissive, prevColor.w);

                // todo: motion

                posW = hit.position;
                V = -ray.direction;
                normal = hit.normal;
                roughness = obj.roughness;
                metallic = obj.metallic;
            } else {
                // todo: skip diffuse rays
                // float3 skyColor = GetSkyColor(-V);
            }
        }
    #endif // USE_PSR

    ray.origin = posW;

    // todo:
    int nRays = gRTConstants.nRays;

    float diffuseHitDistance = 0;
    float specularHitDistance = 0;

    float3 diffuseRadiance = 0;
    float3 specularRadiance = 0;

    for (int i = 0; i < nRays; i++) {
        ray.direction = reflect(-V, normal);
        ray.direction = normalize(ray.direction + RandomInUnitSphere(seed) * roughness * roughness);

        specularRadiance += RayColor(ray, gRTConstants.rayDepth, specularHitDistance, seed);
    }

    for (int i = 0; i < nRays; i++) {
        #if 1
            float3 L = -gScene.directLight.direction;
            float NDotL = dot(normal, L);

            if (NDotL > 0) {
                const float spread = 0.02; // todo:
                Ray shadowRay = CreateRay(ray.origin, normalize(L + RandomInUnitSphere(seed) * spread));
                RayHit shadowHit = Trace(shadowRay); // todo: any hit
                if (shadowHit.tMax == INF) {
                    float3 directLightShade = NDotL * gScene.directLight.color;
                    diffuseRadiance += directLightShade;
                }
            }
        #endif

        ray.direction = normalize(RandomInHemisphere(normal, seed));

        float Diffuse_NDotL = max(dot(normal, ray.direction), 0);
        diffuseRadiance += RayColor(ray, gRTConstants.rayDepth, diffuseHitDistance, seed) * Diffuse_NDotL;
    }

    #if defined(USE_PSR)
        diffuseRadiance *= psrEnergy;
        specularRadiance *= psrEnergy;
    #endif

    diffuseHitDistance /= nRays;
    diffuseRadiance /= nRays;

    specularHitDistance /= nRays;
    specularRadiance /= nRays;

    // color = normal * 0.5 + 0.5;
    // color = reflect(V, normal) * 0.5 + 0.5;
    // radiance = frac(posW);

    float diffuseNormHitDist = REBLUR_FrontEnd_GetNormHitDist(diffuseHitDistance, viewZ, gRTConstants.nrdHitDistParams, roughness);
    gDiffuseOut[id] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(diffuseRadiance, diffuseNormHitDist);

    float specularNormHitDist = REBLUR_FrontEnd_GetNormHitDist(specularHitDistance, viewZ, gRTConstants.nrdHitDistParams, roughness);
    gSpecularOut[id] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(specularRadiance, specularNormHitDist);
}

Texture2D<float4> gBaseColor;
Texture2D<float4> gDiffuse;
Texture2D<float4> gSpecular;

[numthreads(8, 8, 1)]
void RTCombineCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }
    
    float viewZ = gViewZ[id];

    float2 uv = GetUV(id, gRTConstants.rtSize);
    float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
    float3 posW = ReconstructWorldPosition(viewPos);

    float3 V = normalize(gCamera.position - posW);
    if (ViewZDiscard(viewZ)) {
        float3 skyColor = GetSkyColor(-V);
        gColorOut[id] = float4(skyColor, 1);
        return;
    }

    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(gNormal[id]);
    float3 normal = normalRoughness.xyz;
    float  roughness = normalRoughness.w;

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

    float3 emissive = gColorOut[id].xyz;
    gColorOut[id] = float4(color + emissive, 1);
}

float4 HeightFogDensity(float3 posW, float fogStart = -10, float fogEnd = 15) {
    float height = posW.y;
    return 1 - saturate((height - fogStart) / (fogEnd - fogStart));
}

// video about atmospheric scattering
// https://www.youtube.com/watch?v=DxfEbulyFcY&ab_channel=SebastianLague

// https://www.youtube.com/watch?v=Qj_tK_mdRcA&ab_channel=SimonDev
float HenyeyGreenstein(float g, float costh) {
    return (1.0 - g * g) / (4.0 * PI * pow(1.0 + g * g - 2.0 * g * costh, 3.0 / 2.0));
}

float MiScattering(float g, float VDotL) {
    // lerp( forward, backward )
    return lerp(HenyeyGreenstein(g, VDotL), HenyeyGreenstein(g, -VDotL), 0.8);
}

// https://iquilezles.org/articles/fog/
float GetHeightFog(float3 rayOri, float3 rayDir, float distance) {
    float a = 0.002;
    float b = 0.05; // todo:

    // todo: devide by zero
    float fogAmount = (a / b) * exp(-rayOri.y * b) * (1.0 - exp( -distance * rayDir.y * b )) / rayDir.y;
    return saturate(fogAmount);
}

[numthreads(8, 8, 1)]
void RTFogCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    uint seed = GetRandomSeed(id);
    
    float viewZ = gViewZ[id];

    float2 uv = GetUV(id, gRTConstants.rtSize);
    float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
    float3 posW = ReconstructWorldPosition(viewPos);

    float3 V = gCamera.position - posW;
    float dist = length(V);
    V = normalize(V);

    float3 fogColor  = float3(0.5, 0.6, 0.7);

    const float MAX_DIST = 50;
    if (dist > MAX_DIST) {
        dist = MAX_DIST;
    }

    const int maxSteps = gScene.fogNSteps;
    if (maxSteps <= 0) {
        return;
        float4 color = gColorOut[id];
        float fog = GetHeightFog(gCamera.position, -V, dist);
        color.xyz = lerp(color.xyz, fogColor, fog);
        gColorOut[id] = color;

        return;
    }

    float stepLength = dist / maxSteps; // todo: 

    float initialOffset = RandomFloat(seed) * stepLength;
    float3 fogPosW = gCamera.position + -V * initialOffset;

    float accTransmittance = 1;
    float3 accScaterring = 0;

    // accTransmittance *= exp(-initialOffset * 0.2);

    float prevStepLength = initialOffset;

    for(int i = 0; i < maxSteps; ++i) {
        // float t = i / float(maxSteps - 1);
        float fogDensity = saturate(noise(fogPosW * 0.3) - 0.2);
        fogDensity *= saturate(-fogPosW.y / 3);
        fogDensity *= 0.5;

        fogDensity = 0.1;
        fogDensity = HeightFogDensity(fogPosW) * 0.04;

        float3 scattering = 0;

        float3 radiance = 1; // LightRadiance(gScene.directLight, fogPosW);
        #if 1
            float3 L = -gScene.directLight.direction;

            const float spread = 0.02; // todo:
            Ray shadowRay = CreateRay(fogPosW, normalize(L + RandomInUnitSphere(seed) * spread * 0));
            RayHit shadowHit = Trace(shadowRay); // todo: any hit
            if (shadowHit.tMax == INF) {
                radiance = gScene.directLight.color;
            } else {
                radiance = 0;
            }
            float miScattering = MiScattering(0.5, dot(V, L));
            radiance *= miScattering;
        #else
            radiance /= PI;
        #endif
        scattering += fogColor * radiance;

        // for(int i = 0; i < gScene.nLights; ++i) {
        //     float3 radiance = LightRadiance(gLights[i], fogPosW);
        //     // float3 radiance = gLights[i].color; // todo
        //     scattering += fogColor / PI * radiance;
        // }

        // float transmittance = exp(-stepLength * fogDensity);
        float transmittance = exp(-prevStepLength * fogDensity);
        scattering *= accTransmittance * (1 - transmittance);

        accScaterring += scattering;
        accTransmittance *= transmittance;

        // if (accTransmittance < 0.01) {
        //     accTransmittance = 0;
        //     break;
        // }

        fogPosW = fogPosW + -V * stepLength;
        prevStepLength = stepLength;
    }

    float4 color = gColorOut[id];

    color.xyz *= accTransmittance;
    color.xyz += accScaterring;

    // color.xyz = frac(fogPosW);

    gColorOut[id] = color;
}
