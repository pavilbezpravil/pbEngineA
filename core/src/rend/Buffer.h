#pragma once
#include <d3d11.h>

#include "Common.h"
#include "GpuResource.h"
#include "math/Types.h"

namespace pbe {

   class Buffer : public GPUResource {
   public:

      struct Desc {
         uint Size;
         D3D11_USAGE Usage;
         uint BindFlags;
         uint CPUAccessFlags = 0;
         uint StructureByteStride = 0;
         uint MiscFlags = 0;

         static Desc VertexBuffer(int size) {
            Desc desc{};
            desc.Size = size;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            return desc;
         }

         static Desc IndexBuffer(int size) {
            Desc desc{};
            desc.Size = size;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            return desc;
         }

         static Desc ConstantBuffer(int size) {
            Desc desc{};
            desc.Size = size;
            // desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

            return desc;
         }

         static Desc StructureBuffer(int count, uint structureByteSize) {
            Desc desc{};
            desc.Size = structureByteSize * count;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.StructureByteStride = structureByteSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

            return desc;
         }
      };

      static Ref<Buffer> Create(Desc& desc, void* data = nullptr);

      ID3D11Buffer* GetBuffer() { return (ID3D11Buffer*)pResource.Get(); }
      Desc GetDesc() const { return desc; }

      int ElementsCount() const;

      bool Valid() const { return pResource; }

      ComPtr<ID3D11ShaderResourceView> srv;
      ComPtr<ID3D11UnorderedAccessView> uav;

   private:
      friend Ref<Buffer>;

      Buffer(Desc& desc, void* data);

      Desc desc;
   };

}
