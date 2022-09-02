#include "pch.h"
#include "CommandList.h"

#include "RendRes.h"
#include "shared/common.hlsli"

namespace pbe {

   void CommandList::SetCommonSamplers() {
      std::pair<int, ID3D11SamplerState**> samplers[] = {
         {SAMPLER_SLOT_WRAP_POINT, &rendres::samplerStateWrapPoint},
         {SAMPLER_SLOT_WRAP_LINEAR, &rendres::samplerStateWrapLinear},
         {SAMPLER_SLOT_SHADOW, &rendres::samplerStateShadow},
      };

      for (auto [slot, sampler] : samplers) {
         pContext->VSSetSamplers(slot, 1, sampler);
         pContext->PSSetSamplers(slot, 1, sampler);

         pContext->CSSetSamplers(slot, 1, sampler);
      }
   }

}
