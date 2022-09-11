#include "shared/hlslCppShared.hlsli"
#include "common.hlsli"
#include "samplers.hlsli"

cbuffer gTestCB {
   uint offset;
   uint instanceCount;
}

RWBuffer<uint> gIndirectArgsInstanceCount;

[numthreads(1, 1, 1)]
void indirectArgsTest( uint3 dispatchThreadID : SV_DispatchThreadID ) { 
   if (dispatchThreadID.x == 0) {
      gIndirectArgsInstanceCount[offset + 1] = instanceCount;
   }
}
