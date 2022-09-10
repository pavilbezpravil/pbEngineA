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
         std::string name;

         static Desc Vertex(std::string_view name, uint size) {
            Desc desc{};
            desc.Size = size;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.name = name;
            return desc;
         }

         static Desc Index(std::string_view name, uint size) {
            Desc desc{};
            desc.Size = size;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            desc.name = name;
            return desc;
         }

         static Desc Constant(std::string_view name, uint size) {
            Desc desc{};
            desc.Size = size;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.name = name;
            return desc;
         }

         static Desc Structured(std::string_view name, uint count, uint structureByteSize, uint bindFlags = D3D11_BIND_SHADER_RESOURCE) {
            Desc desc{};
            desc.Size = structureByteSize * count;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = bindFlags;
            desc.StructureByteStride = structureByteSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.name = name;

            return desc;
         }

         static Desc StructuredReadback(std::string_view name, uint count, uint structureByteSize) {
            Desc desc{};
            desc.Size = structureByteSize * count;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0; // cant be bounded
            desc.StructureByteStride = structureByteSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.name = name;

            return desc;
         }

         static Desc StructuredDynamic(std::string_view name, uint count, uint structureByteSize, uint bindFlags = D3D11_BIND_SHADER_RESOURCE) {
            Desc desc{};
            desc.Size = structureByteSize * count;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = bindFlags;
            desc.StructureByteStride = structureByteSize;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.name = name;

            return desc;
         }
      };

      static Ref<Buffer> Create(Desc& desc, void* data = nullptr);

      ID3D11Buffer* GetBuffer() { return (ID3D11Buffer*)pResource.Get(); }
      Desc GetDesc() const { return desc; }

      uint ElementsCount() const;

      bool Valid() const { return pResource; }

   private:
      friend Ref<Buffer>;

      Buffer(Desc& desc, void* data);

      Desc desc;
   };

}
