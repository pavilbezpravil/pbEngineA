#include "shared/rt.hlsli"
#include "shared/common.hlsli"
#include "common.inl"
#include "noise.inl"
#include "tonemaping.hlsli"

RWTexture2D<float4> gColor;

cbuffer gCameraCB {
  SCameraCB gCamera;
}

cbuffer gRTConstantsCB {
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

// todo: copy paste from ssao.cs
float3 GetWorldPositionFromDepth(float2 uv, float depth ) {
	float4 ndc = float4(TexToNDC(uv), depth, 1);
	float4 wp = mul(ndc, gCamera.invViewProjection);
	return (wp / wp.w).xyz;
}

Ray CreateCameraRay(float2 uv) {
    float3 origin = gCamera.position;
    // float3 posW = mul(float4(uv, 1, 1), gCamera.invViewProjection).xyz;
    float3 posW = GetWorldPositionFromDepth(uv, 1);
    float3 direction = normalize(posW - origin);
    return CreateRay(origin, direction);
}

#define INF 1000000

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

float3 Shade(inout Ray ray, RayHit hit) {
    if (hit.distance < INF) {
        // Return the normal
        return hit.normal * 0.5f + 0.5f;
    } else {
        // // Sample the skybox and write it
        // float theta = acos(ray.direction.y) / -PI;
        // float phi = atan2(ray.direction.x, -ray.direction.z) / -PI * 0.5f;
        // return _SkyboxTexture.SampleLevel(sampler_SkyboxTexture, float2(phi, theta), 0).xyz;
        float t = 0.5 * (ray.direction.y + 1);
        return lerp(1, float3(0.5, 0.7, 1.0), t);
    }
}

float3 RandomInUnitSphere(float randValue) {
    for (int i = 0; i < 100; i++) {
        float3 p = rand1dTo3d(randValue + i) * 2 - 1;
        if (length(p) < 1) {
            return p;
        }
    }
    return 0;
}

float3 RandomInHemisphere(float3 normal, float randValue) {
    float3 p = RandomInUnitSphere(randValue);
    return p * sign(dot(p, normal));
}

float SumComponents(float3 v) {
    return v.x + v.y + v.z;
}

float3 RayColor(Ray ray) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < 6; depth++) {
        RayHit hit = Trace(ray);
        if (hit.distance < INF) {
            // float3 albedo = hit.normal * 0.5f + 0.5f;
            // return hit.normal * 0.5f + 0.5f;
            SRTObject rtObject = gRtObjects[hit.materialID];

            float3 albedo = rtObject.albedo;

            ray.origin = hit.position + hit.normal * 0.0001;
            if (1) {

                // Shadow test ray
                bool shadow = false;
                float3 L = normalize(float3(0.2, 1, -0.5));
                Ray shadowRay = CreateRay(ray.origin, L);
                RayHit shadowHit = Trace(shadowRay);
                if (shadowHit.distance == INF) {
                    float NdotL = dot(hit.normal, L);
                    float3 directLightShade = max(NdotL, 0) * albedo * 1;
                    color += directLightShade * energy;
                }

                // ray.direction = RandomInHemisphere(hit.normal, SumComponents(ray.direction) * 1000 + gRTConstants.random01 * 0);
                ray.direction = RandomInHemisphere(hit.normal, frac(ray.direction * 1000) * 1000 + gRTConstants.random01 * 0);

                energy *= albedo;
            } else {
                float3 specular = 0.5;
                specular = albedo;

                ray.direction = reflect(ray.direction, hit.normal);

                energy *= specular;
            }
        } else {
            float t = 0.5 * (ray.direction.y + 1);
            float3 skyColor = lerp(1, float3(0.5, 0.7, 1.0), t);
            skyColor = 0;

            color += skyColor * energy;
            break;
        }
    }

    return color;
}

[numthreads(8, 8, 1)]
void rtCS (uint3 id : SV_DispatchThreadID) {
    // Get the dimensions of the RenderTexture
    uint width, height;
    gColor.GetDimensions(width, height);
    // Transform pixel to [-1,1] range
    // float2 uv = float2((id.xy + float2(0.5f, 0.5f)) / float2(width, height) * 2.0f - 1.0f);

    int nRays = gRTConstants.nRays;
    nRays = 25;

    float3 color = 0;

    for (int i = 0; i < nRays; i++) {
        float2 offset = rand1dTo2d(id.xy + i + gRTConstants.random01);
        float2 uv = float2(id.xy + offset) / float2(width, height);

        Ray ray = CreateCameraRay(uv);

        color += RayColor(ray);
    }

    color /= nRays;

    color = ACESFilm(color);
    color = GammaCorrection(color);

    gColor[id.xy] = float4(color, 1);
}