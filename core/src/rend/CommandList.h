#pragma once
// todo:
#include "Buffer.h"
#include "Device.h"
#include "Texture2D.h"
#include "core/Core.h"

namespace pbe {

   class CommandList {
   public:
      CommandList(ID3D11DeviceContext3* pContext) : pContext(pContext) {

      }

      // CommandList() {
      //    sDevice->g_pd3dDevice->CreateDeferredContext(0, &pContext);
      // }

      void UpdateSubresource(Buffer& buffer, void* data, uint offset = 0, size_t size = -1) {
         if (buffer.Valid() && size > 0) {
            D3D11_BOX box{};
            box.left = offset;
            box.right = offset + (uint)size;
            box.bottom = 1;
            box.back = 1;
            pContext->UpdateSubresource(buffer.GetBuffer(), 0, size == -1 ? nullptr : &box, data, 0, 0);
            // pContext->UpdateSubresource(buffer.GetBuffer(), 0, nullptr, data, 0, 0);
         }
      }

      void ClearDepthTarget(Texture2D& depth, float depthValue, uint8 stencilValue = 0, uint clearFlags = D3D11_CLEAR_DEPTH) {
         pContext->ClearDepthStencilView(depth.dsv, clearFlags, depthValue, stencilValue);
      }

      void ClearRenderTarget(Texture2D& rt, const vec4& color) {
         pContext->ClearRenderTargetView(rt.rtv, &color.x);
      }

      void SetRenderTargets(Texture2D* rt, Texture2D* depth = nullptr) {
         int nRtvs = rt ? 1 : 0;
         ID3D11RenderTargetView** rtv = rt ? &rt->rtv : nullptr;
         ID3D11DepthStencilView* dsv = depth ? depth->dsv : nullptr;

         pContext->OMSetRenderTargets(nRtvs, rtv, dsv);
      }

      void SetDepthStencilState(ID3D11DepthStencilState* state, uint stencilRef = 0) {
         pContext->OMSetDepthStencilState(state, stencilRef);
      }

      void SetBlendState(ID3D11BlendState* state, const vec4& blendFactor = vec4{1.f}) {
         pContext->OMSetBlendState(state, &blendFactor.x, 0xffffffff);
      }

      void SetRasterizerState(ID3D11RasterizerState1* state) {
         pContext->RSSetState(state);
      }

      void SetViewport(vec2 top, vec2 size, vec2 minMaxDepth = {0, 1}) {
         D3D11_VIEWPORT viewport = {
            top.x, top.y, size.x, size.y, minMaxDepth.x, minMaxDepth.y
         };
         pContext->RSSetViewports(1, &viewport);
      }

      void BeginEvent(std::string_view name) {
         pContext->BeginEventInt(std::wstring(name.begin(), name.end()).data(), 0);
      }

      void EndEvent() {
         pContext->EndEvent();
      }


      ID3D11CommandList* GetD3DCommandList() {
         return nullptr;
      }


      ID3D11DeviceContext3* pContext{};
   };

   struct GpuMarker {
      GpuMarker(CommandList& cmd, std::string_view name) : cmd(cmd) {
         cmd.BeginEvent(name);
      }

      ~GpuMarker() {
         cmd.EndEvent();
      }

      CommandList& cmd;
   };

#define GPU_MARKER(Name) GpuMarker marker{ cmd, Name };

}
