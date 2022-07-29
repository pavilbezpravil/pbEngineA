#include "pch.h"
#include "Buffer.h"
#include "Device.h"
#include "core/Assert.h"
#include "core/Log.h"

namespace pbe {

   Ref<Buffer> Buffer::Create(Desc& desc, void* data) {
      return Ref<Buffer>::Create(desc, data);
   }

   int Buffer::ElementsCount() const {
      return desc.Size / desc.StructureByteStride;
   }

   Buffer::Buffer(Desc& desc, void* data) : GPUResource(nullptr), desc(desc) {
      D3D11_BUFFER_DESC dxDesc{};

      dxDesc.ByteWidth = desc.Size;
      dxDesc.Usage = desc.Usage;
      dxDesc.BindFlags = desc.BindFlags;
      dxDesc.CPUAccessFlags = desc.CPUAccessFlags;
      dxDesc.StructureByteStride = desc.StructureByteStride;
      dxDesc.MiscFlags = desc.MiscFlags;

      if (desc.Size > 0) {
         ID3D11Device* pDevice = sDevice->g_pd3dDevice;

         D3D11_SUBRESOURCE_DATA initialData = { data };

         ID3D11Buffer* pBuffer;
         pDevice->CreateBuffer(&dxDesc, data ? &initialData : nullptr, &pBuffer);
         if (!pBuffer) {
            WARN("Cant create buffer!");
            return;
         }

         SetDbgName(desc.name);

         if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
            pDevice->CreateShaderResourceView(pBuffer, NULL, srv.GetAddressOf());
         }
         if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
            pDevice->CreateUnorderedAccessView(pBuffer, NULL, uav.GetAddressOf());
         }

         pResource = pBuffer;
         pBuffer->Release();
      }
   }
}
