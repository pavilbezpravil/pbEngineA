#include "Texture2D.h"

#include "Device.h"

template class Ref<Texture2D>;

Texture2D::Texture2D(ID3D11Texture2D* pTexture): GPUResource(pTexture) {
   ID3D11Device* pDevice;
   pTexture->GetDevice(&pDevice);

   pDevice->CreateRenderTargetView(pTexture, NULL, &rtv);

   pDevice->Release();
}
