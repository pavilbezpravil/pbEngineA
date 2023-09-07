#include "shared/rt.hlsli"
#include "commonResources.hlsli"
#include "common.hlsli"
#include "math.hlsli"
#include "random.hlsli"

RWTexture2D<float4> gColorOut : register(u0);

cbuffer gRTConstantsCB : register(b0) {
  SRTConstants gRTConstants;
}

// todo: register s?
Texture2D<float> gDepthPrev : register(s1);
Texture2D<float> gDepth : register(s2);

Texture2D<float4> gNormal : register(s4);

Texture2D<float4> gHistory : register(s5); // prev
Texture2D<uint> gReprojectCount : register(s6); // prev

Texture2D<float4> gColor : register(s7);

RWTexture2D<float4> gHistoryOut : register(u4);
RWTexture2D<uint> gReprojectCountOut : register(u5);

// #define ENABLE_REPROJECTION

[numthreads(8, 8, 1)]
void HistoryAccCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    // float w = gRTConstants.historyWeight;
    // float3 history = gHistoryOut[id.xy].xyz * w + gColorOut[id.xy].xyz * (1 - w);
    // float4 color = float4(history, 1);

    // gColorOut[id.xy] = color;
    // gHistoryOut[id.xy] = color;

    #ifdef ENABLE_REPROJECTION
        float depth = gDepth[id];

        float2 uv = (float2(id) + 0.5) / float2(gCamera.rtSize);
        float3 posW = GetWorldPositionFromDepth(uv, depth, gCamera.invViewProjection);

        float4 prevSample = mul(gCamera.prevViewProjection, float4(posW, 1));
        prevSample /= prevSample.w;

        float2 prevUV = NDCToTex(prevSample.xy);
        uint2 prevSampleIdx = prevUV * gCamera.rtSize;

        bool prevInScreen = all(prevUV > 0 && prevUV < 1); // todo: get info from neigh that are in screen
        // bool prevInScreen = all(prevUV > 0.02 && prevUV < 0.98); // todo: get info from neigh that are in screen

        float historyWeight = gRTConstants.historyWeight;

        float3 color = gColorOut[id].xyz;

        float3 normal = NormalFromTex(gNormal[id].xyz);
        // validDepth = depthRaw != 1
        //            || gDepth[id + int2( 1, 0)] != 1
        //            || gDepth[id + int2(-1, 0)] != 1
        //            || gDepth[id + int2( 0, 1)] != 1
        //            || gDepth[id + int2( 0,-1)] != 1
        //            ;

        float3 dbgColor = 0;

        bool successReproject = true;

        if (prevInScreen) {
            float3 normalPrev = NormalFromTex(gNormalPrev[prevSampleIdx].xyz);

            float normalCoeff = pow(max(dot(normal, normalPrev), 0), 3); // todo:
            bool normalFail = normalCoeff <= 0;
            historyWeight *= normalCoeff;

            float prevDepth = gDepthPrev[prevSampleIdx];
            float3 prevPosW = GetWorldPositionFromDepth(prevUV, prevDepth, gCamera.prevInvViewProjection);

            // gRTConstants.historyMaxDistance
            float depthCoeff = saturate(2 - distance(posW, prevPosW) / 0.1);
            // depthCoeff = 1;
            bool depthFail = depthCoeff <= 0;
            historyWeight *= depthCoeff;

            // dbgColor.r += 1 - depthCoeff;

            if (normalFail || depthFail) {
                historyWeight = 0;
                successReproject = false;

                if (gRTConstants.debugFlags & DBG_FLAG_SHOW_NEW_PIXEL) {
                    dbgColor += float3(1, 0, 0);
                }
            }
        } else {
            successReproject = false;
            historyWeight = 0;
        }

        if (successReproject) {
            int nRays = gRTConstants.nRays;

            uint oldCount = gReprojectCount[prevSampleIdx];
            uint nextCount = oldCount + nRays;
            historyWeight *= float(oldCount) / float(nextCount);

            gReprojectCountOut[id] = min(nextCount, 255);

            // float3 historyColor = gHistory.SampleLevel(gSamplerPoint, prevUV, 0);
            // float3 historyColor = gHistory.SampleLevel(gSamplerLinear, prevUV, 0);
            float3 historyColor = gHistory[prevSampleIdx].xyz;
            color = lerp(color, historyColor, historyWeight);
        } else {
            gReprojectCountOut[id] = 0;
        }

        float4 color4 = float4(color, 1);

        gHistoryOut[id] = color4;
        // dbgColor.r += saturate(float(255 - gReprojectCountOut[id]) / 255);
        color4.xyz += dbgColor;
        // color4.xyz = normal;
        gColorOut[id] = color4;
    #endif
}


#define METHOD 1

[numthreads(8, 8, 1)]
void DenoiseCS (uint2 id : SV_DispatchThreadID) {
    if (any(id >= gRTConstants.rtSize)) {
        return;
    }

    float3 color = gColor[id].xyz;

#if METHOD == 0
    gHistoryOut[id] = float4(color, 1);
    gColorOut[id] = float4(color, 1);
#elif METHOD == 1

    uint seed = id.x * 214234 + id.y * 521334 + asuint(gRTConstants.random01); // todo: first attempt, dont thing about it

    // todo: mb tex size different from rt size?
    float2 rtSizeInv = 1 / float2(gCamera.rtSize);

    float2 uv = (float2(id) + 0.5) * rtSizeInv;

    float depth = gDepth[id];
    
    if (depth > 0.999) {
        gHistoryOut[id] = float4(color, 1);
        gColorOut[id] = float4(color, 1);
        return;
    }
    
    // float2 motion = uMotion.Sample(MinMagMipNearestWrapEdge, input.texCoord).rg;
    float2 motion = 0;
    float3 normal = NormalFromTex(gNormal[id].xyz);
    
    float3 posW = GetWorldPositionFromDepth(uv, depth, gCamera.invViewProjection);

    float3 colNew = color;

    float3 org = colNew;
    float3 mi = colNew;
    float3 ma = colNew;
    
    float tot = 1.0;
    
    const float goldenAngle = 2.39996323;
    const float size = 6.0;
    const float radInc = 1.0;
    
    float radius = 0.5;
    
    // todo: blue noise
    for (float ang = RandomFloat(seed) * goldenAngle; radius < size; ang += goldenAngle) {
        radius += radInc;
        float2 sampleUV = uv + float2(cos(ang), sin(ang)) * rtSizeInv * radius;
        
        float weight = saturate(1.2 - radius / size);
        
        // Check depth
        float sampleDepth = gDepth.SampleLevel(gSamplerPointClamp, sampleUV, 0);
        float3 samplePosW = GetWorldPositionFromDepth(sampleUV, sampleDepth, gCamera.invViewProjection);

        float3 toSample = normalize(samplePosW - posW);
        float toSampleDotN = dot(toSample, normal);
        weight *= step(abs(toSampleDotN), 0.2);
        
        // Check normal
        float3 sampleNormal = NormalFromTex(gNormal.SampleLevel(gSamplerPointClamp, sampleUV, 0).xyz);
        weight *= step(0.9, dot(normal, sampleNormal));
        
        float3 sampleColor = gColor.SampleLevel(gSamplerPointClamp, sampleUV, 0).xyz;
        float3 mc = lerp(org, sampleColor, weight);
        
        mi = min(mi, mc);
        ma = max(ma, mc);
        
        colNew += weight * sampleColor;
        tot += weight;
    }
    
    colNew /= tot;
    
    float3 colOld = gHistory.SampleLevel(gSamplerPointClamp, uv - motion, 0).xyz;
    colOld = clamp(colOld, mi, ma);

    colNew = lerp(colNew, colOld, gRTConstants.historyWeight);

    gHistoryOut[id] = float4(colNew, 1);
    gColorOut[id] = float4(colNew, 1);

#endif
}
