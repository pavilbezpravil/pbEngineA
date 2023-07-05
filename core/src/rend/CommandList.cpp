#include "pch.h"
#include "CommandList.h"

#include "RendRes.h"
#include "shared/hlslCppShared.hlsli"

namespace pbe {

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

   void CommandList::ClearUAV_CS() {
      ID3D11UnorderedAccessView* uavs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
      pContext->CSSetUnorderedAccessViews(0, 8, uavs, nullptr);
   }

   void CommandList::SetCommonCB(int slot, Buffer* buffer, uint offsetInBytes, uint size) {
      auto dxBuffer = buffer->GetBuffer();

      offsetInBytes /= 16;

      pContext->VSSetConstantBuffers1(slot, 1, &dxBuffer, &offsetInBytes, &size);
      pContext->HSSetConstantBuffers1(slot, 1, &dxBuffer, &offsetInBytes, &size);
      pContext->DSSetConstantBuffers1(slot, 1, &dxBuffer, &offsetInBytes, &size);
      pContext->PSSetConstantBuffers1(slot, 1, &dxBuffer, &offsetInBytes, &size);

      pContext->CSSetConstantBuffers1(slot, 1, &dxBuffer, &offsetInBytes, &size);
   }

   void CommandList::SetCommonSRV(int slot, GPUResource& resource)
   {
      pContext->VSSetShaderResources(slot, 1, resource.srv.GetAddressOf());
      pContext->HSSetShaderResources(slot, 1, resource.srv.GetAddressOf());
      pContext->DSSetShaderResources(slot, 1, resource.srv.GetAddressOf());
      pContext->PSSetShaderResources(slot, 1, resource.srv.GetAddressOf());

      pContext->CSSetShaderResources(slot, 1, resource.srv.GetAddressOf());
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
