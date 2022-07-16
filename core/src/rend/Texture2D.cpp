#include "pch.h"
#include "Texture2D.h"
#include "Device.h"
#include "core/Assert.h"

namespace pbe {

   template class Ref<Texture2D>;

   Ref<Texture2D> Texture2D::Create(ID3D11Texture2D* pTexture) {
      return Ref<Texture2D>::Create(pTexture);
   }

   Ref<Texture2D> Texture2D::Create(Desc& desc) {
      return Ref<Texture2D>::Create(desc);
   }

   Texture2D::~Texture2D() {
      GPUResource::~GPUResource();
      SAFE_RELEASE(rtv);
      SAFE_RELEASE(dsv);
      SAFE_RELEASE(srv);
   }

   Texture2D::Desc Texture2D::GetDesc() const {
      return desc;
   }

   Texture2D::Texture2D(ID3D11Texture2D* pTexture) : GPUResource(pTexture) {
      pTexture->Release();

      ID3D11Device* pDevice;
      pTexture->GetDevice(&pDevice);

      pDevice->CreateRenderTargetView(pTexture, NULL, &rtv);

      pDevice->Release();

      D3D11_TEXTURE2D_DESC dxDesc;
      pTexture->GetDesc(&dxDesc);

      desc.bindFlags = dxDesc.BindFlags;
      desc.size = { dxDesc.Width, dxDesc.Height};
      desc.format = dxDesc.Format;
   }

   Texture2D::Texture2D(Desc& desc) : GPUResource(nullptr), desc(desc) {
      D3D11_TEXTURE2D_DESC dxDesc{};

      dxDesc.Width = desc.size.x;
      dxDesc.Height = desc.size.y;
      dxDesc.ArraySize = 1;
      dxDesc.SampleDesc.Count = 1;
      dxDesc.Format = desc.format;
      dxDesc.BindFlags = desc.bindFlags;

      ID3D11Device* pDevice = sDevice->g_pd3dDevice;

      ID3D11Texture2D* pTexture;
      pDevice->CreateTexture2D(&dxDesc, nullptr, &pTexture);
      if (!pTexture) {
         WARN("Cant create texture!");
         return;
      }

      if (desc.bindFlags & D3D11_BIND_RENDER_TARGET) {
         pDevice->CreateRenderTargetView(pTexture, NULL, &rtv);
      }
      if (desc.bindFlags & D3D11_BIND_SHADER_RESOURCE) {
         pDevice->CreateShaderResourceView(pTexture, NULL, &srv);
      }
      if (desc.bindFlags & D3D11_BIND_DEPTH_STENCIL) {
         pDevice->CreateDepthStencilView(pTexture, NULL, &dsv);
      }
      if (desc.bindFlags & D3D11_BIND_UNORDERED_ACCESS) {
         pDevice->CreateUnorderedAccessView(pTexture, NULL, &uav);
      }

      pResource = pTexture;
      pTexture->Release();
   }

}
