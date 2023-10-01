#ifndef COMMON_HEADER
#define COMMON_HEADER

float2 TexToNDC(float2 uv) {
   uv = uv * 2 - 1;
   uv.y *= -1;
   return uv;
}

float2 NDCToTex(float2 ndc) {
   ndc.y *= -1;
   return (ndc + 1) * 0.5;
}

// todo: does it work?
float LinearizeDepth(float depth, float4x4 cameraProjection) {
   // depth = A + B / z
   float A = cameraProjection[2][2];
   float B = cameraProjection[2][3];
   // todo: why minus?
   return -B / (depth - A);
}

float LinearizeDepth(float d, float zNear, float zFar) {
    return zNear * zFar / (zFar + d * (zNear - zFar));
}

float3 GetWorldPositionFromDepth(float2 uv, float depth, float4x4 invViewProjection) {
	float4 ndc = float4(TexToNDC(uv), depth, 1);
	float4 wp = mul(invViewProjection, ndc);
	return (wp / wp.w).xyz;
}

// todo:
float pow2(float v) {
   return v * v;
}

float Square(float v) {
   return v * v;
}

float lengthSq(float3 v) {
  return dot(v, v);
}

float Lerp(float a, float b, float t, bool doClamp = false) {
   if (doClamp) {
      t = saturate(t);
   }
   return a + (b - a) * t;
}

float InvLerp(float a, float b, float v, bool doClamp = false) {
   if (doClamp) {
      v = clamp(v, a, b);
   }
   return (v - a)  / (b - a);
}

float Remap(float a, float b, float c, float d, float v, bool doClamp = false) {
   float t = InvLerp(a, b, v, doClamp);
   return Lerp(c, d, t);
}

float max2(float2 v) {
  return max(v.x, v.y);
}

float max3(float3 v) {
  return max(max2(v.xy), v.z);
}

float max4(float4 v) {
  return max(max2(v.xy), max2(v.zw));
}

float max4(float v1, float v2, float v3, float v4) {
  return max(max(v1, v2), max(v3, v4));
}

float max4(float vs[4]) {
  return max(max(vs[0], vs[1]), max(vs[2], vs[3]));
}

float min4(float v1, float v2, float v3, float v4) {
  return max(min(v1, v2), min(v3, v4));
}

float SumComponents(float3 v) {
    return v.x + v.y + v.z;
}

float SumComponents(float4 v) {
  return v.x + v.y + v.z + v.w;
}

float SDot(float3 x, float3 y, float f = 1.0f) {
    return saturate(dot(x, y) * f);
}

float MaxDot(float3 x, float3 y, float m = 0) {
    return max(dot(x, y), 0);
}

float3 CentralPoint(float3 p0, float3 p1) {
   return (p0 + p1) / 2;
}
float3 Distance(float3 p0, float3 p1) {
   return length(p0 - p1);
}

float3 SafeNormalize(float3 v) {
    float len = length(v);
    if (len > 0) {
        return v / len;
    } else {
        return 0;
    }
}

float3 NormalFromTex(float3 normal) {
    // todo: dont know why, but it produce artifacts
    // return normal.xyz * 2 - 1;
    return SafeNormalize(normal.xyz * 2 - 1);
}

float PlaneDistance(float4 planeEq, float3 pos) {
   return dot(planeEq, float4(pos, 1));
}

bool FrustumSphereTest(float4 planes[6], float3 center, float radius) {
   for (int i = 0; i < 6; ++i) {
      if (PlaneDistance(planes[i], center) <= -radius) {
         return false;
      }
   }

   return true;
}

float2 DispatchUV(uint2 id, uint2 texSize, float2 pixelOffset = 0) {
    return (float2(id) + 0.5 + pixelOffset) / float2(texSize);
}

#endif
