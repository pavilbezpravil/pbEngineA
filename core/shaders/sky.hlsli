#ifndef SKY_HEADER
#define SKY_HEADER

#include "commonResources.hlsli"

// float3 GetSkyColorSimple(float3 direction) {
//    float t = 0.5 * (direction.y + 1);
//    float3 skyColor = lerp(1, float3(0.5, 0.7, 1.0), t);
//    return skyColor;
// }

// todo: move
// https://www.shadertoy.com/view/lt2SR1
float3 GetSkyColor(float3 rd ) {
    float3 sundir = -gScene.directLight.direction;
    
    float yd = min(rd.y, 0.);
    rd.y = max(rd.y, 0.);
    
    float3 col = 0;
    
    col += float3(.4, .4 - exp( -rd.y*20. )*.15, .0) * exp(-rd.y*9.); // Red / Green 
    col += float3(.3, .5, .6) * (1. - exp(-rd.y*8.) ) * exp(-rd.y*.9) ; // Blue
    
    col = lerp(col * 1.2, 0.3,  1 - exp(yd*100.)); // Fog
    
    col += float3(1.0, .8, .55) * pow( max(dot(rd,sundir),0.), 15. ) * .6; // Sun
    col += pow(max(dot(rd, sundir),0.), 150.0) *.15;
    
    return col * gScene.skyIntensity;
}

#endif
