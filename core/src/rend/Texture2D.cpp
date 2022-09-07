#include "pch.h"
#include "Texture2D.h"
#include "Device.h"
#include "core/Assert.h"

namespace pbe {

   static bool IsTypelessFormat(DXGI_FORMAT format) {
      // todo:
      return format == DXGI_FORMAT_R24G8_TYPELESS;
   }
   
   static DXGI_FORMAT FormatToDepth(DXGI_FORMAT format) {
      if (format == DXGI_FORMAT_R24G8_TYPELESS) {
         return DXGI_FORMAT_D24_UNORM_S8_UINT;
      }
      if (format == DXGI_FORMAT_R16_TYPELESS) {
         return DXGI_FORMAT_D16_UNORM;
      }
      return format;
   }
   
   static DXGI_FORMAT FormatToSrv(DXGI_FORMAT format) {
      if (format == DXGI_FORMAT_R24G8_TYPELESS) {
         return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      }
      if (format == DXGI_FORMAT_R16_TYPELESS) {
         return DXGI_FORMAT_R16_UNORM;
      }
      return format;
   }

   template class Ref<Texture2D>;

   Ref<Texture2D> Texture2D::Create(ID3D11Texture2D* pTexture) {
      return Ref<Texture2D>::Create(pTexture);
   }

   Ref<Texture2D> Texture2D::Create(Desc& desc) {
      return Ref<Texture2D>::Create(desc);
   }

   Texture2D::Desc Texture2D::GetDesc() const {
      return desc;
   }

   Texture2D::Texture2D(ID3D11Texture2D* pTexture) : GPUResource(pTexture) {
      pTexture->Release();

      ID3D11Device* pDevice;
      pTexture->GetDevice(&pDevice);

      pDevice->CreateRenderTargetView(pTexture, NULL, rtv.GetAddressOf());

      pDevice->Release();

      D3D11_TEXTURE2D_DESC dxDesc;
      pTexture->GetDesc(&dxDesc);

      desc.bindFlags = dxDesc.BindFlags;
      desc.size = { dxDesc.Width, dxDesc.Height};
      desc.format = dxDesc.Format;
   }

   Texture2D::Texture2D(Desc& _desc) : GPUResource(nullptr), desc(_desc) {
      // todo: calc Mips
      if (desc.mips == 0) {
         int2 size = desc.size;
         int nMips = 0;

         while (size.x > 0 || size.y > 0) {
            size /= 2;
            nMips++;
         }

         desc.mips = nMips;
      }

      D3D11_TEXTURE2D_DESC dxDesc{};

      dxDesc.Width = desc.size.x;
      dxDesc.Height = desc.size.y;
      dxDesc.MipLevels = desc.mips;
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

      SetDbgName(desc.name);

      INFO("Created texture '{}': {}", desc.name, desc.size);

      if (desc.bindFlags & D3D11_BIND_RENDER_TARGET) {
         pDevice->CreateRenderTargetView(pTexture, NULL, rtv.GetAddressOf());
      }
      if (desc.bindFlags & D3D11_BIND_SHADER_RESOURCE) {
         D3D11_SHADER_RESOURCE_VIEW_DESC dxSrv{};
         dxSrv.Format = FormatToSrv(desc.format);
         dxSrv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
         dxSrv.Texture2D.MipLevels = 1; // todo:
         dxSrv.Texture2D.MostDetailedMip = 0;

         pDevice->CreateShaderResourceView(pTexture, &dxSrv, srv.GetAddressOf());
         // pDevice->CreateShaderResourceView(pTexture, NULL, srv.GetAddressOf());

         if (desc.mips != 1) {
            int nMips = desc.mips;
            mipsSrv.resize(nMips);

            for (int iMip = 0; iMip < nMips; ++iMip) {
               D3D11_SHADER_RESOURCE_VIEW_DESC dxSrv{};
               dxSrv.Format = FormatToSrv(desc.format);
               dxSrv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
               dxSrv.Texture2D.MipLevels = 1;
               dxSrv.Texture2D.MostDetailedMip = iMip;

               pDevice->CreateShaderResourceView(pTexture, &dxSrv, mipsSrv[iMip].GetAddressOf());
            }
         }
      }
      if (desc.bindFlags & D3D11_BIND_DEPTH_STENCIL) {
         D3D11_DEPTH_STENCIL_VIEW_DESC dxDsv{};
         dxDsv.Format = FormatToDepth(desc.format);
         dxDsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
         dxDsv.Texture2D.MipSlice = 0;

         pDevice->CreateDepthStencilView(pTexture, &dxDsv, dsv.GetAddressOf());
         // pDevice->CreateDepthStencilView(pTexture, NULL, dsv.GetAddressOf());
      }
      if (desc.bindFlags & D3D11_BIND_UNORDERED_ACCESS) {
         pDevice->CreateUnorderedAccessView(pTexture, NULL, uav.GetAddressOf());

         if (desc.mips != 1) {
            int nMips = desc.mips;
            mipsUav.resize(nMips);

            for (int iMip = 0; iMip < nMips; ++iMip) {
               D3D11_UNORDERED_ACCESS_VIEW_DESC dxSrv{};
               dxSrv.Format = desc.format;
               dxSrv.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
               dxSrv.Texture2D.MipSlice = iMip;

               pDevice->CreateUnorderedAccessView(pTexture, &dxSrv, mipsUav[iMip].GetAddressOf());
            }
         }
      }

      pResource = pTexture;
      pTexture->Release();
   }

}
