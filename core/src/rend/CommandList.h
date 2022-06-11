#pragma once
// todo:
#include "Device.h"
#include "Texture2D.h"

class CommandList {
public:
   CommandList(ID3D11DeviceContext* pContext) : pContext(pContext) {

   }

   // CommandList() {
   //    sDevice->g_pd3dDevice->CreateDeferredContext(0, &pContext);
   // }


   void SetRenderTargets(Texture2D* rt, Texture2D* depth = nullptr) {
      int nRtvs = rt ? 1 : 0;
      ID3D11RenderTargetView** rtv = rt ? &rt->rtv : nullptr;
      ID3D11DepthStencilView* dsv = depth ? depth->dsv : nullptr;

      pContext->OMSetRenderTargets(nRtvs, rtv, dsv);
   }

   void ClearRenderTarget(Texture2D& rt, const vec4& color) {
      pContext->ClearRenderTargetView(rt.rtv, &color.x);
   }


   ID3D11CommandList* GetD3DCommandList() {
      return nullptr;
   }


   ID3D11DeviceContext* pContext{};
};
