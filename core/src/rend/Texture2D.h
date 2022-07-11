#pragma once
#include <d3d11.h>
#include <string_view>

#include "Common.h"
#include "core/Common.h"
#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {

   class GPUResource : public RefCounted {
      NON_COPYABLE(GPUResource);
   public:
      GPUResource(ID3D11Resource* pResource) : pResource(pResource) {

      }

      virtual ~GPUResource() {
         SAFE_RELEASE(pResource);
      }

      void SetDbgName(std::string_view dbgName) {
         ::pbe::SetDbgName(pResource, dbgName);
      }

      ID3D11Resource* pResource{};
   };

   class Texture2D : public GPUResource {
   public:

      struct Desc {
         int2 size;
         DXGI_FORMAT format;
         UINT bindFlags;
      };

      static Ref<Texture2D> Create(ID3D11Texture2D* pTexture);
      static Ref<Texture2D> Create(Desc& desc);

      ~Texture2D() override;

      ID3D11Texture2D* GetTexture2D() { return (ID3D11Texture2D*)pResource; }
      Desc GetDesc() const;

      ID3D11RenderTargetView* rtv{};
      ID3D11DepthStencilView* dsv{};
      ID3D11ShaderResourceView* srv{};

      // todo: Ref::Create cant access to private member
   private:
      friend Ref<Texture2D>;

      Texture2D(ID3D11Texture2D* pTexture);
      Texture2D(Desc& desc);

      Desc desc;
   };

   extern template class Ref<Texture2D>;

}
