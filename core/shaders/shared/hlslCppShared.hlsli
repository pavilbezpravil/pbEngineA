#ifndef SHARED_COMMON
#define SHARED_COMMON

#define GLUE(a,b) a##b
#define DECLARE_REGISTER(prefix, regNum) register(GLUE(prefix,regNum))

#define SAMPLER_SLOT_WRAP_POINT 0
#define SAMPLER_SLOT_WRAP_LINEAR 1
#define SAMPLER_SLOT_SHADOW 2

#define CB_SLOT_SCENE 10
#define CB_SLOT_CAMERA 11
#define CB_SLOT_CULL_CAMERA 12
#define CB_SLOT_EDITOR 13

#define SRV_SLOT_LIGHTS 64
#define SRV_SLOT_SHADOWMAP 65
// #define SRV_SLOT_UNDER_CURSOR_BUFFER 66

struct SMaterial {
   float3 albedo;
   float roughness;

   float metallic;
   float3 _dymmy;
};

struct SInstance {
   float4x4 transform;
   SMaterial material;

   uint entityID;
   float3 _sdfsdg;
};

struct SDecal {
   float4x4 viewProjection;
   float4 albedo;

   float metallic;
   float roughness;
   float2 _sdfdsf;
};

#define SLIGHT_TYPE_DIRECT (1)
#define SLIGHT_TYPE_POINT (2)

struct SLight {
   float3 position;
   int type;

   float3 direction;
   float _dymmy;

   float3 color;
   float radius;

  // todo: dont work
   float3 DirectionToLight(float3 posW) { // L
      if (type == SLIGHT_TYPE_DIRECT) {
         return -direction;
      } else if (type == SLIGHT_TYPE_POINT) {
         float3 L = normalize(position - posW);
         return L;
      }
      return float3(0, 0, 0);
   }
};

struct SSceneCB {
   int iFrame;
   float time;
   float animationTime;
   float deltaTime;

   int nLights;
   int nDecals;
   float2 _sdfasdf;

   SLight directLight;
   float4x4 toShadowSpace;

   int _asdf3221;
   int fogNSteps;
   float exposition;
   float _sdfsdg;
};

struct SWaterCB {
   // todo:
   float waterTessFactor;
   float waterPatchSize;
   float waterPatchSizeAAScale;
   int waterPatchCount;

   int waterPixelNormals;
   float waterWaveScale;
   int nWaves;
   float planeHeight;

   float3 fogColor;
   float fogUnderwaterLength;

   float softZ;
   uint entityID;
   float2 _asfa2f23fd;
};

struct STerrainCB {
   // todo:
   float waterTessFactor;
   float waterPatchSize;
   float _wef23c;
   int waterPatchCount;

   int waterPixelNormals;
   float waterWaveScale;
   int _asdf23ff;
   float _sdf2ccc;

   float3 center;
   uint entityID;

   float3 color;
   float _asdfsdf;
};

struct SCameraCB {
   float4x4 projection;
   float4x4 view;
   float4x4 viewProjection;
   float4x4 invViewProjection;

   float3 position;
   int _sdfsdf2;

   float zNear;
   float zFar;
   float2 _sdf4g34g;

   int2 rtSize;
   float2 _dymmy3;

   float4 frustumPlanes[6];
};

struct SDrawCallCB {
   SInstance instance;

   int instanceStart;
   float3 _dymmy3;
};

struct SEditorCB {
   int2 cursorPixelIdx;
   int underCursorBufferSize;
   float _sdf3ff;
};

struct WaveData {
   float2 direction;
	float amplitude;
	float length;

	float magnitude;
	float frequency;
	float phase;
	float steepness;
};

#endif // SHARED_COMMON
