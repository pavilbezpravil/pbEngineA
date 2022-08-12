#include "shared/rt.hlsli"
#include "shared/common.hlsli"
#include "common.inl"

RWTexture2D<float3> gColor;

cbuffer gCameraCB {
  SCameraCB gCamera;
}

cbuffer gRTConstantsCB {
  SRTConstants gRTConstants;
}

StructuredBuffer<SRTObject> gRtObjects;

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
};

RayHit CreateRayHit() {
    RayHit hit;
    hit.position = float3(0.0f, 0.0f, 0.0f);
    hit.distance = INF;
    hit.normal = float3(0.0f, 0.0f, 0.0f);
    return hit;
}

void IntersectGroundPlane(Ray ray, inout RayHit bestHit) {
    // Calculate distance along the ray where the ground plane is intersected
    float t = -ray.origin.y / ray.direction.y;
    if (t > 0 && t < bestHit.distance) {
        bestHit.distance = t;
        bestHit.position = ray.origin + t * ray.direction;
        bestHit.normal = float3(0.0f, 1.0f, 0.0f);
    }
}

void IntersectSphere(Ray ray, inout RayHit bestHit, float4 sphere) {
    // Calculate distance along the ray where the sphere is intersected
    float3 d = ray.origin - sphere.xyz;
    float p1 = -dot(ray.direction, d);
    float p2sqr = p1 * p1 - dot(d, d) + sphere.w * sphere.w;
    if (p2sqr < 0) {
        return;
    }

    float p2 = sqrt(p2sqr);
    float t = p1 - p2 > 0 ? p1 - p2 : p1 + p2;
    if (t > 0 && t < bestHit.distance) {
        bestHit.distance = t;
        bestHit.position = ray.origin + t * ray.direction;
        bestHit.normal = normalize(bestHit.position - sphere.xyz);
    }
}

RayHit Trace(Ray ray) {
    RayHit bestHit = CreateRayHit();
    IntersectGroundPlane(ray, bestHit);

    // IntersectSphere(ray, bestHit, float4(0, 3.0f, 0, 1.0f));
    // IntersectSphere(ray, bestHit, float4(1, 3.0f, 0, 1.0f));
    // IntersectSphere(ray, bestHit, float4(-2, 3.0f, 0, 1.0f));

    for (int iObject = 0; iObject < gRTConstants.nObjects; iObject++) {
        SRTObject rtObject = gRtObjects[iObject];
        IntersectSphere(ray, bestHit, float4(rtObject.position, 1.0f));
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

[numthreads(8, 8, 1)]
void rtCS (uint3 id : SV_DispatchThreadID) {
    // Get the dimensions of the RenderTexture
    uint width, height;
    gColor.GetDimensions(width, height);
    // Transform pixel to [-1,1] range
    // float2 uv = float2((id.xy + float2(0.5f, 0.5f)) / float2(width, height) * 2.0f - 1.0f);
    float2 uv = float2(id.xy) / float2(width, height);
    // Get a ray for the UVs
    Ray ray = CreateCameraRay(uv);

    // Trace and shade
    RayHit hit = Trace(ray);
    float3 result = Shade(ray, hit);
    gColor[id.xy] = result;
    // gColor[id.xy] = pow(float4(result, 1), 1 / 2.2);
    // gColor[id.xy] = float4(ray.direction * 0.5f + 0.5f, 1.0f);
    // gColor[id.xy] = float4(uv, 0, 1);
}