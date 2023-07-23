#include "pch.h"
#include "CommandList.h"
#include "Shader.h"

#include "RendRes.h"
#include "shared/hlslCppShared.hlsli"

namespace pbe {

   CmdBindsGuard::CmdBindsGuard(CommandList& cmd): cmd(cmd) {
      cmd.PushBindsGuard();
   }

   CmdBindsGuard::~CmdBindsGuard() {
      cmd.PopBindsGuard();
   }

   bool CommandList::SetCompute(GpuProgram& compute) {
      if (compute.Valid()) {
         pContext->CSSetShader(compute.cs->cs.Get(), nullptr, 0);
         return true;
      }
      return false;
   }

   void CommandList::UpdateSubresource(Buffer& buffer, const void* data, uint offset, size_t size) {
      if (buffer.Valid() && size > 0 && data) {
         D3D11_BOX box{};
         box.left = offset;
         box.right = offset + (uint)size;
         box.bottom = 1;
         box.back = 1;

         pContext->UpdateSubresource1(buffer.GetBuffer(), 0,
            size == -1 ? nullptr : &box, data, 0, 0, D3D11_COPY_NO_OVERWRITE);
      }
   }

   void CommandList::ClearSRV_CS() {
      ID3D11ShaderResourceView* srvs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
      pContext->CSSetShaderResources(0, _countof(srvs), srvs);
   }

   void CommandList::ClearUAV_CS() {
      ID3D11UnorderedAccessView* uavs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
      pContext->CSSetUnorderedAccessViews(0, _countof(uavs), uavs, nullptr);
   }

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
