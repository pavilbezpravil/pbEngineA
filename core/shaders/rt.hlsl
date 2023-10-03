#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"
#include "random.hlsli"
#include "intersection.hlsli"
#include "lighting.hlsli"

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

float3 DirectLightDirection() {
    float3 L = -gScene.directLight.direction;
    const float spread = 0.02; // todo:
    return normalize(L + RandomPointInUnitSphere() * spread);
}

#define USE_NEW_TRACING 1

#if USE_NEW_TRACING

struct TraceOpaqueDesc {
    Surface surface;
    float3 V;
    float viewZ;

    uint2 pixelPos;
    uint pathNum;
    uint bounceNum;
};

struct TraceOpaqueResult {
    float3 diffRadiance;
    float diffHitDist;

    float3 specRadiance;
    float specHitDist;
};

// Taken out from NRD
float GetSpecMagicCurve( float roughness ) {
    float f = 1.0 - exp2( -200.0 * roughness * roughness );
    f *= STL::Math::Pow01( roughness, 0.5 );
    return f;
}

float EstimateDiffuseProbability( float3 baseColor, float metalness, float roughness, float3 N, float3 V, bool useMagicBoost = false ) {
    float3 albedo, Rf0;
    ConvertBaseColorMetalnessToAlbedoRf0(baseColor, metalness, albedo, Rf0 );

    float NoV = abs( dot( N, V ) );
    float3 Fenv = STL::BRDF::EnvironmentTerm_Rtg( Rf0, NoV, roughness );

    float lumSpec = STL::Color::Luminance( Fenv );
    float lumDiff = STL::Color::Luminance( albedo * ( 1.0 - Fenv ) );

    float diffProb = lumDiff / ( lumDiff + lumSpec + 1e-6 );

    // Boost diffuse if roughness is high
    if ( useMagicBoost ) {
        diffProb = lerp( diffProb, 1.0, GetSpecMagicCurve( roughness ) );
    }

    return diffProb < 0.005 ? 0.0 : diffProb;
}

float EstimateDiffuseProbability( Surface surface, float3 V, bool useMagicBoost = false ) {
    return EstimateDiffuseProbability( surface.baseColor, surface.metalness, surface.roughness, surface.normalW, V, useMagicBoost );
}

TraceOpaqueResult TraceOpaque( TraceOpaqueDesc desc ) {
    TraceOpaqueResult result = ( TraceOpaqueResult )0;

    uint pathNum = desc.pathNum * 2;
    uint diffPathsNum = 0;

    [loop]
    for( uint iPath = 0; iPath < pathNum; iPath++ ) {
        Surface surface = desc.surface;
        float3 V = desc.V;

        float firstHitDist = 0;

        float3 Lsum = 0.0;
        float3 pathThroughput = 1.0;

        bool isDiffusePath = false;

        [loop]
        for( uint bounceIndex = 1; bounceIndex <= desc.bounceNum; bounceIndex++ ) {
            bool isDiffuse;
            float3 rayDir;

            // Origin point
            {
                float diffuseProbability = EstimateDiffuseProbability( surface, V );
                float rnd = RandomFloat();

                if (bounceIndex == 1) {
                    isDiffuse = iPath & 1;
                } else {
                    isDiffuse = rnd < diffuseProbability;
                    pathThroughput /= abs( float( !isDiffuse ) - diffuseProbability );
                    // isDiffuse = false;
                }

                // isDiffuse = rnd < diffuseProbability;
                // pathThroughput /= abs( float( !isDiffuse ) - diffuseProbability );

                if ( bounceIndex == 1 ) {
                    isDiffusePath = isDiffuse;
                }

                // Choose a ray
                if (0) {
                    float2 rnd = RandomFloat2();
                    
                    float3x3 mLocalBasis = STL::Geometry::GetBasis( surface.normalW );

                    if( isDiffuse ) {
                        rayDir = STL::ImportanceSampling::Cosine::GetRay( rnd );
                    } else {
                        float3 Vlocal = STL::Geometry::RotateVector( mLocalBasis, V );
                        float3 Hlocal = STL::ImportanceSampling::VNDF::GetRay( rnd, surface.roughness, Vlocal, 0.95 );
                        rayDir = reflect( -Vlocal, Hlocal ); // todo: may it point inside surface?
                    }

                    rayDir = STL::Geometry::RotateVectorInverse( mLocalBasis, rayDir );
                } else {
                    if( isDiffuse ) {
                        rayDir = RandomPointOnUnitHemisphere( surface.normalW );
                    } else {
                        float roughness2 = surface.roughness * surface.roughness;
                        rayDir = reflect( -V, surface.normalW );
                        rayDir = normalize( rayDir + RandomPointInUnitSphere() * roughness2 );
                    }
                }

                // todo:
                rayDir = rayDir * sign(dot( rayDir, surface.normalW ));

                float3 albedo, Rf0;
                ConvertBaseColorMetalnessToAlbedoRf0( surface, albedo, Rf0 );

                float3 H = normalize( V + rayDir );
                float VoH = abs( dot( V, H ) );
                float NoL = saturate( dot( surface.normalW, rayDir ) );

                if( isDiffuse ) {
                    float NoV = abs( dot( surface.normalW, V ) );
                    pathThroughput *= STL::Math::Pi( 1.0 ) * albedo * STL::BRDF::DiffuseTerm_Burley( surface.roughness, NoL, NoV, VoH );
                } else {
                    float3 F = STL::BRDF::FresnelTerm_Schlick( Rf0, VoH );
                    pathThroughput *= F;

                    // See paragraph "Usage in Monte Carlo renderer" from http://jcgt.org/published/0007/04/01/paper.pdf
                    pathThroughput *= STL::BRDF::GeometryTerm_Smith( surface.roughness, NoL );
                }

                // pathThroughput *= albedo;

                const float THROUGHPUT_THRESHOLD = 0.001;
                if( THROUGHPUT_THRESHOLD != 0.0 && STL::Color::Luminance( pathThroughput ) < THROUGHPUT_THRESHOLD )
                    break;
            }

            // Trace to the next hit
            Ray ray = CreateRay(surface.posW, rayDir);
            RayHit hit = Trace(ray);

            // Hit point
            if (bounceIndex == 1) {
                firstHitDist = hit.tMax;
            }

            if (hit.tMax < INF) {
                SRTObject obj = gRtObjects[hit.objID];

                surface.posW = hit.position;
                surface.normalW = hit.normal;
                surface.baseColor = obj.baseColor;
                surface.metalness = obj.metallic;
                surface.roughness = obj.roughness;
                
                V = -ray.direction;

                float3 L = obj.baseColor * obj.emissivePower;

                // Shadow test ray
                float3 toLight = DirectLightDirection();
                float NDotL = dot(surface.normalW, toLight);

                if (NDotL > 0) {
                    // todo:
                    Ray shadowRay = CreateRay(surface.posW, toLight);
                    RayHit shadowHit = Trace(shadowRay);
                    if (shadowHit.tMax == INF) {
                        // todo:
                        float3 albedo, Rf0;
                        ConvertBaseColorMetalnessToAlbedoRf0( surface, albedo, Rf0 );

                        float3 directLightShade = NDotL * albedo * gScene.directLight.color;
                        L += directLightShade;
                    }
                }

                // L += Shade(surface, V);
                Lsum += L * pathThroughput;
            } else {
                float3 L = GetSkyColor(ray.direction);
                Lsum += L * pathThroughput;
                break;
            }
        }

        // todo: add ambient light
        float normHitDist = REBLUR_FrontEnd_GetNormHitDist( firstHitDist, desc.viewZ, gRTConstants.nrdHitDistParams, isDiffusePath ? 1.0 : desc.surface.roughness );

        if( isDiffusePath ) {
            result.diffRadiance += Lsum;
            result.diffHitDist += normHitDist;
            diffPathsNum++;
        } else {
            result.specRadiance += Lsum;
            result.specHitDist += normHitDist;
        }
    }

    // Material de-modulation ( convert irradiance into radiance )
    float3 albedo, Rf0;
    ConvertBaseColorMetalnessToAlbedoRf0( desc.surface.baseColor, desc.surface.metalness, albedo, Rf0 );

    float NoV = abs( dot( desc.surface.normalW, desc.V ) );
    float3 Fenv = STL::BRDF::EnvironmentTerm_Rtg( Rf0, NoV, desc.surface.roughness );
    float3 diffDemod = ( 1.0 - Fenv ) * albedo * 0.99 + 0.01;
    float3 specDemod = Fenv * 0.99 + 0.01;

    result.diffRadiance /= diffDemod;
    result.specRadiance /= specDemod;

    // Radiance is already divided by sampling probability, we need to average across all paths
    float radianceNorm = 1.0 / float( desc.pathNum );
    result.diffRadiance *= radianceNorm;
    result.specRadiance *= radianceNorm;

    // Others are not divided by sampling probability, we need to average across diffuse / specular only paths
    float diffNorm = diffPathsNum == 0 ? 0.0 : 1.0 / float( diffPathsNum );
    result.diffHitDist *= diffNorm;

    float specNorm = pathNum == diffPathsNum ? 0.0 : 1.0 / float( pathNum - diffPathsNum );
    result.specHitDist *= specNorm;

    return result;
}

#endif

float3 RayColor(Ray ray, int nRayDepth, inout float hitDistance) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < nRayDepth; depth++) {
        RayHit hit = Trace(ray);

        if (depth == 0) {
            // todo: mb add less
            hitDistance += hit.tMax;
        }

        if (hit.tMax < INF) {
            SRTObject obj = gRtObjects[hit.objID];

            ray.origin = hit.position;

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
                float3 L = DirectLightDirection();
                float NDotL = dot(N, L);

                if (NDotL > 0) {
                    Ray shadowRay = CreateRay(ray.origin, L);
                    RayHit shadowHit = Trace(shadowRay);
                    if (shadowHit.tMax == INF) {
                        float3 directLightShade = NDotL * albedo * gScene.directLight.color;
                        color += directLightShade * energy;
                    }
                }

                ray.direction = normalize(RandomPointOnUnitHemisphere(hit.normal));

                float NDotRayDir = max(dot(N, ray.direction), 0);
                energy *= albedo * NDotRayDir;
            } else {
                // todo:
                // float3 specular = obj.baseColor;
                float3 specular = F;

                float roughness = obj.roughness * obj.roughness; // todo:
                ray.direction = reflect(ray.direction, hit.normal);
                ray.direction = normalize(ray.direction + RandomPointInUnitSphere() * roughness);

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

// todo:
void RTRandomInitSeed(uint2 id) {
    RandomInitSeedFromUint2(id, gScene.iFrame);
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

    float2 uv = DispatchUV(id, gRTConstants.rtSize);
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

    RTRandomInitSeed(id);

    int nRays = gRTConstants.nRays;

    float3 color = 0;

    for (int i = 0; i < nRays; i++) {
        float2 offset = RandomFloat2() - 0.5; // todo: halton sequence
        offset = 0;
        float2 uv = DispatchUV(id, gRTConstants.rtSize, offset);

        Ray ray = CreateCameraRay(uv);
        float firstHitDistance;
        color += RayColor(ray, gRTConstants.rayDepth, firstHitDistance);
    }

    color /= nRays;

    gColorOut[id.xy] = float4(color, 1);
}

Texture2D<float4> gBaseColor;

RWTexture2D<float4> gDiffuseOut;
RWTexture2D<float4> gSpecularOut;
RWTexture2D<float4> gDirectLightingOut;

#if defined(USE_PSR)
    RWTexture2D<float> gViewZOut;
    RWTexture2D<float4> gNormalOut;
    RWTexture2D<float4> gBaseColorOut;

    bool CheckSurfaceForPSR(float roughness, float metallic) {
        // float3 normal, float3 V
        return roughness < 0.01 && metallic > 0.95;
    }

    float RTDiffuseSpecularCS_ReadViewZ(uint2 pixelPos) {
        return gViewZOut[pixelPos];
    }

    float4 RTDiffuseSpecularCS_ReadBaseColorMetalness(uint2 pixelPos) {
        return gBaseColorOut[pixelPos];
    }

    float4 RTDiffuseSpecularCS_ReadNormal(uint2 pixelPos) {
        return gNormalOut[pixelPos];
    }
#else
    float RTDiffuseSpecularCS_ReadViewZ(uint2 pixelPos) {
        return gViewZ[pixelPos];
    }

    float4 RTDiffuseSpecularCS_ReadBaseColorMetalness(uint2 pixelPos) {
        return gBaseColor[pixelPos];
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

    float2 uv = DispatchUV(id, gRTConstants.rtSize);
    float3 viewPos = ReconstructViewPosition( uv, GetCamera().frustumSize, viewZ );
    float3 posW = ReconstructWorldPosition(viewPos);

    float3 V = normalize(gCamera.position - posW);

    float4 normalRoughnessNRD = RTDiffuseSpecularCS_ReadNormal(id);
    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(normalRoughnessNRD);
    float3 normal = normalRoughness.xyz;
    float roughness = normalRoughness.w;

    float4 baseColorMetallic = RTDiffuseSpecularCS_ReadBaseColorMetalness(id);
    float3 baseColor = baseColorMetallic.xyz;
    float  metallic = baseColorMetallic.w;

    RTRandomInitSeed(id);

    #if defined(USE_PSR)
        float3 psrEnergy = 1;

        // todo: better condition
        if (CheckSurfaceForPSR(roughness, metallic)) {
            Ray ray;
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

    float3 albedo, Rf0;
    ConvertBaseColorMetalnessToAlbedoRf0( baseColor, metallic, albedo, Rf0 );

    Surface surface;
    surface.posW = posW;
    surface.normalW = normal;
    surface.baseColor = baseColor;
    surface.metalness = metallic;
    surface.roughness = roughness;

#if USE_NEW_TRACING
    TraceOpaqueDesc opaqueDesc;

    opaqueDesc.surface = surface;
    opaqueDesc.V = V;
    opaqueDesc.viewZ = viewZ;

    opaqueDesc.pixelPos = id;
    opaqueDesc.pathNum = gRTConstants.nRays;
    opaqueDesc.bounceNum = gRTConstants.rayDepth;

    TraceOpaqueResult opaqueResult = TraceOpaque( opaqueDesc );

    gDiffuseOut[id] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(opaqueResult.diffRadiance, opaqueResult.diffHitDist);
    gSpecularOut[id] = REBLUR_FrontEnd_PackRadianceAndNormHitDist(opaqueResult.specRadiance, opaqueResult.specHitDist);
#else
    Ray ray;
    ray.origin = posW;

    // todo:
    int nRays = gRTConstants.nRays;

    float diffuseHitDistance = 0;
    float specularHitDistance = 0;

    float3 diffuseRadiance = 0;
    float3 specularRadiance = 0;

    for (int i = 0; i < nRays; i++) {
        ray.direction = reflect(-V, normal);
        ray.direction = normalize(ray.direction + RandomPointInUnitSphere() * roughness * roughness);

        specularRadiance += RayColor(ray, gRTConstants.rayDepth, specularHitDistance);
    }

    for (int i = 0; i < nRays; i++) {
        ray.direction = normalize(RandomPointOnUnitHemisphere(normal));

        float Diffuse_NDotL = max(dot(normal, ray.direction), 0);
        diffuseRadiance += RayColor(ray, gRTConstants.rayDepth, diffuseHitDistance) * Diffuse_NDotL;
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
#endif

    // todo:
    #if 1
    {
        float3 directLighting = 0;

        float3 L = DirectLightDirection();
        float NDotL = dot(normal, L);

        if (NDotL > 0) {
            Ray shadowRay = CreateRay(surface.posW, L);
            RayHit shadowHit = Trace(shadowRay); // todo: any hit
            if (shadowHit.tMax == INF) {
                directLighting += LightShadeLo(surface, V, gScene.directLight.color, -gScene.directLight.direction);
            }
        }

        gDirectLightingOut[id] = float4(directLighting, 1);
    }
    #endif
}

Texture2D<float4> gDiffuse;
Texture2D<float4> gSpecular;
Texture2D<float4> gDirectLighting;

[numthreads(8, 8, 1)]
void RTCombineCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }
    
    float viewZ = gViewZ[id];

    float2 uv = DispatchUV(id, gRTConstants.rtSize);
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

#if USE_NEW_TRACING
    float3 albedo, Rf0;
    ConvertBaseColorMetalnessToAlbedoRf0( baseColor, metallic, albedo, Rf0 );

    float NoV = abs( dot( normal, V ) );
    float3 Fenv = STL::BRDF::EnvironmentTerm_Rtg( Rf0, NoV, roughness );

    float3 diffDemod = ( 1.0 - Fenv ) * albedo * 0.99 + 0.01;
    float3 specDemod = Fenv * 0.99 + 0.01;

    float3 Ldiff = diffuse * diffDemod;
    float3 Lspec = specular * specDemod;

    float3 directLighting = gDirectLighting[id].xyz;

    float3 color = Ldiff + Lspec + directLighting;
#else
    float3 F0 = lerp(0.04, baseColor, metallic);
    float3 F = fresnelSchlick(max(dot(normal, V), 0.0), F0);

    float3 directLighting = gDirectLighting[id].xyz;
    // todo:
    float3 albedo = baseColor * (1 - metallic);
    float3 color = (1 - F) * albedo * diffuse / PI + F * specular + directLighting;
#endif

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

    RTRandomInitSeed(id);
    
    float viewZ = gViewZ[id];

    float2 uv = DispatchUV(id, gRTConstants.rtSize);
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

    float initialOffset = RandomFloat() * stepLength;
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

            Ray shadowRay = CreateRay(fogPosW, DirectLightDirection());
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
