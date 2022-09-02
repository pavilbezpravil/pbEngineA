float2 TexToNDC(float2 uv) {
   uv = uv * 2 - 1;
   uv.y *= -1;
   return uv;
}

float2 NDCToTex(float2 ndc) {
   ndc.y *= -1;
   return (ndc + 1) * 0.5;
}

float3 GetWorldPositionFromDepth(float2 uv, float depth, float4x4 invViewProjection) {
	float4 ndc = float4(TexToNDC(uv), depth, 1);
	float4 wp = mul(ndc, invViewProjection);
	return (wp / wp.w).xyz;
}
