#pragma once
#include <d3d11.h>

#include "core/Ref.h"

class GPUResource : public RefCounted {
public:
   GPUResource(ID3D11Resource* pResource) : pResource(pResource) {

   }

   virtual ~GPUResource() {
      pResource->Release();
   }

   ID3D11Resource* pResource{};
};

class Texture2D : public GPUResource {
public:
   Texture2D(ID3D11Texture2D* pTexture);

   ~Texture2D() override {
      rtv->Release();
   }

   ID3D11Texture2D* GetTexture2D() { return (ID3D11Texture2D*)pResource; }

   ID3D11RenderTargetView* rtv{};
};
