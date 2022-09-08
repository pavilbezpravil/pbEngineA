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
	float4 wp = mul(ndc, invViewProjection);
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

float min4(float v1, float v2, float v3, float v4) {
  return max(min(v1, v2), min(v3, v4));
}

float ComponentSum(float4 v) {
  return v.x + v.y + v.z + v.w;
}


// float3 GetSkyColorSimple(float3 direction) {
//    float t = 0.5 * (direction.y + 1);
//    float3 skyColor = lerp(1, float3(0.5, 0.7, 1.0), t);
//    return skyColor;
// }

// todo: move
// https://www.shadertoy.com/view/lt2SR1
float3 GetSkyColor(float3 rd ) {
    float3 sundir = normalize( float3(.0, .1, 1.) );
    
    float yd = min(rd.y, 0.);
    rd.y = max(rd.y, 0.);
    
    float3 col = 0;
    
    col += float3(.4, .4 - exp( -rd.y*20. )*.15, .0) * exp(-rd.y*9.); // Red / Green 
    col += float3(.3, .5, .6) * (1. - exp(-rd.y*8.) ) * exp(-rd.y*.9) ; // Blue
    
    col = lerp(col * 1.2, 0.3,  1 - exp(yd*100.)); // Fog
    
    col += float3(1.0, .8, .55) * pow( max(dot(rd,sundir),0.), 15. ) * .6; // Sun
    col += pow(max(dot(rd, sundir),0.), 150.0) *.15;
    
    return col;
}
