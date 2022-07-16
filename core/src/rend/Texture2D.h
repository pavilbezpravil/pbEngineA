#pragma once
#include <d3d11.h>

#include "GpuResource.h"
#include "core/Ref.h"
#include "math/Types.h"

namespace pbe {

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

      ID3D11Texture2D* GetTexture2D() { return (ID3D11Texture2D*)pResource.Get(); }
      Desc GetDesc() const;

      ID3D11RenderTargetView* rtv{};
      ID3D11DepthStencilView* dsv{};
      ID3D11ShaderResourceView* srv{};
      ID3D11UnorderedAccessView* uav{};

   private:
      friend Ref<Texture2D>;

      Texture2D(ID3D11Texture2D* pTexture);
      Texture2D(Desc& desc);

      Desc desc;
   };

   extern template class Ref<Texture2D>;

}
