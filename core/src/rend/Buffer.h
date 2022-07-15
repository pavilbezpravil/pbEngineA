#pragma once
#include <d3d11.h>

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
            // desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            return desc;
         }
      };

      static Ref<Buffer> Create(Desc& desc, void* data = nullptr);

      ~Buffer() override;

      ID3D11Buffer* GetBuffer() { return (ID3D11Buffer*)pResource; }
      Desc GetDesc() const { return desc; }

      ID3D11ShaderResourceView* srv{};
      ID3D11UnorderedAccessView* uav{};

   private:
      friend Ref<Buffer>;

      Buffer(Desc& desc, void* data);

      Desc desc;
   };

}
