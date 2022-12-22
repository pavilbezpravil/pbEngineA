#include "commonResources.hlsli"
#include "common.hlsli"
#include "pbr.hlsli"
#include "tonemaping.hlsli"
#include "noise.inl"
#include "sky.hlsli"

cbuffer gWaterCB {
  SWaterCB gWater;
}

struct VsOut {
   float3 posW : POS_W;
   float3 normalW : NORMAL_W;
   float4 posH : SV_POSITION;
   uint instanceID : SV_InstanceID;
};

VsOut waterVS(uint instanceID : SV_InstanceID, uint vertexID : SV_VertexID) {
   VsOut output = (VsOut)0;

   float halfSize = gWater.waterPatchSize / 2;
   float height = gWater.planeHeight;

   float3 corners[] = {
      float3(-halfSize, height, halfSize),
      float3(halfSize, height, halfSize),
      float3(-halfSize, height, -halfSize),
      float3(halfSize, height, -halfSize),
   };

   float3 posW = corners[vertexID];

   float2 localIdx = float2(instanceID % gWater.waterPatchCount, instanceID / gWater.waterPatchCount)
                   - float(gWater.waterPatchCount) / 2;
   // localIdx *= 1.1;
   posW.xz += localIdx * halfSize * 2;

   output.normalW = float3(0, 1, 0);
   output.posW = posW;
   output.posH = 0;
   output.instanceID = instanceID;

   return output;
}

struct ConstantOutputType {
   float edges[4] : SV_TessFactor;
   float inside[2] : SV_InsideTessFactor;
};

struct HullOutputType {
   float3 posW : POS_W;
};

float TessFactor(float3 p) {
   float3 cameraPos = gCamera.position;
   float s = gWater.waterTessFactor * gWater.waterPatchSize;

   return s / length(p - cameraPos);
}

float TessFactor(float3 p0, float3 p1) {
   return TessFactor(CentralPoint(p0, p1));
}

////////////////////////////////////////////////////////////////////////////////
// Patch Constant Function
////////////////////////////////////////////////////////////////////////////////
ConstantOutputType WaterPatchConstantFunction(InputPatch<VsOut, 4> patch, uint patchId : SV_PrimitiveID) {    
   ConstantOutputType output;

   output.edges[0] = TessFactor(patch[2].posW, patch[0].posW);
   output.edges[1] = TessFactor(patch[0].posW, patch[1].posW);
   output.edges[2] = TessFactor(patch[1].posW, patch[3].posW);
   output.edges[3] = TessFactor(patch[3].posW, patch[2].posW);

   output.inside[0] = output.inside[1] = max4(output.edges);

   return output;
}

////////////////////////////////////////////////////////////////////////////////
// Hull Shader
////////////////////////////////////////////////////////////////////////////////
[domain("quad")]
// [partitioning("integer")]
// [partitioning("fractional_even")]
[partitioning("fractional_odd")]
// [partitioning("pow2")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("WaterPatchConstantFunction")]
HullOutputType waterHS(InputPatch<VsOut, 4> patch, uint pointId : SV_OutputControlPointID, uint patchId : SV_PrimitiveID) {
   HullOutputType output;

   output.posW = patch[pointId].posW;

   return output;
}

struct PixelInputType {
    float3 posW : POS_W;
    float3 normalW : NORMAL_W;
    float4 posH : SV_POSITION;
};

StructuredBuffer<WaveData> gWaves;

float SphericalMask(float3 p, float3 center, float radius, float fade) {
   float dist = length(p - center);
   return 1 - saturate((dist - radius) / fade);
   // return Remap(radius, radius + fade, 1, 0, dist, true);
}

void GertsnerWave(WaveData wave, float3 posW, inout float3 displacement, inout float3 tangent, inout float3 binormal) {
   float2 d = wave.direction;
   float theta = wave.magnitude * dot(d, posW.xz) - wave.frequency * gScene.animationTime + wave.phase;

   float2 sin_cos;
   sincos(theta, sin_cos.x, sin_cos.y);

   float3 offset = wave.amplitude;
   offset.xz *= d * wave.steepness;

   displacement += offset * float3(sin_cos.y, sin_cos.x, sin_cos.y);

   float3 derivative = wave.magnitude * offset * float3(-sin_cos.x, sin_cos.y, -sin_cos.x);
   tangent += derivative * d.x;
   binormal += derivative * d.y;
}

void WaveParamFromWavelength(inout WaveData wave, float wavelength, float amplitude, float2 direction, float2 directionOffset) {
   wave.magnitude = 2 * PI / wavelength;
   wave.frequency = sqrt(9.8 * wave.magnitude);
   wave.amplitude = amplitude;
   // wave.direction = normalize(direction + directionOffset);
}

// todo:
#define FLOWMAP

////////////////////////////////////////////////////////////////////////////////
// Domain Shader
////////////////////////////////////////////////////////////////////////////////
#define WATER_PATCH_BC(patch, param, bc) (lerp(lerp(patch[0].param, patch[1].param, bc.x), lerp(patch[2].param, patch[3].param, bc.x), bc.y))
// #define WATER_PATCH_BC(patch, param, bc) (bc.x * patch[0].param + bc.y * patch[1].param + bc.z * patch[2].param)
[domain("quad")]
PixelInputType waterDS(ConstantOutputType input, float2 bc : SV_DomainLocation, const OutputPatch<HullOutputType, 4> patch) {
   PixelInputType output;

   float3 posW = WATER_PATCH_BC(patch, posW, bc);

   const float polySize = gWater.waterPatchSize * gWater.waterPatchSizeAAScale / TessFactor(posW);

   float3 displacement = 0;

   float3 tangent = float3(1, 0, 0);
   float3 binormal = float3(0, 0, 1);

   float3 gertsnerWavePosW = posW;
   #ifdef FLOWMAP
      const float2 flow = float2(3, 0);
      gertsnerWavePosW.xz -= flow * gScene.animationTime;
   #endif

   for (int iWave = 0; iWave < gWater.nWaves; ++iWave) {
      WaveData wave = gWaves[iWave];

      float antialiasingFade = 1.0f - saturate(polySize * wave.magnitude - 1.0f);
      if (antialiasingFade <= 0) {
         break;
      }
      wave.amplitude *= antialiasingFade;

      wave.amplitude *= gWater.waterWaveScale;
      GertsnerWave(wave, gertsnerWavePosW, displacement, tangent, binormal);
   }

   float3 center = 0;
   float waveMask = SphericalMask(posW, center, 20, 20) * 0; // todo:
   if (waveMask > 0) {
      #if 1
         float2 noisePos = posW.xz * 0.05;
         float2 flow = float2(noise(noisePos), noise(noisePos + float2(102, 278))) * 2 - 1;
         float A = length(flow);
         float2 direction = normalize(flow);
      #else
         float2 direction = normalize(posW.xz - center.xz);
         float A = 1;
      #endif

      WaveData wave = (WaveData)0;
      wave.steepness = 1;
      wave.direction = direction;

      WaveParamFromWavelength(wave, 5, 0.4 * A * waveMask, direction, rand1dTo2d(11));
      GertsnerWave(wave, posW, displacement, tangent, binormal);

      // WaveParamFromWavelength(wave, 2, 0.1 * waveMask, direction, rand1dTo2d(12));
      // GertsnerWave(wave, posW, displacement, tangent, binormal);

      // WaveParamFromWavelength(wave, 4, 0.15 * waveMask, direction, rand1dTo2d(13));
      // GertsnerWave(wave, posW, displacement, tangent, binormal);

      // WaveParamFromWavelength(wave, 8, 0.15 * waveMask, direction, rand1dTo2d(14));
      // GertsnerWave(wave, posW, displacement, tangent, binormal);

      // WaveParamFromWavelength(wave, 16, 0.2 * waveMask, direction, rand1dTo2d(15));
      // GertsnerWave(wave, posW, displacement, tangent, binormal);
   }

   posW += displacement;

   output.posW = posW;
   output.normalW = normalize(cross(binormal, tangent));
   output.posH = mul(float4(posW, 1), gCamera.viewProjection);

   return output;
}

struct PsOut {
   float4 color : SV_Target0;
};

Texture2D<float> gDepth;
Texture2D<float3> gRefraction;

float WaterFresnel(float dotNV) {
	float ior = 1.33f;
	float c = abs(dotNV);
	float g = sqrt(ior * ior + c * c - 1);
	float fresnel = 0.5 * pow2(g - c) / pow2(g + c) * (1 + pow2(c * (g + c) - 1) / pow2(c * (g - c) + 1));
	return saturate(fresnel);
}

float3 PixelNormal(float3 posW) {
   return normalize(cross(ddx(posW), ddy(posW)));
}

PsOut waterPS(PixelInputType input) : SV_TARGET {
   float2 screenUV = input.posH.xy / gCamera.rtSize;

   float3 posW = input.posW;
   float3 normalW = normalize(input.normalW);
   if (gWater.waterPixelNormals > 0) {
      normalW = PixelNormal(input.posW);
   }

   float3 toCamera = gCamera.position - posW;
   float3 V = normalize(toCamera);

   float3 reflectionDirection = reflect(-V, normalW);
   float3 reflectionColor = GetSkyColor(reflectionDirection);

   float3 refractionColor = gRefraction.Sample(gSamplerLinear, screenUV);
   
   float waterDepth = length(toCamera);

   float3 scenePosW = GetWorldPositionFromDepth(screenUV, gDepth.Sample(gSamplerPoint, screenUV), gCamera.invViewProjection);
   float sceneDepth = length(gCamera.position - scenePosW);

   float underwaterLength = sceneDepth - waterDepth;

   float3 fogColor = gWater.fogColor;
   float fogExp = 1 - exp(-underwaterLength / gWater.fogUnderwaterLength);
   float3 underwaterColor = lerp(refractionColor, fogColor, fogExp);

   // float fresnel = fresnelSchlick(max(dot(normalW, V), 0.0), 0.04).x; // todo:
   float fresnel = WaterFresnel(dot(normalW, V)); // todo: so few reflection on high angles
   // fresnel = 1;
   float3 color = lerp(underwaterColor, reflectionColor, fresnel);

   float softZ = exp(-underwaterLength / gWater.softZ);
   color = lerp(color, refractionColor, softZ);

   // color = normalW;
   // color = rand2dTo1d(posW.xz);
   // color = noise(posW.xz);
   // float2 noisePos = posW.xz * 0.2;
   // float2 noise2 = float2(noise(noisePos), noise(noisePos + float2(102, 278))) * 2 - 1;
   // // noise2 = normalize(noise2);
   // float A = length(noise2);
   // color = float3(noise2, 0);

   PsOut output = (PsOut)0;
   output.color.rgb = color;
   output.color.a = 1;

   return output;
}
