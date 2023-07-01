#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "pbr.hlsli"
#include "sky.hlsli"

RWTexture2D<float4> gColor;

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

float3 RayColor(Ray ray, inout uint seed) {
    float3 color = 0;
    float3 energy = 1;

    for (int depth = 0; depth < gRTConstants.rayDepth; depth++) {
        RayHit hit = Trace(ray);
        if (hit.distance < INF) {
            // float3 albedo = hit.normal * 0.5f + 0.5f;
            // return hit.normal * 0.5f + 0.5f;
            SRTObject rtObject = gRtObjects[hit.materialID];

            float3 albedo = rtObject.baseColor;
            ray.origin = hit.position + hit.normal * 0.0001;

            float3 V = -ray.direction;
            float3 N = hit.normal;
            float3 F0 = 0.04; // todo:
            float3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);

            // if (0) {
            if (RandomFloat(seed) > F.x) {
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
                float3 specular = 0.5;
                specular = albedo;

                float roughness = 0.0;//rtObject.roughness;
                ray.direction = reflect(ray.direction, hit.normal + RandomInUnitSphere(seed) * roughness);

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

[numthreads(8, 8, 1)]
void rtCS (uint3 id : SV_DispatchThreadID) {
    uint seed = id.x * 214234 + id.y * 521334 + asuint(gRTConstants.random01); // todo: first attempt, dont thing about it

    // Get the dimensions of the RenderTexture
    uint width, height;
    gColor.GetDimensions(width, height);
    // Transform pixel to [-1,1] range
    // float2 uv = float2((id.xy + float2(0.5f, 0.5f)) / float2(width, height) * 2.0f - 1.0f);

    int nRays = gRTConstants.nRays;

    float3 color = 0;

    for (int i = 0; i < nRays; i++) {
        float2 offset = RandomFloat2(seed); // todo: halton sequence
        float2 uv = float2(id.xy + offset) / float2(width, height);

        Ray ray = CreateCameraRay(uv);
        color += RayColor(ray, seed);
    }

    color /= nRays;

    gColor[id.xy] = float4(color, 1);
}