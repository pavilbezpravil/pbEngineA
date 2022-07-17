float2 TexToNDC(float2 uv) {
   uv = uv * 2 - 1;
   uv.y *= -1;
   return uv;
}

float2 NDCToTex(float2 ndc) {
   ndc.y *= -1;
   return (ndc + 1) * 0.5;
}
